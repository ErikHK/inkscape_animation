/*
 * SVG <g> implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Johan Engelen <j.b.c.engelen@ewi.utwente.nl>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 1999-2006 authors
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glibmm/i18n.h>
#include <cstring>
#include <string>

#include "display/drawing-group.h"
#include "display/curve.h"
#include "xml/repr.h"
#include "svg/svg.h"
#include "document.h"
#include "document-undo.h"
#include "style.h"
#include "attributes.h"
#include "sp-item-transform.h"
#include "sp-root.h"
#include "sp-use.h"
#include "sp-offset.h"
#include "sp-clippath.h"
#include "sp-mask.h"
#include "sp-path.h"
#include "box3d.h"
#include "persp3d.h"
#include "inkscape.h"

#include "selection.h"
#include "live_effects/effect.h"
#include "live_effects/lpeobject.h"
#include "live_effects/lpeobject-reference.h"
#include "sp-title.h"
#include "sp-desc.h"
#include "sp-switch.h"
#include "sp-defs.h"
#include "verbs.h"
#include "layer-model.h"
#include "sp-textpath.h"
#include "sp-flowtext.h"
#include "sp-tspan.h"
#include "selection-chemistry.h"
#include "xml/sp-css-attr.h"
#include "svg/css-ostringstream.h"

using Inkscape::DocumentUndo;

static void sp_group_perform_patheffect(SPGroup *group, SPGroup *topgroup, bool write);

SPGroup::SPGroup() : SPLPEItem(),
    _expanded(false),
    _insertBottom(false),
    _layer_mode(SPGroup::GROUP)
{
}

SPGroup::~SPGroup() {
}

void SPGroup::build(SPDocument *document, Inkscape::XML::Node *repr) {
    this->readAttr( "inkscape:groupmode" );

    SPLPEItem::build(document, repr);
}

void SPGroup::release() {
    if (this->_layer_mode == SPGroup::LAYER) {
        this->document->removeResource("layer", this);
    }

    SPLPEItem::release();
}

void SPGroup::child_added(Inkscape::XML::Node* child, Inkscape::XML::Node* ref) {
    SPLPEItem::child_added(child, ref);

    SPObject *last_child = this->lastChild();

    if (last_child && last_child->getRepr() == child) {
        // optimization for the common special case where the child is being added at the end
        SPItem *item = dynamic_cast<SPItem *>(last_child);
        if ( item ) {
            /* TODO: this should be moved into SPItem somehow */
            SPItemView *v;

            for (v = this->display; v != NULL; v = v->next) {
                Inkscape::DrawingItem *ac = item->invoke_show (v->arenaitem->drawing(), v->key, v->flags);

                if (ac) {
                    v->arenaitem->appendChild(ac);
                }
            }
        }
    } else {    // general case
        SPItem *item = dynamic_cast<SPItem *>(get_child_by_repr(child));
        if ( item ) {
            /* TODO: this should be moved into SPItem somehow */
            SPItemView *v;
            unsigned position = item->pos_in_parent();

            for (v = this->display; v != NULL; v = v->next) {
                Inkscape::DrawingItem *ac = item->invoke_show (v->arenaitem->drawing(), v->key, v->flags);

                if (ac) {
                    v->arenaitem->prependChild(ac);
                    ac->setZOrder(position);
                }
            }
        }
    }

    this->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/* fixme: hide (Lauris) */

