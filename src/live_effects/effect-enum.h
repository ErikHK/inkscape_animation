#ifndef INKSCAPE_LIVEPATHEFFECT_ENUM_H
#define INKSCAPE_LIVEPATHEFFECT_ENUM_H

/*
 * Inkscape::LivePathEffect::EffectType
 *
* Copyright (C) Johan Engelen 2008 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "util/enums.h"

namespace Inkscape {
namespace LivePathEffect {

//Please fill in the same order than in effect.cpp:98
enum EffectType {
    BEND_PATH = 0,
    GEARS,
    PATTERN_ALONG_PATH,
    CURVE_STITCH,
    VONKOCH,
    KNOT,
    CONSTRUCT_GRID,
    SPIRO,
    ENVELOPE,
    INTERPOLATE,
    ROUGH_HATCHES,
    SKETCH,
    RULER,
    POWERSTROKE,
    CLONE_ORIGINAL,
    SIMPLIFY,
    LATTICE2,
    PERSPECTIVE_ENVELOPE,
    INTERPOLATE_POINTS,
    TRANSFORM_2PTS,
    SHOW_HANDLES,
    ROUGHEN,
    BSPLINE,
    JOIN_TYPE,
    TAPER_STROKE,
    MIRROR_SYMMETRY,
    COPY_ROTATE,
    ATTACH_PATH,
    FILL_BETWEEN_STROKES,
    FILL_BETWEEN_MANY,
    ELLIPSE_5PTS,
    BOUNDING_BOX,
    MEASURE_SEGMENTS,
    FILLET_CHAMFER,
    BOOL_OP,
    EMBRODERY_STITCH,
    POWERCLIP,
    POWERMASK,
    PTS2ELLIPSE,
    OFFSET,
    DASH_STROKE,
    DOEFFECTSTACK_TEST,
    ANGLE_BISECTOR,
    CIRCLE_WITH_RADIUS,
    CIRCLE_3PTS,
    DYNASTROKE,
    EXTRUDE,
    LATTICE,
    LINE_SEGMENT,
    PARALLEL,
    PATH_LENGTH,
    PERP_BISECTOR,
    PERSPECTIVE_PATH,
    RECURSIVE_SKELETON,
    TANGENT_TO_CURVE,
    TEXT_LABEL,
    INVALID_LPE // This must be last (I made it such that it is not needed anymore I think..., Don't trust on it being last. - johan)
};

extern const Util::EnumData<EffectType> LPETypeData[];  /// defined in effect.cpp
extern const Util::EnumDataConverter<EffectType> LPETypeConverter; /// defined in effect.cpp

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
