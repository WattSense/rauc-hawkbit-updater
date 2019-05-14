/**
 * \file device_id.c
 * \brief device id implementation
 * \author louis.rubet@wattsense.com
 * \date 2018/02/27
 */

/*
 * includes
 */
#include "device_id.h"
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "log.h"

static char _device_id[DEVICE_ID_LENGTH_MAX] = "<unique_id>";
static const char _device_filename[] = "/data/wattsense/device_id";

/*
 * exported functions
 */

/* see header for comments and parameters */
bool device_id_init(void) {
    int fd;
    ssize_t len;
    ssize_t i;
    bool ret = false;

    fd = open(_device_filename, O_RDONLY);
    if (fd > 0) {
        len = read(fd, _device_id, DEVICE_ID_LENGTH_MAX);

        /* test device_id chars */
        if (len > 0) {
            ret = true;
            for (i = 0; i < len; i++)
                if (!isalnum(_device_id[i])) {
                    ret = false;
                    g_printerr("HPA ---> %d : %c(%d) / %d\n", __LINE__, _device_id[i],
                               _device_id[i], i);
                    break;
                }
        } else if (len == 0)
            g_printerr("device id file '%s' is empty", _device_filename);
        else
            g_printerr("error on device id file '%s'", _device_filename);

        (void)close(fd);
    } else
        g_printerr("device id file '%s' cannot be opened", _device_filename);

    return ret;
}

/* see header for comments and parameters */
const char* device_id_get(void) {
    return _device_id;
}
