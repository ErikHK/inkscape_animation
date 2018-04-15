# -----------------------------------------------------------------------------
# Define the main source
# -----------------------------------------------------------------------------

set(inkscape_SRC
  attribute-rel-css.cpp
  attribute-rel-svg.cpp
  attribute-rel-util.cpp
  attribute-sort-util.cpp
  attributes.cpp
  axis-manip.cpp
  color.cpp
  composite-undo-stack-observer.cpp
  conditions.cpp
  conn-avoid-ref.cpp
  console-output-undo-observer.cpp
  context-fns.cpp
  desktop-events.cpp
  desktop-style.cpp
  desktop.cpp
  device-manager.cpp
  dir-util.cpp
  document-subset.cpp
  document-undo.cpp
  document.cpp
  ege-color-prof-tracker.cpp
  event-log.cpp
  extract-uri.cpp
  file.cpp
  file-update.cpp
  filter-chemistry.cpp
  filter-enums.cpp
  gc-anchored.cpp
  gc-finalized.cpp
  gradient-chemistry.cpp
  gradient-drag.cpp
  graphlayout.cpp
  guide-snapper.cpp
  help.cpp
  id-clash.cpp
  inkscape.cpp
  knot-holder-entity.cpp
  knot-ptr.cpp
  knot.cpp
  knotholder.cpp
  layer-fns.cpp
  layer-manager.cpp
  layer-model.cpp
  line-geometry.cpp
  line-snapper.cpp
  main-cmdlineact.cpp
  main-cmdlinexact.cpp
  media.cpp
  message-context.cpp
  message-stack.cpp
  mod360.cpp
  object-hierarchy.cpp
  object-snapper.cpp
  path-chemistry.cpp
  path-prefix.cpp
  perspective-line.cpp
  preferences.cpp
  prefix.cpp
  print.cpp
  profile-manager.cpp
  proj_pt.cpp
  pure-transform.cpp
  rdf.cpp
  removeoverlap.cpp
  resource-manager.cpp
  rubberband.cpp
  satisfied-guide-cns.cpp
  selcue.cpp
  selection-chemistry.cpp
  selection-describer.cpp
  selection.cpp
  seltrans-handles.cpp
  seltrans.cpp
  shortcuts.cpp
  snap-preferences.cpp
  snap.cpp
  snapped-curve.cpp
  snapped-line.cpp
  snapped-point.cpp
  snapper.cpp
  splivarot.cpp
  sp-cursor.cpp
  sp-item-notify-moveto.cpp 
  style-internal.cpp
  style.cpp
  svg-view-slideshow.cpp
  svg-view-widget.cpp
  svg-view.cpp
  text-chemistry.cpp
  text-editing.cpp
  transf_mat_3x4.cpp
  unclump.cpp
  unicoderange.cpp
  vanishing-point.cpp
  verbs.cpp
  version.cpp

  # -------
  # Headers
  attribute-rel-css.h
  attribute-rel-svg.h
  attribute-rel-util.h
  attribute-sort-util.h
  attributes.h
  axis-manip.h
  bad-uri-exception.h
  cms-color-types.h
  cms-system.h
  color-profile-cms-fns.h
  color-rgba.h
  color.h
  colorspace.h
  composite-undo-stack-observer.h
  conditions.h
  conn-avoid-ref.h
  console-output-undo-observer.h
  context-fns.h
  desktop-events.h
  desktop-style.h
  desktop.h
  device-manager.h
  dir-util.h
  document-private.h
  document-subset.h
  document-undo.h
  document.h
  ege-color-prof-tracker.h
  enums.h
  event-log.h
  event.h
  extract-uri.h
  file.h
  fill-or-stroke.h
  filter-chemistry.h
  filter-enums.h
  gc-anchored.h
  gc-finalized.h
  gradient-chemistry.h
  gradient-drag.h
  graphlayout.h
  guide-snapper.h
  help.h
  helper-fns.h
  id-clash.h
  inkscape-version.h
  inkscape.h
  knot-enums.h
  knot-holder-entity.h
  knot-ptr.h
  knot.h
  knotholder.h
  layer-fns.h
  layer-manager.h
  layer-model.h
  line-geometry.h
  line-snapper.h
  macros.h
  main-cmdlineact.h
  main-cmdlinexact.h
  media.h
  menus-skeleton.h
  message-context.h
  message-stack.h
  message.h
  mod360.h
  number-opt-number.h
  object-hierarchy.h
  object-snapper.h
  path-chemistry.h
  path-prefix.h
  perspective-line.h
  preferences-skeleton.h
  preferences.h
  prefix.h
  print.h
  profile-manager.h
  proj_pt.h
  pure-transform.h
  rdf.h
  remove-last.h
  removeoverlap.h
  resource-manager.h
  rubberband.h
  satisfied-guide-cns.h
  selcue.h
  selection-chemistry.h
  selection-describer.h
  selection.h
  seltrans-handles.h
  seltrans.h
  shortcuts.h
  snap-candidate.h
  snap-enums.h
  snap-preferences.h
  snap.h
  snapped-curve.h
  snapped-line.h
  snapped-point.h
  snapper.h
  sp-cursor.h
  sp-guide-attachment.h
  sp-guide-constraint.h
  sp-item-notify-moveto.h
  splivarot.h
  streq.h
  strneq.h
  style-enums.h
  style-internal.h
  style.h
  svg-profile.h
  svg-view-slideshow.h
  svg-view-widget.h
  svg-view.h
  syseq.h
  text-chemistry.h
  text-editing.h
  text-tag-attributes.h
  transf_mat_3x4.h
  unclump.h
  undo-stack-observer.h
  unicoderange.h
  vanishing-point.h
  verbs.h
  version.h
)
  
