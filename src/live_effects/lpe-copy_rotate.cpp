/** \file
 * LPE <copy_rotate> implementation
 */
/*
 * Authors:
 *   Maximilian Albert <maximilian.albert@gmail.com>
 *   Johan Engelen <j.b.c.engelen@alumnus.utwente.nl>
 *   Jabiertxo Arraiza Cenoz <jabier.arraiza@marker.es>
 * Copyright (C) Authors 2007-2012
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <gtkmm.h>
#include <gdk/gdk.h>
#include <2geom/path-intersection.h>
#include <2geom/sbasis-to-bezier.h>
#include <2geom/intersection-graph.h>
#include "live_effects/lpe-copy_rotate.h"
#include "live_effects/lpeobject.h"
#include "display/curve.h"
#include "svg/path-string.h"
#include "svg/svg.h"
#include "helper/geom.h"
#include "xml/sp-css-attr.h"
#include "path-chemistry.h"

#include "object/sp-path.h"
#include "object/sp-shape.h"

// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>

namespace Inkscape {
namespace LivePathEffect {

static const Util::EnumData<RotateMethod> RotateMethodData[RM_END] = {
    { RM_NORMAL, N_("Normal"), "normal" },
    { RM_KALEIDOSCOPE, N_("Kaleidoscope"), "kaleidoskope" },
    { RM_FUSE, N_("Fuse paths"), "fuse_paths" }
};
static const Util::EnumDataConverter<RotateMethod>
RMConverter(RotateMethodData, RM_END);

bool 
pointInTriangle(Geom::Point const &p, Geom::Point const &p1, Geom::Point const &p2, Geom::Point const &p3)
{
    //http://totologic.blogspot.com.es/2014/01/accurate-point-in-triangle-test.html
    using Geom::X;
    using Geom::Y;
    double denominator = (p1[X]*(p2[Y] - p3[Y]) + p1[Y]*(p3[X] - p2[X]) + p2[X]*p3[Y] - p2[Y]*p3[X]);
    double t1 = (p[X]*(p3[Y] - p1[Y]) + p[Y]*(p1[X] - p3[X]) - p1[X]*p3[Y] + p1[Y]*p3[X]) / denominator;
    double t2 = (p[X]*(p2[Y] - p1[Y]) + p[Y]*(p1[X] - p2[X]) - p1[X]*p2[Y] + p1[Y]*p2[X]) / -denominator;
    double s = t1 + t2;

    return 0 <= t1 && t1 <= 1 && 0 <= t2 && t2 <= 1 && s <= 1;
}

LPECopyRotate::LPECopyRotate(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    method(_("Method:"), _("Rotate methods"), "method", RMConverter, &wr, this, RM_NORMAL),
    origin(_("Origin"), _("Adjust origin of the rotation"), "origin", &wr, this,  _("Adjust origin of the rotation")),
    starting_point(_("Start point"), _("Starting point to define start angle"), "starting_point", &wr, this, _("Adjust starting point to define start angle")),
    starting_angle(_("Starting angle"), _("Angle of the first copy"), "starting_angle", &wr, this, 0.0),
    rotation_angle(_("Rotation angle"), _("Angle between two successive copies"), "rotation_angle", &wr, this, 60.0),
    num_copies(_("Number of copies"), _("Number of copies of the original path"), "num_copies", &wr, this, 6),
    gap(_("Gap"), _("Gap"), "gap", &wr, this, -0.0001),
    copies_to_360(_("360º Copies"), _("No rotation angle, fixed to 360º"), "copies_to_360", &wr, this, true),
    mirror_copies(_("Mirror copies"), _("Mirror between copies"), "mirror_copies", &wr, this, true),
    split_items(_("Split elements"), _("Split elements, this allow gradients and other paints."), "split_items", &wr, this, false),
    dist_angle_handle(100.0)
{
    show_orig_path = true;
    _provides_knotholder_entities = true;
    //0.92 compatibility
    if (this->getRepr()->attribute("fuse_paths") && strcmp(this->getRepr()->attribute("fuse_paths"), "true") == 0){
        this->getRepr()->setAttribute("fuse_paths", NULL);
        this->getRepr()->setAttribute("method", "kaleidoskope");
        this->getRepr()->setAttribute("mirror_copies", "true");
    };
    // register all your parameters here, so Inkscape knows which parameters this effect has:
    registerParameter(&method);
    registerParameter(&num_copies);
    registerParameter(&starting_angle);
    registerParameter(&starting_point);
    registerParameter(&rotation_angle);
    registerParameter(&gap);
    registerParameter(&origin);
    registerParameter(&copies_to_360);
    registerParameter(&mirror_copies);
    registerParameter(&split_items);

    gap.param_set_range(-99999.0, 99999.0);
    gap.param_set_increments(0.1, 0.1);
    gap.param_set_digits(5);
    num_copies.param_set_range(1, 999999);
    num_copies.param_make_integer(true);
    apply_to_clippath_and_mask = true;
    previous_num_copies = num_copies;
    previous_origin = Geom::Point(0,0);
    previous_start_point = Geom::Point(0,0);
    starting_point.param_widget_is_visible(false);
    reset = false;
}

LPECopyRotate::~LPECopyRotate()
{

}

void
LPECopyRotate::doAfterEffect (SPLPEItem const* lpeitem)
{
    if (split_items) {
        is_load = false;
        SPDocument * document = SP_ACTIVE_DOCUMENT;
        if (!document) {
            return;
        }
        items.clear();
        container = dynamic_cast<SPObject *>(sp_lpe_item->parent);
        Inkscape::XML::Node *root = sp_lpe_item->document->getReprRoot();
        Inkscape::XML::Node *root_origin = document->getReprRoot();
        if (root_origin != root) {
            return;
        }
        if (previous_num_copies != num_copies) {
            gint numcopies_gap = previous_num_copies - num_copies;
            if (numcopies_gap > 0 && num_copies != 0) {
                guint counter = num_copies - 1;
                while (numcopies_gap > 0) {
                    Glib::ustring id = Glib::ustring("rotated-");
                    id += std::to_string(counter);
                    id += "-";
                    id += this->lpeobj->getId();
                    if (id.empty()) {
                        return;
                    }
                    SPObject *elemref = NULL;
                    if (elemref = document->getObjectById(id.c_str())) {
                        SP_ITEM(elemref)->setHidden(true);
                    }
                    counter++;
                    numcopies_gap--;
                }
            }
            previous_num_copies = num_copies;
        }
        SPObject *elemref = NULL;
        guint counter = 0;
        Glib::ustring id = "rotated-0-";
        id += this->lpeobj->getId();
        while((elemref = document->getObjectById(id.c_str()))) {
            id = Glib::ustring("rotated-");
            id += std::to_string(counter);
            id += "-";
            id += this->lpeobj->getId();
            if (SP_ITEM(elemref)->isHidden()) {
                items.push_back(id);
            }
            counter++;
        }
        Geom::Affine m = Geom::Translate(-origin) * Geom::Rotate(-(Geom::rad_from_deg(starting_angle)));
        for (size_t i = 1; i < num_copies; ++i) {
            Geom::Affine r = Geom::identity();
            if(mirror_copies && i%2 != 0) {
                r *= Geom::Rotate(Geom::Angle(half_dir)).inverse();
                r *= Geom::Scale(1, -1);
                r *= Geom::Rotate(Geom::Angle(half_dir));
            }

            Geom::Rotate rot(-(Geom::rad_from_deg(rotation_angle * i)));
            Geom::Affine t = m * r * rot * Geom::Rotate(Geom::rad_from_deg(starting_angle)) * Geom::Translate(origin);
            if (method != RM_NORMAL) {
                if(mirror_copies && i%2 != 0) {
                    t = m *  r * rot * Geom::Rotate(-Geom::rad_from_deg(starting_angle)) * Geom::Translate(origin);
                }
            } else {
                if(mirror_copies && i%2 != 0) {
                    t = m * Geom::Rotate(Geom::rad_from_deg(-rotation_angle)) * r * rot * Geom::Rotate(-Geom::rad_from_deg(starting_angle)) * Geom::Translate(origin);
                }
            }
            t *= sp_lpe_item->transform;
            toItem(t, i-1, reset);
        }
        reset = false;
    } else {
        processObjects(LPE_ERASE);
        items.clear();
    }
}

void
LPECopyRotate::cloneD(SPObject *orig, SPObject *dest, Geom::Affine transform, bool reset) 
{
    SPDocument * document = SP_ACTIVE_DOCUMENT;
    if (!document) {
        return;
    }
    Inkscape::XML::Document *xml_doc = document->getReprDoc();
    if ( SP_IS_GROUP(orig) && SP_IS_GROUP(dest) && SP_GROUP(orig)->getItemCount() == SP_GROUP(dest)->getItemCount() ) {
        std::vector< SPObject * > childs = orig->childList(true);
        size_t index = 0;
        for (std::vector<SPObject * >::iterator obj_it = childs.begin(); 
             obj_it != childs.end(); ++obj_it) {
            SPObject *dest_child = dest->nthChild(index); 
            cloneD(*obj_it, dest_child, transform, reset); 
            index++;
        }
        return;
    }
    SPShape * shape =  SP_SHAPE(orig);
    SPPath * path =  SP_PATH(dest);
    if (shape && !path) {   
        const char * id = dest->getId();
        shape->removeAllPathEffects(true);
        Inkscape::XML::Node *dest_node = sp_selected_item_to_curved_repr(SP_ITEM(dest), 0);
        dest->updateRepr(xml_doc, dest_node, SP_OBJECT_WRITE_ALL);
        dest->getRepr()->setAttribute("d", id);
        path =  SP_PATH(dest);
    }
    if (path && shape) {
        SPCurve *c = shape->getCurve();
        if (c) {
            gchar *str = sp_svg_write_path(c->get_pathvector());
            path->getRepr()->setAttribute("d", str);
            g_free(str);
            c->unref();
        } else {
            path->getRepr()->setAttribute("d", NULL);
        }
        if (reset) {
            dest->getRepr()->setAttribute("style", shape->getRepr()->attribute("style"));
        }
    }
}

Inkscape::XML::Node *
LPECopyRotate::createPathBase(SPObject *elemref) {
    SPDocument * document = SP_ACTIVE_DOCUMENT;
    if (!document) {
        return NULL;
    }
    Inkscape::XML::Document *xml_doc = document->getReprDoc();
    Inkscape::XML::Node *prev = elemref->getRepr();
    SPGroup *group = dynamic_cast<SPGroup *>(elemref);
    if (group) {
        Inkscape::XML::Node *container = xml_doc->createElement("svg:g");
        container->setAttribute("transform", prev->attribute("transform"));
        std::vector<SPItem*> const item_list = sp_item_group_item_list(group);
        Inkscape::XML::Node *previous = NULL;
        for ( std::vector<SPItem*>::const_iterator iter=item_list.begin();iter!=item_list.end();++iter) {
            SPObject *sub_item = *iter;
            Inkscape::XML::Node *resultnode = createPathBase(sub_item);
            container->addChild(resultnode, previous);
            previous = resultnode;
        }
        return container;
    }
    Inkscape::XML::Node *resultnode = xml_doc->createElement("svg:path");
    resultnode->setAttribute("transform", prev->attribute("transform"));
    return resultnode;
}

void
LPECopyRotate::toItem(Geom::Affine transform, size_t i, bool reset)
{
    SPDocument * document = SP_ACTIVE_DOCUMENT;
    if (!document) {
        return;
    }
    Inkscape::XML::Document *xml_doc = document->getReprDoc();
    Glib::ustring elemref_id = Glib::ustring("rotated-");
    elemref_id += std::to_string(i);
    elemref_id += "-";
    elemref_id += this->lpeobj->getId();
    items.push_back(elemref_id);
    SPObject *elemref= NULL;
    Inkscape::XML::Node *phantom = NULL;
    if (elemref = document->getObjectById(elemref_id.c_str())) {
        phantom = elemref->getRepr();
    } else {
        phantom = createPathBase(sp_lpe_item);
        phantom->setAttribute("id", elemref_id.c_str());
        reset = true;
    }
    if (!elemref) {
        elemref = container->appendChildRepr(phantom);
        Inkscape::GC::release(phantom);
    }
    cloneD(SP_OBJECT(sp_lpe_item), elemref, transform, reset);
    gchar *str = sp_svg_transform_write(transform);
    elemref->getRepr()->setAttribute("transform" , str);
    g_free(str);
    SP_ITEM(elemref)->setHidden(false);
    if (elemref->parent != container) {
        Inkscape::XML::Node *copy = phantom->duplicate(xml_doc);
        copy->setAttribute("id", elemref_id.c_str());
        container->appendChildRepr(copy);
        Inkscape::GC::release(copy);
        elemref->deleteObject();
    }
}

void
LPECopyRotate::resetStyles(){
    reset = true;
    doAfterEffect(sp_lpe_item);
}

Gtk::Widget * LPECopyRotate::newWidget()
{
    // use manage here, because after deletion of Effect object, others might
    // still be pointing to this widget.
    Gtk::VBox *vbox = Gtk::manage(new Gtk::VBox(Effect::newWidget()));

    vbox->set_border_width(5);
    vbox->set_homogeneous(false);
    vbox->set_spacing(2);
    std::vector<Parameter *>::iterator it = param_vector.begin();
    while (it != param_vector.end()) {
        if ((*it)->widget_is_visible) {
            Parameter *param = *it;
            Gtk::Widget *widg = dynamic_cast<Gtk::Widget *>(param->param_newWidget());
            Glib::ustring *tip = param->param_getTooltip();
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
    Gtk::Button * reset_button = Gtk::manage(new Gtk::Button(Glib::ustring(_("Reset styles"))));
    reset_button->signal_clicked().connect(sigc::mem_fun (*this,&LPECopyRotate::resetStyles));
    reset_button->set_size_request(110,20);
    vbox->pack_start(*hbox, true,true,2);
    hbox->pack_start(*reset_button, false, false,2);
    if(Gtk::Widget* widg = defaultParamSet()) {
        vbox->pack_start(*widg, true, true, 2);
    }
    return dynamic_cast<Gtk::Widget *>(vbox);
}


void
LPECopyRotate::doOnApply(SPLPEItem const* lpeitem)
{
    using namespace Geom;
    original_bbox(lpeitem, false, true);

    A = Point(boundingbox_X.min(), boundingbox_Y.middle());
    B = Point(boundingbox_X.middle(), boundingbox_Y.middle());
    origin.param_setValue(A);
    origin.param_update_default(A);
    dist_angle_handle = L2(B - A);
    dir = unit_vector(B - A);
}

void
LPECopyRotate::transform_multiply(Geom::Affine const& postmul, bool set)
{
    // cycle through all parameters. Most parameters will not need transformation, but path and point params do.
    for (std::vector<Parameter *>::iterator it = param_vector.begin(); it != param_vector.end(); ++it) {
        Parameter * param = *it;
        if(param->param_key != "starting_point" || postmul.isRotation()) {
            param->param_transform_multiply(postmul, set);
        }
    }
    sp_lpe_item_update_patheffect(sp_lpe_item, false, false);
}

void
LPECopyRotate::doBeforeEffect (SPLPEItem const* lpeitem)
{
    using namespace Geom;
    original_bbox(lpeitem, false, true);
    if (copies_to_360 && num_copies > 2) {
        rotation_angle.param_set_value(360.0/(double)num_copies);
    }
    if (method != RM_NORMAL && rotation_angle * num_copies > 360.1 && rotation_angle > 0 && copies_to_360) {
        num_copies.param_set_value(floor(360/rotation_angle));
    }
    if (method != RM_NORMAL  && mirror_copies && copies_to_360) {
        num_copies.param_set_increments(2.0,10.0);
        if ((int)num_copies%2 !=0) {
            num_copies.param_set_value(num_copies+1);
            rotation_angle.param_set_value(360.0/(double)num_copies);
        }
    } else {
        num_copies.param_set_increments(1.0, 10.0);
    }

    A = Point(boundingbox_X.min(), boundingbox_Y.middle());
    B = Point(boundingbox_X.middle(), boundingbox_Y.middle());
    if (Geom::are_near(A, B, 0.01)) {
        B += Geom::Point(1.0, 0.0);
    }
    dir = unit_vector(B - A);
    // I first suspected the minus sign to be a bug in 2geom but it is
    // likely due to SVG's choice of coordinate system orientation (max)
    bool near_start_point = Geom::are_near(previous_start_point, (Geom::Point)starting_point, 0.01);
    bool near_origin = Geom::are_near(previous_origin, (Geom::Point)origin, 0.01);
    if (!near_start_point) {
        starting_angle.param_set_value(deg_from_rad(-angle_between(dir, starting_point - origin)));
        if (GDK_SHIFT_MASK) {
            dist_angle_handle = L2(B - A);
        } else {
            dist_angle_handle = L2(starting_point - origin);
        }
    }
    if (dist_angle_handle < 1.0) {
        dist_angle_handle = 1.0;
    }
    double distance = dist_angle_handle;
    if (previous_start_point != Geom::Point(0,0) || previous_origin != Geom::Point(0,0)) {
        distance = Geom::distance(previous_origin, starting_point);
    }
    start_pos = origin + dir * Rotate(-rad_from_deg(starting_angle)) * distance;
    if (!near_start_point || !near_origin || split_items) {
        starting_point.param_setValue(start_pos);
    }

    previous_origin = (Geom::Point)origin;
    previous_start_point = (Geom::Point)starting_point;
}

void
LPECopyRotate::split(Geom::PathVector &path_on, Geom::Path const &divider)
{
    Geom::PathVector tmp_path;
    double time_start = 0.0;
    Geom::Path original = path_on[0];
    int position = 0;
    Geom::Crossings cs = crossings(original,divider);
    std::vector<double> crossed;
    for(unsigned int i = 0; i < cs.size(); i++) {
        crossed.push_back(cs[i].ta);
    }
    std::sort(crossed.begin(), crossed.end());
    for (unsigned int i = 0; i < crossed.size(); i++) {
        double time_end = crossed[i];
        if (time_start == time_end || time_end - time_start < Geom::EPSILON) {
            continue;
        }
        Geom::Path portion_original = original.portion(time_start,time_end);
        if (!portion_original.empty()) {
            Geom::Point side_checker = portion_original.pointAt(0.0001);
            position = Geom::sgn(Geom::cross(divider[1].finalPoint() - divider[0].finalPoint(), side_checker - divider[0].finalPoint()));
            if (rotation_angle != 180) {
                position = pointInTriangle(side_checker, divider.initialPoint(), divider[0].finalPoint(), divider[1].finalPoint());
            }
            if (position == 1) {
                tmp_path.push_back(portion_original);
            }
            portion_original.clear();
            time_start = time_end;
        }
    }
    position = Geom::sgn(Geom::cross(divider[1].finalPoint() - divider[0].finalPoint(), original.finalPoint() - divider[0].finalPoint()));
    if (rotation_angle != 180) {
        position = pointInTriangle(original.finalPoint(), divider.initialPoint(), divider[0].finalPoint(), divider[1].finalPoint());
    }
    if (cs.size() > 0 && position == 1) {
        Geom::Path portion_original = original.portion(time_start, original.size());
        if(!portion_original.empty()){
            if (!original.closed()) {
                tmp_path.push_back(portion_original);
            } else {
                if (tmp_path.size() > 0 && tmp_path[0].size() > 0 ) {
                    portion_original.setFinal(tmp_path[0].initialPoint());
                    portion_original.append(tmp_path[0]);
                    tmp_path[0] = portion_original;
                } else {
                    tmp_path.push_back(portion_original);
                }
            }
            portion_original.clear();
        }
    }
    if (cs.size()==0  && position == 1) {
        tmp_path.push_back(original);
    }
    path_on = tmp_path;
}

void
LPECopyRotate::setFusion(Geom::PathVector &path_on, Geom::Path divider, double size_divider)
{
    split(path_on,divider);
    Geom::PathVector tmp_path;
    Geom::Affine pre = Geom::Translate(-origin);
    for (Geom::PathVector::const_iterator path_it = path_on.begin(); path_it != path_on.end(); ++path_it) {
        Geom::Path original = *path_it;
        if (path_it->empty()) {
            continue;
        }
        Geom::PathVector tmp_path_helper;
        Geom::Path append_path = original;
        Geom::Point previous = original.finalPoint();
        for (int i = 0; i < num_copies; ++i) {
            Geom::Rotate rot(-Geom::rad_from_deg(rotation_angle * (i)));
            Geom::Affine m = pre * rot * Geom::Translate(origin);
            if (i%2 != 0 && mirror_copies) {
                Geom::Point point_a = (Geom::Point)origin;
                Geom::Point point_b = origin + dir * Geom::Rotate(-Geom::rad_from_deg((rotation_angle*i)+starting_angle)) * size_divider;
                Geom::Line ls(point_a, point_b);
                m = Geom::reflection (ls.vector(), point_a);
                append_path *= m;
            } else {
                append_path = original;
                append_path *= m;
            }
            previous = append_path.finalPoint();
            if (tmp_path_helper.size() > 0) {
                if (Geom::are_near(tmp_path_helper[tmp_path_helper.size()-1].finalPoint(), append_path.finalPoint())) {
                    Geom::Path tmp_append = append_path.reversed();
                    tmp_append.setInitial(tmp_path_helper[tmp_path_helper.size()-1].finalPoint());
                    tmp_path_helper[tmp_path_helper.size()-1].append(tmp_append);
                } else if (Geom::are_near(tmp_path_helper[tmp_path_helper.size()-1].initialPoint(), append_path.initialPoint())) {
                    Geom::Path tmp_append = append_path;
                    tmp_path_helper[tmp_path_helper.size()-1] = tmp_path_helper[tmp_path_helper.size()-1].reversed();
                    tmp_append.setInitial(tmp_path_helper[tmp_path_helper.size()-1].finalPoint());
                    tmp_path_helper[tmp_path_helper.size()-1].append(tmp_append);
                } else if (Geom::are_near(tmp_path_helper[tmp_path_helper.size()-1].finalPoint(), append_path.initialPoint())) {
                    Geom::Path tmp_append = append_path;
                    tmp_append.setInitial(tmp_path_helper[tmp_path_helper.size()-1].finalPoint());
                    tmp_path_helper[tmp_path_helper.size()-1].append(tmp_append);
                } else if (Geom::are_near(tmp_path_helper[tmp_path_helper.size()-1].initialPoint(), append_path.finalPoint())) {
                    Geom::Path tmp_append = append_path.reversed();
                    tmp_path_helper[tmp_path_helper.size()-1] = tmp_path_helper[tmp_path_helper.size()-1].reversed();
                    tmp_append.setInitial(tmp_path_helper[tmp_path_helper.size()-1].finalPoint());
                    tmp_path_helper[tmp_path_helper.size()-1].append(tmp_append);
                } else if (Geom::are_near(tmp_path_helper[0].finalPoint(), append_path.finalPoint())) {
                    Geom::Path tmp_append = append_path.reversed();
                    tmp_append.setInitial(tmp_path_helper[0].finalPoint());
                    tmp_path_helper[0].append(tmp_append);
                } else if (Geom::are_near(tmp_path_helper[0].initialPoint(), append_path.initialPoint())) {
                    Geom::Path tmp_append = append_path;
                    tmp_path_helper[0] = tmp_path_helper[0].reversed();
                    tmp_append.setInitial(tmp_path_helper[0].finalPoint());
                    tmp_path_helper[0].append(tmp_append);
                } else {
                    tmp_path_helper.push_back(append_path);
                }
                if ( Geom::are_near(tmp_path_helper[tmp_path_helper.size()-1].finalPoint(),tmp_path_helper[tmp_path_helper.size()-1].initialPoint())) {
                    tmp_path_helper[tmp_path_helper.size()-1].close();
                }
            } else {
                tmp_path_helper.push_back(append_path);
            }
        }
        if (tmp_path_helper.size() > 0) {
            tmp_path_helper[tmp_path_helper.size()-1] = tmp_path_helper[tmp_path_helper.size()-1];
            tmp_path_helper[0] = tmp_path_helper[0];
            if (rotation_angle * num_copies != 360) {
                Geom::Ray base_a(divider.pointAt(1),divider.pointAt(0));
                double diagonal = Geom::distance(Geom::Point(boundingbox_X.min(),boundingbox_Y.min()),Geom::Point(boundingbox_X.max(),boundingbox_Y.max()));
                Geom::Rect bbox(Geom::Point(boundingbox_X.min(),boundingbox_Y.min()),Geom::Point(boundingbox_X.max(),boundingbox_Y.max()));
                double size_divider = Geom::distance(origin,bbox) + (diagonal * 2);
                Geom::Point base_point = origin + dir * Geom::Rotate(-Geom::rad_from_deg((rotation_angle * num_copies) + starting_angle)) * size_divider;
                Geom::Ray base_b(divider.pointAt(1), base_point);
                if (Geom::are_near(tmp_path_helper[0].initialPoint(),base_a) && 
                    Geom::are_near(tmp_path_helper[0].finalPoint(),base_a)) 
                {
                    tmp_path_helper[0].close();
                    if (tmp_path_helper.size() > 1) {
                        tmp_path_helper[tmp_path_helper.size()-1].close();
                    }
                } else if (Geom::are_near(tmp_path_helper[tmp_path_helper.size()-1].initialPoint(),base_b) && 
                           Geom::are_near(tmp_path_helper[tmp_path_helper.size()-1].finalPoint(),base_b)) 
                {
                    tmp_path_helper[0].close();
                    if (tmp_path_helper.size() > 1) {
                        tmp_path_helper[tmp_path_helper.size()-1].close();
                    }
                } else if ((Geom::are_near(tmp_path_helper[0].initialPoint(),base_a) && 
                           Geom::are_near(tmp_path_helper[tmp_path_helper.size()-1].finalPoint(),base_b)) ||
                           (Geom::are_near(tmp_path_helper[0].initialPoint(),base_b) && 
                           Geom::are_near(tmp_path_helper[tmp_path_helper.size()-1].finalPoint(),base_a))) 
                {
                    Geom::Path close_path = Geom::Path(tmp_path_helper[tmp_path_helper.size()-1].finalPoint());
                    close_path.appendNew<Geom::LineSegment>((Geom::Point)origin);
                    close_path.appendNew<Geom::LineSegment>(tmp_path_helper[0].initialPoint());
                    tmp_path_helper[0].append(close_path);
                }
            }

            if (Geom::are_near(tmp_path_helper[0].finalPoint(),tmp_path_helper[0].initialPoint())) {
                tmp_path_helper[0].close();
            }
        }
        tmp_path.insert(tmp_path.end(), tmp_path_helper.begin(), tmp_path_helper.end());
        tmp_path_helper.clear();
    }
    path_on = tmp_path;
    tmp_path.clear();
}

Geom::PathVector
LPECopyRotate::doEffect_path (Geom::PathVector const & path_in)
{
    Geom::PathVector path_out;
    double diagonal = Geom::distance(Geom::Point(boundingbox_X.min(),boundingbox_Y.min()),Geom::Point(boundingbox_X.max(),boundingbox_Y.max()));
    Geom::OptRect bbox = sp_lpe_item->geometricBounds();
    size_divider = Geom::distance(origin,bbox) + (diagonal * 6);
    Geom::Point line_start  = origin + dir * Geom::Rotate(-(Geom::rad_from_deg(starting_angle))) * size_divider;
    Geom::Point line_end = origin + dir * Geom::Rotate(-(Geom::rad_from_deg(rotation_angle + starting_angle))) * size_divider;
    divider = Geom::Path(line_start);
    divider.appendNew<Geom::LineSegment>((Geom::Point)origin);
    divider.appendNew<Geom::LineSegment>(line_end);
    divider.close();
    half_dir = unit_vector(Geom::middle_point(line_start,line_end) - (Geom::Point)origin);
    if (method != RM_NORMAL) {
        if (method != RM_KALEIDOSCOPE) {
            path_out = doEffect_path_post(path_in);
        } else {
            path_out = pathv_to_linear_and_cubic_beziers(path_in);
        }
        if (num_copies == 0) {
            return path_out;
        }
        Geom::PathVector triangle;
        triangle.push_back(divider);
        Geom::PathIntersectionGraph *pig = new Geom::PathIntersectionGraph(triangle, path_out);
        if (pig && !path_out.empty() && !triangle.empty()) {
            path_out = pig->getIntersection();
        }
        path_out *= Geom::Translate(half_dir * gap);
        if ( !split_items ) {
            path_out *= Geom::Translate(half_dir * gap).inverse();
            path_out = doEffect_path_post(path_out);
        }
    } else {
        path_out = doEffect_path_post(path_in);
    }
    return pathv_to_linear_and_cubic_beziers(path_out);
}

Geom::PathVector
LPECopyRotate::doEffect_path_post (Geom::PathVector const & path_in)
{
    if ((split_items || num_copies == 1) && method == RM_NORMAL) {
         if (split_items) {
             Geom::PathVector path_out = pathv_to_linear_and_cubic_beziers(path_in);
             Geom::Affine m = Geom::Translate(-origin) * Geom::Rotate(-(Geom::rad_from_deg(starting_angle)));
             Geom::Affine t = m * Geom::Rotate(-Geom::rad_from_deg(starting_angle)) * Geom::Rotate(Geom::rad_from_deg(starting_angle)) * Geom::Translate(origin);
             return path_out * t;
         }
        return path_in;
    }

    Geom::Affine pre = Geom::Translate(-origin) * Geom::Rotate(-Geom::rad_from_deg(starting_angle));
    Geom::PathVector original_pathv = pathv_to_linear_and_cubic_beziers(path_in);
    Geom::PathVector output_pv;
    Geom::PathVector output;
    for (int i = 0; i < num_copies; ++i) {
        Geom::Rotate rot(-Geom::rad_from_deg(rotation_angle * i));
        Geom::Affine r = Geom::identity();
        if( i%2 != 0 && mirror_copies) {
            r *= Geom::Rotate(Geom::Angle(half_dir)).inverse();
            r *= Geom::Scale(1, -1);
            r *= Geom::Rotate(Geom::Angle(half_dir));
        }
        Geom::Affine t = pre * r * rot * Geom::Rotate(Geom::rad_from_deg(starting_angle)) * Geom::Translate(origin);
        if(mirror_copies && i%2 != 0) {
            t = pre * r * rot * Geom::Rotate(Geom::rad_from_deg(starting_angle)).inverse() * Geom::Translate(origin);
        }
        if (method != RM_NORMAL) {
            Geom::PathVector join_pv = original_pathv * t;
            join_pv *= Geom::Translate(half_dir * rot * gap);
            Geom::PathIntersectionGraph *pig = new Geom::PathIntersectionGraph(output_pv, join_pv);
            if (pig) {
                if (!output_pv.empty()) {
                    output_pv = pig->getUnion();
                } else {
                    output_pv = join_pv;
                }
            }
        } else {
            t = pre * Geom::Rotate(-Geom::rad_from_deg(starting_angle)) * r * rot * Geom::Rotate(Geom::rad_from_deg(starting_angle)) * Geom::Translate(origin);
            if(mirror_copies && i%2 != 0) {
                t = pre * Geom::Rotate(Geom::rad_from_deg(-starting_angle-rotation_angle)) * r * rot * Geom::Rotate(-Geom::rad_from_deg(starting_angle)) * Geom::Translate(origin);
            }
            output_pv = path_in * t;
            output.insert(output.end(), output_pv.begin(), output_pv.end());
        }
    }
    if (method != RM_NORMAL) {
        output = output_pv;
    }
    return output;
}

void
LPECopyRotate::addCanvasIndicators(SPLPEItem const */*lpeitem*/, std::vector<Geom::PathVector> &hp_vec)
{
    using namespace Geom;
    hp_vec.clear();
    Geom::Path hp;
    hp.start(start_pos);
    hp.appendNew<Geom::LineSegment>((Geom::Point)origin);
    hp.appendNew<Geom::LineSegment>(origin + dir * Rotate(-rad_from_deg(rotation_angle+starting_angle)) * Geom::distance(origin,starting_point));
    Geom::PathVector pathv;
    pathv.push_back(hp);
    hp_vec.push_back(pathv);
}

void
LPECopyRotate::resetDefaults(SPItem const* item)
{
    Effect::resetDefaults(item);
    original_bbox(SP_LPE_ITEM(item), false, true);
}

void
LPECopyRotate::doOnVisibilityToggled(SPLPEItem const* /*lpeitem*/)
{
    processObjects(LPE_VISIBILITY);
}

void 
LPECopyRotate::doOnRemove (SPLPEItem const* /*lpeitem*/)
{
    //set "keep paths" hook on sp-lpe-item.cpp
    if (keep_paths) {
        processObjects(LPE_TO_OBJECTS);
        items.clear();
        return;
    }
    processObjects(LPE_ERASE);
}

} //namespace LivePathEffect
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
