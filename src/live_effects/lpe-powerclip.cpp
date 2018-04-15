/*
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
#include "live_effects/lpe-powerclip.h"
#include <2geom/path-intersection.h>
#include <2geom/intersection-graph.h>
#include "display/curve.h"
#include "helper/geom.h"
#include "ui/tools-switch.h"
#include "path-chemistry.h"
#include "extract-uri.h"
#include <bad-uri-exception.h>

#include "object/sp-clippath.h"
#include "object/sp-path.h"
#include "object/sp-shape.h"
#include "object/sp-item-group.h"
#include "object/uri.h"

// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>

namespace Inkscape {
namespace LivePathEffect {

LPEPowerClip::LPEPowerClip(LivePathEffectObject *lpeobject)
    : Effect(lpeobject),
    hide_clip(_("Hide clip"), _("Hide clip"), "hide_clip", &wr, this, false),
    is_inverse("Store the last inverse apply", "", "is_inverse", &wr, this, "false", false),
    uri("Store the uri of clip", "", "uri", &wr, this, "false", false),
    inverse(_("Inverse clip"), _("Inverse clip"), "inverse", &wr, this, false),
    flatten(_("Flatten clip"), _("Flatten clip, see fill rule once convert to paths"), "flatten", &wr, this, false)
{
    registerParameter(&uri);
    registerParameter(&inverse);
    registerParameter(&flatten);
    registerParameter(&hide_clip);
    registerParameter(&is_inverse);
    convert_shapes = false;
}

LPEPowerClip::~LPEPowerClip() {}

void
LPEPowerClip::doBeforeEffect (SPLPEItem const* lpeitem){
    SPObject * clip_path = SP_ITEM(sp_lpe_item)->clip_ref->getObject();
    gchar * uri_str = uri.param_getSVGValue();
    if(hide_clip && clip_path) {
        SP_ITEM(sp_lpe_item)->clip_ref->detach();
    } else if (!hide_clip && !clip_path && uri_str) {
        try {
            SP_ITEM(sp_lpe_item)->clip_ref->attach(Inkscape::URI(uri_str));
        } catch (Inkscape::BadURIException &e) {
            g_warning("%s", e.what());
            SP_ITEM(sp_lpe_item)->clip_ref->detach();
        }
    }
    clip_path = SP_ITEM(sp_lpe_item)->clip_ref->getObject();
    if (clip_path) {
        uri.param_setValue(Glib::ustring(extract_uri(sp_lpe_item->getRepr()->attribute("clip-path"))), true);
        SP_ITEM(sp_lpe_item)->clip_ref->detach();
        Geom::OptRect bbox = sp_lpe_item->visualBounds();
        if(!bbox) {
            return;
        }
        uri_str = uri.param_getSVGValue();
        if (uri_str) {
            try {
                SP_ITEM(sp_lpe_item)->clip_ref->attach(Inkscape::URI(uri_str));
            } catch (Inkscape::BadURIException &e) {
                g_warning("%s", e.what());
                SP_ITEM(sp_lpe_item)->clip_ref->detach();
            }
        } else {
            SP_ITEM(sp_lpe_item)->clip_ref->detach();
        }
        g_free(uri_str);
        Geom::Rect bboxrect = (*bbox);
        bboxrect.expandBy(1);
        Geom::Point topleft      = bboxrect.corner(0);
        Geom::Point topright     = bboxrect.corner(1);
        Geom::Point bottomright  = bboxrect.corner(2);
        Geom::Point bottomleft   = bboxrect.corner(3);
        clip_box.clear();
        clip_box.start(topleft);
        clip_box.appendNew<Geom::LineSegment>(topright);
        clip_box.appendNew<Geom::LineSegment>(bottomright);
        clip_box.appendNew<Geom::LineSegment>(bottomleft);
        clip_box.close();
        //clip_box *= sp_lpe_item->i2dt_affine();
        std::vector<SPObject*> clip_path_list = clip_path->childList(true);
        for ( std::vector<SPObject*>::const_iterator iter=clip_path_list.begin();iter!=clip_path_list.end();++iter) {
            SPObject * clip_data = *iter;
            SPObject * clip_to_path = NULL;
            if (SP_IS_SHAPE(clip_data) && !SP_IS_PATH(clip_data) && convert_shapes) {
                SPDocument * document = SP_ACTIVE_DOCUMENT;
                if (!document) {
                    return;
                }
                Inkscape::XML::Document *xml_doc = document->getReprDoc();
                SP_SHAPE(clip_data)->removeAllPathEffects(true);
                Inkscape::XML::Node *clip_path_node = sp_selected_item_to_curved_repr(SP_ITEM(clip_data), 0);
                // remember the position of the item
                gint pos = clip_data->getRepr()->position();
                Geom::Affine affine = SP_ITEM(clip_data)->transform;
                // remember parent
                Inkscape::XML::Node *parent = clip_data->getRepr()->parent();
                // remember id
                char const *id = clip_data->getRepr()->attribute("id");
                // remember title
                gchar *title = clip_data->title();
                // remember description
                gchar *desc = clip_data->desc();

                // It's going to resurrect, so we delete without notifying listeners.
                clip_data->deleteObject(false);

                // restore id
                clip_path_node->setAttribute("id", id);
                
                // add the new repr to the parent
                parent->appendChild(clip_path_node);
                clip_to_path = document->getObjectByRepr(clip_path_node);

                // transform position
                SPCurve * c = NULL;
                c = SP_SHAPE(clip_to_path)->getCurve();
                if (c) {
                    Geom::PathVector c_pv = c->get_pathvector();
                    c_pv *= affine;
                    c->set_pathvector(c_pv);
                    SP_SHAPE(clip_to_path)->setCurve(c);
                    c->unref();
                }
                
                clip_path_node->setAttribute("transform", NULL);
 
                if (title && clip_to_path) {
                    clip_to_path->setTitle(title);
                    g_free(title);
                }
                if (desc && clip_to_path) {
                    clip_to_path->setDesc(desc);
                    g_free(desc);
                }
                // move to the saved position
                clip_path_node->setPosition(pos > 0 ? pos : 0);
                Inkscape::GC::release(clip_path_node);
                clip_to_path->emitModified(SP_OBJECT_MODIFIED_CASCADE);
            }
            gchar * is_inverse_str = is_inverse.param_getSVGValue();
            if(!strcmp(is_inverse_str,"false") && inverse && isVisible()) {
                if (clip_to_path) {
                    addInverse(SP_ITEM(clip_to_path));
                } else {
                    addInverse(SP_ITEM(clip_data));
                }
            } else if(!strcmp(is_inverse_str,"true") && !inverse && isVisible()) {
                if (clip_to_path) {
                    removeInverse(SP_ITEM(clip_to_path));
                } else {
                    removeInverse(SP_ITEM(clip_data));
                }
            } else if (inverse && !is_visible && is_inverse_str == (Glib::ustring)"true"){
                removeInverse(SP_ITEM(clip_data));
            }
            g_free(is_inverse_str);
        }
    }
}

void
LPEPowerClip::doAfterEffect (SPLPEItem const* lpeitem){
    if (!hide_clip && flatten && isVisible()) {
        SP_ITEM(sp_lpe_item)->clip_ref->detach();
    }
}

void
LPEPowerClip::addInverse (SPItem * clip_data){
    gchar * is_inverse_str = is_inverse.param_getSVGValue();
    if(!strcmp(is_inverse_str,"false")) {
        if (SP_IS_GROUP(clip_data)) {
            std::vector<SPItem*> item_list = sp_item_group_item_list(SP_GROUP(clip_data));
            for ( std::vector<SPItem*>::const_iterator iter=item_list.begin();iter!=item_list.end();++iter) {
                SPItem *subitem = *iter;
                addInverse(subitem);
            }
        } else if (SP_IS_PATH(clip_data)) {
            SPCurve * c = NULL;
            c = SP_SHAPE(clip_data)->getCurve();
            if (c) {
                Geom::PathVector c_pv = c->get_pathvector();
                //TODO: this can be not correct but no better way
                bool dir_a = Geom::path_direction(c_pv[0]);
                bool dir_b = Geom::path_direction(clip_box);
                if (dir_a == dir_b) {
                   clip_box = clip_box.reversed();
                }
                c_pv.push_back(clip_box);
                c->set_pathvector(c_pv);
                SP_SHAPE(clip_data)->setCurve(c);
                c->unref();
                gchar * is_inverse_str = is_inverse.param_getSVGValue();
                if (strcmp(is_inverse_str, "true") != 0) {
                    is_inverse.param_setValue((Glib::ustring)"true", true);
                }
                SPDesktop *desktop = SP_ACTIVE_DESKTOP;
                if (desktop) {
                    if (tools_isactive(desktop, TOOLS_NODES)) {
                        Inkscape::Selection * sel = SP_ACTIVE_DESKTOP->getSelection();
                        SPItem * item = sel->singleItem();
                        if (item != NULL) {
                            sel->remove(item);
                            sel->add(item);
                        }
                    }
                }
            }
        }
    }
    g_free(is_inverse_str);
}

void
LPEPowerClip::removeInverse (SPItem * clip_data){
    gchar * is_inverse_str = is_inverse.param_getSVGValue();
    if(!strcmp(is_inverse_str,"true")) {
        if (SP_IS_GROUP(clip_data)) {
             std::vector<SPItem*> item_list = sp_item_group_item_list(SP_GROUP(clip_data));
             for ( std::vector<SPItem*>::const_iterator iter=item_list.begin();iter!=item_list.end();++iter) {
                 SPItem *subitem = *iter;
                 removeInverse(subitem);
             }
        } else if (SP_IS_PATH(clip_data)) {
            SPCurve * c = NULL;
            c = SP_SHAPE(clip_data)->getCurve();
            if (c) {
                Geom::PathVector c_pv = c->get_pathvector();
                if(c_pv.size() > 1) {
                    c_pv.pop_back();
                }
                c->set_pathvector(c_pv);
                SP_SHAPE(clip_data)->setCurve(c);
                c->unref();
                gchar * is_inverse_str = is_inverse.param_getSVGValue();
                if (strcmp(is_inverse_str, "false") != 0) {
                    is_inverse.param_setValue((Glib::ustring)"false", true);
                }
                SPDesktop *desktop = SP_ACTIVE_DESKTOP;
                if (desktop) {
                    if (tools_isactive(desktop, TOOLS_NODES)) {
                        Inkscape::Selection * sel = SP_ACTIVE_DESKTOP->getSelection();
                        SPItem * item = sel->singleItem();
                        if (item != NULL) {
                            sel->remove(item);
                            sel->add(item);
                        }
                    }
                }
            }
        }
    }
    g_free(is_inverse_str);
}

void
LPEPowerClip::convertShapes() {
    convert_shapes = true;
    sp_lpe_item_update_patheffect(SP_LPE_ITEM(sp_lpe_item), false, false);
}

Gtk::Widget *
LPEPowerClip::newWidget()
{
    // use manage here, because after deletion of Effect object, others might still be pointing to this widget.
    Gtk::VBox * vbox = Gtk::manage( new Gtk::VBox(Effect::newWidget()));

    vbox->set_border_width(5);
    vbox->set_homogeneous(false);
    vbox->set_spacing(6);

    std::vector<Parameter *>::iterator it = param_vector.begin();
    while (it != param_vector.end()) {
        if ((*it)->widget_is_visible) {
            Parameter * param = *it;
            Gtk::Widget * widg = dynamic_cast<Gtk::Widget *>(param->param_newWidget());
            Glib::ustring * tip = param->param_getTooltip();
            if (widg) {
                vbox->pack_start(*widg, true, true, 2);
                if (tip) {
                    widg->set_tooltip_text(*tip);
                } else {
                    widg->set_tooltip_text("");
                    widg->set_has_tooltip(false);
                }
            }
        }
        ++it;
    }
    Gtk::HBox * hbox = Gtk::manage(new Gtk::HBox(false,0));
    Gtk::Button * topaths_button = Gtk::manage(new Gtk::Button(Glib::ustring(_("Convert clips to paths, undoable"))));
    topaths_button->signal_clicked().connect(sigc::mem_fun (*this,&LPEPowerClip::convertShapes));
    topaths_button->set_size_request(220,30);
    hbox->pack_start(*topaths_button, false, false,2);
    vbox->pack_start(*hbox, true,true,2);
    if(Gtk::Widget* widg = defaultParamSet()) {
        vbox->pack_start(*widg, true, true, 2);
    }
    return dynamic_cast<Gtk::Widget *>(vbox);
}

void 
LPEPowerClip::doOnRemove (SPLPEItem const* /*lpeitem*/)
{
    SPClipPath *clip_path = SP_ITEM(sp_lpe_item)->clip_ref->getObject();
    if(!keep_paths) {
        if(clip_path) {
            gchar * is_inverse_str = is_inverse.param_getSVGValue();
            std::vector<SPObject*> clip_path_list = clip_path->childList(true);
            for ( std::vector<SPObject*>::const_iterator iter=clip_path_list.begin();iter!=clip_path_list.end();++iter) {
                SPObject * clip_data = *iter;
                if(!strcmp(is_inverse_str,"true")) {
                    removeInverse(SP_ITEM(clip_data));
                }
            }
            g_free(is_inverse_str);
        }
    } else {
        if (flatten && clip_path) {
            clip_path->deleteObject();
        }
    }
}

