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


static void handleEaseChanged(AnimationDialog * add, gpointer user_data)
{
	AnimationDialog* ad = reinterpret_cast<AnimationDialog*>(user_data);

	SPDesktop * desktop = ad->getDesktop();

	ad->updateEaseValue();
	desktop->emitToolSubselectionChanged(NULL);


}


AnimationDialog::AnimationDialog() :
    // UI::Widget::Panel("AnimationDialog Label", "/dialogs/AnimationDialog", SP_VERB_DIALOG_AnimationDialog,
    //                "Prototype Apply Label", true),
    UI::Widget::Panel("", "/dialogs/animation", SP_VERB_DIALOG_ANIMATION_DIALOG),

	_tweenFrame("Tween"),
    desktopTracker() //,
    // desktopChangedConnection()
{
    std::cout << "AnimationDialog::AnimationDialog()" << std::endl;

    // A widget for demonstration that displays the current SVG's id.
    //_getContents()->pack_start(label);  // Panel::_getContents()


    // desktop is set by Panel constructor so this should never be NULL.
    // Note, we need to use getDesktop() since _desktop is private in Panel.h.
    // It should probably be protected instead... but need to verify in doesn't break anything.
    if (getDesktop() == NULL) {
        std::cerr << "AnimationDialog::AnimationDialog: desktop is NULL!" << std::endl;
    }

    connectionDesktopChanged = desktopTracker.connectDesktopChanged(
        sigc::mem_fun(*this, &AnimationDialog::handleDesktopChanged) );
    desktopTracker.connect(GTK_WIDGET(gobj()));

    // This results in calling handleDocumentReplaced twice. Fix me!
    connectionDocumentReplaced = getDesktop()->connectDocumentReplaced(
        sigc::mem_fun(this, &AnimationDialog::handleDocumentReplaced));

    // Alternative mechanism but results in calling handleDocumentReplaced four times.
    // signalDocumentReplaced().connect(
    //    sigc::mem_fun(this, &AnimationDialog::handleDocumentReplaced));

    connectionSelectionChanged = getDesktop()->getSelection()->connectChanged(
        sigc::hide(sigc::mem_fun(this, &AnimationDialog::handleSelectionChanged)));

    getDesktop()->connectCurrentLayerChanged(
    	sigc::hide(sigc::mem_fun(this, &AnimationDialog::handleCurrentLayerChanged)));


    updateLabel();
    Gtk::Label * ease = new Gtk::Label("Ease:");
    scale = new Gtk::HScale(-10, 10.1, .1);
    scale->set_size_request(100, -1);
    scale->set_sensitive(false);

    scale->set_update_policy(Gtk::UPDATE_DISCONTINUOUS);

    //scale->connect_property_changed("value", sigc::mem_fun(&AnimationDialog::handleEaseChanged));


    g_signal_connect( scale->gobj(),
    						  "value-changed",
    						  G_CALLBACK(handleEaseChanged),
    						  this);

    ease->set_size_request(50, -1);
    ease->set_property("valign", Gtk::ALIGN_END);
    _easeBox.set_property("valign", Gtk::ALIGN_END);

    Gtk::Label * rotate = new Gtk::Label("Rotate:");
    Gtk::Label * rotate2 = new Gtk::Label("revs +");
    Gtk::Label * rotate3 = new Gtk::Label("°");

    Gtk::Adjustment *adj = new Gtk::Adjustment(1.0, 1.0, 10000000, 1.0, 5.0, 0.0);
    Gtk::Adjustment *adj2 = new Gtk::Adjustment(1.0, 1.0, 10000000, 1.0, 5.0, 0.0);
	Gtk::SpinButton * spin_revolutions = new Gtk::SpinButton(*adj);
	Gtk::SpinButton * spin_degrees = new Gtk::SpinButton(*adj2);

	spin_revolutions->set_size_request(100, -1);
	spin_degrees->set_size_request(100, -1);
	spin_revolutions->set_sensitive(false);
	spin_degrees->set_sensitive(false);

	rotate->set_size_request(50, -1);
	rotate->set_property("valign", Gtk::ALIGN_END);
	_rotateBox.set_property("valign", Gtk::ALIGN_END);


    //_tweenBox.add(lbl);
    //_testBox.pack_start(lbl);
	/*
    _easeBox.add(*ease);
    _easeBox.add(*scale);

    _rotateBox.add(*rotate);
	_rotateBox.add(*spin_revolutions);
	_rotateBox.add(*rotate2);
	_rotateBox.add(*spin_degrees);
	_rotateBox.add(*rotate3);
	*/

	_easeRotateBox.attach(*ease, 0, 1, 0, 1);
	_easeRotateBox.attach(*scale, 1, 5, 0, 1);
	_easeRotateBox.attach(*rotate, 0, 1, 1, 2);
	_easeRotateBox.attach(*spin_revolutions, 1, 2, 1, 2);
	_easeRotateBox.attach(*rotate2, 2, 3, 1, 2);
	_easeRotateBox.attach(*spin_degrees, 3, 4, 1, 2);
	_easeRotateBox.attach(*rotate3, 4, 5, 1, 2);


    //_testBox->add(label);
    //_tweenBox.pack_start(_easeBox);
    //_tweenBox.pack_start(_rotateBox);
	_tweenBox.pack_start(_easeRotateBox);

    _tweenFrame.add(_tweenBox);
    _getContents()->pack_start(_tweenFrame);  // Panel::_getContents()

}

