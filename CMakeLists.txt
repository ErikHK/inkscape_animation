cmake_minimum_required(VERSION 2.8.2)

message("------------------------------")
message("Building Makefile for Inkscape")
message("------------------------------")
message("Source Dir: ${CMAKE_CURRENT_SOURCE_DIR}")
message("Binary Dir: ${CMAKE_CURRENT_BINARY_DIR}")

include(CMakeScripts/HelperFunctions.cmake)
include(CMakeScripts/ConfigEnv.cmake)
include(CMakeScripts/ConfigPaths.cmake)

# -----------------------------------------------------------------------------
# CMake Configuration
# -----------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeScripts/Modules")

# avoid having empty buildtype
set(CMAKE_BUILD_TYPE_INIT "Release")

project(inkscape)
set(PROJECT_NAME inkscape)

set(INKSCAPE_VERSION_MAJOR  0)
set(INKSCAPE_VERSION_MINOR 92)
set(INKSCAPE_VERSION_PATCH  0)
set(INKSCAPE_VERSION_SUFFIX "+devel")

set(INKSCAPE_VERSION ${INKSCAPE_VERSION_MAJOR}.${INKSCAPE_VERSION_MINOR})
if(INKSCAPE_VERSION_PATCH)
    set(INKSCAPE_VERSION ${INKSCAPE_VERSION}.${INKSCAPE_VERSION_PATCH})
endif()
if(INKSCAPE_VERSION_SUFFIX)
    set(INKSCAPE_VERSION ${INKSCAPE_VERSION}${INKSCAPE_VERSION_SUFFIX})
endif()

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

if(APPLE)
    SET(CMAKE_MACOSX_RPATH TRUE)
    SET(CMAKE_INSTALL_RPATH "@loader_path/../lib/inkscape")
else()
    SET(CMAKE_INSTALL_RPATH "$ORIGIN/../lib/inkscape")
endif()

cmake_policy(SET CMP0003 NEW) # don't be prolific with library paths
cmake_policy(SET CMP0005 NEW) # proper define quoting
cmake_policy(SET CMP0009 NEW) # don't follow symbolic links when using GLOB

# this can be removed if/when cmake 3.1 is made the minimum required version
set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH ON)

# workaround for omission in cmake 2.8.4's GNU.cmake, fixed in 2.8.5
if(CMAKE_COMPILER_IS_GNUCC)
    if(NOT DARWIN)
    set(CMAKE_INCLUDE_SYSTEM_FLAG_C "-isystem ")
    endif()
endif()

# console output is slow as hell on Windows and as a result status messages of the "install" target slow down
# the whole build process considerably (especially since we also copy a lot of files from the devlibs)
# TODO: Is this worth to be configurable / also applicable to other platforms?
if(WIN32 AND NOT CMAKE_INSTALL_MESSAGE)
    set(CMAKE_INSTALL_MESSAGE "LAZY")
endif()

# Define a very strict set of build flags that will prevent any use of deprecated symbols.
# This will almost certainly cause compilation failure and is intended only for developer use.
set(CMAKE_CXX_FLAGS_STRICT "${CMAKE_CXX_FLAGS_DEBUG} -Werror=deprecated-declarations -DGTK_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGTKMM_DISABLE_DEPRECATED -DGDKMM_DISABLE_DEPRECATED"
	CACHE STRING
	"Flags used by C++ compiler during Strict builds"
	FORCE)
set(CMAKE_C_FLAGS_STRICT   "${CMAKE_C_FLAGS_DEBUG}   -Werror=deprecated-declarations -DGTK_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGTKMM_DISABLE_DEPRECATED -DGDKMM_DISABLE_DEPRECATED"
	CACHE STRING
	"Flags used by C compiler during Strict builds"
	FORCE)
mark_as_advanced(
	CMAKE_CXX_FLAGS_STRICT
	CMAKE_C_FLAGS_STRICT)

set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING
	"Choose the type of build, options are: None(CMAKE_CXXFLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel Strict."
	FORCE)

# -----------------------------------------------------------------------------
# Redirect output files
# -----------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE PATH "Output directory for runtime binaries")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE PATH "Output directory for shared libraries")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE PATH "Output directory for static libraries")

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
option(WITH_DBUS "Compile with support for DBus interface" OFF)
option(ENABLE_LCMS "Compile with LCMS support" ON)
option(WITH_SVG2 "Compile with support for new SVG2 features" ON)
option(WITH_LPETOOL "Compile with LPE Tool and experimental LPEs enabled" ON)
option(WITH_OPENMP "Compile with OpenMP support" ON)

option(WITH_PROFILING "Turn on profiling" OFF) # Set to true if compiler/linker should enable profiling
option(BUILD_SHARED_LIBS "Compile libraries as shared and not static" ON)