# -----------------------------------------------------------------------------
# Generate version file
# -----------------------------------------------------------------------------

# configure with values known at configure time
configure_file(inkscape-version.cpp.in ${CMAKE_BINARY_DIR}/src/inkscape-version.cpp.in @ONLY)

# configure further at build time (always built as run_always.txt is never actually created)
add_custom_command(
    OUTPUT inkscape-version.cpp run_always.txt
    COMMAND ${CMAKE_COMMAND}
        -DINKSCAPE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DINKSCAPE_BINARY_DIR=${CMAKE_BINARY_DIR}
        -P ${CMAKE_SOURCE_DIR}/CMakeScripts/inkscape-version.cmake
    COMMENT "Generating inkscape-version.cpp")

list(APPEND inkscape_SRC
  ${CMAKE_BINARY_DIR}/src/inkscape-version.cpp
)


# -----------------------------------------------------------------------------
# Load in subdirectories
# -----------------------------------------------------------------------------

# All folders for internal inkscape
# these call add_inkscape_source
add_subdirectory(debug)
add_subdirectory(display)
add_subdirectory(extension)
add_subdirectory(helper)
add_subdirectory(io)
add_subdirectory(live_effects)
add_subdirectory(object)
add_subdirectory(svg)
add_subdirectory(trace)
add_subdirectory(ui)
add_subdirectory(util)
add_subdirectory(widgets)
add_subdirectory(xml)
add_subdirectory(2geom)

# Directories containing lists files that describe building internal libraries
add_subdirectory(libavoid)
add_subdirectory(libcola)
add_subdirectory(libcroco)
add_subdirectory(inkgc)
add_subdirectory(libuemf)
add_subdirectory(libvpsc)
add_subdirectory(livarot)
add_subdirectory(libnrtype)
add_subdirectory(libdepixelize)

get_property(inkscape_global_SRC GLOBAL PROPERTY inkscape_global_SRC)

set(inkscape_SRC
  ${inkscape_global_SRC}
  ${inkscape_SRC}
)

