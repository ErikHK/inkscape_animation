/*
 * Inkscape - an ambitious vector drawing program
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Frank Felfe <innerspace@iname.com>
 *   Davide Puricelli <evo@debian.org>
 *   Mitsuru Oka <oka326@parkcity.ne.jp>
 *   Masatake YAMATO  <jet@gyve.org>
 *   F.J.Franklin <F.J.Franklin@sheffield.ac.uk>
 *   Michael Meeks <michael@helixcode.com>
 *   Chema Celorio <chema@celorio.com>
 *   Pawel Palucha
 * ... and various people who have worked with various projects
 *   Abhishek Sharma
 *
 * Copyright (C) 1999-2002 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Inkscape authors:
 *   Johan Ceuppens
 *
 * Copyright (C) 2004 Inkscape authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */


#ifndef SEEN_SP_SVG_SLIDESHOW_H
#define SEEN_SP_SVG_SLIDESHOW_H

#include <gtkmm/applicationwindow.h>

/**
 * The main application window for the slideshow
 */
class SPSlideShow : public Gtk::ApplicationWindow {
public:
    SPSlideShow(std::vector<Glib::ustring> const &slides,
                bool fullscreen,
                int timer,
                double scale);

private:                                     
    std::vector<Glib::ustring>  _slides;     // list of filenames for each slide
    int                         _current;    // index of the currently displayed slide
    SPDocument                 *_doc;        // parsed SPDocument of the currently displayed slide
    bool                        _fullscreen; // is window fullscreen? (also controls whether to launch in fullscreen mode)
    int                         _timer;      // time after which slides are automatically changed (in seconds)
    double                      _scale;      // scale factor for images
    GtkWidget                  *_view;       // the canvas to which the images are drawn
    Gtk::Window                *_ctrlwin;    // window containing slideshow control buttons

    void control_show();
    void show_next();
    void show_prev();
    void goto_first();
    void goto_last();
    bool timer_callback();

    bool key_press (GdkEventKey *event);
    bool main_delete (GdkEventAny *event);
    bool ctrlwin_delete (GdkEventAny *event);

    void update_title();
    void waiting_cursor();
    void normal_cursor();
    void set_document(SPDocument *doc,
                      int         current);
};

#endif // SEEN_SP_SVG_SLIDESHOW_H

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
