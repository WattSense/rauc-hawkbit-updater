#ifndef __SD_HELPER_H__
#define __SD_HELPER_H__

#include <glib-2.0/glib.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-event.h>

/**
 * @brief Binding GSource and sd_event together.
 */
struct SDSource {
    GSource source;
    sd_event* event;
    GPollFD pollfd;
};

/**
 * @brief Attach GSource to GMainLoop
 *
 * @param[in] source Glib GSource
 * @param[in] loop GMainLoop the GSource should be attached to.
 * @return 0 if success else != 0 if error
 * @see https://developer.gnome.org/glib/stable/glib-The-Main-Event-Loop.html#GMainLoop
 * @see https://developer.gnome.org/glib/stable/glib-The-Main-Event-Loop.html#GSource
 */
int sd_source_attach(GSource* source, GMainLoop* loop);

/**
 * @brief Create GSource from a sd_event
 *
 * @param[in] event Systemd event that should be converted to a Glib GSource
 * @return GSource
 * @see https://www.freedesktop.org/software/systemd/man/sd-event.html
 * @see https://developer.gnome.org/glib/stable/glib-The-Main-Event-Loop.html#GSource
 */
GSource* sd_source_new(sd_event* event);

#endif  // __SD_HELPER_H__
