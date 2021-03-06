/**
 * @file config-file.c
 * @author Lasse Mikkelsen <lkmi@prevas.dk>
 * @date 19 Sep 2018
 * @brief Configuration file parser
 *
 */

#include "config-file.h"

static const gint DEFAULT_CONNECTTIMEOUT  = 20;     // 20 sec.
static const gint DEFAULT_TIMEOUT         = 60;     // 1 min.
static const gint DEFAULT_RETRY_WAIT      = 5 * 60; // 5 min.
static const gboolean DEFAULT_SSL         = TRUE;
static const gboolean DEFAULT_SSL_VERIFY  = TRUE;
static const gchar * DEFAULT_LOG_LEVEL    = "message";

void config_file_free(struct config *config)
{
        g_free(config->hawkbit_server);
        g_free(config->controller_id);
        g_free(config->tenant_id);
        g_free(config->auth_token);
        g_free(config->bundle_download_location);
        g_hash_table_destroy(config->device);
}

gboolean get_key_string(GKeyFile *key_file, const gchar* group, const gchar* key, gchar** value, const gchar* default_value, GError **error)
{
        gchar *val = NULL;
        val = g_key_file_get_string(key_file, group, key, error);
        if (val == NULL) {
                if (default_value != NULL) {
                        *value = g_strdup(default_value);
                        return TRUE;
                }
                return FALSE;
        }
        *value = val;
        return TRUE;
}

gboolean get_key_bool(GKeyFile *key_file, const gchar* group, const gchar* key, gboolean* value, const gboolean default_value, GError **error)
{
        g_autofree gchar *val = NULL;
        val = g_key_file_get_string(key_file, group, key, NULL);
        if (val == NULL) {
                *value = default_value;
                return TRUE;
        }
        gboolean val_false = (g_strcmp0(val, "0") == 0 || g_ascii_strcasecmp(val, "no") == 0 || g_ascii_strcasecmp(val, "false") == 0);
        if (val_false) {
                *value = FALSE;
                return TRUE;
        }
        gboolean val_true = (g_strcmp0(val, "1") == 0 || g_ascii_strcasecmp(val, "yes") == 0 || g_ascii_strcasecmp(val, "true") == 0);
        if (val_true) {
                *value = TRUE;
                return TRUE;
        }
        return FALSE;
}

gboolean get_key_int(GKeyFile *key_file, const gchar* group, const gchar* key, gint* value, const gint default_value, GError **error)
{
        gint val = g_key_file_get_integer(key_file, group, key, NULL);
        if (val == 0) {
                *value = default_value;
                return TRUE;
        }
        *value = val;
        return TRUE;
}

gboolean get_group(GKeyFile *key_file, const gchar *group, GHashTable **hash, GError *error)
{
        guint key;
        gsize num_keys;
        gchar **keys, *value;

        *hash = g_hash_table_new(g_str_hash, g_str_equal);
        keys = g_key_file_get_keys(key_file, group, &num_keys, &error);
        for(key = 0; key < num_keys; key++)
        {
                value = g_key_file_get_value(key_file,
                                             group,
                                             keys[key],
                                             &error);
                g_hash_table_insert(*hash, keys[key], value);
                //g_debug("\t\tkey %u/%lu: \t%s => %s\n", key, num_keys - 1, keys[key], value);
        }
        return (num_keys > 0);
}

GLogLevelFlags log_level_from_string(const gchar *log_level)
{
        if (g_strcmp0(log_level, "error") == 0) {
                return G_LOG_LEVEL_ERROR;
        } else if (g_strcmp0(log_level, "critical") == 0) {
                return G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL;
        } else if (g_strcmp0(log_level, "warning") == 0) {
                return G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL |
                       G_LOG_LEVEL_WARNING;
        } else if (g_strcmp0(log_level, "message") == 0) {
                return G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL |
                       G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE;
        } else if (g_strcmp0(log_level, "info") == 0) {
                return G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL |
                       G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE |
                       G_LOG_LEVEL_INFO;
        } else if (g_strcmp0(log_level, "debug") == 0) {
                return G_LOG_LEVEL_MASK;
        } else {
                return G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL |
                       G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE;
        }
}

struct config* load_config_file(const gchar* config_file, GError** error)
{
        struct config *config = g_new0(struct config, 1);

        gint val_int;
        g_autofree gchar *val = NULL;
        g_autofree GKeyFile *ini_file = g_key_file_new();

        if (!g_key_file_load_from_file(ini_file, config_file, G_KEY_FILE_NONE, error)) {
                return NULL;
        }

        if (!get_key_string(ini_file, "client", "hawkbit_server", &config->hawkbit_server, NULL, error))
                return NULL;
        if (!get_key_string(ini_file, "client", "auth_token", &config->auth_token, NULL, error))
                return NULL;
        if (!get_key_string(ini_file, "client", "target_name", &config->controller_id, NULL, error))
                return NULL;
        if (!get_key_string(ini_file, "client", "tenant_id", &config->tenant_id, "DEFAULT", error))
                return NULL;
        if (!get_key_string(ini_file, "client", "bundle_download_location", &config->bundle_download_location, NULL, error))
                return NULL;
        if (!get_key_bool(ini_file, "client", "ssl", &config->ssl, DEFAULT_SSL, error))
                return NULL;
        if (!get_key_bool(ini_file, "client", "ssl_verify", &config->ssl_verify, DEFAULT_SSL_VERIFY, error))
                return NULL;
        if (!get_group(ini_file, "device", &config->device, *error))
                return NULL;

        if (!get_key_int(ini_file, "client", "connect_timeout", &val_int, DEFAULT_CONNECTTIMEOUT, error))
                return NULL;
        config->connect_timeout = val_int;

        if (!get_key_int(ini_file, "client", "timeout", &val_int, DEFAULT_TIMEOUT, error))
                return NULL;
        config->timeout = val_int;

        if (!get_key_int(ini_file, "client", "retry_wait", &val_int, DEFAULT_RETRY_WAIT, error))
                return NULL;
        config->retry_wait = val_int;

        if (!get_key_string(ini_file, "client", "log_level", &val, DEFAULT_LOG_LEVEL, error))
                return NULL;
        config->log_level = log_level_from_string(val);

        if (config->timeout > 0 && config->connect_timeout > 0 && config->timeout < config->connect_timeout) {
                g_set_error(error,
                            1,                   // error domain
                            5,                   // error code
                            "timeout should be greater than connect_timeout. Timeout: %ld, Connect timeout: %ld",
                            config->timeout,
                            config->connect_timeout
                            );
                return NULL;
        }

        return config;
}