void SPGroup::remove_child(Inkscape::XML::Node *child) {
    SPLPEItem::remove_child(child);

    this->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

void SPGroup::order_changed (Inkscape::XML::Node *child, Inkscape::XML::Node *old_ref, Inkscape::XML::Node *new_ref)
{
	SPLPEItem::order_changed(child, old_ref, new_ref);

    SPItem *item = dynamic_cast<SPItem *>(get_child_by_repr(child));
    if ( item ) {
        /* TODO: this should be moved into SPItem somehow */
        SPItemView *v;
        unsigned position = item->pos_in_parent();
        for ( v = item->display ; v != NULL ; v = v->next ) {
            v->arenaitem->setZOrder(position);
        }
    }

    this->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

void SPGroup::update(SPCtx *ctx, unsigned int flags) {
    // std::cout << "SPGroup::update(): " << (getId()?getId():"null") << std::endl;
    SPItemCtx *ictx, cctx;

    ictx = (SPItemCtx *) ctx;
    cctx = *ictx;

    unsigned childflags = flags;

    if (flags & SP_OBJECT_MODIFIED_FLAG) {
      childflags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }
    childflags &= SP_OBJECT_MODIFIED_CASCADE;
    std::vector<SPObject*> l=this->childList(true, SPObject::ActionUpdate);
    for(std::vector<SPObject*> ::const_iterator i=l.begin();i!=l.end();++i){
        SPObject *child = *i;

        if (childflags || (child->uflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            SPItem *item = dynamic_cast<SPItem *>(child);
            if (item) {
                cctx.i2doc = item->transform * ictx->i2doc;
                cctx.i2vp = item->transform * ictx->i2vp;
                child->updateDisplay((SPCtx *)&cctx, childflags);
            } else {
                child->updateDisplay(ctx, childflags);
            }
        }

        sp_object_unref(child);
    }

    // For a group, we need to update ourselves *after* updating children.
    // this is because the group might contain shapes such as rect or ellipse,
    // which recompute their equivalent path (a.k.a curve) in the update callback,
    // and this is in turn used when computing bbox.
    SPLPEItem::update(ctx, flags);

    if (flags & SP_OBJECT_STYLE_MODIFIED_FLAG) {
        for (SPItemView *v = this->display; v != NULL; v = v->next) {
            Inkscape::DrawingGroup *group = dynamic_cast<Inkscape::DrawingGroup *>(v->arenaitem);
            if( this->parent ) {
                this->context_style = this->parent->context_style;
            }
            group->setStyle(this->style, this->context_style);
        }
    }
}

void SPGroup::modified(guint flags) {
    // std::cout << "SPGroup::modified(): " << (getId()?getId():"null") << std::endl;
    SPLPEItem::modified(flags);
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
    	flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }

    flags &= SP_OBJECT_MODIFIED_CASCADE;

    if (flags & SP_OBJECT_STYLE_MODIFIED_FLAG) {
        for (SPItemView *v = this->display; v != NULL; v = v->next) {
            Inkscape::DrawingGroup *group = dynamic_cast<Inkscape::DrawingGroup *>(v->arenaitem);
            group->setStyle(this->style);
        }
    }

    std::vector<SPObject*> l=this->childList(true);
    for(std::vector<SPObject*>::const_iterator i=l.begin();i!=l.end();++i){
        SPObject *child = *i;

        if (flags || (child->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            child->emitModified(flags);
        }

        sp_object_unref(child);
    }
}

Inkscape::XML::Node* SPGroup::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if (flags & SP_OBJECT_WRITE_BUILD) {
        GSList *l = NULL;

        if (!repr) {
            if (dynamic_cast<SPSwitch *>(this)) {
                repr = xml_doc->createElement("svg:switch");
            } else {
                repr = xml_doc->createElement("svg:g");
            }
        }

        l = NULL;

        for (SPObject *child = firstChild(); child; child = child->getNext() ) {
            if ( !dynamic_cast<SPTitle *>(child) && !dynamic_cast<SPDesc *>(child) ) {
                Inkscape::XML::Node *crepr = child->updateRepr(xml_doc, NULL, flags);

                if (crepr) {
                    l = g_slist_prepend (l, crepr);
                }
            }
        }

        while (l) {
            repr->addChild((Inkscape::XML::Node *) l->data, NULL);
            Inkscape::GC::release((Inkscape::XML::Node *) l->data);
            l = g_slist_remove (l, l->data);
        }
    } else {
        for (SPObject *child = firstChild() ; child ; child = child->getNext() ) {
            if ( !dynamic_cast<SPTitle *>(child) && !dynamic_cast<SPDesc *>(child) ) {
                child->updateRepr(flags);
            }
        }
    }

    if ( flags & SP_OBJECT_WRITE_EXT ) {
        const char *value;
        if ( _layer_mode == SPGroup::LAYER ) {
            value = "layer";
        } else if ( _layer_mode == SPGroup::MASK_HELPER ) {
            value = "maskhelper";
        } else if ( flags & SP_OBJECT_WRITE_ALL ) {
            value = "group";
        } else {
            value = NULL;
        }

        repr->setAttribute("inkscape:groupmode", value);
    }

    SPLPEItem::write(xml_doc, repr, flags);

    return repr;
}

Geom::OptRect SPGroup::bbox(Geom::Affine const &transform, SPItem::BBoxType bboxtype) const
{
    Geom::OptRect bbox;

    // TODO CPPIFY: replace this const_cast later
    std::vector<SPObject*> l = const_cast<SPGroup*>(this)->childList(false, SPObject::ActionBBox);
    for(std::vector<SPObject*>::const_iterator i=l.begin();i!=l.end();++i){
        SPObject *o = *i;
        SPItem *item = dynamic_cast<SPItem *>(o);
        if (item && !item->isHidden()) {
            Geom::Affine const ct(item->transform * transform);
            bbox |= item->bounds(bboxtype, ct);
        }
    }

    return bbox;
}

void SPGroup::print(SPPrintContext *ctx) {
	std::vector<SPObject*> l=this->childList(false);
    for(std::vector<SPObject*>::const_iterator i=l.begin();i!=l.end();++i){
        SPObject *o = *i;
        SPItem *item = dynamic_cast<SPItem *>(o);
        if (item) {
            item->invoke_print(ctx);
        }
    }
}

const char *SPGroup::displayName() const {
    return _("Group");
}

gchar *SPGroup::description() const {
    gint len = this->getItemCount();
    return g_strdup_printf(
        ngettext(_("of <b>%d</b> object"), _("of <b>%d</b> objects"), len), len);
}

void SPGroup::set(unsigned int key, gchar const* value) {
    switch (key) {
        case SP_ATTR_INKSCAPE_GROUPMODE:
            if ( value && !strcmp(value, "layer") ) {
                this->setLayerMode(SPGroup::LAYER);
            } else if ( value && !strcmp(value, "maskhelper") ) {
                this->setLayerMode(SPGroup::MASK_HELPER);
            } else {
                this->setLayerMode(SPGroup::GROUP);
            }
            break;

        default:
            SPLPEItem::set(key, value);
            break;
    }
}

Inkscape::DrawingItem *SPGroup::show (Inkscape::Drawing &drawing, unsigned int key, unsigned int flags) {
    // std::cout << "SPGroup::show(): " << (getId()?getId():"null") << std::endl;
    Inkscape::DrawingGroup *ai;

    ai = new Inkscape::DrawingGroup(drawing);
    ai->setPickChildren(this->effectiveLayerMode(key) == SPGroup::LAYER);
    if( this->parent ) {
        this->context_style = this->parent->context_style;
    }
    ai->setStyle(this->style, this->context_style);

    this->_showChildren(drawing, ai, key, flags);
    return ai;
}

void SPGroup::hide (unsigned int key) {
	std::vector<SPObject*> l=this->childList(false, SPObject::ActionShow);
    for(std::vector<SPObject*>::const_iterator i=l.begin();i!=l.end();++i){
        SPObject *o = *i;

        SPItem *item = dynamic_cast<SPItem *>(o);
        if (item) {
            item->invoke_hide(key);
        }
    }

//    SPLPEItem::onHide(key);
}


void SPGroup::snappoints(std::vector<Inkscape::SnapCandidatePoint> &p, Inkscape::SnapPreferences const *snapprefs) const {
    for ( SPObject const *o = this->firstChild(); o; o = o->getNext() )
    {
        SPItem const *item = dynamic_cast<SPItem const *>(o);
        if (item) {
            item->getSnappoints(p, snapprefs);
        }
    }
}

void sp_item_group_ungroup_handle_clones(SPItem *parent, Geom::Affine const g)
{
    for(std::list<SPObject*>::const_iterator refd=parent->hrefList.begin();refd!=parent->hrefList.end();++refd){
        SPItem *citem = dynamic_cast<SPItem *>(*refd);
        if (citem && !citem->cloned) {
            SPUse *useitem = dynamic_cast<SPUse *>(citem);
            if (useitem && useitem->get_original() == parent) {
                Geom::Affine ctrans;
                ctrans = g.inverse() * citem->transform;
                gchar *affinestr = sp_svg_transform_write(ctrans);
                citem->setAttribute("transform", affinestr);
                g_free(affinestr);
            }
        }
    }
}

void
sp_recursive_scale_text_size(Inkscape::XML::Node *repr, double scale){
    for (Inkscape::XML::Node *child = repr->firstChild() ; child; child = child->next() ){
        if ( child) {
            sp_recursive_scale_text_size(child, scale);
        }
    }
    SPCSSAttr * css = sp_repr_css_attr(repr,"style");
    Glib::ustring element = g_quark_to_string(repr->code());
    if (css && element == "svg:text" || element == "svg:tspan") {
        gchar const *w = sp_repr_css_property(css, "font-size", NULL);
        if (w) {
            gchar *units = NULL;
            double wd = g_ascii_strtod(w, &units);
            wd *= scale;
            if (w != units) {
                Inkscape::CSSOStringStream os;
                os << wd << units; // reattach units
                sp_repr_css_set_property(css, "font-size", os.str().c_str());
                Glib::ustring css_str;
                sp_repr_css_write_string(css,css_str);
                repr->setAttribute("style", css_str.c_str());
            }
        }
        w = NULL;
        w = sp_repr_css_property(css, "letter-spacing", NULL);
        if (w) {
            gchar *units = NULL;
            double wd = g_ascii_strtod(w, &units);
            wd *= scale;
            if (w != units) {
                Inkscape::CSSOStringStream os;
                os << wd << units; // reattach units
                sp_repr_css_set_property(css, "letter-spacing", os.str().c_str());
                Glib::ustring css_str;
                sp_repr_css_write_string(css,css_str);
                repr->setAttribute("style", css_str.c_str());
            }
        }
        w = NULL;
        w = sp_repr_css_property(css, "word-spacing", NULL);
        if (w) {
            gchar *units = NULL;
            double wd = g_ascii_strtod(w, &units);
            wd *= scale;
            if (w != units) {
                Inkscape::CSSOStringStream os;
                os << wd << units; // reattach units
                sp_repr_css_set_property(css, "word-spacing", os.str().c_str());
                Glib::ustring css_str;
                sp_repr_css_write_string(css,css_str);
                repr->setAttribute("style", css_str.c_str());
            }
        }
        gchar const *dx = repr->attribute("dx");
        if (dx) {
            gchar ** dxarray = g_strsplit(dx, " ", 0);
            Inkscape::SVGOStringStream dx_data;
            while (*dxarray != NULL) {
                double pos;
                sp_svg_number_read_d(*dxarray, &pos);
                pos *= scale;
                dx_data << pos << " ";
                dxarray++;
            }
            repr->setAttribute("dx", dx_data.str().c_str());
        }
        gchar const *dy = repr->attribute("dy");
        if (dy) {
            gchar ** dyarray = g_strsplit(dy, " ", 0);
            Inkscape::SVGOStringStream dy_data;
            while (*dyarray != NULL) {
                double pos;
                sp_svg_number_read_d(*dyarray, &pos);
                pos *= scale;
                dy_data << pos << " ";
                dyarray++;
            }
            repr->setAttribute("dy", dy_data.str().c_str());
        }
    }
}

void
sp_item_group_ungroup (SPGroup *group, std::vector<SPItem*> &children, bool do_done)
{
    g_return_if_fail (group != NULL);

    SPDocument *doc = group->document;
    SPRoot *root = doc->getRoot();
    SPObject *defs = root->defs;

    Inkscape::XML::Node *grepr = group->getRepr();

    g_return_if_fail (!strcmp (grepr->name(), "svg:g")
                   || !strcmp (grepr->name(), "svg:a")
                   || !strcmp (grepr->name(), "svg:switch")
                   || !strcmp (grepr->name(), "svg:svg"));

    // this converts the gradient/pattern fill/stroke on the group, if any, to userSpaceOnUse
    group->adjust_paint_recursive (Geom::identity(), Geom::identity(), false);

    SPItem *pitem = dynamic_cast<SPItem *>(group->parent);
    g_assert(pitem);
    Inkscape::XML::Node *prepr = pitem->getRepr();

    {
        SPBox3D *box = dynamic_cast<SPBox3D *>(group);
        if (box) {
            group = box3d_convert_to_group(box);
        }
    }

    group->removeAllPathEffects(false);

    /* Step 1 - generate lists of children objects */
    GSList *items = NULL;
    GSList *objects = NULL;
    Geom::Affine const g(group->transform);

    for (SPObject *child = group->firstChild() ; child; child = child->getNext() )
        if (SPItem *citem = dynamic_cast<SPItem *>(child))
            sp_item_group_ungroup_handle_clones(citem,g);


    for (SPObject *child = group->firstChild() ; child; child = child->getNext() ) {
        SPItem *citem = dynamic_cast<SPItem *>(child);
        if (citem) {
            /* Merging of style */
            // this converts the gradient/pattern fill/stroke, if any, to userSpaceOnUse; we need to do
            // it here _before_ the new transform is set, so as to use the pre-transform bbox
            citem->adjust_paint_recursive (Geom::identity(), Geom::identity(), false);

            child->style->merge( group->style );
            /*
             * fixme: We currently make no allowance for the case where child is cloned
             * and the group has any style settings.
             *
             * (This should never occur with documents created solely with the current
             * version of inkscape without using the XML editor: we usually apply group
             * style changes to children rather than to the group itself.)
             *
             * If the group has no style settings, then style->merge() should be a no-op. Otherwise
             * (i.e. if we change the child's style to compensate for its parent going away)
             * then those changes will typically be reflected in any clones of child,
             * whereas we'd prefer for Ungroup not to affect the visual appearance.
             *
             * The only way of preserving styling appearance in general is for child to
             * be put into a new group -- a somewhat surprising response to an Ungroup
             * command.  We could add a new groupmode:transparent that would mostly
             * hide the existence of such groups from the user (i.e. editing behaves as
             * if the transparent group's children weren't in a group), though that's
             * extra complication & maintenance burden and this case is rare.
             */

            child->updateRepr();

            Inkscape::XML::Node *nrepr = child->getRepr()->duplicate(prepr->document());

            // Merging transform
            Geom::Affine ctrans = citem->transform * g;
                // We should not apply the group's transformation to both a linked offset AND to its source
                if (dynamic_cast<SPOffset *>(citem)) { // Do we have an offset at hand (whether it's dynamic or linked)?
                    SPItem *source = sp_offset_get_source(dynamic_cast<SPOffset *>(citem));
                    // When dealing with a chain of linked offsets, the transformation of an offset will be
                    // tied to the transformation of the top-most source, not to any of the intermediate
                    // offsets. So let's find the top-most source
                    while (source != NULL && dynamic_cast<SPOffset *>(source)) {
                        source = sp_offset_get_source(dynamic_cast<SPOffset *>(source));
                    }
                    if (source != NULL && // If true then we must be dealing with a linked offset ...
                        group->isAncestorOf(source) ) { // ... of which the source is in the same group
                        ctrans = citem->transform; // then we should apply the transformation of the group to the offset
                    }
                }

            // FIXME: constructing a transform that would fully preserve the appearance of a
            // textpath if it is ungrouped with its path seems to be impossible in general
            // case. E.g. if the group was squeezed, to keep the ungrouped textpath squeezed
            // as well, we'll need to relink it to some "virtual" path which is inversely
            // stretched relative to the actual path, and then squeeze the textpath back so it
            // would both fit the actual path _and_ be squeezed as before. It's a bummer.

            // This is just a way to temporarily remember the transform in repr. When repr is
            // reattached outside of the group, the transform will be written more properly
            // (i.e. optimized into the object if the corresponding preference is set)
            gchar *affinestr=sp_svg_transform_write(ctrans);
            SPText * text = dynamic_cast<SPText *>(citem);
            if (text) {
                //this causes a change in text-on-path appearance when there is a non-conformal transform, see bug #1594565
                double scale = (ctrans.expansionX() + ctrans.expansionY()) / 2.0;
                SPTextPath * text_path = dynamic_cast<SPTextPath *>(text->children);
                if (!text_path) {
                    nrepr->setAttribute("transform", affinestr);
                } else {
                    sp_recursive_scale_text_size(nrepr, scale);
                    Geom::Affine ttrans = ctrans.inverse() * SP_ITEM(text)->transform * ctrans;
                    gchar *affinestr = sp_svg_transform_write(ttrans);
                    nrepr->setAttribute("transform", affinestr);
                    g_free(affinestr);
                }
            } else {
                nrepr->setAttribute("transform", affinestr);
            }
            g_free(affinestr);

            items = g_slist_prepend (items, nrepr);

        } else {
            Inkscape::XML::Node *nrepr = child->getRepr()->duplicate(prepr->document());
            objects = g_slist_prepend (objects, nrepr);
        }
    }

    /* Step 2 - clear group */
    // remember the position of the group
    gint pos = group->getRepr()->position();

    // the group is leaving forever, no heir, clones should take note; its children however are going to reemerge
    group->deleteObject(true, false);

    /* Step 3 - add nonitems */
    if (objects) {
        Inkscape::XML::Node *last_def = defs->getRepr()->lastChild();
        while (objects) {
            Inkscape::XML::Node *repr = (Inkscape::XML::Node *) objects->data;
            if (!sp_repr_is_meta_element(repr)) {
                defs->getRepr()->addChild(repr, last_def);
            }
            Inkscape::GC::release(repr);
            objects = g_slist_remove (objects, objects->data);
        }
    }

    /* Step 4 - add items */
    while (items) {
        Inkscape::XML::Node *repr = (Inkscape::XML::Node *) items->data;
        // add item
        prepr->appendChild(repr);
        // restore position; since the items list was prepended (i.e. reverse), we now add
        // all children at the same pos, which inverts the order once again
        repr->setPosition(pos > 0 ? pos : 0);

        // fill in the children list if non-null
        SPItem *item = static_cast<SPItem *>(doc->getObjectByRepr(repr));

        if (item) {
            item->doWriteTransform(repr, item->transform, NULL, false);
            children.insert(children.begin(),item);
        } else {
            g_assert_not_reached();
        }

        Inkscape::GC::release(repr);
        items = g_slist_remove (items, items->data);
    }

    if (do_done) {
        DocumentUndo::done(doc, SP_VERB_NONE, _("Ungroup"));
    }
}

/*
 * some API for list aspect of SPGroup
 */

std::vector<SPItem*> sp_item_group_item_list(SPGroup * group)
{
    std::vector<SPItem*> s;
    g_return_val_if_fail(group != NULL, s);

    for (SPObject *o = group->firstChild() ; o ; o = o->getNext() ) {
        if ( dynamic_cast<SPItem *>(o) ) {
            s.push_back((SPItem*)o);
        }
    }
    return s;
}

SPObject *sp_item_group_get_child_by_name(SPGroup *group, SPObject *ref, const gchar *name)
{
    SPObject *child = (ref) ? ref->getNext() : group->firstChild();
    while ( child && strcmp(child->getRepr()->name(), name) ) {
        child = child->getNext();
    }
    return child;
}

void SPGroup::setLayerMode(LayerMode mode) {
    if ( _layer_mode != mode ) {
        if ( mode == LAYER ) {
            this->document->addResource("layer", this);
        } else if ( _layer_mode == LAYER ) {
            this->document->removeResource("layer", this);
        }
        _layer_mode = mode;
        _updateLayerMode();
    }
}

SPGroup::LayerMode SPGroup::layerDisplayMode(unsigned int dkey) const {
    std::map<unsigned int, LayerMode>::const_iterator iter;
    iter = _display_modes.find(dkey);
    if ( iter != _display_modes.end() ) {
        return (*iter).second;
    } else {
        return GROUP;
    }
}

void SPGroup::setExpanded(bool isexpanded) {
    if ( _expanded != isexpanded ){
        _expanded = isexpanded;
    }
}

void SPGroup::setInsertBottom(bool insertbottom) {
    if ( _insertBottom != insertbottom) {
        _insertBottom = insertbottom;
    }
}

void SPGroup::setLayerDisplayMode(unsigned int dkey, SPGroup::LayerMode mode) {
    if ( layerDisplayMode(dkey) != mode ) {
        _display_modes[dkey] = mode;
        _updateLayerMode(dkey);
    }
}

void SPGroup::_updateLayerMode(unsigned int display_key) {
    SPItemView *view;
    for ( view = this->display ; view ; view = view->next ) {
        if ( !display_key || view->key == display_key ) {
            Inkscape::DrawingGroup *g = dynamic_cast<Inkscape::DrawingGroup *>(view->arenaitem);
            if (g) {
                g->setPickChildren(effectiveLayerMode(view->key) == SPGroup::LAYER);
            }
        }
    }
}

void SPGroup::translateChildItems(Geom::Translate const &tr)
{
    if ( hasChildren() ) {
        for (SPObject *o = firstChild() ; o ; o = o->getNext() ) {
            SPItem *item = dynamic_cast<SPItem *>(o);
            if ( item ) {
                sp_item_move_rel(item, tr);
            }
        }
    }
}

// Recursively (or not) scale child items around a point
void SPGroup::scaleChildItemsRec(Geom::Scale const &sc, Geom::Point const &p, bool noRecurse)
{
    if ( hasChildren() ) {
        for (SPObject *o = firstChild() ; o ; o = o->getNext() ) {
            if ( SPDefs *defs = dynamic_cast<SPDefs *>(o) ) { // select symbols from defs, ignore clips, masks, patterns
                for (SPObject *defschild = defs->firstChild() ; defschild ; defschild = defschild->getNext() ) {
                    SPGroup *defsgroup = dynamic_cast<SPGroup *>(defschild);
                    if (defsgroup)
                        defsgroup->scaleChildItemsRec(sc, p, false);
                }
            } else if ( SPItem *item = dynamic_cast<SPItem *>(o) ) {
                SPGroup *group = dynamic_cast<SPGroup *>(item);
                if (group && !dynamic_cast<SPBox3D *>(item)) {
                    /* Using recursion breaks clipping because transforms are applied 
                       in coordinates for draws but nothing in defs is changed
                       instead change the transform on the entire group, and the transform
                       is applied after any references to clipping paths.  However NOT using
                       recursion apparently breaks as of r13544 other parts of Inkscape
                       involved with showing/modifying units.  So offer both for use
                       in different contexts.
                    */
                    if(noRecurse) {
                        // used for EMF import
                        Geom::Translate const s(p);
                        Geom::Affine final = s.inverse() * sc * s;
                        Geom::Affine tAff = item->i2dt_affine() * final;
                        item->set_i2d_affine(tAff);
                        tAff = item->transform;
                        // Eliminate common rounding error affecting EMF/WMF input.
                        // When the rounding error persists it converts the simple 
                        //    transform=scale() to transform=matrix().
                        if(std::abs(tAff[4]) < 1.0e-5 && std::abs(tAff[5]) < 1.0e-5){
                           tAff[4] = 0.0;
                           tAff[5] = 0.0;
                        }
                        item->doWriteTransform(item->getRepr(), tAff, NULL, true);
                    } else {
                        // used for other import
                        SPItem *subItem = NULL;
                        if (item->clip_ref->getObject()) {
                            subItem = dynamic_cast<SPItem *>(item->clip_ref->getObject()->firstChild());
                        }
                        if (subItem != NULL) {
                            subItem->doWriteTransform(subItem->getRepr(), subItem->transform*sc, NULL, true);
                        }
                        subItem = NULL;
                        if (item->mask_ref->getObject()) {
                            subItem = dynamic_cast<SPItem *>(item->mask_ref->getObject()->firstChild());
                        }
                        if (subItem != NULL) {
                            subItem->doWriteTransform(subItem->getRepr(), subItem->transform*sc, NULL, true);
                        }
                        item->doWriteTransform(item->getRepr(), sc.inverse()*item->transform*sc, NULL, true);
                        group->scaleChildItemsRec(sc, p, false);
                    }
                } else {
//                    Geom::OptRect bbox = item->desktopVisualBounds();
//                    if (bbox) {  // test not needed, this was causing a failure to scale <circle> and <rect> in the clipboard, see LP Bug 1365451
                        // Scale item
                        Geom::Translate const s(p);
                        Geom::Affine final = s.inverse() * sc * s;
                        
                        gchar const *conn_type = NULL;
                        SPText *textItem = dynamic_cast<SPText *>(item);
                        bool isTextTextpath = textItem && textItem->firstChild() && dynamic_cast<SPTextPath *>(textItem->firstChild());
                        if (isTextTextpath) {
                            textItem->optimizeTextpathText();
                        } else {
                            SPFlowtext *flowText = dynamic_cast<SPFlowtext *>(item);
                            if (flowText) {
                                flowText->optimizeScaledText();
                            } else {
                                SPBox3D *box = dynamic_cast<SPBox3D *>(item);
                                if (box) {
                                    // Force recalculation from perspective
                                    box3d_position_set(box);
                                } else if (item->getAttribute("inkscape:connector-type") != NULL
                                           && (item->getAttribute("inkscape:connection-start") == NULL
                                               || item->getAttribute("inkscape:connection-end") == NULL)) {
                                    // Remove and store connector type for transform if disconnected
                                    conn_type = item->getAttribute("inkscape:connector-type");
                                    item->removeAttribute("inkscape:connector-type");
                                }
                            }
                        }
                        
                        Persp3D *persp = dynamic_cast<Persp3D *>(item);
                        if (persp) {
                            persp3d_apply_affine_transformation(persp, final);
                        } else if (isTextTextpath && !item->transform.isIdentity()) {
                            // Save and reset current transform
                            Geom::Affine tmp(item->transform);
                            item->transform = Geom::Affine();
                            // Apply scale
                            item->set_i2d_affine(item->i2dt_affine() * sc);
                            item->doWriteTransform(item->getRepr(), item->transform, NULL, true);
                            // Scale translation and restore original transform
                            tmp[4] *= sc[0];
                            tmp[5] *= sc[1];
                            item->doWriteTransform(item->getRepr(), tmp, NULL, true);
                        } else if (dynamic_cast<SPUse *>(item)) {
                            // calculate the matrix we need to apply to the clone
                            // to cancel its induced transform from its original
                            Geom::Affine move = final.inverse() * item->transform * final;
                            item->doWriteTransform(item->getRepr(), move, &move, true);
                        } else {
                            item->doWriteTransform(item->getRepr(), item->transform*sc, NULL, true);
                        }
                        
                        if (conn_type != NULL) {
                            item->setAttribute("inkscape:connector-type", conn_type);
                        }
                        
                        if (item->isCenterSet() && !(final.isTranslation() || final.isIdentity())) {
                            item->scaleCenter(sc); // All coordinates have been scaled, so also the center must be scaled
                            item->updateRepr();
                        }
//                    }
                }
            }
        }
    }
}

gint SPGroup::getItemCount() const {
    gint len = 0;
    for (SPObject const *o = this->firstChild() ; o ; o = o->getNext() ) {
        if (dynamic_cast<SPItem const *>(o)) {
            len++;
        }
    }

    return len;
}

void SPGroup::_showChildren (Inkscape::Drawing &drawing, Inkscape::DrawingItem *ai, unsigned int key, unsigned int flags) {
    Inkscape::DrawingItem *ac = NULL;
    std::vector<SPObject*> l=this->childList(false, SPObject::ActionShow);
    for(std::vector<SPObject*>::const_iterator i=l.begin();i!=l.end();++i){
        SPObject *o = *i;
        SPItem * child = dynamic_cast<SPItem *>(o);
        if (child) {
            ac = child->invoke_show (drawing, key, flags);
            if (ac) {
                ai->appendChild(ac);
            }
        }
    }
}

void SPGroup::update_patheffect(bool write) {
#ifdef GROUP_VERBOSE
    g_message("sp_group_update_patheffect: %p\n", lpeitem);
#endif

    std::vector<SPItem*> const item_list = sp_item_group_item_list(this);

    for ( std::vector<SPItem*>::const_iterator iter=item_list.begin();iter!=item_list.end();++iter) {
        SPObject *subitem = *iter;

        SPLPEItem *lpeItem = dynamic_cast<SPLPEItem *>(subitem);
        if (lpeItem) {
            lpeItem->update_patheffect(write);
        }
    }

    if (hasPathEffect() && pathEffectsEnabled()) {
        for (PathEffectList::iterator it = this->path_effect_list->begin(); it != this->path_effect_list->end(); ++it)
        {
            LivePathEffectObject *lpeobj = (*it)->lpeobject;

            if (lpeobj && lpeobj->get_lpe()) {
                lpeobj->get_lpe()->doBeforeEffect_impl(this);
            }
        }

        sp_group_perform_patheffect(this, this, write);
    }
}

static void
sp_group_perform_patheffect(SPGroup *group, SPGroup *topgroup, bool write)
{
    std::vector<SPItem*> const item_list = sp_item_group_item_list(group);

    for ( std::vector<SPItem*>::const_iterator iter=item_list.begin();iter!=item_list.end();++iter) {
        SPObject *subitem = *iter;

        SPGroup *subGroup = dynamic_cast<SPGroup *>(subitem);
        if (subGroup) {
            sp_group_perform_patheffect(subGroup, topgroup, write);
        } else {
            SPShape *subShape = dynamic_cast<SPShape *>(subitem);
            if (subShape) {
                SPCurve * c = NULL;

                SPPath *subPath = dynamic_cast<SPPath *>(subShape);
                if (subPath) {
                    c = subPath->get_original_curve();
                } else {
                    c = subShape->getCurve();
                }

                // only run LPEs when the shape has a curve defined
                if (c) {
                    c->transform(i2anc_affine(subitem, topgroup));
                    topgroup->performPathEffect(c);
                    c->transform(i2anc_affine(subitem, topgroup).inverse());
                    subShape->setCurve(c, TRUE);

                    if (write) {
                        Inkscape::XML::Node *repr = subitem->getRepr();
                        gchar *str = sp_svg_write_path(c->get_pathvector());
                        repr->setAttribute("d", str);
#ifdef GROUP_VERBOSE
                        g_message("sp_group_perform_patheffect writes 'd' attribute");
#endif
                        g_free(str);
                    }

                    c->unref();
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
