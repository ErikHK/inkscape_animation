
/*
 * System abstraction utility routines
 *
 * Authors:
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2004-2005 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <fstream>
#include <glib.h>
#include <glib/gstdio.h>
#include <glibmm/fileutils.h>
#include <glibmm/ustring.h>
#include <gtk/gtk.h>

#include "preferences.h"
#include "sys.h"

//#define INK_DUMP_FILENAME_CONV 1
#undef INK_DUMP_FILENAME_CONV

//#define INK_DUMP_FOPEN 1
#undef INK_DUMP_FOPEN

void dump_str(gchar const *str, gchar const *prefix);
void dump_ustr(Glib::ustring const &ustr);

extern guint update_in_progress;


void Inkscape::IO::dump_fopen_call( char const *utf8name, char const *id )
{
#ifdef INK_DUMP_FOPEN
    Glib::ustring str;
    for ( int i = 0; utf8name[i]; i++ )
    {
        if ( utf8name[i] == '\\' )
        {
            str += "\\\\";
        }
        else if ( (utf8name[i] >= 0x20) && ((0x0ff & utf8name[i]) <= 0x7f) )
        {
            str += utf8name[i];
        }
        else
        {
            gchar tmp[32];
            g_snprintf( tmp, sizeof(tmp), "\\x%02x", (0x0ff & utf8name[i]) );
            str += tmp;
        }
    }
    g_message( "fopen call %s for [%s]", id, str.data() );
#else
    (void)utf8name;
    (void)id;
#endif
}

FILE *Inkscape::IO::fopen_utf8name( char const *utf8name, char const *mode )
{
    FILE* fp = NULL;
    gchar *filename = g_filename_from_utf8( utf8name, -1, NULL, NULL, NULL );
    if ( filename )
    {
        // ensure we open the file in binary mode (not needed in POSIX but doesn't hurt either)
        Glib::ustring how( mode );
        if ( how.find("b") == Glib::ustring::npos )
        {
            how.append("b");
        }
        // when opening a file for writing: create parent directories if they don't exist already
        if ( how.find("w") != Glib::ustring::npos )
        {
            gchar *dirname = g_path_get_dirname(utf8name);
            if (g_mkdir_with_parents(dirname, 0777)) {
                g_warning("Could not create directory '%s'", dirname);
            }
            g_free(dirname);
        }
        fp = g_fopen(filename, how.c_str());
        g_free(filename);
        filename = 0;
    }
    return fp;
}


int Inkscape::IO::mkdir_utf8name( char const *utf8name )
{
    int retval = -1;
    gchar *filename = g_filename_from_utf8( utf8name, -1, NULL, NULL, NULL );
    if ( filename )
    {
        retval = g_mkdir(filename, S_IRWXU | S_IRGRP | S_IXGRP); // The mode argument is ignored on Windows.
        g_free(filename);
        filename = 0;
    }
    return retval;
}

/* 
 * Wrapper around Glib::file_open_tmp().
 * Returns a handle to the temp file.
 * name_used contains the actual name used (a raw filename, not necessarily utf8).
 * 
 * Returns:
 * A file handle (as from open()) to the file opened for reading and writing. 
 * The file is opened in binary mode on platforms where there is a difference. 
 * The file handle should be closed with close().
 * 
 * Note:
 * On Windows Vista Glib::file_open_tmp fails with the current version of glibmm
 * A special case is implemented for WIN32. This can be removed if the issue is fixed
 * in future versions of glibmm 
 * */
int Inkscape::IO::file_open_tmp(std::string& name_used, const std::string& prefix)
{
    return Glib::file_open_tmp(name_used, prefix);
}

bool Inkscape::IO::file_test( char const *utf8name, GFileTest test )
{
    bool exists = false;

    if ( utf8name ) {
        gchar *filename = NULL;
        if (utf8name && !g_utf8_validate(utf8name, -1, NULL)) {
            /* FIXME: Trying to guess whether or not a filename is already in utf8 is unreliable.
               If any callers pass non-utf8 data (e.g. using g_get_home_dir), then change caller to
               use simple g_file_test.  Then add g_return_val_if_fail(g_utf_validate(...), false)
               to beginning of this function. */
            filename = g_strdup(utf8name);
            // Looks like g_get_home_dir isn't safe.
            //g_warning("invalid UTF-8 detected internally. HUNT IT DOWN AND KILL IT!!!");
        } else {
            filename = g_filename_from_utf8 ( utf8name, -1, NULL, NULL, NULL );
        }
        if ( filename ) {
            exists = g_file_test (filename, test);
            g_free(filename);
            filename = NULL;
        } else {
            g_warning( "Unable to convert filename in IO:file_test" );
        }
    }

    return exists;
}

bool Inkscape::IO::file_is_writable( char const *utf8name)
{
    bool success = true;

    if ( utf8name) {
        gchar *filename = NULL;
        if (utf8name && !g_utf8_validate(utf8name, -1, NULL)) {
            /* FIXME: Trying to guess whether or not a filename is already in utf8 is unreliable.
               If any callers pass non-utf8 data (e.g. using g_get_home_dir), then change caller to
               use simple g_file_test.  Then add g_return_val_if_fail(g_utf_validate(...), false)
               to beginning of this function. */
            filename = g_strdup(utf8name);
            // Looks like g_get_home_dir isn't safe.
            //g_warning("invalid UTF-8 detected internally. HUNT IT DOWN AND KILL IT!!!");
        } else {
            filename = g_filename_from_utf8 ( utf8name, -1, NULL, NULL, NULL );
        }
        if ( filename ) {
            GStatBuf st;
            if (g_file_test (filename, G_FILE_TEST_EXISTS)){ 
                if (g_lstat (filename, &st) == 0) {
                    success = ((st.st_mode & S_IWRITE) != 0);
                }
            }
            g_free(filename);
            filename = NULL;
        } else {
            g_warning( "Unable to convert filename in IO:file_test" );
        }
    }

    return success;
}