option(ENABLE_POPPLER "Compile with support of libpoppler" ON)
option(ENABLE_POPPLER_CAIRO "Compile with support of libpoppler-cairo for rendering PDF preview (depends on ENABLE_POPPLER)" ON)
option(WITH_IMAGE_MAGICK "Compile with support of ImageMagick for raster extensions and image import resolution" ON)
option(WITH_LIBCDR "Compile with support of libcdr for CorelDRAW Diagrams" ON)
option(WITH_LIBVISIO "Compile with support of libvisio for Microsoft Visio Diagrams" ON)
option(WITH_LIBWPG "Compile with support of libwpg for WordPerfect Graphics" ON)
option(WITH_NLS "Compile with Native Language Support (using gettext)" ON)
option(WITH_YAML "Compile with YAML support (enables xverbs)" ON)
option(WITH_JEMALLOC "Compile with JEMALLOC support" ON)

option(WITH_FUZZ "Compile for fuzzing purpose (use 'make fuzz' only)" OFF)
mark_as_advanced(WITH_FUZZ)

option(ENABLE_BINRELOC "Enable relocatable binaries" OFF)


include(CMakeScripts/DefineDependsandFlags.cmake) # Includes, Compiler Flags, and Link Libraries
include(CMakeScripts/HelperMacros.cmake) # Misc Utility Macros

# -----------------------------------------------------------------------------
# BAD HACKS, NEED TO INVESTIGATE MAKING THESE LESS BAD
if (NOT CMAKE_BUILD_TYPE MATCHES Debug)
  add_definitions(-D_FORTIFY_SOURCE=2)
endif()
add_definitions(-DHAVE_CONFIG_H)
add_definitions(-DHAVE_CAIRO_PDF=1)  # needed for src/libnrtype/Layout-TNG.h
if(BUILD_SHARED_LIBS AND NOT WIN32)
    add_definitions(-fPIC)
endif()
#
# end badness
# -----------------------------------------------------------------------------



# -----------------------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------------------
add_subdirectory(src)

if(ENABLE_NLS)
    add_subdirectory(po)
endif(ENABLE_NLS)

if(NOT WIN32)
    include(CMakeScripts/Pod2man.cmake)
    add_subdirectory(man)
endif()

# -----------------------------------------------------------------------------
# Test Harness
# -----------------------------------------------------------------------------
find_package(GTest) # gtest 1.8.0 and later already include gmock
if(GTEST_FOUND AND EXISTS "${GTEST_INCLUDE_DIR}/gmock" AND IS_DIRECTORY "${GTEST_INCLUDE_DIR}/gmock")
    set(GMOCK_PRESENT ON)
else()
    unset(GTEST_FOUND)
    if(NOT GMOCK_DIR)
        find_path(GMOCK_DIR
              NAMES src/gmock.cc
              PATHS "$ENV{GMOCK_DIR}"
                "/usr/src/googletest/googlemock"
                    "/usr/src/gmock"
                "${CMAKE_SOURCE_DIR}/gtest/gmock-1.7.0"
             )
    endif(NOT GMOCK_DIR)
    if(EXISTS "${GMOCK_DIR}" AND IS_DIRECTORY "${GMOCK_DIR}")
        set(GMOCK_PRESENT ON)
    else()
        set(GMOCK_PRESENT OFF)
        message("No gmock/gtest found! Perhaps you wish to run 'bash download-gtest.sh' to download it.")
    endif()
endif()
if(GMOCK_PRESENT)
    enable_testing()
    set(CMAKE_CTEST_COMMAND ctest -V)
    if(WIN32)
        set(CMAKE_CTEST_ENV INKSCAPE_DATADIR=${CMAKE_CURRENT_SOURCE_DIR}/share)
    endif()
    add_subdirectory(testfiles EXCLUDE_FROM_ALL)
    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} DEPENDS tests)
    add_dependencies(check inkscape)
endif()

# -----------------------------------------------------------------------------
# Canonicalize the flags to speed up recompilation using ccache/etc.
# This should be the last thing we do:
# -----------------------------------------------------------------------------
include(CMakeScripts/CanonicalizeFlagsVar.cmake)
canonicalize_flags_var("${CMAKE_CXX_FLAGS}" _new_cxx)
set(CMAKE_CXX_FLAGS "${_new_cxx}" CACHE STRING "" FORCE)
# message(FATAL_ERROR "CMAKE_CXX_FLAGS = <${CMAKE_CXX_FLAGS}>")




# -----------------------------------------------------------------------------
# Clean Targets
# -----------------------------------------------------------------------------
add_custom_target(clean-cmake-files
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/CMakeScripts/CleanAll.cmake"
)

add_custom_target(clean-all
    COMMAND ${CMAKE_BUILD_TOOL} clean
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/CMakeScripts/CleanAll.cmake"
)

# -----------------------------------------------------------------------------
# Install Target
# -----------------------------------------------------------------------------
add_subdirectory(share)

