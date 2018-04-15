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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glibmm/main.h>
#include <gtkmm/button.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/image.h>
#include <gtkmm/main.h>

#include "document.h"
#include "ui/icon-names.h"
#include "ui/monitor.h"
#include "util/units.h"

#include "svg-view.h"
#include "svg-view-slideshow.h"
#include "svg-view-widget.h"


SPSlideShow::SPSlideShow(std::vector<Glib::ustring> const &slides, bool full_screen, int timer, double scale)
    : _slides(slides)
    , _current(0)
    , _doc(SPDocument::createNewDoc(_slides[0].c_str(), true, false))
    , _fullscreen(full_screen)
    , _timer(timer)
    , _scale(scale)
    , _view(NULL)
    , _ctrlwin(NULL)
{
    // setup initial document
    Gdk::Rectangle monitor_geometry = Inkscape::UI::get_monitor_geometry_primary(); // TODO: is inkview always launched on primary monitor?
    set_default_size(MIN ((int)_doc->getWidth().value("px")*_scale,  monitor_geometry.get_width()  - 64),
                     MIN ((int)_doc->getHeight().value("px")*_scale, monitor_geometry.get_height() - 64));

    _view = sp_svg_view_widget_new(_doc);
    SP_SVG_VIEW_WIDGET(_view)->setResize( false, _doc->getWidth().value("px"), _doc->getHeight().value("px") );
    gtk_widget_show (_view);
    add(*Glib::wrap(_view));

    update_title();
    show();
    if(_fullscreen) {
        fullscreen();
    }

    // connect signals
    this->signal_key_press_event().connect(sigc::mem_fun(*this, &SPSlideShow::key_press), false);
    this->signal_delete_event().connect(sigc::mem_fun(*this, &SPSlideShow::main_delete), false);
    if(_timer) {
        Glib::signal_timeout().connect_seconds(sigc::mem_fun(*this, &SPSlideShow::timer_callback), _timer);
    }
}



/**
 * @brief Show the control buttons (next, previous etc) for the application
 */
void SPSlideShow::control_show()
{
    if (!_ctrlwin) {
        _ctrlwin = new Gtk::Window();
        _ctrlwin->set_resizable(false);
        _ctrlwin->set_transient_for(*this);

        _ctrlwin->signal_key_press_event().connect(sigc::mem_fun(*this, &SPSlideShow::key_press), false);
        _ctrlwin->signal_delete_event().connect(sigc::mem_fun(*this, &SPSlideShow::ctrlwin_delete), false);

        auto t = Gtk::manage(new Gtk::ButtonBox());
        _ctrlwin->add(*t);

        auto btn_go_first = Gtk::manage(new Gtk::Button());
        auto img_go_first = Gtk::manage(new Gtk::Image());
        img_go_first->set_from_icon_name(INKSCAPE_ICON("go-first"), Gtk::ICON_SIZE_BUTTON);
        btn_go_first->set_image(*img_go_first);
        t->add(*btn_go_first);
        btn_go_first->signal_clicked().connect(sigc::mem_fun(*this, &SPSlideShow::goto_first));

        auto btn_go_prev = Gtk::manage(new Gtk::Button());
        auto img_go_prev = Gtk::manage(new Gtk::Image());
        img_go_prev->set_from_icon_name(INKSCAPE_ICON("go-previous"), Gtk::ICON_SIZE_BUTTON);
        btn_go_prev->set_image(*img_go_prev);
        t->add(*btn_go_prev);
        btn_go_prev->signal_clicked().connect(sigc::mem_fun(*this, &SPSlideShow::show_prev));

        auto btn_go_next = Gtk::manage(new Gtk::Button());
        auto img_go_next = Gtk::manage(new Gtk::Image());
        img_go_next->set_from_icon_name(INKSCAPE_ICON("go-next"), Gtk::ICON_SIZE_BUTTON);
        btn_go_next->set_image(*img_go_next);
        t->add(*btn_go_next);
        btn_go_next->signal_clicked().connect(sigc::mem_fun(*this, &SPSlideShow::show_next));

        auto btn_go_last = Gtk::manage(new Gtk::Button());
        auto img_go_last = Gtk::manage(new Gtk::Image());
        img_go_last->set_from_icon_name(INKSCAPE_ICON("go-last"), Gtk::ICON_SIZE_BUTTON);
        btn_go_last->set_image(*img_go_last);
        t->add(*btn_go_last);
        btn_go_last->signal_clicked().connect(sigc::mem_fun(*this, &SPSlideShow::goto_last));

        _ctrlwin->show_all();
    } else {
        _ctrlwin->present();
    }
}

