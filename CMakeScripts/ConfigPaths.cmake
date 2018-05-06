message(STATUS "Creating build files in: ${CMAKE_CURRENT_BINARY_DIR}")

if(WIN32)
  if(${CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT})
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/inkscape"
        CACHE PATH "Install path prefix, prepended onto install directories." FORCE)
  endif()
  
  set(INKSCAPE_LIBDIR "\\\\lib")
  set(INKSCAPE_DATADIR "")
  
  set(PACKAGE_LOCALE_DIR "\\\\share\\\\locale")
  
  set(SHARE_INSTALL "share" CACHE STRING "Data file install path. Must be a relative path (from CMAKE_INSTALL_PREFIX), with no trailing slash.")
  set(INKSCAPE_SHARE_INSTALL "${SHARE_INSTALL}") # share/inkscape goes directly into /share on Windows
  
  mark_as_advanced(SHARE_INSTALL)
else()
  set(INKSCAPE_LIBDIR "${CMAKE_INSTALL_PREFIX}/lib")
  set(INKSCAPE_DATADIR "${CMAKE_INSTALL_PREFIX}/share")

  # TODO: check and change this to correct value:
  if(NOT PACKAGE_LOCALE_DIR)
    set(PACKAGE_LOCALE_DIR "${CMAKE_INSTALL_PREFIX}/share/locale") # packagers might overwrite this
  endif(NOT PACKAGE_LOCALE_DIR)

  if(NOT SHARE_INSTALL)
    set(SHARE_INSTALL "share" CACHE STRING "Data file install path. Must be a relative path (from CMAKE_INSTALL_PREFIX), with no trailing slash.")
  endif(NOT SHARE_INSTALL)
  set(INKSCAPE_SHARE_INSTALL "${SHARE_INSTALL}/inkscape")

  mark_as_advanced(SHARE_INSTALL)
endif()