Geom::PathVector
LPEPowerClip::doEffect_path(Geom::PathVector const & path_in){
    Geom::PathVector path_out = pathv_to_linear_and_cubic_beziers(path_in);
    if (!hide_clip && flatten && isVisible()) {
        SPClipPath *clip_path = SP_ITEM(sp_lpe_item)->clip_ref->getObject();
        if(clip_path) {
            std::vector<SPObject*> clip_path_list = clip_path->childList(true);
            for ( std::vector<SPObject*>::const_iterator iter=clip_path_list.begin();iter!=clip_path_list.end();++iter) {
                SPObject * clip_data = *iter;
                flattenClip(SP_ITEM(clip_data), path_out);
            }
        }
    }
    return path_out;
}

void 
LPEPowerClip::doOnVisibilityToggled(SPLPEItem const* lpeitem)
{
    doBeforeEffect(lpeitem);
}


//void
//LPEPowerClip::transform_multiply(Geom::Affine const& postmul, bool set)
//{
//    SPDocument * doc = SP_ACTIVE_DOCUMENT;
//    SPClipPath *clip_path = SP_ITEM(sp_lpe_item)->clip_ref->getObject();
//    if (clip_path && lock) {
//        std::vector<SPObject*> clip_path_list = clip_path->childList(true);
//        Glib::ustring clip_id = (Glib::ustring)clip_path->getId();
//        Glib::ustring box_id = clip_id + (Glib::ustring)"_box";
//        for ( std::vector<SPObject*>::const_iterator iter=clip_path_list.begin();iter!=clip_path_list.end();++iter) {
//            SPItem * clip_data = SP_ITEM(*iter);
//            if(inverse && lock) {
//                removeInverse(clip_data);
//            }
//            if (lock) {
//                clip_data->transform *= postmul;
////                if (!inverse) {
////                    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
////                    if (desktop) {
////                        if (tools_isactive(desktop, TOOLS_NODES)) {
////                            Inkscape::Selection * sel = SP_ACTIVE_DESKTOP->getSelection();
////                            SPItem * item = sel->singleItem();
////                            if (item != NULL) {
////                                sel->remove(item);
////                                sel->add(item);
////                            }
////                        }
////                    }
////                }
//            }
//            if(inverse && lock) {
//                doBeforeEffect(sp_lpe_item);
//            }
//        }
//    }
//    //cycle through all parameters. Most parameters will not need transformation, but path and point params
//    for (std::vector<Parameter *>::iterator it = param_vector.begin(); it != param_vector.end(); ++it) {
//        Parameter * param = *it;
//        param->param_transform_multiply(postmul, set);
//    }
//    toggleClipVisibility();
//    toggleClipVisibility();
//}

