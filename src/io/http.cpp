/*
 * Inkscape::IO::HTTP - make internet requests using libsoup
 *
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

/*
 * How to use:
 *
 *  #include "io/http.cpp"
 *  void _async_test_call(Glib::ustring filename) {
 *      g_warning("HTTP request saved to %s", filename.c_str());
 *  }
 *  uint timeout = 20 * 60 * 60; // 20 hours
 *  Glib::ustring filename = Inkscape::IO::HTTP::get_file("https://media.inkscape.org/media/messages.xml", timeout, _async_test_call);
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <string>
#include <time.h>

#include "io/sys.h"
#include "io/http.h"
#include "io/resource.h"

typedef std::function<void(Glib::ustring)> callback;

namespace Resource = Inkscape::IO::Resource;

namespace Inkscape {
namespace IO {
namespace HTTP {

void _save_data_as_file(Glib::ustring filename, const char *result) {
    FILE *fileout = Inkscape::IO::fopen_utf8name(filename.c_str(), "wb");
    if (!fileout) {
        g_warning("HTTP Cache: Can't open %s for write.", filename.c_str());
        return;
    }

    fputs(result, fileout);
    fflush(fileout);
    if (ferror(fileout)) {
        g_warning("HTTP Cache: Error writing data to %s.", filename.c_str());
    }

    fclose(fileout);
}

void _get_file_callback(SoupSession *session, SoupMessage *msg, gpointer user_data) {
    auto data = static_cast<std::pair<callback, Glib::ustring>*>(user_data);
    data->first(data->second);
    delete data;
}

/*
 * Downloads a file, caches it in the local filesystem and then returns the
 * new filename of the cached file.
 *
 * Returns: filename where the file has been (blocking) or will be (async) stored.
 *
 * uri - The uri of the desired resource, the filename comes from this uri
 * timeout - Number of seconds to keep the cache, default is forever
 * func - An optional function, if given the function becomes asynchronous
 *        the returned filename will be where the file 'hopes' to be saved
 *        and this function will be called when the http request is complete.
 *
 *  NOTE: If the cache file exists, then it's returned without any async request
 *        your func will be called in a blocking way BEFORE this function returns.
 *
 */
Glib::ustring get_file(Glib::ustring uri, unsigned int timeout, callback func) {

    SoupURI *s_uri = soup_uri_new(uri.c_str());
    const char *host = soup_uri_get_host(s_uri);
    std::string path = std::string(soup_uri_decode(soup_uri_get_path(s_uri)));
    std::string filepart;

    // Parse the url into a filename suitable for caching.
    if(path.back() == '/') {
        filepart = path.replace(path.begin(), path.end(), '/', '_');
        filepart += ".url";
    } else {
        filepart = path.substr(path.rfind("/") + 1);
    }

    const char *ret = get_path(Resource::CACHE, Resource::NONE, filepart.c_str());
    Glib::ustring filename = Glib::ustring(ret);

    // We test first if the cache already exists
    if(file_test(filename.c_str(), G_FILE_TEST_EXISTS) && timeout > 0) {
        GStatBuf st;
        if(g_stat(filename.c_str(), &st) != -1) {
	    time_t changed = st.st_mtime;
	    time_t now = time(0);
            // The cache hasn't timed out, so return the filename.
	    if(now - changed < timeout) {
                if(func) {
                    // Non-async func callback return, may block.
                    func(filename);
                }
                return filename;
            }
            g_debug("HTTP Cache is stale: %s", filename.c_str());
        }
    }

    // Only then do we get the http request
    SoupMessage *msg = soup_message_new_from_uri("GET", s_uri);
    SoupSession *session = soup_session_new();

#ifdef DEBUG_HTTP
    SoupLogger *logger;
    logger = soup_logger_new(SOUP_LOGGER_LOG_BODY, -1);
    soup_session_add_feature(session, SOUP_SESSION_FEATURE (logger));
    g_object_unref (logger);
#endif

    if(func) {
        auto *user_data = new std::pair<callback, Glib::ustring>(func, filename);
        soup_session_queue_message(session, msg, _get_file_callback, user_data);
    } else {
        guint status = soup_session_send_message (session, msg);
        if(status == SOUP_STATUS_OK) {
            g_debug("HTTP Cache saved to: %s", filename.c_str());
            _save_data_as_file(filename, msg->response_body->data);
        } else {
            g_warning("Can't download %s", uri.c_str());
        }
    }
    return filename;
}


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
