#ifndef SEEN_SP_LIVAROT_H
#define SEEN_SP_LIVAROT_H

/*
 * boolops and outlines
 *
 * public domain
 */

#include <2geom/forward.h>
#include <2geom/path.h>
#include "livarot/Path.h"
#include "object/object-set.h"  // bool_op

class SPCurve;
class SPDesktop;
class SPItem;

namespace Inkscape {
    class Selection;
    class ObjectSet;
}

// offset/inset of a curve
// takes the fill-rule in consideration
// offset amount is the stroke-width of the curve
void sp_selected_path_offset (SPDesktop *desktop);
void sp_selected_path_offset_screen (SPDesktop *desktop, double pixels);
void sp_selected_path_inset (SPDesktop *desktop);
void sp_selected_path_inset_screen (SPDesktop *desktop, double pixels);
void sp_selected_path_create_offset (SPDesktop *desktop);
void sp_selected_path_create_inset (SPDesktop *desktop);
void sp_selected_path_create_updating_offset (SPDesktop *desktop);
void sp_selected_path_create_updating_inset (SPDesktop *desktop);

void sp_selected_path_create_offset_object_zero (SPDesktop *desktop);
void sp_selected_path_create_updating_offset_object_zero (SPDesktop *desktop);

// outline of a curve
// uses the stroke-width
void sp_selected_path_outline (SPDesktop *desktop, bool legacy = false);
bool sp_item_path_outline(SPItem *item, SPDesktop *desktop, bool legacy);
Geom::PathVector* item_outline(SPItem const *item, bool bbox_only = false);

// simplifies a path (removes small segments and the like)
void sp_selected_path_simplify (SPDesktop *desktop);

Path *Path_for_pathvector(Geom::PathVector const &pathv);
Path *Path_for_item(SPItem *item, bool doTransformation, bool transformFull = true);
Path *Path_for_item_before_LPE(SPItem *item, bool doTransformation, bool transformFull = true);
Geom::PathVector* pathvector_for_curve(SPItem *item, SPCurve *curve, bool doTransformation, bool transformFull, Geom::Affine extraPreAffine, Geom::Affine extraPostAffine);
SPCurve *curve_for_item(SPItem *item);
SPCurve *curve_for_item_before_LPE(SPItem *item);
boost::optional<Path::cut_position> get_nearest_position_on_Path(Path *path, Geom::Point p, unsigned seg = 0);
Geom::Point get_point_on_Path(Path *path, int piece, double t);
Geom::PathVector sp_pathvector_boolop(Geom::PathVector const &pathva, Geom::PathVector const &pathvb, bool_op bop, FillRule fra, FillRule frb);

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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
