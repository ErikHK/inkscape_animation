#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_ORIGINALPATHARRAY_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_ORIGINALPATHARRAY_H

/*
 * Inkscape::LivePathEffectParameters
 *
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <vector>

#include <gtkmm/box.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/scrolledwindow.h>

#include "live_effects/parameter/parameter.h"
#include "live_effects/parameter/path-reference.h"

#include "svg/svg.h"
#include "svg/stringstream.h"
#include "path-reference.h"

class SPObject;

namespace Inkscape {

namespace LivePathEffect {

class PathAndDirectionAndVisible {
public:
    PathAndDirectionAndVisible(SPObject *owner)
    : href(NULL),
    ref(owner),
    _pathvector(Geom::PathVector()),
    reversed(false),
    visibled(true)
    {
        
    }
    gchar *href;
    URIReference ref;
    Geom::PathVector _pathvector;
    bool reversed;
    bool visibled;
    
    sigc::connection linked_changed_connection;
    sigc::connection linked_delete_connection;
    sigc::connection linked_modified_connection;
    sigc::connection linked_transformed_connection;
};
    
class OriginalPathArrayParam : public Parameter {
public:
    class ModelColumns;
    
    OriginalPathArrayParam( const Glib::ustring& label,
                const Glib::ustring& tip,
                const Glib::ustring& key,
                Inkscape::UI::Widget::Registry* wr,
                Effect* effect);

    virtual ~OriginalPathArrayParam();

    virtual Gtk::Widget * param_newWidget();
    virtual bool param_readSVGValue(const gchar * strvalue);
    virtual gchar * param_getSVGValue() const;
    virtual gchar * param_getDefaultSVGValue() const;
    virtual void param_set_default();
    virtual void param_update_default(const gchar * default_value){};
    /** Disable the canvas indicators of parent class by overriding this method */
    virtual void param_editOncanvas(SPItem * /*item*/, SPDesktop * /*dt*/) {};
    /** Disable the canvas indicators of parent class by overriding this method */
    virtual void addCanvasIndicators(SPLPEItem const* /*lpeitem*/, std::vector<Geom::PathVector> & /*hp_vec*/) {};
    void setFromOriginalD(bool from_original_d){ _from_original_d = from_original_d; update();};
    void allowOnlyBsplineSpiro(bool allow_only_bspline_spiro){ _allow_only_bspline_spiro = allow_only_bspline_spiro; update();};

    std::vector<PathAndDirectionAndVisible*> _vector;

protected:
    bool _updateLink(const Gtk::TreeIter& iter, PathAndDirectionAndVisible* pd);
    bool _selectIndex(const Gtk::TreeIter& iter, int* i);
    void unlink(PathAndDirectionAndVisible* to);
    void remove_link(PathAndDirectionAndVisible* to);
    void setPathVector(SPObject *linked_obj, guint flags, PathAndDirectionAndVisible* to);
    
    void linked_changed(SPObject *old_obj, SPObject *new_obj, PathAndDirectionAndVisible* to);
    void linked_modified(SPObject *linked_obj, guint flags, PathAndDirectionAndVisible* to);
    void linked_transformed(Geom::Affine const *, SPItem *, PathAndDirectionAndVisible*) {}
    void linked_delete(SPObject *deleted, PathAndDirectionAndVisible* to);
    
    ModelColumns *_model;
    Glib::RefPtr<Gtk::TreeStore> _store;
    Gtk::TreeView _tree;
    Gtk::CellRendererText *_text_renderer;
    Gtk::CellRendererToggle *_toggle_reverse;
    Gtk::CellRendererToggle *_toggle_visible;
    Gtk::TreeView::Column *_name_column;
    Gtk::ScrolledWindow _scroller;
    
    void on_link_button_click();
    void on_remove_button_click();
    void on_up_button_click();
    void on_down_button_click();
    void on_reverse_toggled(const Glib::ustring& path);
    void on_visible_toggled(const Glib::ustring& path);
    
private:
    bool _from_original_d;
    bool _allow_only_bspline_spiro;
    void update();
    OriginalPathArrayParam(const OriginalPathArrayParam&);
    OriginalPathArrayParam& operator=(const OriginalPathArrayParam&);
};

} //namespace LivePathEffect

} //namespace Inkscape

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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
