#ifndef INKSCAPE_LPE_FILLET_CHAMFER_H
#define INKSCAPE_LPE_FILLET_CHAMFER_H

/*
 * Author(s):
 *     Jabiertxo Arraiza Cenoz <jabier.arraiza@marker.es>
 *
 * Copyright (C) 2014 Author(s)
 *
 * Jabiertxof:Thanks to all people help me
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "live_effects/parameter/enum.h"
#include "live_effects/parameter/satellitesarray.h"
#include "live_effects/effect.h"
#include "live_effects/parameter/unit.h"
#include "live_effects/parameter/hidden.h"
#include "helper/geom-pathvectorsatellites.h"
#include "helper/geom-satellite.h"

namespace Inkscape {
namespace LivePathEffect {

enum Filletmethod {
    FM_AUTO,
    FM_ARC,
    FM_BEZIER,
    FM_END
};

class LPEFilletChamfer : public Effect {
public:
    LPEFilletChamfer(LivePathEffectObject *lpeobject);
    virtual void doBeforeEffect(SPLPEItem const *lpeItem);
    virtual Geom::PathVector doEffect_path(Geom::PathVector const &path_in);
    virtual void doOnApply(SPLPEItem const *lpeItem);
    virtual Gtk::Widget *newWidget();
    Geom::Ray getRay(Geom::Point start, Geom::Point end, Geom::Curve *curve, bool reverse);
    void addChamferSteps(Geom::Path &tmp_path, Geom::Path path_chamfer, Geom::Point end_arc_point, size_t steps);
    void addCanvasIndicators(SPLPEItem const */*lpeitem*/, std::vector<Geom::PathVector> &hp_vec);
    void updateSatelliteType(SatelliteType satellitetype);
    void setSelected(PathVectorSatellites *_pathvector_satellites);
    //void convertUnit();
    void updateChamferSteps();
    void updateAmount();
    void refreshKnots();

    SatellitesArrayParam satellites_param;

private:
    UnitParam unit;
    EnumParam<Filletmethod> method;
    ScalarParam radius;
    ScalarParam chamfer_steps;
    BoolParam flexible;
    HiddenParam mode;
    BoolParam mirror_knots;
    BoolParam only_selected;
    BoolParam use_knot_distance;
    BoolParam hide_knots;
    BoolParam apply_no_radius;
    BoolParam apply_with_radius;
    ScalarParam helper_size;
    bool _degenerate_hide;
    PathVectorSatellites *_pathvector_satellites;
    Geom::PathVector _hp;

    LPEFilletChamfer(const LPEFilletChamfer &);
    LPEFilletChamfer &operator=(const LPEFilletChamfer &);

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
