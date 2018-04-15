/*
 * Copyright (C) Johan Engelen 2012 <j.b.c.engelen@alumnus.utwente.nl>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <gtkmm/box.h>
#include "live_effects/parameter/originalitem.h"

#include <glibmm/i18n.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>

#include "display/curve.h"
#include "live_effects/effect.h"

#include "inkscape.h"
#include "desktop.h"
#include "selection.h"

#include "object/uri.h"

#include "ui/icon-names.h"

namespace Inkscape {

namespace LivePathEffect {

OriginalItemParam::OriginalItemParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect)
    : ItemParam(label, tip, key, wr, effect, "")
{
}

OriginalItemParam::~OriginalItemParam()
{

}

Gtk::Widget *
OriginalItemParam::param_newWidget()
{
    Gtk::HBox *_widget = Gtk::manage(new Gtk::HBox());

    { // Label
        Gtk::Label *pLabel = Gtk::manage(new Gtk::Label(param_label));
        static_cast<Gtk::HBox*>(_widget)->pack_start(*pLabel, true, true);
        pLabel->set_tooltip_text(param_tooltip);
    }

    { // Paste item to link button
        Gtk::Image *pIcon = Gtk::manage(new Gtk::Image());
        pIcon->set_from_icon_name("edit-paste", Gtk::ICON_SIZE_BUTTON);
        Gtk::Button *pButton = Gtk::manage(new Gtk::Button());
        pButton->set_relief(Gtk::RELIEF_NONE);
        pIcon->show();
        pButton->add(*pIcon);
        pButton->show();
        pButton->signal_clicked().connect(sigc::mem_fun(*this, &OriginalItemParam::on_link_button_click));
        static_cast<Gtk::HBox*>(_widget)->pack_start(*pButton, true, true);
        pButton->set_tooltip_text(_("Link to item"));
    }

    { // Select original button
        Gtk::Image *pIcon = Gtk::manage(new Gtk::Image());
        pIcon->set_from_icon_name("edit-select-original", Gtk::ICON_SIZE_BUTTON);
        Gtk::Button *pButton = Gtk::manage(new Gtk::Button());
        pButton->set_relief(Gtk::RELIEF_NONE);
        pIcon->show();
        pButton->add(*pIcon);
        pButton->show();
        pButton->signal_clicked().connect(sigc::mem_fun(*this, &OriginalItemParam::on_select_original_button_click));
        static_cast<Gtk::HBox*>(_widget)->pack_start(*pButton, true, true);
        pButton->set_tooltip_text(_("Select original"));
    }

    static_cast<Gtk::HBox*>(_widget)->show_all_children();

    return dynamic_cast<Gtk::Widget *> (_widget);
}

void
OriginalItemParam::linked_modified_callback(SPObject *linked_obj, guint /*flags*/)
{
    emit_changed();
    SP_OBJECT(param_effect->getLPEObj())->requestModified(SP_OBJECT_MODIFIED_FLAG);
    last_transform = Geom::identity();
}

void
OriginalItemParam::linked_transformed_callback(Geom::Affine const * rel_transf, SPItem *moved_item)
{
    last_transform = *rel_transf;
    SP_OBJECT(param_effect->getLPEObj())->requestModified(SP_OBJECT_MODIFIED_FLAG);
}


void
OriginalItemParam::on_select_original_button_click()
{
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    SPItem *original = ref.getObject();
    if (desktop == NULL || original == NULL) {
        return;
    }
    Inkscape::Selection *selection = desktop->getSelection();
    selection->clear();
    selection->set(original);
}

} /* namespace LivePathEffect */

} /* namespace Inkscape */

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
