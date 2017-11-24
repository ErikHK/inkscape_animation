/**
 * @file
 * XML editor.
 */
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   MenTaLguY <mental@rydia.net>
 *   bulia byak <buliabyak@users.sf.net>
 *   Johan Engelen <goejendaagh@zonnet.nl>
 *   David Turner
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 1999-2006 Authors
 * Released under GNU GPL, read the file 'COPYING' for more information
 *
 */

#include "animation-dialog.h"
#include "widgets/icon.h"
#include <gdk/gdkkeysyms.h>
#include <glibmm/i18n.h>
#include <gtkmm/stock.h>

#include "desktop.h"

#include "ui/dialog-events.h"
#include "document.h"
#include "document-undo.h"
#include "ui/tools/tool-base.h"
#include "inkscape.h"
#include "ui/interface.h"
#include "macros.h"
#include "message-context.h"
#include "message-stack.h"
#include "preferences.h"
//#include "selection.h"
#include "shortcuts.h"
#include "sp-root.h"
//#include "sp-string.h"
//#include "sp-tspan.h"
//#include "ui/icon-names.h"
#include "verbs.h"
//#include "widgets/icon.h"

namespace Inkscape {
namespace UI {
namespace Dialog {

AnimationDialog::AnimationDialog (void) :
    UI::Widget::Panel ("", "/dialogs/animation/", SP_VERB_DIALOG_ANIMATION_DIALOG),
    blocked (0),
    _message_stack (NULL),
    _message_context (NULL),
    current_desktop (NULL),
    current_document (NULL),
    selected_attr (0),
    selected_repr (NULL),
    new_window(NULL)
{

    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    if (!desktop) {
        return;
    }

    _message_stack = new Inkscape::MessageStack();
    _message_context = new Inkscape::MessageContext(_message_stack);
    //_message_changed_connection = _message_stack->connectChanged(
    //        sigc::bind(sigc::ptr_fun(_set_status_message), GTK_WIDGET(status.gobj())));

    /* tree view */
    //paned.pack1(left_box);

    /* initial show/hide */
    show_all();

/*
    // hide() doesn't seem to work in the constructor, so moved this to present()
    text_container.hide();
    attr_container.hide();
*/
    g_assert(desktop != NULL);
}

void AnimationDialog::present()
{
    UI::Widget::Panel::present();
}

AnimationDialog::~AnimationDialog (void)
{

    _message_changed_connection.disconnect();
    delete _message_context;
    _message_context = NULL;
    Inkscape::GC::release(_message_stack);
    _message_stack = NULL;
    _message_changed_connection.~connection();
}

void AnimationDialog::_set_status_message(Inkscape::MessageType /*type*/, const gchar *message, GtkWidget *widget)
{
    if (widget) {
        gtk_label_set_markup(GTK_LABEL(widget), message ? message : "");
    }
}

}
}
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
