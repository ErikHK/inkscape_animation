/*
 * Inkscape::Debug::log_display_config - log display configuration
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2007 MenTaLguY
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <iostream>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "debug/event-tracker.h"
#include "debug/logger.h"
#include "debug/simple-event.h"
#include "debug/log-display-config.h"

namespace Inkscape {

namespace Debug {

namespace {

typedef SimpleEvent<Event::CONFIGURATION> ConfigurationEvent;

class Monitor : public ConfigurationEvent {
public:
#if GTK_CHECK_VERSION(3,22,0)
    Monitor(GdkMonitor *monitor)
#else
    Monitor(GdkScreen *screen, gint monitor)
#endif
        : ConfigurationEvent("monitor") {
        GdkRectangle area;

#if GTK_CHECK_VERSION(3,22,0)
        gdk_monitor_get_geometry(monitor, &area);
#else
        gdk_screen_get_monitor_geometry(screen, monitor, &area);
#endif

        _addProperty("x", area.x);
        _addProperty("y", area.y);
        _addProperty("width", area.width);
        _addProperty("height", area.height);
    }
};

#if !GTK_CHECK_VERSION(3,22,0)
// We don't need this in newer Gtk+ versions as GdkMonitor information is now
// returned directly from GdkDisplay rather than needing GdkScreen first
class Screen : public ConfigurationEvent {
public:
    Screen(GdkScreen *s) : ConfigurationEvent("screen"), screen(s) {
        _addProperty("width",  gdk_screen_get_width(screen));
        _addProperty("height", gdk_screen_get_height(screen));
    }
    void generateChildEvents() const {
        gint n_monitors = gdk_screen_get_n_monitors(screen);
        for ( gint i = 0 ; i < n_monitors ; i++ ) {
            Logger::write<Monitor>(screen, i);
        }
    }

private:
    GdkScreen *screen;
};
#endif

class Display : public ConfigurationEvent {
public:
    Display() : ConfigurationEvent("display") {}
    void generateChildEvents() const {
        GdkDisplay *display=gdk_display_get_default();

#if GTK_CHECK_VERSION(3,22,0)
        gint const n_monitors = gdk_display_get_n_monitors(display);

        // Loop through all monitors and log their details
        for (gint i_monitor = 0; i_monitor < n_monitors; ++i_monitor) {
            GdkMonitor *monitor = gdk_display_get_monitor(display, i_monitor);
            Logger::write<Monitor>(monitor);
        }
#else
        // We used to find the number of screens, and log info for
        // each of them.  However, the number of screens is always
        // one in Gtk+ 3
        GdkScreen *screen = gdk_display_get_default_screen(display);
        Logger::write<Screen>(screen);
#endif
    }
};

}

void log_display_config() {
    Logger::write<Display>();
}

}

}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
