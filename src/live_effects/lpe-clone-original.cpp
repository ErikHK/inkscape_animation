/*
 * Copyright (C) Johan Engelen 2012 <j.b.c.engelen@alumnus.utwente.nl>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "live_effects/lpe-clone-original.h"
#include "live_effects/lpe-spiro.h"
#include "live_effects/lpe-bspline.h"
#include "live_effects/lpeobject.h"
#include "live_effects/lpeobject-reference.h"
#include "display/curve.h"
#include "svg/path-string.h"
#include "svg/svg.h"

#include "object/sp-clippath.h"
#include "object/sp-mask.h"
#include "object/sp-path.h"
#include "object/sp-shape.h"

#include "xml/sp-css-attr.h"

// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>

namespace Inkscape {
namespace LivePathEffect {

static const Util::EnumData<Clonelpemethod> ClonelpemethodData[] = {
    { CLM_NONE, N_("No shape"), "none" },
    { CLM_ORIGINALD, N_("Without LPE's"), "originald" }, 
    { CLM_BSPLINESPIRO, N_("With Spiro or BSpline"), "bsplinespiro" },
    { CLM_D, N_("With LPE's"), "d" }
};
static const Util::EnumDataConverter<Clonelpemethod> CLMConverter(ClonelpemethodData, CLM_END);

LPECloneOriginal::LPECloneOriginal(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    linkeditem(_("Linked Item:"), _("Item from which to take the original data"), "linkeditem", &wr, this),
    method(_("Shape linked"), _("Shape linked"), "method", CLMConverter, &wr, this, CLM_D),
    attributes("Attributes linked", "Attributes linked, comma separated attributes", "attributes", &wr, this,""),
    style_attributes("Style attributes linked", "Style attributes linked, comma separated attributes like fill, filter, opacity", "style_attributes", &wr, this,""),
    allow_transforms(_("Allow transforms"), _("Allow transforms"), "allow_transforms", &wr, this, true)
{
    //0.92 compatibility
    const gchar * linkedpath = this->getRepr()->attribute("linkedpath");
    if (linkedpath && strcmp(linkedpath, "") != 0){
        this->getRepr()->setAttribute("linkeditem", linkedpath);
        this->getRepr()->setAttribute("linkedpath", NULL);
        this->getRepr()->setAttribute("method", "bsplinespiro");
        this->getRepr()->setAttribute("allow_transforms", "false");
    };
    is_updating = false;
    listening = false;
    linked = g_strdup(this->getRepr()->attribute("linkeditem"));
    registerParameter(&linkeditem);
    registerParameter(&method);
    registerParameter(&attributes);
    registerParameter(&style_attributes);
    registerParameter(&allow_transforms);
    prev_allow_trans = allow_transforms;
    previus_method = method;
    prev_affine = g_strdup("");
    attributes.param_hide_canvas_text();
    style_attributes.param_hide_canvas_text();
}

void
LPECloneOriginal::cloneAttrbutes(SPObject *origin, SPObject *dest, const char * attributes, const char * style_attributes) 
{
    SPDocument * document = SP_ACTIVE_DOCUMENT;
    if (!document || !origin || !dest) {
        return;
    }
    if ( SP_IS_GROUP(origin) && SP_IS_GROUP(dest) && SP_GROUP(origin)->getItemCount() == SP_GROUP(dest)->getItemCount() ) {
        std::vector< SPObject * > childs = origin->childList(true);
        size_t index = 0;
        for (std::vector<SPObject * >::iterator obj_it = childs.begin(); 
             obj_it != childs.end(); ++obj_it) {
            SPObject *dest_child = dest->nthChild(index); 
            cloneAttrbutes((*obj_it), dest_child, attributes, style_attributes); 
            index++;
        }
    }
    //Attributes
    SPShape * shape_origin =  SP_SHAPE(origin);
    SPPath * path_origin =  SP_PATH(origin);
    SPShape * shape_dest =  SP_SHAPE(dest);
    SPMask *mask_origin = SP_ITEM(origin)->mask_ref->getObject();
    SPMask *mask_dest = SP_ITEM(dest)->mask_ref->getObject();
    if(mask_origin && mask_dest) {
        std::vector<SPObject*> mask_list = mask_origin->childList(true);
        std::vector<SPObject*> mask_list_dest = mask_dest->childList(true);
        if (mask_list.size() == mask_list_dest.size()) {
            size_t i = 0;
            for ( std::vector<SPObject*>::const_iterator iter=mask_list.begin();iter!=mask_list.end();++iter) {
                SPObject * mask_data = *iter;
                SPObject * mask_dest_data = mask_list_dest[i];
                cloneAttrbutes(mask_data, mask_dest_data, attributes, style_attributes);
                i++;
            }
        }
    }
    SPClipPath *clippath_origin = SP_ITEM(origin)->clip_ref->getObject();
    SPClipPath *clippath_dest = SP_ITEM(dest)->clip_ref->getObject();
    if(clippath_origin && clippath_dest) {
        std::vector<SPObject*> clippath_list = clippath_origin->childList(true);
        std::vector<SPObject*> clippath_list_dest = clippath_dest->childList(true);
        if (clippath_list.size() == clippath_list_dest.size()) {
            size_t i = 0;
            for ( std::vector<SPObject*>::const_iterator iter=clippath_list.begin();iter!=clippath_list.end();++iter) {
                SPObject * clippath_data = *iter;
                SPObject * clippath_dest_data = clippath_list_dest[i];
                cloneAttrbutes(clippath_data, clippath_dest_data, attributes, style_attributes);
                i++;
            }
        }
    }
    gchar ** attarray = g_strsplit(attributes, ",", 0);
    gchar ** iter = attarray;
    while (*iter != NULL) {
        const char* attribute = (*iter);
        if (strlen(attribute)) {
            if ( shape_dest && shape_origin && (std::strcmp(attribute, "d") == 0)) {
                SPCurve *c = NULL;
                if (method == CLM_BSPLINESPIRO) {
                    c = shape_origin->getCurveForEdit();
                    SPLPEItem * lpe_item = SP_LPE_ITEM(origin);
                    if (lpe_item) {
                        PathEffectList lpelist = lpe_item->getEffectList();
                        PathEffectList::iterator i;
                        for (i = lpelist.begin(); i != lpelist.end(); ++i) {
                            LivePathEffectObject *lpeobj = (*i)->lpeobject;
                            if (lpeobj) {
                                Inkscape::LivePathEffect::Effect *lpe = lpeobj->get_lpe();
                                if (dynamic_cast<Inkscape::LivePathEffect::LPEBSpline *>(lpe)) {
                                    LivePathEffect::sp_bspline_do_effect(c, 0);
                                } else if (dynamic_cast<Inkscape::LivePathEffect::LPESpiro *>(lpe)) {
                                    LivePathEffect::sp_spiro_do_effect(c);
                                }
                            }
                        }
                    }
                } else if(method == CLM_ORIGINALD) {
                    c = shape_origin->getCurveForEdit();
                } else {
                    c = shape_origin->getCurve();
                }
                if (c) {
                    Geom::PathVector c_pv = c->get_pathvector();
                    c_pv *= i2anc_affine(dest, sp_lpe_item);
                    c->set_pathvector(c_pv);
                    if (!path_origin) {
                        shape_dest->setCurveBeforeLPE(c);
                        gchar *str = sp_svg_write_path(c_pv);
                        dest->getRepr()->setAttribute(attribute, str);
                        g_free(str);
                    } else {
                        shape_dest->setCurveBeforeLPE(c);
                    }
                    c->unref();
                } else {
                    dest->getRepr()->setAttribute(attribute, NULL);
                }
            } else {
                dest->getRepr()->setAttribute(attribute, origin->getRepr()->attribute(attribute));
            }
        }
        iter++;
    }
    g_strfreev (attarray);
    SPCSSAttr *css_origin = sp_repr_css_attr_new();
    sp_repr_css_attr_add_from_string(css_origin, origin->getRepr()->attribute("style"));
    SPCSSAttr *css_dest = sp_repr_css_attr_new();
    sp_repr_css_attr_add_from_string(css_dest, dest->getRepr()->attribute("style"));
    gchar ** styleattarray = g_strsplit(style_attributes, ",", 0);
    gchar ** styleiter = styleattarray;
    while (*styleiter != NULL) {
        const char* attribute = (*styleiter);
        if (strlen(attribute)) {
            const char* origin_attribute = sp_repr_css_property(css_origin, attribute, "");
            if (!strlen(origin_attribute)) { //==0
                sp_repr_css_set_property (css_dest, attribute, NULL);
            } else {
                sp_repr_css_set_property (css_dest, attribute, origin_attribute);
            }
        }
        styleiter++;
    }
    g_strfreev (styleattarray);
    Glib::ustring css_str;
    sp_repr_css_write_string(css_dest,css_str);
    dest->getRepr()->setAttribute("style", g_strdup(css_str.c_str()));
}
void
LPECloneOriginal::doBeforeEffect (SPLPEItem const* lpeitem){
    start_listening();
    if (linkeditem.linksToItem()) {
        Glib::ustring attr = "";
        if (method != CLM_NONE) {
            attr += Glib::ustring("d,");
        }
        gchar * attributes_str = attributes.param_getSVGValue();
        attr += Glib::ustring(attributes_str) + Glib::ustring(",");
        if (attr.size()  && !Glib::ustring(attributes_str).size()) {
            attr.erase (attr.size()-1, 1);
        }
        gchar * style_attributes_str = style_attributes.param_getSVGValue();
        Glib::ustring style_attr = "";
        if (style_attr.size() && !Glib::ustring( style_attributes_str).size()) {
            style_attr.erase (style_attr.size()-1, 1);
        }
        style_attr += Glib::ustring( style_attributes_str) + Glib::ustring(",");

        SPItem * orig =  SP_ITEM(linkeditem.getObject());
        if(!orig) {
            return;
        }
        SPItem * dest =  SP_ITEM(sp_lpe_item); 
        Geom::OptRect o_bbox = orig->geometricBounds();
        Geom::OptRect d_bbox = dest->geometricBounds();
        gchar * id = g_strdup(orig->getId());
        if (allow_transforms && 
            !linkeditem.last_transform.isIdentity() && 
            linkeditem.last_transform.isTranslation() &&
            method != CLM_NONE) 
        {
            Geom::Point expansion_dest = dest->transform.expansion();
            Geom::Point expansion_orig = orig->transform.expansion();
            dest->transform *= Geom::Scale(expansion_dest).inverse();
            dest->transform *= Geom::Scale(expansion_orig);
            dest->transform *= linkeditem.last_transform.inverse();
            dest->transform *= Geom::Scale(expansion_orig).inverse();
            dest->transform *= Geom::Scale(expansion_dest);
        }
        if ((strcmp(id, linked) != 0 || (previus_method != method && previus_method == CLM_NONE )) &&
             allow_transforms && 
             o_bbox && 
             d_bbox) 
        {
            dest->transform *= Geom::Translate((*o_bbox).corner(0) - (*d_bbox).corner(0)).inverse();
        }
        cloneAttrbutes(orig, dest, g_strdup(attr.c_str()), g_strdup(style_attr.c_str()));
        if (allow_transforms &&
            previus_method != method &&
            method == CLM_NONE) 
        {
            dest->transform *= Geom::Translate((*d_bbox).corner(0) - (*o_bbox).corner(0)).inverse();
        }

        if (!allow_transforms) {
            SP_ITEM(dest)->getRepr()->setAttribute("transform", SP_ITEM(orig)->getAttribute("transform"));
        } else {
            SP_ITEM(dest)->getRepr()->setAttribute("transform", sp_svg_transform_write(dest->transform));
            if (prev_allow_trans == allow_transforms) {
                prev_affine = g_strdup(SP_ITEM(dest)->getAttribute("transform"));
            }
        }

        if (prev_allow_trans != allow_transforms && allow_transforms) {
            SP_ITEM(dest)->getRepr()->setAttribute("transform", prev_affine);
        }

        linked = g_strdup(id);
        g_free(style_attributes_str);
        g_free(attributes_str);
        g_free(id);
    } else {
        linked = g_strdup("");
    }
    previus_method = method;
    prev_allow_trans = allow_transforms;
}

void
LPECloneOriginal::start_listening()
{
    if ( !sp_lpe_item || listening ) {
        return;
    }
    quit_listening();
    modified_connection = SP_OBJECT(sp_lpe_item)->connectModified(sigc::mem_fun(*this, &LPECloneOriginal::modified));
    listening = true;
}

void
LPECloneOriginal::quit_listening(void)
{
    modified_connection.disconnect();
    listening = false;
}

void
LPECloneOriginal::modified(SPObject */*obj*/, guint /*flags*/)
{
    if ( !sp_lpe_item || is_updating) {
        is_updating = false;
        return;
    }
    SP_OBJECT(this->getLPEObj())->requestModified(SP_OBJECT_MODIFIED_FLAG);
    is_updating = true;
}

