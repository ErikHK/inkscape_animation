/*
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "live_effects/parameter/originalitemarray.h"

#include <gtkmm/widget.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/imagemenuitem.h>
#include <gtkmm/separatormenuitem.h>
#include <gtkmm/scrolledwindow.h>

#include <glibmm/i18n.h>

#include "inkscape.h"
#include "ui/clipboard.h"
#include "svg/svg.h"
#include "svg/stringstream.h"
#include "originalitem.h"

#include "object/uri.h"

#include "live_effects/effect.h"

#include "verbs.h"
#include "document-undo.h"
#include "document.h"

namespace Inkscape {

namespace LivePathEffect {

class OriginalItemArrayParam::ModelColumns : public Gtk::TreeModel::ColumnRecord
{
public:

    ModelColumns()
    {
        add(_colObject);
        add(_colLabel);
        add(_colActive);
    }
    virtual ~ModelColumns() {}

    Gtk::TreeModelColumn<ItemAndActive*> _colObject;
    Gtk::TreeModelColumn<Glib::ustring> _colLabel;
    Gtk::TreeModelColumn<bool> _colActive;
};

OriginalItemArrayParam::OriginalItemArrayParam( const Glib::ustring& label,
        const Glib::ustring& tip,
        const Glib::ustring& key,
        Inkscape::UI::Widget::Registry* wr,
        Effect* effect )
: Parameter(label, tip, key, wr, effect), 
        _vector(),
        _tree(),
        _text_renderer(),
        _toggle_active(),
        _scroller()
{    
    _model = new ModelColumns();
    _store = Gtk::TreeStore::create(*_model);
    _tree.set_model(_store);

    _tree.set_reorderable(true);
    _tree.enable_model_drag_dest (Gdk::ACTION_MOVE);
    
    Gtk::CellRendererToggle * _toggle_active = manage(new Gtk::CellRendererToggle());
    int activeColNum = _tree.append_column(_("Active"), *_toggle_active) - 1;
    Gtk::TreeViewColumn* col_active = _tree.get_column(activeColNum);
    _toggle_active->set_activatable(true);
    _toggle_active->signal_toggled().connect(sigc::mem_fun(*this, &OriginalItemArrayParam::on_active_toggled));
    col_active->add_attribute(_toggle_active->property_active(), _model->_colActive);
    
    _text_renderer = manage(new Gtk::CellRendererText());
    int nameColNum = _tree.append_column(_("Name"), *_text_renderer) - 1;
    _name_column = _tree.get_column(nameColNum);
    _name_column->add_attribute(_text_renderer->property_text(), _model->_colLabel);

    _tree.set_expander_column( *_tree.get_column(nameColNum) );
    _tree.set_search_column(_model->_colLabel);
    
    //quick little hack -- newer versions of gtk gave the item zero space allotment
    _scroller.set_size_request(-1, 120);

    _scroller.add(_tree);
    _scroller.set_policy( Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC );
    //_scroller.set_shadow_type(Gtk::SHADOW_IN);
    
    oncanvas_editable = true;
}

OriginalItemArrayParam::~OriginalItemArrayParam()
{
    while (!_vector.empty()) {
        ItemAndActive *w = _vector.back();
        _vector.pop_back();
        unlink(w);
        delete w;
    }
    delete _model;
}

void OriginalItemArrayParam::on_active_toggled(const Glib::ustring& item)
{
    Gtk::TreeModel::iterator iter = _store->get_iter(item);
    Gtk::TreeModel::Row row = *iter;
    ItemAndActive *w = row[_model->_colObject];
    row[_model->_colActive] = !row[_model->_colActive];
    w->actived = row[_model->_colActive];
    
    gchar * full = param_getSVGValue();
    param_write_to_repr(full);
    g_free(full);
    DocumentUndo::done(param_effect->getSPDoc(), SP_VERB_DIALOG_LIVE_PATH_EFFECT,
                       _("Link item parameter to item"));
}

void OriginalItemArrayParam::param_set_default()
{
    
}

Gtk::Widget* OriginalItemArrayParam::param_newWidget()
{
    Gtk::VBox* vbox = Gtk::manage(new Gtk::VBox());
    Gtk::HBox* hbox = Gtk::manage(new Gtk::HBox());

    vbox->pack_start(_scroller, Gtk::PACK_EXPAND_WIDGET);
    
    
    { // Paste item to link button
        Gtk::Image *pIcon = Gtk::manage(new Gtk::Image());
        pIcon->set_from_icon_name("edit-clone", Gtk::ICON_SIZE_BUTTON);
        Gtk::Button *pButton = Gtk::manage(new Gtk::Button());
        pButton->set_relief(Gtk::RELIEF_NONE);
        pIcon->show();
        pButton->add(*pIcon);
        pButton->show();
        pButton->signal_clicked().connect(sigc::mem_fun(*this, &OriginalItemArrayParam::on_link_button_click));
        hbox->pack_start(*pButton, Gtk::PACK_SHRINK);
        pButton->set_tooltip_text(_("Link to item"));
    }
    
    { // Remove linked item
        Gtk::Image *pIcon = Gtk::manage(new Gtk::Image());
        pIcon->set_from_icon_name("gtk-remove", Gtk::ICON_SIZE_BUTTON);
        Gtk::Button *pButton = Gtk::manage(new Gtk::Button());
        pButton->set_relief(Gtk::RELIEF_NONE);
        pIcon->show();
        pButton->add(*pIcon);
        pButton->show();
        pButton->signal_clicked().connect(sigc::mem_fun(*this, &OriginalItemArrayParam::on_remove_button_click));
        hbox->pack_start(*pButton, Gtk::PACK_SHRINK);
        pButton->set_tooltip_text(_("Remove Item"));
    }
    
    { // Move Down
        Gtk::Image *pIcon = Gtk::manage(new Gtk::Image());
        pIcon->set_from_icon_name( "gtk-go-down", Gtk::ICON_SIZE_BUTTON);
        Gtk::Button *pButton = Gtk::manage(new Gtk::Button());
        pButton->set_relief(Gtk::RELIEF_NONE);
        pIcon->show();
        pButton->add(*pIcon);
        pButton->show();
        pButton->signal_clicked().connect(sigc::mem_fun(*this, &OriginalItemArrayParam::on_down_button_click));
        hbox->pack_end(*pButton, Gtk::PACK_SHRINK);
        pButton->set_tooltip_text(_("Move Down"));
    }
    
    { // Move Down
        Gtk::Image *pIcon = Gtk::manage(new Gtk::Image());
        pIcon->set_from_icon_name( "gtk-go-up", Gtk::ICON_SIZE_BUTTON);
        Gtk::Button *pButton = Gtk::manage(new Gtk::Button());
        pButton->set_relief(Gtk::RELIEF_NONE);
        pIcon->show();
        pButton->add(*pIcon);
        pButton->show();
        pButton->signal_clicked().connect(sigc::mem_fun(*this, &OriginalItemArrayParam::on_up_button_click));
        hbox->pack_end(*pButton, Gtk::PACK_SHRINK);
        pButton->set_tooltip_text(_("Move Up"));
    }
    
    vbox->pack_end(*hbox, Gtk::PACK_SHRINK);
    
    vbox->show_all_children(true);
    
    return vbox;
}

bool OriginalItemArrayParam::_selectIndex(const Gtk::TreeIter& iter, int* i)
{
    if ((*i)-- <= 0) {
        _tree.get_selection()->select(iter);
        return true;
    }
    return false;
}

void OriginalItemArrayParam::on_up_button_click()
{
    Gtk::TreeModel::iterator iter = _tree.get_selection()->get_selected();
    if (iter) {
        Gtk::TreeModel::Row row = *iter;
        
        int i = -1;
        std::vector<ItemAndActive*>::iterator piter = _vector.begin();
        for (std::vector<ItemAndActive*>::iterator iter = _vector.begin(); iter != _vector.end(); piter = iter, i++, ++iter) {
            if (*iter == row[_model->_colObject]) {
                _vector.erase(iter);
                _vector.insert(piter, row[_model->_colObject]);
                break;
            }
        }
        
        gchar * full = param_getSVGValue();
        param_write_to_repr(full);
        g_free(full);
        
        DocumentUndo::done(param_effect->getSPDoc(), SP_VERB_DIALOG_LIVE_PATH_EFFECT,
                       _("Move item up"));
        
        _store->foreach_iter(sigc::bind<int*>(sigc::mem_fun(*this, &OriginalItemArrayParam::_selectIndex), &i));
    }
}

void OriginalItemArrayParam::on_down_button_click()
{
    Gtk::TreeModel::iterator iter = _tree.get_selection()->get_selected();
    if (iter) {
        Gtk::TreeModel::Row row = *iter;

        int i = 0;
        for (std::vector<ItemAndActive*>::iterator iter = _vector.begin(); iter != _vector.end(); i++, ++iter) {
            if (*iter == row[_model->_colObject]) {
                std::vector<ItemAndActive*>::iterator niter = _vector.erase(iter);
                if (niter != _vector.end()) {
                    ++niter;
                    i++;
                }
                _vector.insert(niter, row[_model->_colObject]);
                break;
            }
        }
        
        gchar * full = param_getSVGValue();
        param_write_to_repr(full);
        g_free(full);
        
        DocumentUndo::done(param_effect->getSPDoc(), SP_VERB_DIALOG_LIVE_PATH_EFFECT,
                       _("Move item down"));
        
        _store->foreach_iter(sigc::bind<int*>(sigc::mem_fun(*this, &OriginalItemArrayParam::_selectIndex), &i));
    }
}

void OriginalItemArrayParam::on_remove_button_click()
{
    Gtk::TreeModel::iterator iter = _tree.get_selection()->get_selected();
    if (iter) {
        Gtk::TreeModel::Row row = *iter;
        remove_link(row[_model->_colObject]);
        
        gchar * full = param_getSVGValue();
        param_write_to_repr(full);
        g_free(full);
        
        DocumentUndo::done(param_effect->getSPDoc(), SP_VERB_DIALOG_LIVE_PATH_EFFECT,
                       _("Remove item"));
    }
    
}

void
OriginalItemArrayParam::on_link_button_click()
{
    Inkscape::UI::ClipboardManager *cm = Inkscape::UI::ClipboardManager::get();
     //without second parameter populate all elements filled inside the called function
    std::vector<Glib::ustring> itemsid = cm->getElementsOfType(SP_ACTIVE_DESKTOP);

    if (itemsid.empty()) {
        return;
    }
    
    bool foundOne = false;
    Inkscape::SVGOStringStream os;
    for (std::vector<ItemAndActive*>::const_iterator iter = _vector.begin(); iter != _vector.end(); ++iter) {
        if (foundOne) {
            os << "|";
        } else {
            foundOne = true;
        }
        os << (*iter)->href << "," << ((*iter)->actived ? "1" : "0");
    }
    for (auto i=itemsid.begin();i!=itemsid.end();++i) {
        Glib::ustring itemid = *i;
        // add '#' at start to make it an uri.
        itemid.insert(itemid.begin(), '#');

        if (foundOne) {
            os << "|";
        } else {
            foundOne = true;
        }
        os << itemid.c_str() << ",1";
    }
    param_write_to_repr(os.str().c_str());
    DocumentUndo::done(param_effect->getSPDoc(), SP_VERB_DIALOG_LIVE_PATH_EFFECT,
                       _("Link itemarray parameter to item"));
}

void OriginalItemArrayParam::unlink(ItemAndActive* to)
{
    to->linked_modified_connection.disconnect();
    to->linked_delete_connection.disconnect();
    to->ref.detach();
    if (to->href) {
        g_free(to->href);
        to->href = NULL;
    }    
}

void OriginalItemArrayParam::remove_link(ItemAndActive* to)
{
    unlink(to);
    for (std::vector<ItemAndActive*>::iterator iter = _vector.begin(); iter != _vector.end(); ++iter) {
        if (*iter == to) {
            ItemAndActive *w = *iter;
            _vector.erase(iter);
            delete w;
            return;
        }
    }
}

void OriginalItemArrayParam::linked_delete(SPObject */*deleted*/, ItemAndActive* /*to*/)
{
    //remove_link(to);
    
    gchar * full = param_getSVGValue();
    param_write_to_repr(full);
    g_free(full);
}

