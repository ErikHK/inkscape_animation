/*
 * Inkscape::Util::ptr_shared<T> - like T const *, but stronger.
 * Used to hold c-style strings for objects that are managed by the gc.
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2006 MenTaLguY
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifndef SEEN_INKSCAPE_UTIL_SHARE_H
#define SEEN_INKSCAPE_UTIL_SHARE_H

#include "inkgc/gc-core.h"
#include <cstring>
#include <cstddef>

namespace Inkscape {
namespace Util {

class ptr_shared {
public:

    ptr_shared() : _string(NULL) {}
        ptr_shared(ptr_shared const &other) : _string(other._string) {}

    operator char const *() const { return _string; }
    operator bool() const { return _string; }

    char const *pointer() const { return _string; }
    char const &operator[](int i) const { return _string[i]; }

    ptr_shared operator+(int i) const {
        return share_unsafe(_string+i);
    }
    ptr_shared operator-(int i) const {
        return share_unsafe(_string-i);
    }
    //WARNING: No bounds checking in += and -= functions. Moving the pointer
    //past the end of the string and then back could probably cause the garbage
    //collector to deallocate the string inbetween, as there's temporary no
    //valid reference pointing into the allocated space.
    ptr_shared &operator+=(int i) {
        _string += i;
        return *this;
    }
    ptr_shared &operator-=(int i) {
        _string -= i;
        return *this;
    }
    std::ptrdiff_t operator-(ptr_shared const &other) {
        return _string - other._string;
    }

    ptr_shared &operator=(ptr_shared const &other) {
        _string = other._string;
        return *this;
    }

    bool operator==(ptr_shared const &other) const {
        return _string == other._string;
    }
    bool operator!=(ptr_shared const &other) const {
        return _string != other._string;
    }
    bool operator>(ptr_shared const &other) const {
        return _string > other._string;
    }
    bool operator<(ptr_shared const &other) const {
        return _string < other._string;
    }

    friend ptr_shared share_unsafe(char const *string); 

private:
    ptr_shared(char const *string) : _string(string) {}
    static ptr_shared share_unsafe(char const *string) {
        return ptr_shared(string);
    }

    //This class (and code usign it) assumes that it never has to free this
    //pointer, and that the memory it points to will not be freed as long as a
    //ptr_shared pointing to it exists.
    char const *_string;
};

ptr_shared share_string(char const *string);
ptr_shared share_string(char const *string, std::size_t length);

inline ptr_shared share_unsafe(char const *string) {
    return ptr_shared::share_unsafe(string);
}

//TODO: Do we need this function?
inline ptr_shared share_static_string(char const *string) {
    return share_unsafe(string);
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