# -----------------------------------------------------------------------------
# Setup the executable
# -----------------------------------------------------------------------------
set(main_SRC main.cpp)
set(view_SRC
  inkview.cpp
  inkview-options-group.cpp
  inkview-options-group.h
)

if(WIN32)
  # Sources for the inkscape executable on Windows.
  list(APPEND main_SRC
    registrytool.h
    registrytool.cpp
  )

  # Configure and add the platform specific resource files (enabling the app icon).
  string(TIMESTAMP COPYRIGHT_YEAR %Y)
  
  set(FILE_NAME inkscape)
  set(PROGRAM_NAME Inkscape)
  set(PROGRAM_DESCRIPTION "Inkscape vector graphics editor")
  configure_file(inkscape.rc ${CMAKE_BINARY_DIR}/src/inkscape.rc)
  configure_file(inkscape-manifest.xml ${CMAKE_BINARY_DIR}/src/inkscape-manifest.xml)  
  list(APPEND main_SRC ${CMAKE_BINARY_DIR}/src/inkscape.rc)
  
  set(FILE_NAME inkview)
  set(PROGRAM_NAME Inkview)
  set(PROGRAM_DESCRIPTION "Inkview vector graphics viewer")
  configure_file(inkscape.rc ${CMAKE_BINARY_DIR}/src/inkview.rc)
  configure_file(inkscape-manifest.xml ${CMAKE_BINARY_DIR}/src/inkview-manifest.xml)
  list(APPEND view_SRC ${CMAKE_BINARY_DIR}/src/inkview.rc)
  
  # Build console applications as wrappers around inkscape/inkview.exe as GUI applications can't print to the console.
  add_executable(inkscape_com winconsole.cpp)
  set_target_properties(inkscape_com
                        PROPERTIES
                          LINK_FLAGS "-mconsole"
                          OUTPUT_NAME "inkscape"
                          SUFFIX ".com")
  add_executable(inkview_com winconsole.cpp)
  set_target_properties(inkview_com
                        PROPERTIES
                          LINK_FLAGS "-mconsole"
                          OUTPUT_NAME "inkview"
                          SUFFIX ".com")
endif()

# make executables for inkscape and inkview
add_executable(inkscape ${main_SRC})
add_executable(inkview ${view_SRC})



set(INKSCAPE_TARGET_LIBS
        # order from automake
        #sp_LIB
        #nrtype_LIB
        #inkscape_LIB
        #sp_LIB # annoying, we need both!
        nrtype_LIB # annoying, we need both!
        croco_LIB
        avoid_LIB
        cola_LIB
        vpsc_LIB
        livarot_LIB
        uemf_LIB
        2geom_LIB
        depixelize_LIB
        util_LIB
        gc_LIB
)

# Build everything except main and inkview.c in a shared library.
add_library(inkscape_base ${inkscape_SRC} ${sp_SRC})

if(WITH_DBUS)
    add_dependencies(inkscape_base inkscape_dbus)
endif()

# Link the inkscape_base library against all external dependencies
target_link_libraries(inkscape_base PRIVATE ${INKSCAPE_TARGET_LIBS} PUBLIC ${INKSCAPE_LIBS})

# Link inkscape and inkview against inkscape_base
target_link_libraries(inkscape inkscape_base)
target_link_libraries(inkview inkscape_base)



#Define the installation
if(NOT WIN32)
  install(TARGETS
    inkscape
    inkview 
    RUNTIME DESTINATION bin)
  if(BUILD_SHARED_LIBS)
    install(TARGETS inkscape_base LIBRARY DESTINATION lib/inkscape)
  endif()
else()
  install(TARGETS
    inkscape
    inkscape_com
    inkview
    inkview_com
    RUNTIME DESTINATION .)
  if(BUILD_SHARED_LIBS)
    install(TARGETS inkscape_base RUNTIME DESTINATION .)
  endif()
endif()
