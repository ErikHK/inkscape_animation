/*
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *   Martin Owens <doctormo@gmail.com>
 *
 * Copyright (C) 2006-2017 Authors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * See the file COPYING for details.
 *
 */

#ifndef SEEN_INKSCAPE_IO_RESOURCE_H
#define SEEN_INKSCAPE_IO_RESOURCE_H

#include <vector>
#include <glibmm/ustring.h>
#include "util/share.h"

namespace Inkscape {

namespace IO {

/**
 * simple resource API
 */
namespace Resource {

enum Type {
    APPICONS,
    EXTENSIONS,
    FONTS,
    GRADIENTS,
    ICONS,
    KEYS,
    MARKERS,
    NONE,
    PALETTES,
    PATTERNS,
    SCREENS,
    TEMPLATES,
    TUTORIALS,
    SYMBOLS,
    FILTERS,
    THEMES,
    UIS
};

enum Domain {
    SYSTEM,
    CREATE,
    CACHE,
    USER
};

Util::ptr_shared get_path(Domain domain, Type type,
                                char const *filename=NULL);

Glib::ustring get_path_ustring(Domain domain, Type type,
                                char const *filename=NULL);

Glib::ustring get_filename(Type type, char const *filename,
                                char const *locale=NULL);
Glib::ustring get_filename(Glib::ustring path, Glib::ustring filename);

std::vector<Glib::ustring> get_filenames(Type type,
                                std::vector<const char *> extensions={},
                                std::vector<const char *> exclusions={});

std::vector<Glib::ustring> get_filenames(Domain domain, Type type,
                                std::vector<const char *> extensions={},
                                std::vector<const char *> exclusions={});

std::vector<Glib::ustring> get_filenames(Glib::ustring path,
                                std::vector<const char *> extensions={},
                                std::vector<const char *> exclusions={});

void get_filenames_from_path(std::vector<Glib::ustring> &files,
                              Glib::ustring path,
                              std::vector<const char *> extensions={},
                              std::vector<const char *> exclusions={});


char *profile_path(const char *filename);
char *homedir_path(const char *filename);
char *log_path(const char *filename);

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
