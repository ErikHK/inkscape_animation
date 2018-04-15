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

#ifndef SEEN_MONITOR_H
#define SEEN_MONITOR_H

#include <gdkmm/rectangle.h>
#include <gdkmm/window.h>

namespace Inkscape {
namespace UI {
    Gdk::Rectangle get_monitor_geometry_primary();
    Gdk::Rectangle get_monitor_geometry_at_window(const Glib::RefPtr<Gdk::Window>& window);
    Gdk::Rectangle get_monitor_geometry_at_point(int x, int y);
}
}

#endif // SEEN_MONITOR_H

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
