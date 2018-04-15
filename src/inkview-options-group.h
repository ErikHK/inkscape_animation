#ifndef INKVIEW_OPTIONS_GROUP_H
#define INKVIEW_OPTIONS_GROUP_H

#include <glibmm.h>

/**
 * \brief Set of command-line options for Inkview
 */
class InkviewOptionsGroup : public Glib::OptionGroup
{
public:
    // list of all input filenames;
    // this list contains all arguments that are not recognized as an option (so needs to be checked)
    Glib::OptionGroup::vecustrings filenames;

    bool fullscreen = false; // whether to launch in fullscreen mode
    bool recursive = false;  // whether to search folders for SVG files recursively
    int timer = 0;           // time (in seconds) after which the next image of the slideshow is automatically loaded
    double scale = 1;        // scale factor for images
                             //   (currently only applied to the first image - others are resized to window dimensions)

    InkviewOptionsGroup();
};
#endif // INKVIEW_OPTIONS_GROUP_H

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