void SPSlideShow::waiting_cursor()
{
    auto display = Gdk::Display::get_default();
    auto waiting = Gdk::Cursor::create(display, Gdk::WATCH);
    get_window()->set_cursor(waiting);

    if (_ctrlwin) {
        _ctrlwin->get_window()->set_cursor(waiting);
    }
    while(Gtk::Main::events_pending()) {
        Gtk::Main::iteration();
    }
}

void SPSlideShow::normal_cursor()
{
    get_window()->set_cursor();
    if (_ctrlwin) {
        _ctrlwin->get_window()->set_cursor();
    }
}


/// Update the window title with current document name
void SPSlideShow::update_title()
{
    Glib::ustring title(_doc->getName());
    if (_slides.size() > 1) {
        title += Glib::ustring::compose("  (%1/%2)", _current+1, _slides.size());
    }

    set_title(title);
}

void SPSlideShow::set_document(SPDocument *doc,
                               int         current)
{
    if (doc && doc != _doc) {
        doc->ensureUpToDate();
        reinterpret_cast<SPSVGView*>(SP_VIEW_WIDGET_VIEW (_view))->setDocument (doc);
        _doc = doc;
        _current = current;
        update_title();
    }
}

/**
 * @brief Show the next file in the slideshow
 */
void SPSlideShow::show_next()
{
    waiting_cursor();

    SPDocument *doc = NULL;
    while (!doc && (_current < _slides.size() - 1)) {
        doc = SPDocument::createNewDoc ((_slides[++_current]).c_str(), TRUE, false);
    }

    set_document(doc, _current);
    normal_cursor();
}

/**
 * @brief Show the previous file in the slideshow
 */
void SPSlideShow::show_prev()
{
    waiting_cursor();

    SPDocument *doc = NULL;
    while (!doc && (_current > 0)) {
        doc = SPDocument::createNewDoc ((_slides[--_current]).c_str(), TRUE, false);
    }

    set_document(doc, _current);
    normal_cursor();
}

/**
 * @brief Switch to first slide in slideshow
 */
void SPSlideShow::goto_first()
{
    waiting_cursor();

    SPDocument *doc = NULL;
    int current = 0;
    while ( !doc && (current < _slides.size() - 1)) {
        doc = SPDocument::createNewDoc((_slides[current++]).c_str(), TRUE, false);
    }

    set_document(doc, current - 1);

    normal_cursor();
}

/**
 * @brief Switch to last slide in slideshow
 */
void SPSlideShow::goto_last()
{
    waiting_cursor();

    SPDocument *doc = NULL;
    int current = _slides.size() - 1;
    while (!doc && (current >= 0)) {
        doc = SPDocument::createNewDoc((_slides[current--]).c_str(), TRUE, false);
    }

    set_document(doc, current + 1);

    normal_cursor();
}

bool SPSlideShow::timer_callback()
{
    show_next();

    // stop the timer if the last slide is reached
    if (_current == _slides.size()-1) {
        return false;
    } else {
        return true;
    }
}

bool SPSlideShow::ctrlwin_delete (GdkEventAny */*event*/)
{
    if(_ctrlwin) delete _ctrlwin;
    _ctrlwin = NULL;
    return true;
}

bool SPSlideShow::main_delete (GdkEventAny */*event*/)
{
    Gtk::Main::quit();
    return true;
}

bool SPSlideShow::key_press(GdkEventKey* event)
{
    switch (event->keyval) {
        case GDK_KEY_Up:
        case GDK_KEY_Home:
            goto_first();
            break;
        case GDK_KEY_Down:
        case GDK_KEY_End:
            goto_last();
            break;
        case GDK_KEY_F11:
            if (_fullscreen) {
                unfullscreen();
                _fullscreen = false;
            } else {
                fullscreen();
                _fullscreen = true;
            }
            break;
        case GDK_KEY_Return:
            control_show();
        break;
        case GDK_KEY_KP_Page_Down:
        case GDK_KEY_Page_Down:
        case GDK_KEY_Right:
        case GDK_KEY_space:
            show_next();
        break;
        case GDK_KEY_KP_Page_Up:
        case GDK_KEY_Page_Up:
        case GDK_KEY_Left:
        case GDK_KEY_BackSpace:
            show_prev();
            break;
        case GDK_KEY_Escape:
        case GDK_KEY_q:
        case GDK_KEY_Q:
            Gtk::Main::quit();
            break;
        default:
            break;
    }
    return false;
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
