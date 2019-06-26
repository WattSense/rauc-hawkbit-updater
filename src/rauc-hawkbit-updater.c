/**
 * @file rauc-hawkbit-updater.c
 * @author Lasse Mikkelsen <lkmi@prevas.dk>
 * @date 19 Sep 2018
 * @brief RAUC HawkBit updater daemon
 *
 */

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/reboot.h>
#include "config-file.h"
#include "hawkbit-client.h"
#include "log.h"
#include "rauc-installer.h"

#define PROGRAM "rauc-hawkbit-updater"
#define VERSION 0.1

#define WATTSENSE

// program arguments
static gchar* config_file = NULL;
static gboolean opt_version = FALSE;
static gboolean opt_debug = FALSE;
static gboolean opt_run_once = FALSE;
static gboolean opt_output_systemd = FALSE;

// Commandline options
static GOptionEntry entries[] = {
    {"config-file", 'c', G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME, &config_file,
     "Configuration file", NULL},
    {"version", 'v', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &opt_version, "Version information",
     NULL},
    {"debug", 'd', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &opt_debug, "Enable debug output", NULL},
    {"run-once", 'r', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &opt_run_once,
     "Check and install new software and exit", NULL},
#ifdef WITH_SYSTEMD
    {"output-systemd", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &opt_output_systemd,
     "Enable output to systemd", NULL},
#endif
    {NULL}};

// hawkbit callbacks
static GSourceFunc notify_hawkbit_install_progress;
static GSourceFunc notify_hawkbit_install_complete;

#ifdef WATTSENSE
static gint read_apps_version(gchar** apps_version) {
    const gchar* filename = "/opt/wattsense/app-version.txt";
    gsize len;
    FILE* f;
    GError* error = NULL;

    if (!g_file_get_contents(filename, apps_version, &len, &error)) {
    	if (error != NULL) {
    		g_printerr("Unable to read file: %s\n", error->message);
   		    g_error_free (error);
    	}
        *apps_version = g_strdup("0.0.0");
        return false;
    }
    if (len == 0) {
        *apps_version = g_strdup("0.0.0");
        return false;
    } else
        return true;
}

static gint read_rootfs_version(gchar** rootfs_version) {
    const gchar* filename = "/etc/os-release";
    gchar line[128];
    gsize len;
    FILE* fp;
    GError* error = NULL;

    fp = g_fopen(filename, "r");
    if (fp == NULL) {
        *rootfs_version = g_strdup("0.0.0");
        return false;
    }
    while (!feof(fp)) {
        char version[32];

        fgets(line, 128, fp);
        if (sscanf(line, "VERSION_ID=\"%32[^\"]\"", version) != 0) {
            fclose(fp);
            *rootfs_version = g_strdup(version);
            return true;
        }
    }

    fclose(fp);
    *rootfs_version = g_strdup("0.0.0");
    return false;
}
#endif

/**
 * @brief RAUC callback when new progress.
 */
static gboolean on_rauc_install_progress_cb(gpointer data) {
    struct install_context* context = data;

    g_mutex_lock(&context->status_mutex);
    while (!g_queue_is_empty(&context->status_messages)) {
        gchar* msg = g_queue_pop_head(&context->status_messages);
        g_message("Installing: %s : %s", context->bundle, msg);
        // notify hawkbit server about progress
        notify_hawkbit_install_progress(msg);
        g_free(msg);
    }
    g_mutex_unlock(&context->status_mutex);

    return G_SOURCE_REMOVE;
}

/**
 * @brief RAUC callback when install is complete.
 */
#define FILE_DO_REBOOT "/tmp/.do_reboot"

static gboolean on_rauc_install_complete_cb(gpointer data) {
    struct install_context* context = data;

    struct on_install_complete_userdata userdata = {.install_success =
                                                        (context->status_result == 0)};
    // lets notify hawkbit with install result
    notify_hawkbit_install_complete(&userdata);

    if( access(FILE_DO_REBOOT, F_OK ) != -1 ) {
    	sync();
    	reboot(RB_AUTOBOOT);
    }

    return G_SOURCE_REMOVE;
}

/**
 * @brief hawkbit callback when new software is available.
 */
static gboolean on_new_software_ready_cb(gpointer data) {
    struct on_new_software_userdata* userdata = data;
    notify_hawkbit_install_progress = userdata->install_progress_callback;
    notify_hawkbit_install_complete = userdata->install_complete_callback;
    rauc_install(userdata->file, on_rauc_install_progress_cb, on_rauc_install_complete_cb);

    return G_SOURCE_REMOVE;
}

int main(int argc, char** argv) {
    GError* error = NULL;
    GOptionContext* context;
    gint exit_code = 0;
    gchar** args;
    GLogLevelFlags log_level;
    gchar* apps_version;
    gchar* apps_rootfs;

    // Lets support unicode filenames
    args = g_strdupv(argv);

    context = g_option_context_new("");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse_strv(context, &args, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        g_error_free(error);
        exit_code = 1;
        goto out;
    }

    if (opt_version) {
        g_printf("Version %.1f\n", VERSION);
        goto out;
    }

#ifndef WATTSENSE
    if (!g_file_test(config_file, G_FILE_TEST_EXISTS)) {
        g_printerr("No such configuration file: %s\n", config_file);
        exit_code = 3;
        goto out;
    }

    if (opt_run_once) {
        run_once = TRUE;
        force_check_run = TRUE;
    }

    struct config* config = load_config_file(config_file, &error);
    if (config == NULL) {
        g_printerr("Loading config file failed: %s\n", error->message);
        g_error_free(error);
        exit_code = 4;
        goto out;
    }
#else
    /* Create hardcoded config for Wattsense */

    if (!device_id_init()) {
        exit_code = 5;
        goto out;
    }
    struct config* config = g_new0(struct config, 1);

    config->controller_id = g_new(char, 32);
    g_stpcpy(config->controller_id, device_id_get());
    config->auth_token = g_new(char, 32);
    g_stpcpy(config->auth_token, device_id_get());
    config->ssl = true;
    config->ssl_verify = true;
    config->hawkbit_server = g_strdup("eagle.wattsense.com");
    config->tenant_id = g_strdup("DEFAULT");
    config->bundle_download_location = g_strdup("/tmp/bundle.raucb");
    config->connect_timeout = 40;  // in sec
    config->timeout = 60;
    config->retry_wait = 60;
    config->log_level = G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING;
    config->device = g_hash_table_new(g_str_hash, g_str_equal);
    read_apps_version(&apps_version);
    g_hash_table_insert(config->device, "APP_VERS", g_strstrip(apps_version));
    read_rootfs_version(&apps_rootfs);
    g_hash_table_insert(config->device, "ROOTFS_VERS", g_strstrip(apps_rootfs));

#endif
    if (opt_debug) {
        log_level = G_LOG_LEVEL_MASK;
    } else {
        log_level = config->log_level;
    }

    setup_logging(PROGRAM, log_level, opt_output_systemd);
    hawkbit_init(config, on_new_software_ready_cb);

    identify(); /* Identify ourselve to push new attibuts */
    force_check_run = true;
    hawkbit_start_service_sync();

#ifndef WATTSENSE
    config_file_free(config);
#endif

out:
    g_option_context_free(context);
    g_strfreev(args);
    return exit_code;
}
