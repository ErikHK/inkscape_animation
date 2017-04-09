/** @file
 * @brief Live Path Effect editing dialog
 */
/* Author:
 *   Johan Engelen <j.b.c.engelen@ewi.utwente.nl>
 *
 * Copyright (C) 2007 Author
 * Released under GNU GPL.  Read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_LIVE_PATH_EFFECT_H
#define INKSCAPE_UI_DIALOG_LIVE_PATH_EFFECT_H

#include "ui/widget/panel.h"

#include <gtkmm/label.h>
#include <gtkmm/frame.h>
#include "ui/widget/combo-enums.h"
#include "ui/widget/frame.h"
#include "live_effects/effect-enum.h"
#include <gtkmm/liststore.h>
#include <gtkmm/treeview.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/toolbar.h>
#include <gtkmm/buttonbox.h>
#include "ui/dialog/desktop-tracker.h"

class SPDesktop;
class SPLPEItem;

namespace Inkscape {

namespace LivePathEffect {
    class Effect;
    class LPEObjectReference;
}

namespace UI {
namespace Dialog {

class LivePathEffectEditor : public UI::Widget::Panel {
public:
    LivePathEffectEditor();
    virtual ~LivePathEffectEditor();

    static LivePathEffectEditor &getInstance() { return *new LivePathEffectEditor(); }

    void onSelectionChanged(Inkscape::Selection *sel);
    virtual void on_effect_selection_changed();
    void setDesktop(SPDesktop *desktop);

private:

    /**
     * Auxiliary widget to keep track of desktop changes for the floating dialog.
     */
    DesktopTracker deskTrack;

    /**
     * Link to callback function for a change in desktop (window).
     */
    sigc::connection desktopChangeConn;
    sigc::connection selection_changed_connection;
    sigc::connection selection_modified_connection;

    void set_sensitize_all(bool sensitive);

    void showParams(LivePathEffect::Effect& effect);
    void showText(Glib::ustring const &str);
    void selectInList(LivePathEffect::Effect* effect);

    // void add_entry(const char* name );
    void effect_list_reload(SPLPEItem *lpeitem);

    // callback methods for buttons on grids page.
    void onAdd();
    void onRemove();
    void onUp();
    void onDown();

    class ModelColumns : public Gtk::TreeModel::ColumnRecord
    {
      public:
        ModelColumns()
        {
            add(col_name);
            add(lperef);
            add(col_visible);
        }
        virtual ~ModelColumns() {}

        Gtk::TreeModelColumn<Glib::ustring> col_name;
        Gtk::TreeModelColumn<LivePathEffect::LPEObjectReference *> lperef;
        Gtk::TreeModelColumn<bool> col_visible;
    };

    bool lpe_list_locked;

    //Inkscape::UI::Widget::ComboBoxEnum<LivePathEffect::EffectType> combo_effecttype;
    
    Gtk::Widget * effectwidget;
    Gtk::Label status_label;
    UI::Widget::Frame effectcontrol_frame;
    Gtk::HBox effectapplication_hbox;
    Gtk::VBox effectcontrol_vbox;
    Gtk::VBox effectlist_vbox;
    ModelColumns columns;
    Gtk::ScrolledWindow scrolled_window;
    Gtk::TreeView effectlist_view;
    Glib::RefPtr<Gtk::ListStore> effectlist_store;
    Glib::RefPtr<Gtk::TreeSelection> effectlist_selection;

    void on_visibility_toggled( Glib::ustring const& str );

#if WITH_GTKMM_3_0
    Gtk::ButtonBox toolbar_hbox;
#else
    Gtk::HButtonBox toolbar_hbox;
#endif
    Gtk::Button button_add;
    Gtk::Button button_remove;
    Gtk::Button button_up;
    Gtk::Button button_down;

    SPDesktop * current_desktop;
    
    SPLPEItem * current_lpeitem;

    friend void lpeeditor_selection_changed (Inkscape::Selection * selection, gpointer data);

    LivePathEffectEditor(LivePathEffectEditor const &d);
    LivePathEffectEditor& operator=(LivePathEffectEditor const &d);
};

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_DIALOG_LIVE_PATH_EFFECT_H

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
