/**
 * \file
 * \brief helper functions for retrieving monitor geometry, etc.
 *//*
 * Authors:
 *   Eduard Braun <eduard.braun2@gmx.de>
 *
 * Copyright 2018 Authors
 *
 * This file is part of Inkscape.
 *
 * Inkscape is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Inkscape is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Inkscape.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <gdkmm/rectangle.h>
#include <gdkmm/window.h>

#if GTKMM_CHECK_VERSION(3,22,0)
# include <gdkmm/monitor.h>
#else
# include <gdkmm/screen.h>
#endif


namespace Inkscape {
namespace UI {

/** get monitor geometry of primary monitor */
Gdk::Rectangle get_monitor_geometry_primary() {
    Gdk::Rectangle monitor_geometry;
#if GTKMM_CHECK_VERSION(3,22,0)
    auto const display = Gdk::Display::get_default();
    auto monitor = display->get_primary_monitor();

    // Fallback to monitor number 0 if the user hasn't configured a primary monitor
    if (!monitor) {
        monitor = display->get_monitor(0);
    }

    monitor->get_geometry(monitor_geometry);
#else
    auto const default_screen = Gdk::Screen::get_default();
    auto const monitor_number = default_screen->get_primary_monitor();
    default_screen->get_monitor_geometry(monitor_number, monitor_geometry);
#endif
    return monitor_geometry;
}

/** get monitor geometry of monitor containing largest part of window */
Gdk::Rectangle get_monitor_geometry_at_window(const Glib::RefPtr<Gdk::Window>& window) {
    Gdk::Rectangle monitor_geometry;
#if GTKMM_CHECK_VERSION(3,22,0)
    auto const display = Gdk::Display::get_default();
    auto const monitor = display->get_monitor_at_window(window);
    monitor->get_geometry(monitor_geometry);
#else
    auto const default_screen = Gdk::Screen::get_default();
    auto const monitor_number = default_screen->get_monitor_at_window(window);
    default_screen->get_monitor_geometry(monitor_number, monitor_geometry);
#endif
    return monitor_geometry;
}

/** get monitor geometry of monitor at (or closest to) point on combined screen area */
Gdk::Rectangle get_monitor_geometry_at_point(int x, int y) {
    Gdk::Rectangle monitor_geometry;
#if GTKMM_CHECK_VERSION(3,22,0)
    auto const display = Gdk::Display::get_default();
    auto const monitor = display->get_monitor_at_point(x ,y);
    monitor->get_geometry(monitor_geometry);
#else
    auto const default_screen = Gdk::Screen::get_default();
    auto const monitor_number = default_screen->get_monitor_at_point(x, y);
    default_screen->get_monitor_geometry(monitor_number, monitor_geometry);
#endif
    return monitor_geometry;
}

}
}


/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace .0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99:
