/*
 * Authors:
 *   Martin Owens <doctormo@gmail.com>
 *
 * Copyright (C) 2017 Authors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * See the file COPYING for details.
 *
 */

#ifndef SEEN_INKSCAPE_IO_HTTP_H
#define SEEN_INKSCAPE_IO_HTTP_H

#include <functional>
#include <glibmm/ustring.h>

/**
 * simple libsoup based resource API
 */

namespace Inkscape {
namespace IO {
namespace HTTP {

    Glib::ustring get_file(Glib::ustring uri, unsigned int timeout=0, std::function<void(Glib::ustring)> func=NULL);

}
}
}

#endif
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