/**Checks if directory of file exists, useful
 * because inkscape doesn't create directories.*/
bool Inkscape::IO::file_directory_exists( char const *utf8name ){
    bool exists = true;

    if ( utf8name) {
        gchar *filename = NULL;
        if (utf8name && !g_utf8_validate(utf8name, -1, NULL)) {
            /* FIXME: Trying to guess whether or not a filename is already in utf8 is unreliable.
               If any callers pass non-utf8 data (e.g. using g_get_home_dir), then change caller to
               use simple g_file_test.  Then add g_return_val_if_fail(g_utf_validate(...), false)
               to beginning of this function. */
            filename = g_strdup(utf8name);
            // Looks like g_get_home_dir isn't safe.
            //g_warning("invalid UTF-8 detected internally. HUNT IT DOWN AND KILL IT!!!");
        } else {
            filename = g_filename_from_utf8 ( utf8name, -1, NULL, NULL, NULL );
        }
        if ( filename ) {
            gchar *dirname = g_path_get_dirname(filename);
            exists = Inkscape::IO::file_test( dirname, G_FILE_TEST_EXISTS);
            g_free(filename);
            g_free(dirname);
            filename = NULL;
            dirname = NULL;
        } else {
            g_warning( "Unable to convert filename in IO:file_test" );
        }
    }

    return exists;

}

/** Wrapper around g_dir_open, but taking a utf8name as first argument. */
GDir *
Inkscape::IO::dir_open(gchar const *const utf8name, guint const flags, GError **const error)
{
    gchar *const opsys_name = g_filename_from_utf8(utf8name, -1, NULL, NULL, error);
    if (opsys_name) {
        GDir *ret = g_dir_open(opsys_name, flags, error);
        g_free(opsys_name);
        return ret;
    } else {
        return NULL;
    }
}

/**
 * Like g_dir_read_name, but returns a utf8name (which must be freed, unlike g_dir_read_name).
 *
 * N.B. Skips over any dir entries that fail to convert to utf8.
 */
gchar *
Inkscape::IO::dir_read_utf8name(GDir *dir)
{
    for (;;) {
        gchar const *const opsys_name = g_dir_read_name(dir);
        if (!opsys_name) {
            return NULL;
        }
        gchar *utf8_name = g_filename_to_utf8(opsys_name, -1, NULL, NULL, NULL);
        if (utf8_name) {
            return utf8_name;
        }
    }
}


gchar* Inkscape::IO::locale_to_utf8_fallback( const gchar *opsysstring,
                                              gssize len,
                                              gsize *bytes_read,
                                              gsize *bytes_written,
                                              GError **error )
{
    gchar *result = NULL;
    if ( opsysstring ) {
        gchar *newFileName = g_locale_to_utf8( opsysstring, len, bytes_read, bytes_written, error );
        if ( newFileName ) {
            if ( !g_utf8_validate(newFileName, -1, NULL) ) {
                g_warning( "input filename did not yield UTF-8" );
                g_free( newFileName );
            } else {
                result = newFileName;
            }
            newFileName = 0;
        } else if ( g_utf8_validate(opsysstring, -1, NULL) ) {
            // This *might* be a case that we want
            // g_warning( "input failed filename->utf8, fell back to original" );
            // TODO handle cases when len >= 0
            result = g_strdup( opsysstring );
        } else {
            gchar const *charset = 0;
            g_get_charset(&charset);
            g_warning( "input filename conversion failed for file with locale charset '%s'", charset );
        }
    }
    return result;
}

void
Inkscape::IO::spawn_async_with_pipes( const std::string& working_directory,
                                      const Glib::ArrayHandle<std::string>& argv,
                                      Glib::SpawnFlags flags,
                                      const sigc::slot<void>& child_setup,
                                      Glib::Pid* child_pid,
                                      int* standard_input,
                                      int* standard_output,
                                      int* standard_error)
{
    Glib::spawn_async_with_pipes(working_directory,
                                 argv,
                                 flags,
                                 child_setup,
                                 child_pid,
                                 standard_input,
                                 standard_output,
                                 standard_error);
}


gchar* Inkscape::IO::sanitizeString( gchar const * str )
{
    gchar *result = NULL;
    if ( str ) {
        if ( g_utf8_validate(str, -1, NULL) ) {
            result = g_strdup(str);
        } else {
            guchar scratch[8];
            Glib::ustring buf;
            guchar const *ptr = (guchar const*)str;
            while ( *ptr )
            {
                if ( *ptr == '\\' )
                {
                    buf.append("\\\\");
                } else if ( *ptr < 0x80 ) {
                    buf += (char)(*ptr);
                } else {
                    g_snprintf((gchar*)scratch, sizeof(scratch), "\\x%02x", *ptr);
                    buf.append((const char*)scratch);
                }
                ptr++;
            }
            result = g_strdup(buf.c_str());
        }
    }
    return result;
}

/* 
 * Returns the file extension of a path/filename
 */
Glib::ustring Inkscape::IO::get_file_extension(Glib::ustring path)
{
    Glib::ustring::size_type period_location = path.find_last_of(".");
    return path.substr(period_location);
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
