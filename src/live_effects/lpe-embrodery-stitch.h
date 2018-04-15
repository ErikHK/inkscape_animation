/*
 *  Embroidery stitch live path effect
 *
 * Copyright (C) 2016 Michael Soegtrop
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifndef INKSCAPE_LPE_EMBRODERY_STITCH_H
#define INKSCAPE_LPE_EMBRODERY_STITCH_H

#include "live_effects/effect.h"
#include "live_effects/parameter/parameter.h"
#include "live_effects/parameter/bool.h"
#include "live_effects/parameter/enum.h"
#include "live_effects/lpe-embrodery-stitch-ordering.h"

namespace Inkscape {
namespace LivePathEffect {

using namespace LPEEmbroderyStitchOrdering;

class LPEEmbroderyStitch : public Effect {
public:

    LPEEmbroderyStitch(LivePathEffectObject *lpeobject);
    virtual ~LPEEmbroderyStitch();

    virtual Geom::PathVector doEffect_path(Geom::PathVector const &path_in);

    virtual void resetDefaults(SPItem const *item);

    virtual void transform_multiply(Geom::Affine const &postmul, bool set);

    enum order_method {
        order_method_no_reorder,
        order_method_zigzag,
        order_method_zigzag_rev_first,
        order_method_closest,
        order_method_closest_rev_first,
        order_method_tsp_kopt_2,
        order_method_tsp_kopt_3,
        order_method_tsp_kopt_4,
        order_method_tsp_kopt_5,
        order_method_count
    };
    enum connect_method {
        connect_method_line,
        connect_method_move_point_from,
        connect_method_move_point_mid,
        connect_method_move_point_to,
        connect_method_count
    };

private:
    EnumParam<order_method> ordering;
    EnumParam<connect_method> connection;
    ScalarParam stitch_length;
    ScalarParam stitch_min_length;
    ScalarParam stitch_pattern;
    BoolParam show_stitches;
    ScalarParam show_stitch_gap;
    ScalarParam jump_if_longer;

    LPEEmbroderyStitch(const LPEEmbroderyStitch &);
    LPEEmbroderyStitch &operator=(const LPEEmbroderyStitch &);

    double GetPatternInitialStep(int pattern, int line);
    Geom::Point GetStartPointInterpolAfterRev(std::vector<OrderingInfo> const &info, unsigned i);
    Geom::Point GetEndPointInterpolAfterRev(std::vector<OrderingInfo> const &info, unsigned i);
    Geom::Point GetStartPointInterpolBeforeRev(std::vector<OrderingInfo> const &info, unsigned i);
    Geom::Point GetEndPointInterpolBeforeRev(std::vector<OrderingInfo> const &info, unsigned i);
};

} //namespace LivePathEffect
} //namespace Inkscape

#endif