LPECloneOriginal::~LPECloneOriginal()
{
    quit_listening();
    g_free(linked);
    g_free(prev_affine);
}

void
LPECloneOriginal::transform_multiply(Geom::Affine const& postmul, bool set)
{
    if (!allow_transforms && linkeditem.linksToItem()) {
        sp_lpe_item->transform *= postmul.inverse();
    }
}

void LPECloneOriginal::doOnRemove(SPLPEItem const* lpeitem)
{
   if (linkeditem.linksToItem() && lpeitem->path_effect_list->size() == 1 && !keep_paths) {
        SPDesktop * dt = SP_ACTIVE_DESKTOP;
        SPObject * obj = dynamic_cast<SPObject*>(sp_lpe_item);
        SPItem * orig  = SP_ITEM(linkeditem.getObject());
        if(!orig) {
            return;
        }
        const gchar * transform = lpeitem->getRepr()->attribute("transform");
        dt->selection->clear();
        dt->selection->add(orig, true);
        dt->selection->clone();
        dt->selection->singleItem()->setAttribute("transform" , transform);
        //TODO: get a way to realy delete current LPEItem
        obj->getRepr()->setAttribute("original-d", NULL);
        obj->getRepr()->setAttribute("d", "0,0");   
        obj->getRepr()->setAttribute("inkscape:label", "SAFE TO REMOVE");   
        obj->getRepr()->setAttribute("inkscape:connector-curvature", NULL);
        obj->getRepr()->setAttribute("transform", NULL);
        obj->getRepr()->setAttribute("style", "opacity:0");
    }
}

void 
LPECloneOriginal::doEffect (SPCurve * curve)
{
}

} // namespace LivePathEffect
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
