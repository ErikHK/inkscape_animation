/** \file
 * LPE <offset> implementation
 */
/*
 * Authors:
 *   Maximilian Albert
 *   Jabiertxo Arraiza
 *
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 * Copyright (C) Maximilian Albert 2008 <maximilian.albert@gmail.com>
 * Copyright (C) Jabierto Arraiza 2015 <jabier.arraiza@marker.es>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "live_effects/parameter/enum.h"
#include "live_effects/lpe-offset.h"
#include "display/curve.h"
#include "inkscape.h"
#include "helper/geom.h"
#include "helper/geom-pathstroke.h"
#include <2geom/sbasis-to-bezier.h>
#include <2geom/piecewise.h>
#include <2geom/path-intersection.h>
#include <2geom/intersection-graph.h>
#include <2geom/elliptical-arc.h>
#include <2geom/angle.h>
#include <2geom/curve.h>
#include "object/sp-shape.h"
#include "knot-holder-entity.h"
#include "knotholder.h"
#include "knot.h"
#include <algorithm>
//this is only to flatten nonzero fillrule
#include "livarot/Path.h"
#include "livarot/Shape.h"

#include "svg/svg.h"

#include <2geom/elliptical-arc.h>
// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>

namespace Inkscape {
namespace LivePathEffect {

namespace OfS {
    class KnotHolderEntityOffsetPoint : public LPEKnotHolderEntity {
    public:
        KnotHolderEntityOffsetPoint(LPEOffset * effect) : LPEKnotHolderEntity(effect) {inset = false; previous = Geom::Point(Geom::infinity(),Geom::infinity());}
        virtual void knot_set(Geom::Point const &p, Geom::Point const &origin, guint state);
        virtual Geom::Point knot_get() const;
    private:
        bool inset;
        Geom::Point previous;
    };
} // OfS


static const Util::EnumData<unsigned> JoinTypeData[] = {
    {JOIN_BEVEL,       N_("Beveled"),    "bevel"},
    {JOIN_ROUND,       N_("Rounded"),    "round"},
    {JOIN_MITER,       N_("Miter"),      "miter"},
    {JOIN_MITER_CLIP,  N_("Miter Clip"), "miter-clip"},
    {JOIN_EXTRAPOLATE, N_("Extrapolated arc"), "extrp_arc"},
    {JOIN_EXTRAPOLATE1, N_("Extrapolated arc Alt1"), "extrp_arc1"},
    {JOIN_EXTRAPOLATE2, N_("Extrapolated arc Alt2"), "extrp_arc2"},
    {JOIN_EXTRAPOLATE3, N_("Extrapolated arc Alt3"), "extrp_arc3"},
};

static const Util::EnumDataConverter<unsigned> JoinTypeConverter(JoinTypeData, sizeof(JoinTypeData)/sizeof(*JoinTypeData));


LPEOffset::LPEOffset(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    offset(_("Offset:"), _("Offset)"), "offset", &wr, this, 0.0),
    linejoin_type(_("Join:"), _("Determines the shape of the path's corners"),  "linejoin_type", JoinTypeConverter, &wr, this, JOIN_ROUND),
    miter_limit(_("Miter limit:"), _("Maximum length of the miter join (in units of stroke width)"), "miter_limit", &wr, this, 4.0),
    attempt_force_join(_("Force miter"), _("Overrides the miter limit and forces a join."), "attempt_force_join", &wr, this, true),
    update_on_knot_move(_("Update on knot move"), _("Update on knot move"), "update_on_knot_move", &wr, this, true)
{
    show_orig_path = true;
    registerParameter(&linejoin_type);
    registerParameter(&offset);
    registerParameter(&miter_limit);
    registerParameter(&attempt_force_join);
    registerParameter(&update_on_knot_move);
    offset.param_set_increments(0.1, 0.1);
    offset.param_set_digits(4);
    offset_pt = Geom::Point();
    origin = Geom::Point();
    evenodd = true;
    _knot_entity = NULL;
    _provides_knotholder_entities = true;
    apply_to_clippath_and_mask = true;
}

LPEOffset::~LPEOffset()
{
}

static void
sp_flatten(Geom::PathVector &pathvector, bool evenodd)
{
    Path *orig = new Path;
    orig->LoadPathVector(pathvector);
    Shape *theShape = new Shape;
    Shape *theRes = new Shape;
    orig->ConvertWithBackData (1.0);
    orig->Fill (theShape, 0);
    if (evenodd) {
        theRes->ConvertToShape (theShape, fill_oddEven);
    } else {
        theRes->ConvertToShape (theShape, fill_nonZero);
    }
    Path *originaux[1];
    originaux[0] = orig;
    Path *res = new Path;
    theRes->ConvertToForme (res, 1, originaux, true);

    delete theShape;
    delete theRes;
    char *res_d = res->svg_dump_path ();
    delete res;
    delete orig;
    pathvector  = sp_svg_read_pathv(res_d);
}

Geom::PathVector
sp_get_outer(Geom::PathVector pathvector)
{
    Geom::OptRect bbox;
    Geom::Path ret;
    Geom::PathVector ret_pv;
    for (Geom::PathVector::iterator path_it = pathvector.begin(); path_it != pathvector.end(); ++path_it) {
        Geom::Path iter = (*path_it);
        if (iter.empty()) {
            continue;
        }
        if (bbox && iter.boundsFast()->contains(bbox)) {
            ret = iter;
        } else if(!bbox) {
            ret = iter;
        }
        bbox = iter.boundsFast();
    }
    if (!ret.empty()) {
        ret_pv.push_back(ret);
    }
    return ret_pv;
}

Geom::PathVector
sp_get_inner(Geom::PathVector pathvector)
{
    Geom::Path ret;
    Geom::PathVector ret_pv;
    Geom::PathVector outer = sp_get_outer(pathvector);
    Geom::OptRect bbox = outer.boundsFast();
    for (Geom::PathVector::iterator path_it = pathvector.begin(); path_it != pathvector.end(); ++path_it) {
        Geom::Path iter = (*path_it);
        if (iter.empty()) {
            continue;
        }
        if (bbox && bbox != iter.boundsFast()) {
            ret_pv.push_back(iter);
        }
    }
    return ret_pv;
}

static void
sp_set_origin(Geom::PathVector original_pathv, Geom::Point &origin)
{
    double size = 0;
    Geom::PathVector bigger = sp_get_outer(original_pathv);
    boost::optional< Geom::PathVectorTime > pathvectortime = bigger.nearestTime(origin);
    if (pathvectortime) {
        Geom::PathTime pathtime = pathvectortime->asPathTime();
        origin = bigger[(*pathvectortime).path_index].pointAt(pathtime.curve_index + pathtime.t);
    }
}

void
LPEOffset::doOnApply(SPLPEItem const* lpeitem)
{
    if (!SP_IS_SHAPE(lpeitem)) {
        g_warning("LPE measure line can only be applied to shapes (not groups).");
        SPLPEItem * item = const_cast<SPLPEItem*>(lpeitem);
        item->removeCurrentPathEffect(false);
        return;
    }
}

void
LPEOffset::doBeforeEffect (SPLPEItem const* lpeitem)
{
    if (!hp.empty()) {
        hp.clear();
    }
    original_bbox(lpeitem);
    origin = Geom::Point(boundingbox_X.middle(), boundingbox_Y.min());
    SPItem * item = SP_ITEM(sp_lpe_item);
    SPCSSAttr *css;
    const gchar *val;
    css = sp_repr_css_attr (item->getRepr() , "style");
    val = sp_repr_css_property (css, "fill-rule", NULL);
    bool upd_fill_rule = false;
    if (val && strcmp (val, "nonzero") == 0)
    {
        if (evenodd == true) {
            upd_fill_rule = true;
        }
        evenodd = false;
    }
    else if (val && strcmp (val, "evenodd") == 0)
    {
        if (evenodd == false) {
            upd_fill_rule = true;
        }
        evenodd = true;
    }
    original_pathv = pathv_to_linear_and_cubic_beziers(pathvector_before_effect);
    sp_flatten(original_pathv, evenodd);
    origin = offset_pt;
    sp_set_origin(original_pathv, origin);
}

Geom::PathVector 
LPEOffset::doEffect_path(Geom::PathVector const & path_in)
{
    if (offset == 0.0) {
        offset_pt = original_pathv[0].initialPoint();
        origin = offset_pt;
        return original_pathv;
    }
    Geom::PathVector ret;
    Geom::PathVector ret_painter;
    Geom::PathVector ret_eraser;
    for (Geom::PathVector::const_iterator path_it = original_pathv.begin(); path_it != original_pathv.end(); ++path_it) {
        Geom::PathVector ret_painter_tmp;
        Geom::PathVector ret_eraser_tmp;
        
        Geom::Path original = (*path_it);
        if (original.empty()) {
            continue;
        }
        Geom::PathVector original_pv;
        original_pv.push_back(original);
        Geom::OptRect bounds = original_pv.boundsFast();
        Geom::Point winding_point = original[0].initialPoint();
        int wind = 0;
        double dist = Geom::infinity();
        pathv_matrix_point_bbox_wind_distance(original_pathv, Geom::identity(), winding_point, NULL, &wind, &dist, 0.5, NULL);
        bool path_inside = wind % 2 != 0;
        Geom::PathVector outline = Inkscape::outline(original, std::abs(offset) * 2 , 
                               (attempt_force_join ? std::numeric_limits<double>::max() : miter_limit),
                                static_cast<LineJoinType>(linejoin_type.get_value()),
                                static_cast<LineCapType>(BUTT_FLAT));
        sp_flatten(outline, false);
        if (outline.empty()) {
            continue;
        }
        if (offset > 0 && !path_inside) {
            ret_painter_tmp = sp_get_outer(outline);
        } else if (offset < 0 && !path_inside) {
            ret_painter_tmp = sp_get_inner(outline);
        } else if (offset > 0 && path_inside) {
            Geom::OptRect boundsOutline = outline.boundsFast();
            if(!boundsOutline || 
               !bounds || 
               ((*boundsOutline).width()+(*boundsOutline).height())/2.0 > (*bounds).width() + (*bounds).height()){
                continue;
            }
            ret_eraser_tmp = sp_get_inner(outline);
        } else /*if (offset < 0 && path_inside) */ {
            ret_eraser_tmp = sp_get_outer(outline);
        }
        ret_painter.insert(ret_painter.end(),ret_painter_tmp.begin(),ret_painter_tmp.end());
        ret_eraser.insert(ret_eraser.end(),ret_eraser_tmp.begin(),ret_eraser_tmp.end());
    }

    Geom::PathIntersectionGraph *pig = new Geom::PathIntersectionGraph(ret_eraser, ret_painter);
    if (pig && !ret_painter.empty() && !ret_eraser.empty()) {
        ret = pig->getBminusA();
    } else {
        ret = ret_painter;
    }
    Geom::PathVector re_painter;
    for (Geom::PathVector::const_iterator eraser_it = ret_eraser.begin(); eraser_it != ret_eraser.end(); ++eraser_it) {
        Geom::Path eraser = (*eraser_it);
        if (eraser.empty()) {
            continue;
        }
        Geom::OptRect eraser_bbox = eraser.boundsFast();
        for (Geom::PathVector::iterator painter_it = ret_painter.begin(); painter_it != ret_painter.end();) {
            Geom::Path painter = (*painter_it);
            if (painter.empty()) {
                painter_it = ret_painter.erase(painter_it);
                continue;
            }
            Geom::OptRect painter_bbox = painter.boundsFast();
            if ((eraser_bbox.intersects(painter_bbox) &&
                 !painter_bbox.contains(eraser_bbox) ) ||
                 eraser_bbox.contains(painter_bbox) ) 
            {
                re_painter.push_back(painter);
                painter_it = ret_painter.erase(painter_it);
            } else {
                ++painter_it;
            }
        }
    }
    Geom::PathIntersectionGraph *pig_b = new Geom::PathIntersectionGraph(ret, re_painter);
    if (pig && !ret.empty() && !re_painter.empty()) {
        ret = pig_b->getUnion();
    }
    return ret;
}

