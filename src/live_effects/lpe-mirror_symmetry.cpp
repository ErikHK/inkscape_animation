/** \file
 * LPE <mirror_symmetry> implementation: mirrors a path with respect to a given line.
 */
/*
 * Authors:
 *   Maximilian Albert
 *   Johan Engelen
 *   Abhishek Sharma
 *   Jabiertxof
 *
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 * Copyright (C) Maximilin Albert 2008 <maximilian.albert@gmail.com>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <gtkmm.h>
#include "live_effects/lpe-mirror_symmetry.h"
#include <display/curve.h>
#include <svg/path-string.h>
#include "helper/geom.h"
#include <2geom/path-intersection.h>
// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>

namespace Inkscape {
namespace LivePathEffect {

static const Util::EnumData<ModeType> ModeTypeData[MT_END] = {
    { MT_V, N_("Vertical Page Center"), "vertical" },
    { MT_H, N_("Horizontal Page Center"), "horizontal" },
    { MT_FREE, N_("Free from reflection line"), "free" },
    { MT_X, N_("X from middle knot"), "X" },
    { MT_Y, N_("Y from middle knot"), "Y" }
};
static const Util::EnumDataConverter<ModeType>
MTConverter(ModeTypeData, MT_END);

LPEMirrorSymmetry::LPEMirrorSymmetry(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    mode(_("Mode"), _("Symmetry move mode"), "mode", MTConverter, &wr, this, MT_FREE),
    discard_orig_path(_("Discard original path?"), _("Check this to only keep the mirrored part of the path"), "discard_orig_path", &wr, this, false),
    fuse_paths(_("Fuse paths"), _("Fuse original and the reflection into a single path"), "fuse_paths", &wr, this, false),
    oposite_fuse(_("Opposite fuse"), _("Picks the other side of the mirror as the original"), "oposite_fuse", &wr, this, false),
    start_point(_("Start mirror line"), _("Start mirror line"), "start_point", &wr, this, _("Adjust start of mirroring")),
    end_point(_("End mirror line"), _("End mirror line"), "end_point", &wr, this, _("Adjust end of mirroring")),
    center_point(_("Center mirror line"),_("Center mirror line"), "center_point", &wr, this, _("Adjust center of mirroring"))
{
    show_orig_path = true;
    registerParameter(&mode);
    registerParameter( &discard_orig_path);
    registerParameter( &fuse_paths);
    registerParameter( &oposite_fuse);
    registerParameter( &start_point);
    registerParameter( &end_point);
    registerParameter( &center_point);
    previous_center = Geom::Point(0,0);
    apply_to_clippath_and_mask = true;
}

LPEMirrorSymmetry::~LPEMirrorSymmetry()
{
}

Gtk::Widget * LPEMirrorSymmetry::newWidget()
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
                if (param->param_key != "center_point") {
                    vbox->pack_start(*widg, true, true, 2);
                    if (tip) {
                        widg->set_tooltip_text(*tip);
                    } else {
                        widg->set_tooltip_text("");
                        widg->set_has_tooltip(false);
                    }
                }
            }
        }

        ++it;
    }
    return dynamic_cast<Gtk::Widget *>(vbox);
}


void
LPEMirrorSymmetry::doBeforeEffect (SPLPEItem const* lpeitem)
{
    using namespace Geom;
    original_bbox(lpeitem);
    Point point_a(boundingbox_X.max(), boundingbox_Y.min());
    Point point_b(boundingbox_X.max(), boundingbox_Y.max());
    Point point_c(boundingbox_X.max(), boundingbox_Y.middle());
    if (mode == MT_Y) {
        point_a = Geom::Point(boundingbox_X.min(),center_point[Y]);
        point_b = Geom::Point(boundingbox_X.max(),center_point[Y]);
    }
    if (mode == MT_X) {
        point_a = Geom::Point(center_point[X],boundingbox_Y.min());
        point_b = Geom::Point(center_point[X],boundingbox_Y.max());
    }
    if ((Geom::Point)start_point == (Geom::Point)end_point) {
        start_point.param_setValue(point_a, true);
        end_point.param_setValue(point_b, true);
        previous_center = Geom::middle_point((Geom::Point)start_point, (Geom::Point)end_point);
        center_point.param_setValue(previous_center, true);
        return;
    }
    if ( mode == MT_X || mode == MT_Y ) {
        if (!are_near(previous_center, (Geom::Point)center_point, 0.01)) {
            center_point.param_setValue(Geom::middle_point(point_a, point_b), true);
            end_point.param_setValue(point_b, true);
            start_point.param_setValue(point_a, true);
        } else {
            if ( mode == MT_X ) {
                if (!are_near(start_point[X], point_a[X], 0.01)) {
                    start_point.param_setValue(point_a, true);
                }
                if (!are_near(end_point[X], point_b[X], 0.01)) {
                    end_point.param_setValue(point_b, true);
                }
            } else {  //MT_Y
                if (!are_near(start_point[Y], point_a[Y], 0.01)) {
                    start_point.param_setValue(point_a, true);
                }
                if (!are_near(end_point[Y], point_b[Y], 0.01)) {
                    end_point.param_setValue(point_b, true);
                }
            }
        }
    } else if ( mode == MT_FREE) {
        if (are_near(previous_center, (Geom::Point)center_point, 0.01)) {
            center_point.param_setValue(Geom::middle_point((Geom::Point)start_point, (Geom::Point)end_point), true);
        } else {
            Geom::Point trans = center_point - Geom::middle_point((Geom::Point)start_point, (Geom::Point)end_point);
            start_point.param_setValue(start_point * trans, true);
            end_point.param_setValue(end_point * trans, true);
        }
    } else if ( mode == MT_V){
        SPDocument * document = SP_ACTIVE_DOCUMENT;
        if (document) {
            Geom::Affine transform = i2anc_affine(SP_OBJECT(lpeitem), NULL).inverse();
            Geom::Point sp = Geom::Point(document->getWidth().value("px")/2.0, 0) * transform;
            start_point.param_setValue(sp, true);
            Geom::Point ep = Geom::Point(document->getWidth().value("px")/2.0, document->getHeight().value("px")) * transform;
            end_point.param_setValue(ep, true);
            center_point.param_setValue(Geom::middle_point((Geom::Point)start_point, (Geom::Point)end_point), true);
        }
    } else { //horizontal page
        SPDocument * document = SP_ACTIVE_DOCUMENT;
        if (document) {
            Geom::Affine transform = i2anc_affine(SP_OBJECT(lpeitem), NULL).inverse();
            Geom::Point sp = Geom::Point(0, document->getHeight().value("px")/2.0) * transform;
            start_point.param_setValue(sp, true);
            Geom::Point ep = Geom::Point(document->getWidth().value("px"), document->getHeight().value("px")/2.0) * transform;
            end_point.param_setValue(ep, true);
            center_point.param_setValue(Geom::middle_point((Geom::Point)start_point, (Geom::Point)end_point), true);
        }
    }
    previous_center = center_point;
}

void
LPEMirrorSymmetry::transform_multiply(Geom::Affine const& postmul, bool set)
{
    // cycle through all parameters. Most parameters will not need transformation, but path and point params do.
    for (std::vector<Parameter *>::iterator it = param_vector.begin(); it != param_vector.end(); ++it) {
        Parameter * param = *it;
        param->param_transform_multiply(postmul, set);
    }
    previous_center = Geom::middle_point((Geom::Point)start_point, (Geom::Point)end_point);
}

void
LPEMirrorSymmetry::doOnApply (SPLPEItem const* lpeitem)
{
    using namespace Geom;

    original_bbox(lpeitem);

    Point point_a(boundingbox_X.max(), boundingbox_Y.min());
    Point point_b(boundingbox_X.max(), boundingbox_Y.max());
    Point point_c(boundingbox_X.max(), boundingbox_Y.middle());
    start_point.param_setValue(point_a, true);
    start_point.param_update_default(point_a);
    end_point.param_setValue(point_b, true);
    end_point.param_update_default(point_b);
    center_point.param_setValue(point_c, true);
    previous_center = center_point;
}


Geom::PathVector
LPEMirrorSymmetry::doEffect_path (Geom::PathVector const & path_in)
{
    Geom::PathVector const original_pathv = pathv_to_linear_and_cubic_beziers(path_in);
    Geom::PathVector path_out;
    
    if (!discard_orig_path && !fuse_paths) {
        path_out = pathv_to_linear_and_cubic_beziers(path_in);
    }

    Geom::Line line_separation((Geom::Point)start_point, (Geom::Point)end_point);
    Geom::Affine m = Geom::reflection (line_separation.vector(), (Geom::Point)start_point);

    if (fuse_paths && !discard_orig_path) {
        for (Geom::PathVector::const_iterator path_it = original_pathv.begin();
             path_it != original_pathv.end(); ++path_it) 
        {
            if (path_it->empty()) {
                continue;
            }
            Geom::PathVector tmp_path;
            double time_start = 0.0;
            int position = 0;
            bool end_open = false;
            if (path_it->closed()) {
                const Geom::Curve &closingline = path_it->back_closed();
                if (!are_near(closingline.initialPoint(), closingline.finalPoint())) {
                    end_open = true;
                }
            }
            Geom::Path original = *path_it;
            if (end_open && path_it->closed()) {
                original.close(false);
                original.appendNew<Geom::LineSegment>( original.initialPoint() );
                original.close(true);
            }
            Geom::Point s = start_point;
            Geom::Point e = end_point;
            double dir = line_separation.angle();
            double diagonal = Geom::distance(Geom::Point(boundingbox_X.min(),boundingbox_Y.min()),Geom::Point(boundingbox_X.max(),boundingbox_Y.max()));
            Geom::Rect bbox(Geom::Point(boundingbox_X.min(),boundingbox_Y.min()),Geom::Point(boundingbox_X.max(),boundingbox_Y.max()));
            double size_divider = Geom::distance(center_point, bbox) + diagonal;
            s = Geom::Point::polar(dir,size_divider) + center_point;
            e = Geom::Point::polar(dir + Geom::rad_from_deg(180),size_divider) + center_point;
            Geom::Path divider = Geom::Path(s);
            divider.appendNew<Geom::LineSegment>(e);
            Geom::Crossings cs = crossings(original, divider);
            std::vector<double> crossed;
            for(unsigned int i = 0; i < cs.size(); i++) {
                crossed.push_back(cs[i].ta);
            }
            std::sort(crossed.begin(), crossed.end());
            for (unsigned int i = 0; i < crossed.size(); i++) {
                double time_end = crossed[i];
                if (time_start != time_end && time_end - time_start > Geom::EPSILON) {
                    Geom::Path portion = original.portion(time_start, time_end);
                    if (!portion.empty()) {
                        Geom::Point middle = portion.pointAt((double)portion.size()/2.0);
                        position = Geom::sgn(Geom::cross(e - s, middle - s));
                        if (!oposite_fuse) {
                            position *= -1;
                        }
                        if (position == 1) {
                            Geom::Path mirror = portion.reversed() * m;
                            mirror.setInitial(portion.finalPoint());
                            portion.append(mirror);
                            if(i!=0) {
                                portion.setFinal(portion.initialPoint());
                                portion.close();
                            }
                            tmp_path.push_back(portion);
                        }
                        portion.clear();
                    }
                }
                time_start = time_end;
            }
            position = Geom::sgn(Geom::cross(e - s, original.finalPoint() - s));
            if (!oposite_fuse) {
                position *= -1;
            }
            if (cs.size()!=0 && position == 1) {
                if (time_start != original.size() && original.size() - time_start > Geom::EPSILON) {
                    Geom::Path portion = original.portion(time_start, original.size());
                    if (!portion.empty()) {
                        portion = portion.reversed();
                        Geom::Path mirror = portion.reversed() * m;
                        mirror.setInitial(portion.finalPoint());
                        portion.append(mirror);
                        portion = portion.reversed();
                        if (!original.closed()) {
                            tmp_path.push_back(portion);
                        } else {
                            if (cs.size() > 1 && tmp_path.size() > 0 && tmp_path[0].size() > 0 ) {
                                portion.setFinal(tmp_path[0].initialPoint());
                                portion.setInitial(tmp_path[0].finalPoint());
                                tmp_path[0].append(portion);
                            } else {
                                tmp_path.push_back(portion);
                            }
                            tmp_path[0].close();
                        }
                        portion.clear();
                    }
                }
            }
            if (cs.size() == 0 && position == 1) {
                tmp_path.push_back(original);
                tmp_path.push_back(original * m);
            }
            path_out.insert(path_out.end(), tmp_path.begin(), tmp_path.end());
            tmp_path.clear();
        }
    }

    if (!fuse_paths || discard_orig_path) {
        for (size_t i = 0; i < original_pathv.size(); ++i) {
            path_out.push_back(original_pathv[i] * m);
        }
    }

    return path_out;
}

void
LPEMirrorSymmetry::addCanvasIndicators(SPLPEItem const */*lpeitem*/, std::vector<Geom::PathVector> &hp_vec)
{
    using namespace Geom;
    hp_vec.clear();
    Geom::Path path;
    Geom::Point s = start_point;
    Geom::Point e = end_point;
    path.start( s );
    path.appendNew<Geom::LineSegment>( e );
    Geom::PathVector helper;
    helper.push_back(path);
    hp_vec.push_back(helper);
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