install(FILES
	AUTHORS
	TRANSLATORS
	DESTINATION ${INKSCAPE_SHARE_INSTALL}/doc)

if(HAVE_MSYS2)
  include(CMakeScripts/InstallMSYS2.cmake)
else()
  include(CMakeScripts/Install.cmake)
endif()

# -----------------------------------------------------------------------------
# Uninstall Target
# -----------------------------------------------------------------------------
if(WIN32)
    add_custom_target(uninstall
        "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_INSTALL_PREFIX}")
else()
    configure_file(
        "${CMAKE_SOURCE_DIR}/CMakeScripts/cmake_uninstall.cmake.in"
        "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
        @ONLY)
    add_custom_target(uninstall
        "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
endif()

# -----------------------------------------------------------------------------
# Dist Targets
# -----------------------------------------------------------------------------
include(CMakeScripts/Dist.cmake)

# -----------------------------------------------------------------------------
# Packaging (CPack)
# -----------------------------------------------------------------------------
include(CMakeScripts/ConfigCPack.cmake)



# ----------------------------------------------------------------------
# Information Summary
# ----------------------------------------------------------------------
message("------------------------------------------------------------------------")
message("Configuration Summary")
message("------------------------------------------------------------------------")
# project info
message("PROJECT_NAME:            ${PROJECT_NAME}")
message("INKSCAPE_VERSION:        ${INKSCAPE_VERSION}")
message("INKSCAPE_DIST_PREFIX:    ${INKSCAPE_DIST_PREFIX}")
message("INKSCAPE_CPACK_PREFIX:   ${INKSCAPE_CPACK_PREFIX}")
message("")

# cmake info
message("CMAKE_BINARY_DIR:        ${CMAKE_BINARY_DIR}")
message("CMAKE_INSTALL_PREFIX:    ${CMAKE_INSTALL_PREFIX}")
message("PACKAGE_LOCALE_DIR       ${PACKAGE_LOCALE_DIR}")
message("CMAKE_SYSTEM_NAME:       ${CMAKE_SYSTEM_NAME}")
message("CMAKE_SYSTEM_VERSION:    ${CMAKE_SYSTEM_VERSION}")
message("CMAKE_SYSTEM_PROCESSOR:  ${CMAKE_SYSTEM_PROCESSOR}")
message("CMAKE_C_COMPILER:        ${CMAKE_C_COMPILER}")
message("CMAKE_CXX_COMPILER:      ${CMAKE_CXX_COMPILER}")
message("CMAKE_BUILD_TYPE:        ${CMAKE_BUILD_TYPE}")
message("")

if(WIN32)
message("CMAKE_PREFIX_PATH:           ${CMAKE_PREFIX_PATH}")
message("CMAKE_FIND_LIBRARY_PREFIXES: ${CMAKE_FIND_LIBRARY_PREFIXES}")
message("CMAKE_FIND_LIBRARY_SUFFIXES: ${CMAKE_FIND_LIBRARY_SUFFIXES}")
message("")
endif()

# dependency info
message("ENABLE_LCMS:             ${ENABLE_LCMS}")
message("ENABLE_POPPLER:          ${ENABLE_POPPLER}")
message("ENABLE_POPPLER_CAIRO:    ${ENABLE_POPPLER_CAIRO}")
message("GMOCK_PRESENT:           ${GMOCK_PRESENT}")
message("WITH_DBUS:               ${WITH_DBUS}")
message("WITH_GTKSPELL:           ${WITH_GTKSPELL}")
message("WITH_IMAGE_MAGICK:       ${WITH_IMAGE_MAGICK}")
message("WITH_LIBCDR:             ${WITH_LIBCDR}")
message("WITH_LIBVISIO:           ${WITH_LIBVISIO}")
message("WITH_LIBWPG:             ${WITH_LIBWPG}")
message("WITH_NLS:                ${WITH_NLS}")
message("WITH_OPENMP:             ${WITH_OPENMP}")
message("WITH_PROFILING:          ${WITH_PROFILING}")
message("WITH_YAML:               ${WITH_YAML}")
message("WITH_JEMALLOC:           ${WITH_JEMALLOC}")

if(WIN32)
message("")
message("HAVE_MINGW:              ${HAVE_MINGW}")
message("HAVE_MINGW64:            ${HAVE_MINGW64}")
message("MINGW_PATH:              ${MINGW_PATH}")
message("MINGW_ARCH:              ${MINGW_ARCH}")
message("MINGW_ARCH_PATH:         ${MINGW_ARCH_PATH}")
if(NOT HAVE_MSYS2)
message("DEVLIBS_PATH:            ${DEVLIBS_PATH}")
message("DEVLIBS_LIB:             ${DEVLIBS_LIB}")
message("DEVLIBS_BIN:             ${DEVLIBS_BIN}")
message("PKG_CONFIG_PATH:         ${PKG_CONFIG_PATH}")
endif()
endif()

message("------------------------------------------------------------------------")