AnimationDialog::~AnimationDialog()
{
    // Never actually called.
    std::cout << "AnimationDialog::~AnimationDialog()" << std::endl;
    connectionDesktopChanged.disconnect();
    connectionDocumentReplaced.disconnect();
    connectionSelectionChanged.disconnect();
}

/*
 * Called when a dialog is displayed, including when a dialog is reopened.
 * (When a dialog is closed, it is not destroyed so the contructor is not called.
 * This function can handle any reinitialization needed.)
 */
void
AnimationDialog::present()
{
    std::cout << "AnimationDialog::present()" << std::endl;
    UI::Widget::Panel::present();
}

/*
 * When Inkscape is first opened, a default document is shown. If another document is immediately
 * opened, it will replace the default document in the same desktop. This function handles the
 * change. Bug: This is called twice for some reason.
 */
void
AnimationDialog::handleDocumentReplaced(SPDesktop *desktop, SPDocument * /* document */)
{
    if (getDesktop() != desktop) {
        std::cerr << "AnimationDialog::handleDocumentReplaced(): Error: panel desktop not equal to existing desktop!" << std::endl;
    }

    connectionSelectionChanged.disconnect();

    connectionSelectionChanged = desktop->getSelection()->connectChanged(
        sigc::hide(sigc::mem_fun(this, &AnimationDialog::handleSelectionChanged)));

    // Update demonstration widget.
    updateLabel();
}

/*
 * When a dialog is floating, it is connected to the active desktop.
 */
void
AnimationDialog::handleDesktopChanged(SPDesktop* desktop) {

    if (getDesktop() == desktop) {
        // This will happen after construction of AnimationDialog. We've already
        // set up signals so just return.
        return;
    }

    // Connections are disconnect safe.
    connectionSelectionChanged.disconnect();
    connectionDocumentReplaced.disconnect();

    setDesktop( desktop );

    connectionSelectionChanged = desktop->getSelection()->connectChanged(
        sigc::hide(sigc::mem_fun(this, &AnimationDialog::handleSelectionChanged)));
    connectionDocumentReplaced = desktop->connectDocumentReplaced(
        sigc::mem_fun(this, &AnimationDialog::handleDocumentReplaced));

    // Update demonstration widget.
    updateLabel();
}

/*
 * Handle a change in which objects are selected in a document.
 */
void
AnimationDialog::handleSelectionChanged() {
    std::cout << "AnimationDialog::handleSelectionChanged()" << std::endl;

    // Update demonstration widget.
    label.set_label("Selection Changed!");
}

void AnimationDialog::updateEaseValue()
{
	SPDesktop * desktop = getDesktop();
		if(!desktop)
			return;

		//desktop->getDocument()->
		SPObject * obj = desktop->currentLayer();

		if(obj->getRepr() && obj->getRepr()->attribute("inkscape:tween"))
		{
			//while(!obj->getRepr()->attribute("inkscape:tweenstart"))
			SPObject * tween_start = NULL;
			const char * id = obj->getRepr()->attribute("inkscape:tweenstartid");

			if(id)
				tween_start = desktop->getDocument()->getObjectById(id);

			scale->set_sensitive(true);

			if(tween_start && tween_start->getRepr())
				tween_start->getRepr()->setAttribute("inkscape:ease", Glib::ustring::format(scale->get_value()));
		}
		else
		{
			scale->set_sensitive(false);
		}
}

//bool AnimationDialog::handleEaseChanged() {
//	updateEaseValue();
//	return true;
//}


void
AnimationDialog::handleCurrentLayerChanged() {
	SPDesktop * desktop = getDesktop();
	if(!desktop)
		return;

	SPObject * obj = desktop->currentLayer();

	if(obj->getRepr() && obj->getRepr()->attribute("inkscape:tween"))
		scale->set_sensitive(true);
	else
		scale->set_sensitive(false);
}

/*
 * Update label... just a utility function for this example.
 */
void
AnimationDialog::updateLabel() {

    const gchar* root_id = getDesktop()->getDocument()->getRoot()->getId();
    Glib::ustring label_string("Document's SVG id: ");
    label_string += (root_id?root_id:"null");
    label.set_label(label_string);
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
