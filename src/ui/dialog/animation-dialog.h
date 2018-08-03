#ifndef SEEN_ANIMATION_DIALOG_H
#define SEEN_ANIMATION_DIALOG_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <iostream>
#include "ui/widget/panel.h"
#include "ui/widget/frame.h"

#include <gtkmm/frame.h>
#include <gtkmm/button.h>
#include <gtkmm/scale.h>
#include <gtkmm/table.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/checkbutton.h>
#include "sp-object.h"

#include "ui/dialog/desktop-tracker.h"

// Only to display status.
#include <gtkmm/label.h>

namespace Inkscape {
namespace UI {
namespace Dialog {

/**
 * A panel that does almost nothing!
 */
class AnimationDialog : public UI::Widget::Panel
{
public:
    virtual ~AnimationDialog();

    UI::Widget::Frame _tweenFrame;
    Gtk::VBox _tweenBox;
    Gtk::Table _easeRotateBox;
    Gtk::HBox _easeBox;
    Gtk::HBox _rotateBox;
    Gtk::Scale * scale;
    Gtk::SpinButton * spin_revolutions;
    Gtk::SpinButton * spin_degrees;
    Gtk::CheckButton * in;
    Gtk::CheckButton * out;

    static AnimationDialog& getInstance() { return *new AnimationDialog(); };

    virtual void present();
    void updateEaseValue();
	SPObject * getTweenStart();

private:

    // No default constructor, noncopyable, nonassignable
    AnimationDialog();
    AnimationDialog(AnimationDialog const &d);
    AnimationDialog operator=(AnimationDialog const &d);

    // Signals and handlers
    sigc::connection connectionDocumentReplaced;
    sigc::connection connectionDesktopChanged;
    sigc::connection connectionSelectionChanged;

    void handleDocumentReplaced(SPDesktop* desktop, SPDocument *document);
    void handleDesktopChanged(SPDesktop* desktop);
    void handleSelectionChanged();
    void handleCurrentLayerChanged();
    //bool handleEaseChanged();


    DesktopTracker desktopTracker;

    // Just for example
    Gtk::Label label;
    void updateLabel();

protected:

};

} //namespace Dialogs
} //namespace UI
} //namespace Inkscape

#endif // SEEN_ANIMATION_DIALOG_H