Gtk::Widget *LPEOffset::newWidget()
{
    // use manage here, because after deletion of Effect object, others might still be pointing to this widget.
    Gtk::VBox * vbox = Gtk::manage( new Gtk::VBox(Effect::newWidget()) );

    vbox->set_border_width(5);
    vbox->set_homogeneous(false);
    vbox->set_spacing(6);
    Gtk::HBox * hbox = Gtk::manage(new Gtk::HBox(false,0));
    std::vector<Parameter *>::iterator it = param_vector.begin();
    while (it != param_vector.end()) {
        if ((*it)->widget_is_visible) {
            Parameter * param = *it;
            Gtk::Widget * widg = dynamic_cast<Gtk::Widget *>(param->param_newWidget());
            Glib::ustring * tip = param->param_getTooltip();
            if (widg) {
                if (param->param_key == "offset") {
                    offset_widget = Gtk::manage(dynamic_cast<Inkscape::UI::Widget::Scalar *>(widg));
                    widg = dynamic_cast<Gtk::Widget *>(offset_widget);
                    if (widg) {
                        Gtk::HBox * hbox_weight_steps = dynamic_cast<Gtk::HBox *>(widg);
                        std::vector< Gtk::Widget* > childList = hbox_weight_steps->get_children();
                        Gtk::Entry* entry_widget = dynamic_cast<Gtk::Entry *>(childList[1]);
                        entry_widget->set_width_chars(9);
                    }
                }
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
        }

        ++it;
    }
    return dynamic_cast<Gtk::Widget *>(vbox);
}

void
LPEOffset::addCanvasIndicators(SPLPEItem const */*lpeitem*/, std::vector<Geom::PathVector> &hp_vec)
{
    hp_vec.push_back(hp);
}

void
LPEOffset::drawHandle(Geom::Point p)
{
    double r = 3.0;
    char const * svgd;
    svgd = "M 0.7,0.35 A 0.35,0.35 0 0 1 0.35,0.7 0.35,0.35 0 0 1 0,0.35 0.35,0.35 0 0 1 0.35,0 0.35,0.35 0 0 1 0.7,0.35 Z";
    Geom::PathVector pathv = sp_svg_read_pathv(svgd);
    pathv *= Geom::Scale(r) * Geom::Translate(p - Geom::Point(0.35*r,0.35*r));
    hp.push_back(pathv[0]);
}

void LPEOffset::addKnotHolderEntities(KnotHolder *knotholder, SPItem *item)
{
    _knot_entity = new OfS::KnotHolderEntityOffsetPoint(this);
    _knot_entity->create(NULL, item, knotholder, Inkscape::CTRL_TYPE_UNKNOWN, _("Offset point"), SP_KNOT_SHAPE_CIRCLE);
    knotholder->add(_knot_entity);
}


namespace OfS {

void KnotHolderEntityOffsetPoint::knot_set(Geom::Point const &p, Geom::Point const& /*origin*/, guint state)
{
    using namespace Geom;
    LPEOffset* lpe = dynamic_cast<LPEOffset *>(_effect);
    Geom::Point s = snap_knot_position(p, state);
    previous = s;
    inset = false;
    int winding_value = lpe->original_pathv.winding(s); 
    if (winding_value % 2 != 0) {
        inset = true;
    }

    lpe->offset_pt = s;
    double offset = 0;
    Geom::Point origin = lpe->offset_pt;
    sp_set_origin(lpe->original_pathv, origin);
    offset = Geom::distance(origin, lpe->offset_pt);
    if (inset) {
        offset *= -1;
    }
    lpe->offset.param_set_value(offset);
    if (lpe->update_on_knot_move) {
        sp_lpe_item_update_patheffect (SP_LPE_ITEM(item), false, false);
    }
}

Geom::Point KnotHolderEntityOffsetPoint::knot_get() const
{
    LPEOffset const * lpe = dynamic_cast<LPEOffset const*> (_effect);
    return lpe->offset_pt;
}

} // namespace OfS
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