void
LPEPowerClip::flattenClip(SPItem * clip_data, Geom::PathVector &path_in)
{
    if (SP_IS_GROUP(clip_data)) {
         std::vector<SPItem*> item_list = sp_item_group_item_list(SP_GROUP(clip_data));
         for ( std::vector<SPItem*>::const_iterator iter=item_list.begin();iter!=item_list.end();++iter) {
             SPItem *subitem = *iter;
             flattenClip(subitem, path_in);
         }
    } else if (SP_IS_PATH(clip_data)) {
        SPCurve * c = NULL;
        c = SP_SHAPE(clip_data)->getCurve();
        if (c) {
            Geom::PathVector c_pv = c->get_pathvector();
            Geom::PathIntersectionGraph *pig = new Geom::PathIntersectionGraph(c_pv, path_in);
            if (pig && !c_pv.empty() && !path_in.empty()) {
                path_in = pig->getIntersection();
            }
            c->unref();
        }
    }
}

void sp_inverse_powerclip(Inkscape::Selection *sel) {
    if (!sel->isEmpty()) {
        auto selList = sel->items();
        for(auto i = boost::rbegin(selList); i != boost::rend(selList); ++i) {
            SPLPEItem* lpeitem = dynamic_cast<SPLPEItem*>(*i);
            if (lpeitem) {
                Effect::createAndApply(POWERCLIP, SP_ACTIVE_DOCUMENT, lpeitem);
                Effect* lpe = lpeitem->getCurrentLPE();
                lpe->getRepr()->setAttribute("is_inverse", "false");
                lpe->getRepr()->setAttribute("is_visible", "true");
                lpe->getRepr()->setAttribute("inverse", "true");
                lpe->getRepr()->setAttribute("flatten", "false");
                lpe->getRepr()->setAttribute("hide_clip", "false");
                dynamic_cast<LPEPowerClip *>(lpe)->convertShapes();
            }
        }
    }
}

}; //namespace LivePathEffect
}; /* namespace Inkscape */

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