bool OriginalItemArrayParam::_updateLink(const Gtk::TreeIter& iter, ItemAndActive* pd)
{
    Gtk::TreeModel::Row row = *iter;
    if (row[_model->_colObject] == pd) {
        SPObject *obj = pd->ref.getObject();
        row[_model->_colLabel] = obj && obj->getId() ? ( obj->label() ? obj->label() : obj->getId() ) : pd->href;
        return true;
    }
    return false;
}

void OriginalItemArrayParam::linked_changed(SPObject */*old_obj*/, SPObject *new_obj, ItemAndActive* to)
{
    to->linked_delete_connection.disconnect();
    to->linked_modified_connection.disconnect();
    to->linked_transformed_connection.disconnect();
    
    if (new_obj && SP_IS_ITEM(new_obj)) {
        to->linked_delete_connection = new_obj->connectDelete(sigc::bind<ItemAndActive*>(sigc::mem_fun(*this, &OriginalItemArrayParam::linked_delete), to));
        to->linked_modified_connection = new_obj->connectModified(sigc::bind<ItemAndActive*>(sigc::mem_fun(*this, &OriginalItemArrayParam::linked_modified), to));
        to->linked_transformed_connection = SP_ITEM(new_obj)->connectTransformed(sigc::bind<ItemAndActive*>(sigc::mem_fun(*this, &OriginalItemArrayParam::linked_transformed), to));

        linked_modified(new_obj, SP_OBJECT_MODIFIED_FLAG, to);
    } else {
        SP_OBJECT(param_effect->getLPEObj())->requestModified(SP_OBJECT_MODIFIED_FLAG);
        _store->foreach_iter(sigc::bind<ItemAndActive*>(sigc::mem_fun(*this, &OriginalItemArrayParam::_updateLink), to));
    }
}

