/*
 * Separate the inkscape paths from the prefix code, as that is kind of
 * a separate package (binreloc)
 * 	http://autopackage.org/downloads.html
 *
 * Since the directories set up by autoconf end up in config.h, we can't
 * _change_ them, since config.h isn't protected by a set of
 * one-time-include directives and is repeatedly re-included by some
 * chains of .h files.  As a result, nothing should refer to those
 * define'd directories, and instead should use only the paths defined here.
 *
 */
#ifndef SEEN_PATH_PREFIX_H
#define SEEN_PATH_PREFIX_H

#ifndef INKSCAPE_DATADIR
# error "INKSCAPE_DATADIR undefined. Must #include config.h before anything else."
#endif
#include "prefix.h"


char *append_inkscape_datadir(const char *relative_path);

#ifdef _WIN32
#undef INKSCAPE_DATADIR
#define INKSCAPE_DATADIR append_inkscape_datadir(NULL)
#endif

#ifdef ENABLE_BINRELOC
/* The way that we're building now is with a shared library between Inkscape
   and Inkview, and the code will find the path to the library then. But we
   don't really want that. This prefix then pulls things out of the lib directory
   and back into the root install dir. */
#  define INKSCAPE_LIBPREFIX      "/../.."
#  define INKSCAPE_APPICONDIR     BR_DATADIR( INKSCAPE_LIBPREFIX "/share/pixmaps" )
#  define INKSCAPE_ATTRRELDIR     BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/attributes" )
#  define INKSCAPE_BINDDIR        BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/bind" )
#  define INKSCAPE_DOCDIR         BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/doc" )
#  define INKSCAPE_EXAMPLESDIR    BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/examples" )
#  define INKSCAPE_EXTENSIONDIR   BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/extensions" )
#  define INKSCAPE_FILTERDIR      BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/filters" )
#  define INKSCAPE_FONTSDIR       BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/fonts" )
#  define INKSCAPE_GRADIENTSDIR   BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/gradients" )
#  define INKSCAPE_KEYSDIR        BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/keys" )
#  define INKSCAPE_PIXMAPDIR      BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/icons" )
#  define INKSCAPE_MARKERSDIR     BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/markers" )
#  define INKSCAPE_PALETTESDIR    BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/palettes" )
#  define INKSCAPE_PATTERNSDIR    BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/patterns" )
#  define INKSCAPE_SCREENSDIR     BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/screens" )
#  define INKSCAPE_SYMBOLSDIR     BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/symbols" )
#  define INKSCAPE_THEMEDIR       BR_DATADIR( INKSCAPE_LIBPREFIX "/share/icons" )
#  define INKSCAPE_TUTORIALSDIR   BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/tutorials" )
#  define INKSCAPE_TEMPLATESDIR   BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/templates" )
#  define INKSCAPE_UIDIR          BR_DATADIR( INKSCAPE_LIBPREFIX "/share/inkscape/ui" )
//CREATE V0.1 support
#    define CREATE_GRADIENTSDIR   BR_DATADIR( INKSCAPE_LIBPREFIX "/share/create/gradients/gimp" )
#    define CREATE_PALETTESDIR    BR_DATADIR( INKSCAPE_LIBPREFIX "/share/create/swatches" )
#    define CREATE_PATTERNSDIR    BR_DATADIR( INKSCAPE_LIBPREFIX "/share/create/patterns/vector" )
#else
#  ifdef _WIN32
#    define INKSCAPE_APPICONDIR   append_inkscape_datadir("pixmaps")
#    define INKSCAPE_ATTRRELDIR   append_inkscape_datadir("attributes")
#    define INKSCAPE_BINDDIR      append_inkscape_datadir("bind")
#    define INKSCAPE_DOCDIR       append_inkscape_datadir("doc")
#    define INKSCAPE_EXAMPLESDIR  append_inkscape_datadir("examples")
#    define INKSCAPE_EXTENSIONDIR append_inkscape_datadir("extensions")
#    define INKSCAPE_FILTERDIR    append_inkscape_datadir("filters")
#    define INKSCAPE_FONTSDIR     append_inkscape_datadir("fonts")
#    define INKSCAPE_GRADIENTSDIR append_inkscape_datadir("gradients")
#    define INKSCAPE_KEYSDIR      append_inkscape_datadir("keys")
#    define INKSCAPE_PIXMAPDIR    append_inkscape_datadir("icons")
#    define INKSCAPE_MARKERSDIR   append_inkscape_datadir("markers")
#    define INKSCAPE_PALETTESDIR  append_inkscape_datadir("palettes")
#    define INKSCAPE_PATTERNSDIR  append_inkscape_datadir("patterns")
#    define INKSCAPE_SCREENSDIR   append_inkscape_datadir("screens")
#    define INKSCAPE_SYMBOLSDIR   append_inkscape_datadir("symbols")
#    define INKSCAPE_THEMEDIR     append_inkscape_datadir("icons")
#    define INKSCAPE_TUTORIALSDIR append_inkscape_datadir("tutorials")
#    define INKSCAPE_TEMPLATESDIR append_inkscape_datadir("templates")
#    define INKSCAPE_UIDIR        append_inkscape_datadir("ui")
//CREATE V0.1  WIN32 support
#    define CREATE_GRADIENTSDIR   append_inkscape_datadir("create\\gradients\\gimp")
#    define CREATE_PALETTESDIR    append_inkscape_datadir("create\\swatches")
#    define CREATE_PATTERNSDIR    append_inkscape_datadir("create\\patterns\\vector")
#  elif defined ENABLE_OSX_APP_LOCATIONS
#    define INKSCAPE_APPICONDIR   "Contents/Resources/share/pixmaps"
#    define INKSCAPE_ATTRRELDIR   "Contents/Resources/share/inkscape/attributes"
#    define INKSCAPE_BINDDIR      "Contents/Resources/share/inkscape/bind"
#    define INKSCAPE_DOCDIR       "Contents/Resources/share/inkscape/doc"
#    define INKSCAPE_EXAMPLESDIR  "Contents/Resources/share/inkscape/examples"
#    define INKSCAPE_EXTENSIONDIR "Contents/Resources/share/inkscape/extensions"
#    define INKSCAPE_FILTERDIR    "Contents/Resources/share/inkscape/filters"
#    define INKSCAPE_FONTSDIR     "Contents/Resources/share/inkscape/fonts"
#    define INKSCAPE_GRADIENTSDIR "Contents/Resources/share/inkscape/gradients"
#    define INKSCAPE_KEYSDIR      "Contents/Resources/share/inkscape/keys"
#    define INKSCAPE_PIXMAPDIR    "Contents/Resources/share/inkscape/icons"
#    define INKSCAPE_MARKERSDIR   "Contents/Resources/share/inkscape/markers"
#    define INKSCAPE_PALETTESDIR  "Contents/Resources/share/inkscape/palettes"
#    define INKSCAPE_PATTERNSDIR  "Contents/Resources/share/inkscape/patterns"
#    define INKSCAPE_SCREENSDIR   "Contents/Resources/share/inkscape/screens"
#    define INKSCAPE_SYMBOLSDIR   "Contents/Resources/share/inkscape/symbols"
#    define INKSCAPE_THEMEDIR     INKSCAPE_PIXMAPDIR
#    define INKSCAPE_TUTORIALSDIR "Contents/Resources/share/inkscape/tutorials"
#    define INKSCAPE_TEMPLATESDIR "Contents/Resources/share/inkscape/templates"
#    define INKSCAPE_UIDIR        "Contents/Resources/share/inkscape/ui"
//CREATE V0.1 support
#    define CREATE_GRADIENTSDIR  "/Library/Application Support/create/gradients/gimp"
#    define CREATE_PALETTESDIR   "/Library/Application Support/create/swatches"
#    define CREATE_PATTERNSDIR   "/Library/Application Support/create/patterns/vector"
#  else
#    define INKSCAPE_APPICONDIR   append_inkscape_datadir("pixmaps")
#    define INKSCAPE_ATTRRELDIR   append_inkscape_datadir("inkscape/attributes")
#    define INKSCAPE_BINDDIR      append_inkscape_datadir("inkscape/bind")
#    define INKSCAPE_DOCDIR       append_inkscape_datadir("inkscape/doc")
#    define INKSCAPE_EXAMPLESDIR  append_inkscape_datadir("inkscape/examples")
#    define INKSCAPE_EXTENSIONDIR append_inkscape_datadir("inkscape/extensions")
#    define INKSCAPE_FILTERDIR    append_inkscape_datadir("inkscape/filters")
#    define INKSCAPE_FONTSDIR     append_inkscape_datadir("inkscape/fonts")
#    define INKSCAPE_GRADIENTSDIR append_inkscape_datadir("inkscape/gradients")
#    define INKSCAPE_KEYSDIR      append_inkscape_datadir("inkscape/keys")
#    define INKSCAPE_PIXMAPDIR    append_inkscape_datadir("inkscape/icons")
#    define INKSCAPE_MARKERSDIR   append_inkscape_datadir("inkscape/markers")
#    define INKSCAPE_PALETTESDIR  append_inkscape_datadir("inkscape/palettes")
#    define INKSCAPE_PATTERNSDIR  append_inkscape_datadir("inkscape/patterns")
#    define INKSCAPE_SCREENSDIR   append_inkscape_datadir("inkscape/screens")
#    define INKSCAPE_SYMBOLSDIR   append_inkscape_datadir("inkscape/symbols")
#    define INKSCAPE_THEMEDIR     append_inkscape_datadir("icons")
#    define INKSCAPE_TUTORIALSDIR append_inkscape_datadir("inkscape/tutorials")
#    define INKSCAPE_TEMPLATESDIR append_inkscape_datadir("inkscape/templates")
#    define INKSCAPE_UIDIR        append_inkscape_datadir("inkscape/ui")
//CREATE V0.1 support
#    define CREATE_GRADIENTSDIR   append_inkscape_datadir("create/gradients/gimp")
#    define CREATE_PALETTESDIR    append_inkscape_datadir("create/swatches")
#    define CREATE_PATTERNSDIR    append_inkscape_datadir("create/patterns/vector")
#  endif
#endif

#endif /* _PATH_PREFIX_H_ */
