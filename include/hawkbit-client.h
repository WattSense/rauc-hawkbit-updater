#ifndef __HAWKBIT_CLIENT_H__
#define __HAWKBIT_CLIENT_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <curl/curl.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <libgen.h>
#include <string.h>
#include <sys/statvfs.h>
#include <time.h>
#include "config-file.h"
#include "json-helper.h"

#ifdef WITH_SYSTEMD
#include "sd-helper.h"
#endif

#define HAWKBIT_USERAGENT "rauc-hawkbit-c-agent/1.0"
#define DEFAULT_CURL_REQUEST_BUFFER_SIZE 512
#define DEFAULT_SLEEP_TIME_SEC 60 * 60               // 1 hour
#define DEFAULT_CURL_DOWNLOAD_BUFFER_SIZE 64 * 1024  // 64KB

extern gboolean volatile force_check_run; /**< force software check */
extern gboolean run_once;                 /**< only run software check once and exit */

/**
 * @brief HTTP methods.
 */
enum HTTPMethod { GET, HEAD, PUT, POST, PATCH, DELETE };

/**
 * @brief struct containing the payload and size of REST body.
 */
struct rest_payload {
    gchar* payload; /**< string representation of payload */
    size_t size;    /**< size of payload */
};

/**
 * @brief struct containing Curl write callback context.
 */
struct get_binary {
    FILE* fp;            /**< filepointer to download file */
    gint64 filesize;     /**< expected file size of download file */
    gint64 written;      /**< number of bytes written to download file */
    GChecksum* checksum; /**< checksum of download file */
};

/**
 * @brief struct containing the checksum of downloaded file.
 */
struct get_binary_checksum {
    gchar* checksum_result; /**< checksum as string */
    GChecksumType
        checksum_type; /**< checksum type. See also https://developer.gnome.org/glib/stable/glib-Data-Checksums.html#GChecksumType */
};

/**
 * @brief struct containing
 */
struct artifact {
    gchar* name;         /**< name of software */
    gchar* version;      /**< software version */
    gint64 size;         /**< size of software bundle file */
    gchar* download_url; /**< download URL of software bundle file */
    gchar* feedback_url; /**< URL status feedback should be sent to */
    gchar* sha1;         /**< sha1 checksum of software bundle file */
    gchar* md5;          /**< md5 checksum of software bundle file */
};

/**
 * @brief struct containing the new downloaded file.
 */
struct on_new_software_userdata {
    GSourceFunc install_progress_callback; /**< callback function to be called when new progress */
    GSourceFunc
        install_complete_callback; /**< callback function to be called when installation is complete */
    gchar* file;                   /**< downloaded new software file */
};

/**
 * @brief struct containing the result of the installation.
 */
struct on_install_complete_userdata {
    gboolean install_success; /**< status of installation */
};

void hawkbit_init(struct config* config, GSourceFunc on_install_ready);
void hawkbit_start_service_sync();
gboolean hawkbit_progress(const gchar* msg);
gboolean install_complete_cb(gpointer ptr);

gint read_apps_version(gchar** apps_version);
gint read_rootfs_version(gchar** rootfs_version);
#endif  // __HAWKBIT_CLIENT_H__
