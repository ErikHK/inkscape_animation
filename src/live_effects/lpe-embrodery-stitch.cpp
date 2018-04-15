/*
 * Embroidery stitch live path effect (Implementation)
 *
 * Copyright (C) 2016 Michael Soegtrop
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "ui/widget/scalar.h"
#include <glibmm/i18n.h>

#include "live_effects/lpe-embrodery-stitch.h"
#include "live_effects/lpe-embrodery-stitch-ordering.h"

#include <2geom/path.h>
#include <2geom/piecewise.h>
#include <2geom/sbasis.h>
#include <2geom/sbasis-geometric.h>
#include <2geom/bezier-to-sbasis.h>
#include <2geom/sbasis-to-bezier.h>

namespace Inkscape {
namespace LivePathEffect {

using namespace Geom;
using namespace LPEEmbroderyStitchOrdering;

static const Util::EnumData<LPEEmbroderyStitch::order_method> OrderMethodData[LPEEmbroderyStitch::order_method_count] = {
    { LPEEmbroderyStitch::order_method_no_reorder, N_("no reordering"),                                "no-reorder" },
    { LPEEmbroderyStitch::order_method_zigzag,            N_("zig-zag"),                               "zig-zag" },
    { LPEEmbroderyStitch::order_method_zigzag_rev_first,  N_("zig-zag, reverse first"),                "zig-zag-rev-first" },
    { LPEEmbroderyStitch::order_method_closest,           N_("closest"),                               "closest" },
    { LPEEmbroderyStitch::order_method_closest_rev_first, N_("closest, reverse first"),                "closest-rev-first" },
    { LPEEmbroderyStitch::order_method_tsp_kopt_2,        N_("traveling salesman 2-opt (fast, bad)"),  "tsp-2opt" },
    { LPEEmbroderyStitch::order_method_tsp_kopt_3,        N_("traveling salesman 3-opt (fast, ok)"),   "tsp-3opt" },
    { LPEEmbroderyStitch::order_method_tsp_kopt_4,        N_("traveling salesman 4-opt (seconds)"),    "tsp-4opt" },
    { LPEEmbroderyStitch::order_method_tsp_kopt_5,        N_("traveling salesman 5-opt (minutes)"),    "tsp-5opt" }
};

static const Util::EnumDataConverter<LPEEmbroderyStitch::order_method>
OrderMethodConverter(OrderMethodData, sizeof(OrderMethodData) / sizeof(*OrderMethodData));

static const Util::EnumData<LPEEmbroderyStitch::connect_method> ConnectMethodData[LPEEmbroderyStitch::connect_method_count] = {
    { LPEEmbroderyStitch::connect_method_line, N_("straight line"), "line" },
    { LPEEmbroderyStitch::connect_method_move_point_from, N_("move to begin"), "move-begin" },
    { LPEEmbroderyStitch::connect_method_move_point_mid, N_("move to middle"), "move-middle" },
    { LPEEmbroderyStitch::connect_method_move_point_to, N_("move to end"), "move-end" }
};

static const Util::EnumDataConverter<LPEEmbroderyStitch::connect_method>
ConnectMethodConverter(ConnectMethodData, sizeof(ConnectMethodData) / sizeof(*ConnectMethodData));

LPEEmbroderyStitch::LPEEmbroderyStitch(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    ordering(_("Ordering method"), _("Method used to order sub paths"), "ordering", OrderMethodConverter, &wr, this, order_method_no_reorder),
    connection(_("Connection method"), _("Method to connect end points of sub paths"), "connection", ConnectMethodConverter, &wr, this, connect_method_line),
    stitch_length(_("Stitch length"), _("If not 0, linearize path with given step length"), "stitch-length", &wr, this, 10.0),
    stitch_min_length(_("Minimum stitch length [%]"), _("Combine steps shorter than this [%]"), "stitch-min-length", &wr, this, 25.0),
    stitch_pattern(_("Stitch pattern"), _("Select between different stitch patterns"), "stitch_pattern", &wr, this, 0),
    show_stitches(_("Show stitches"), _("Show stitches as small gaps (just for inspection - don't use for output)"), "show-stitches", &wr, this, false),
    show_stitch_gap(_("Show stitch gap"), _("Gap between stitches when showing stitches"), "show-stitch-gap", &wr, this, 0.5),
    jump_if_longer(_("Jump if longer"), _("Jump connection if longer than"), "jump-if-longer", &wr, this, 100)
{
    registerParameter(dynamic_cast<Parameter *>(&ordering));
    registerParameter(dynamic_cast<Parameter *>(&connection));
    registerParameter(dynamic_cast<Parameter *>(&stitch_length));
    registerParameter(dynamic_cast<Parameter *>(&stitch_min_length));
    registerParameter(dynamic_cast<Parameter *>(&stitch_pattern));
    registerParameter(dynamic_cast<Parameter *>(&show_stitches));
    registerParameter(dynamic_cast<Parameter *>(&show_stitch_gap));
    registerParameter(dynamic_cast<Parameter *>(&jump_if_longer));

    stitch_length.param_set_digits(1);
    stitch_length.param_set_range(1, 10000);
    stitch_min_length.param_set_digits(1);
    stitch_min_length.param_set_range(0, 100);
    stitch_pattern.param_make_integer();
    stitch_pattern.param_set_range(0, 2);
    show_stitch_gap.param_set_range(0.001, 10);
    jump_if_longer.param_set_range(0.0, 1000000);
}

LPEEmbroderyStitch::~LPEEmbroderyStitch()
{

}

double LPEEmbroderyStitch::GetPatternInitialStep(int pattern, int line)
{
    switch (pattern) {
    case 0:
        return 0.0;

    case 1:
        switch (line % 4) {
        case 0:
            return 0.0;
        case 1:
            return 0.25;
        case 2:
            return 0.50;
        case 3:
            return 0.75;
        }
        return 0.0;

    case 2:
        switch (line % 4) {
        case 0:
            return 0.0;
        case 1:
            return 0.5;
        case 2:
            return 0.75;
        case 3:
            return 0.25;
        }
        return 0.0;

    default:
        return 0.0;
    }

}

Point LPEEmbroderyStitch::GetStartPointInterpolAfterRev(std::vector<OrderingInfo> const &info, unsigned i)
{
    Point start_this = info[i].GetBegRev();

    if (i == 0) {
        return start_this;
    }

    if (!info[i - 1].connect) {
        return start_this;
    }

    Point end_prev = info[i - 1].GetEndRev();

    switch (connection.get_value()) {
    case connect_method_line:
        return start_this;
    case connect_method_move_point_from:
        return end_prev;
    case connect_method_move_point_mid:
        return 0.5 * start_this + 0.5 * end_prev;
    case connect_method_move_point_to:
        return start_this;
    default:
        return start_this;
    }
}
Point LPEEmbroderyStitch::GetEndPointInterpolAfterRev(std::vector<OrderingInfo> const &info, unsigned i)
{
    Point end_this = info[i].GetEndRev();

    if (i + 1 == info.size()) {
        return end_this;
    }

    if (!info[i].connect) {
        return end_this;
    }

    Point start_next = info[i + 1].GetBegRev();

    switch (connection.get_value()) {
    case connect_method_line:
        return end_this;
    case connect_method_move_point_from:
        return end_this;
    case connect_method_move_point_mid:
        return 0.5 * start_next + 0.5 * end_this;
    case connect_method_move_point_to:
        return start_next;
    default:
        return end_this;
    }
}

Point LPEEmbroderyStitch::GetStartPointInterpolBeforeRev(std::vector<OrderingInfo> const &info, unsigned i)
{
    if (info[i].reverse) {
        return GetEndPointInterpolAfterRev(info, i);
    } else {
        return GetStartPointInterpolAfterRev(info, i);
    }
}

Point LPEEmbroderyStitch::GetEndPointInterpolBeforeRev(std::vector<OrderingInfo> const &info, unsigned i)
{
    if (info[i].reverse) {
        return GetStartPointInterpolAfterRev(info, i);
    } else {
        return GetEndPointInterpolAfterRev(info, i);
    }
}

PathVector LPEEmbroderyStitch::doEffect_path(PathVector const &path_in)
{
    if (path_in.size() >= 2) {
        PathVector path_out;

        // Create vectors with start and end points
        std::vector<OrderingInfo> orderinginfos(path_in.size());
        // connect next path to this one
        bool connect_with_previous = false;

        for (PathVector::const_iterator it = path_in.begin(); it != path_in.end(); ++it) {
            OrderingInfo &info = orderinginfos[ it - path_in.begin() ];
            info.index = it - path_in.begin();
            info.reverse = false;
            info.used = false;
            info.connect = true;
            info.begOrig = it->front().initialPoint();
            info.endOrig = it->back().finalPoint();
        }

        // Compute sub-path ordering
        switch (ordering.get_value()) {
        case order_method_no_reorder:
            OrderingOriginal(orderinginfos);
            break;

        case order_method_zigzag:
            OrderingZigZag(orderinginfos, false);
            break;

        case order_method_zigzag_rev_first:
            OrderingZigZag(orderinginfos, true);
            break;

        case order_method_closest:
            OrderingClosest(orderinginfos, false);
            break;

        case order_method_closest_rev_first:
            OrderingClosest(orderinginfos, true);
            break;

        case order_method_tsp_kopt_2:
            OrderingAdvanced(orderinginfos, 2);
            break;

        case order_method_tsp_kopt_3:
            OrderingAdvanced(orderinginfos, 3);
            break;

        case order_method_tsp_kopt_4:
            OrderingAdvanced(orderinginfos, 4);
            break;

        case order_method_tsp_kopt_5:
            OrderingAdvanced(orderinginfos, 5);
            break;

        }

        // Iterate over sub-paths in order found above
        // Divide paths into stitches (currently always equidistant)
        // Interpolate between neighboring paths, so that their ends coincide
        for (std::vector<OrderingInfo>::const_iterator it = orderinginfos.begin(); it != orderinginfos.end(); ++it) {
            // info index
            unsigned iInfo = it - orderinginfos.begin();
            // subpath index
            unsigned iPath = it->index;
            // decide of path shall be reversed
            bool reverse = it->reverse;
            // minimum stitch length in absolute measure
            double stitch_min_length_abs = stitch_min_length * 0.01 * stitch_length;

            // convert path to piecewise
            Piecewise<D2<SBasis> > pwOrig = path_in[iPath].toPwSb();
            // make piecewise equidistant in time
            Piecewise<D2<SBasis> > pwEqdist = arc_length_parametrization(pwOrig);
            Piecewise<D2<SBasis> > pwStitch;

            // cut into stitches
            double cutpos = 0.0;
            Interval pwdomain = pwEqdist.domain();

            // step length of first stitch
            double step = GetPatternInitialStep(stitch_pattern, iInfo) * stitch_length;
            if (step < stitch_min_length_abs) {
                step += stitch_length;
            }

            bool last = false;
            bool first = true;
            double posnext;
            for (double pos = pwdomain.min(); !last; pos = posnext, cutpos += 1.0) {
                // start point
                Point p1;
                if (first) {
                    p1 = GetStartPointInterpolBeforeRev(orderinginfos, iInfo);
                    first = false;
                } else {
                    p1 =  pwEqdist.valueAt(pos);
                }

                // end point of this stitch
                Point p2;
                posnext = pos + step;
                // last stitch is to end
                if (posnext >= pwdomain.max() - stitch_min_length_abs) {
                    p2 = GetEndPointInterpolBeforeRev(orderinginfos, iInfo);
                    last = true;
                } else {
                    p2 = pwEqdist.valueAt(posnext);
                }

                pwStitch.push_cut(cutpos);
                pwStitch.push_seg(D2<SBasis>(SBasis(Linear(p1[X], p2[X])), SBasis(Linear(p1[Y], p2[Y]))));

                // stitch length for all except first step
                step = stitch_length;
            }
            pwStitch.push_cut(cutpos);

            if (reverse) {
                pwStitch = Geom::reverse(pwStitch);
            }

            if (it->connect && iInfo != orderinginfos.size() - 1) {
                // Connect this segment with the previous segment by a straight line
                Point end = pwStitch.lastValue();
                Point start_next = GetStartPointInterpolAfterRev(orderinginfos, iInfo + 1);
                // connect end and start point
                if (end != start_next && distance(end, start_next) <= jump_if_longer) {
                    cutpos += 1.0;
                    pwStitch.push_seg(D2<SBasis>(SBasis(Linear(end[X], start_next[X])), SBasis(Linear(end[Y], start_next[Y]))));
                    pwStitch.push_cut(cutpos);
                }
            }

            if (show_stitches) {
                for (std::vector< D2<SBasis> >::iterator it = pwStitch.segs.begin(); it != pwStitch.segs.end(); ++it) {
                    // Create  anew piecewise with just one segment
                    Piecewise<D2<SBasis> > pwOne;
                    pwOne.push_cut(0);
                    pwOne.push_seg(*it);
                    pwOne.push_cut(1);

                    // make piecewise equidistant in time
                    Piecewise<D2<SBasis> > pwOneEqdist = arc_length_parametrization(pwOne);
                    Interval pwdomain = pwOneEqdist.domain();

                    // Compute the points of the shortened piece
                    Coord len = pwdomain.max() - pwdomain.min();
                    Coord offs = 0.5 * (show_stitch_gap < 0.5 * len ? show_stitch_gap : 0.5 * len);
                    Point p1 = pwOneEqdist.valueAt(pwdomain.min() + offs);
                    Point p2 = pwOneEqdist.valueAt(pwdomain.max() - offs);
                    Piecewise<D2<SBasis> > pwOneGap;

                    // Create Linear SBasis
                    D2<SBasis> sbasis = D2<SBasis>(SBasis(Linear(p1[X], p2[X])), SBasis(Linear(p1[Y], p2[Y])));

                    // Convert to path and add to path list
                    Geom::Path path = path_from_sbasis(sbasis , LPE_CONVERSION_TOLERANCE, false);
                    path_out.push_back(path);
                }
            } else {
                PathVector pathv = path_from_piecewise(pwStitch, LPE_CONVERSION_TOLERANCE);
                for (size_t ipv = 0; ipv < pathv.size(); ipv++) {
                    if (connect_with_previous) {
                        path_out.back().append(pathv[ipv]);
                    } else {
                        path_out.push_back(pathv[ipv]);
                    }
                }
            }

            connect_with_previous = it->connect;
        }

        return path_out;
    } else {
        return path_in;
    }
}

void
LPEEmbroderyStitch::resetDefaults(SPItem const *item)
{
    Effect::resetDefaults(item);
}


/** /todo check whether this special case is necessary. It seems to "bug" editing behavior:
 * scaling an object with transforms preserved behaves differently from scaling with
 * transforms optimized (difference caused by this special method).
 * special casing is probably needed, because rotation should not be propagated to the strokepath.
 */
void
LPEEmbroderyStitch::transform_multiply(Affine const &postmul, bool set)
{
}

} //namespace LivePathEffect
} /* namespace Inkscape */