void OriginalItemArrayParam::linked_modified(SPObject *linked_obj, guint flags, ItemAndActive* to)
{
    if (!to) {
        return;
    }
    SP_OBJECT(param_effect->getLPEObj())->requestModified(SP_OBJECT_MODIFIED_FLAG);
    _store->foreach_iter(sigc::bind<ItemAndActive*>(sigc::mem_fun(*this, &OriginalItemArrayParam::_updateLink), to));
}

bool OriginalItemArrayParam::param_readSVGValue(const gchar* strvalue)
{
    if (strvalue) {
        while (!_vector.empty()) {
            ItemAndActive *w = _vector.back();
            unlink(w);
            _vector.pop_back();
            delete w;
        }
        _store->clear();

        gchar ** strarray = g_strsplit(strvalue, "|", 0);
        for (gchar ** iter = strarray; *iter != NULL; iter++) {
            if ((*iter)[0] == '#') {
                gchar ** substrarray = g_strsplit(*iter, ",", 0);
                ItemAndActive* w = new ItemAndActive((SPObject *)param_effect->getLPEObj());
                w->href = g_strdup(*substrarray);
                w->actived = *(substrarray+1) != NULL && (*(substrarray+1))[0] == '1';
                w->linked_changed_connection = w->ref.changedSignal().connect(sigc::bind<ItemAndActive *>(sigc::mem_fun(*this, &OriginalItemArrayParam::linked_changed), w));
                w->ref.attach(URI(w->href));

                _vector.push_back(w);

                Gtk::TreeModel::iterator iter = _store->append();
                Gtk::TreeModel::Row row = *iter;
                SPObject *obj = w->ref.getObject();

                row[_model->_colObject] = w;
                row[_model->_colLabel] = obj ? ( obj->label() ? obj->label() : obj->getId() ) : w->href;
                row[_model->_colActive] = w->actived;
                g_strfreev (substrarray);
            }
        }
        g_strfreev (strarray);
        return true;
    }
    return false;
}

gchar * OriginalItemArrayParam::param_getSVGValue() const
{
    Inkscape::SVGOStringStream os;
    bool foundOne = false;
    for (std::vector<ItemAndActive*>::const_iterator iter = _vector.begin(); iter != _vector.end(); ++iter) {
        if (foundOne) {
            os << "|";
        } else {
            foundOne = true;
        }
        os << (*iter)->href << "," << ((*iter)->actived ? "1" : "0");
    }
    return g_strdup(os.str().c_str());
}

gchar * OriginalItemArrayParam::param_getDefaultSVGValue() const
{
    return g_strdup("");
}

void OriginalItemArrayParam::update()
{
    for (std::vector<ItemAndActive*>::iterator iter = _vector.begin(); iter != _vector.end(); ++iter) {
        SPObject *linked_obj = (*iter)->ref.getObject();
        linked_modified(linked_obj, SP_OBJECT_MODIFIED_FLAG, *iter);
    }
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
