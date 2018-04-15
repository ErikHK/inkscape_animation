#ifndef INKSCAPE_LPE_DASH_STROKE_H
#define INKSCAPE_LPE_DASH_STROKE_H

/*
 * Inkscape::LPEDashStroke
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "live_effects/effect.h"
#include "live_effects/parameter/message.h"

namespace Inkscape {
namespace LivePathEffect {

class LPEDashStroke : public Effect {
public:
    LPEDashStroke(LivePathEffectObject *lpeobject);
    virtual ~LPEDashStroke();
    virtual void doBeforeEffect (SPLPEItem const* lpeitem);
    virtual Geom::PathVector doEffect_path (Geom::PathVector const & path_in);
    double timeAtLength(double const A, Geom::Path const &segment);
    double timeAtLength(double const A, Geom::Piecewise<Geom::D2<Geom::SBasis> > pwd2);
private:
    ScalarParam numberdashes;
    ScalarParam holefactor;
    BoolParam splitsegments;
    BoolParam halfextreme;
    BoolParam unifysegment;
    MessageParam message;
};

} //namespace LivePathEffect
} //namespace Inkscape
#endif
