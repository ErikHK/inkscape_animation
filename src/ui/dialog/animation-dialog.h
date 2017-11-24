/** @file
 * @brief XML tree editing dialog for Inkscape
 */
/* Copyright Lauris Kaplinski, 2000
 *
 * Released under GNU General Public License.
 *
 * This is XML tree editor, which allows direct modifying of all elements
 *   of Inkscape document, including foreign ones.
 *
 */

#ifndef SEEN_DIALOGS_XML_TREE_H
#define SEEN_DIALOGS_XML_TREE_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ui/widget/panel.h"
#include <gtkmm/entry.h>
#include <gtkmm/textview.h>
#include <gtkmm/toolbar.h>
#include <gtkmm/separatortoolitem.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/paned.h>

#include "ui/dialog/desktop-tracker.h"
#include "message.h"

class SPDesktop;
class SPObject;
struct SPXMLViewAttrList;
struct SPXMLViewContent;
struct SPXMLViewTree;

namespace Inkscape {
class MessageStack;
class MessageContext;

namespace XML {
class Node;
}

namespace UI {
namespace Dialog {

/**
 * A ...
 *
 */

class AnimationDialog : public Widget::Panel {
public:
    AnimationDialog ();
    ~AnimationDialog ();

    static AnimationDialog &getInstance() { return *new AnimationDialog(); }

private:

    void onNameChanged();
    void onCreateNameChanged();

    /**
      * Callbacks for changes in desktop selection and current document
      */
    void on_desktop_selection_changed();
    void on_document_replaced(SPDesktop *dt, SPDocument *document);
    static void on_document_uri_set(gchar const *uri, SPDocument *document);

    static void _set_status_message(Inkscape::MessageType type, const gchar *message, GtkWidget *dialog);

    virtual void present();


    bool in_dt_coordsys(SPObject const &item);

    /**
     * Flag to ensure only one operation is perfomed at once
     */
    gint blocked;

    /**
     * Status bar
     */
    Inkscape::MessageStack *_message_stack;
    Inkscape::MessageContext *_message_context;

    /**
     * Signal handlers
     */
    sigc::connection _message_changed_connection;
    sigc::connection document_replaced_connection;
    sigc::connection document_uri_set_connection;
    sigc::connection sel_changed_connection;

    /**
    /**
     * Current document and desktop this dialog is attached to
     */
    SPDesktop *current_desktop;
    SPDocument *current_document;

    gint selected_attr;
    Inkscape::XML::Node *selected_repr;

#if WITH_GTKMM_3_0
    Gtk::Paned paned;
#else
    Gtk::HPaned paned;
#endif

#if WITH_GTKMM_3_0
    Gtk::Paned attr_subpaned_container;
#else
    Gtk::VPaned attr_subpaned_container;
#endif

    Gtk::Button set_attr;

    GtkWidget *new_window;

    DesktopTracker deskTrack;
    sigc::connection desktopChangeConn;
};

}
}
}

#endif

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
