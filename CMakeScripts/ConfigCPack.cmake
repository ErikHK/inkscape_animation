############################
# CPack configuration file #
############################


## General ##

set(CPACK_PACKAGE_NAME "Inkscape")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Inkscape, open-source vector graphics editor")
set(CPACK_PACKAGE_VENDOR "Inkscape")
set(CPACK_PACKAGE_CONTACT "Inkscape developers <inkscape-devel@lists.sourceforge.net>")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/GPL2.txt")
set(CPACK_PACKAGE_VERSION_MAJOR ${INKSCAPE_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${INKSCAPE_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${INKSCAPE_VERSION_PATCH})

set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}${INKSCAPE_VERSION_SUFFIX}")
set(CPACK_SOURCE_IGNORE_FILES "~$;[.]swp$;/[.]svn/;/[.]git/;.gitignore;/build/;/obj*/;tags;cscope.*")
set(INKSCAPE_CPACK_PREFIX ${PROJECT_NAME}-${INKSCAPE_VERSION}_${INKSCAPE_REVISION_DATE}_${INKSCAPE_REVISION_HASH})
set(CPACK_SOURCE_PACKAGE_FILE_NAME ${INKSCAPE_CPACK_PREFIX})
set(CPACK_PACKAGE_FILE_NAME ${INKSCAPE_CPACK_PREFIX})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "inkscape")
set(CPACK_SOURCE_GENERATOR "TXZ")

## Windows ##

if (WIN32)
    set(CPACK_GENERATOR "ZIP")
    ### nsis generator
    find_package(NSIS)
    if (NSIS_MAKE)
        set(CPACK_GENERATOR "${CPACK_GENERATOR};NSIS")
        set(CPACK_NSIS_DISPLAY_NAME "Inkscape")
        set(CPACK_NSIS_COMPRESSOR "/SOLID zlib")
        set(CPACK_NSIS_MENU_LINKS "https://inkscape.org/" "Inkscape homepage")
    endif (NSIS_MAKE)
endif (WIN32)

## Linux ##

### DEB ###
if(UNIX)
    SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libaspell15 (>= 0.60.7~20110707), libatkmm-1.6-1v5 (>= 2.24.0), libc6 (>= 2.14), libcairo2 (>= 1.14.0), libcairomm-1.0-1v5 (>= 1.12.0), libcdr-0.1-1, libdbus-glib-1-2 (>= 0.88), libfontconfig1 (>= 2.12), libfreetype6 (>= 2.2.1), libgc1c2 (>= 1:7.2d), libgcc1 (>= 1:4.0), libgdk-pixbuf2.0-0 (>= 2.22.0), libgdl-3-5 (>= 3.8.1), libglib2.0-0 (>= 2.41.1), libglibmm-2.4-1v5 (>= 2.54.0), libgomp1 (>= 4.9), libgsl23, libgslcblas0, libgtk-3-0 (>= 3.21.5), libgtkmm-3.0-1v5 (>= 3.22.0), libgtkspell3-3-0, libharfbuzz0b (>= 1.2.6), libjpeg8 (>= 8c), liblcms2-2 (>= 2.2+git20110628), libmagick++-6.q16-7 (>= 8:6.9.6.8), libpango-1.0-0 (>= 1.37.2), libpangocairo-1.0-0 (>= 1.14.0), libpangoft2-1.0-0 (>= 1.37.2), libpangomm-1.4-1v5 (>= 2.40.0), libpng16-16 (>= 1.6.2-1), libpoppler-glib8 (>= 0.18.0), libpoppler68 (>= 0.57.0), libpopt0 (>= 1.14), libpotrace0, librevenge-0.0-0, libsigc++-2.0-0v5 (>= 2.8.0), libsoup2.4-1 (>= 2.41.90), libstdc++6 (>= 5.2), libvisio-0.1-1, libwpg-0.3-3, libx11-6, libxml2 (>= 2.7.4), libxslt1.1 (>= 1.1.25), zlib1g (>= 1:1.1.4)")
    IF(NOT CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
      FIND_PROGRAM(DPKG_CMD dpkg)
      IF(NOT DPKG_CMD)
        MESSAGE(STATUS "Can not find dpkg in your path, default to i386.")
        SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE i386)
      ENDIF(NOT DPKG_CMD)
        EXECUTE_PROCESS(COMMAND "${DPKG_CMD}" --print-architecture
            OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    ENDIF(NOT CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
    SET(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    SET(CPACK_DEBIAN_PACKAGE_RECOMMENDS "aspell, imagemagick, libwmf-bin, perlmagick, python-numpy, python-lxml, python-scour, python-uniconvertor")
    SET(CPACK_DEBIAN_PACKAGE_SUGGESTS "dia, libsvg-perl, libxml-xql-perl, pstoedit, python-uniconvertor, ruby")
endif(UNIX)

### RPM ###


## MacOS ##


include(CPack)
