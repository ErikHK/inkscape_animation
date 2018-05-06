/*
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <gtkmm/box.h>

#include "live_effects/lpe-fill-between-many.h"

#include "display/curve.h"
#include "sp-item.h"
#include "2geom/path.h"
#include "sp-shape.h"
#include "sp-text.h"
#include "2geom/bezier-curve.h"

#include <glibmm/i18n.h>

namespace Inkscape {
namespace LivePathEffect {

LPEFillBetweenMany::LPEFillBetweenMany(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    linked_paths(_("Linked path:"), _("Paths from which to take the original path data"), "linkedpaths", &wr, this),
    allow_transforms(_("Allow transforms"), _("Allow transforms"), "allow_transforms", &wr, this, false)
{
    registerParameter(&linked_paths);
    registerParameter(&allow_transforms);
    //perceived_path = true;
    linked_paths.allowOnlyBsplineSpiro(true);
}

LPEFillBetweenMany::~LPEFillBetweenMany()
{

}

void LPEFillBetweenMany::doEffect (SPCurve * curve)
{
    Geom::PathVector res_pathv;
    SPItem * firstObj = NULL;
    for (std::vector<PathAndDirection*>::iterator iter = linked_paths._vector.begin(); iter != linked_paths._vector.end(); ++iter) {
        SPObject *obj;
        if ((*iter)->ref.isAttached() && (obj = (*iter)->ref.getObject()) && SP_IS_ITEM(obj) && !(*iter)->_pathvector.empty()) {
            Geom::Path linked_path;
            if ((*iter)->reversed) {
                linked_path = (*iter)->_pathvector.front().reversed();
            } else {
                linked_path = (*iter)->_pathvector.front();
            }
            
            if (!res_pathv.empty()) {
                if(!allow_transforms) {
                    Geom::Affine affine = Geom::identity();
                    sp_svg_transform_read(SP_ITEM(obj)->getAttribute("transform"), &affine);
                    linked_path *= affine;
                }
                res_pathv.front().appendNew<Geom::LineSegment>(linked_path.initialPoint());
                res_pathv.front().append(linked_path);
            } else {
                firstObj = SP_ITEM(obj);
                if(!allow_transforms) {
                    Geom::Affine affine = Geom::identity();
                    sp_svg_transform_read(SP_ITEM(obj)->getAttribute("transform"), &affine);
                    linked_path *= affine;
                }
                res_pathv.push_back(linked_path);
            }
        }
    }
    
    if(!allow_transforms && sp_lpe_item) {
        SP_ITEM(sp_lpe_item)->transform = Geom::identity();
    }
    
    if (!res_pathv.empty()) {
        res_pathv.front().close();
    }
    
    if (res_pathv.empty()) {
        res_pathv = curve->get_pathvector();
    }

    curve->set_pathvector(res_pathv);
}

void
LPEFillBetweenMany::transform_multiply(Geom::Affine const& postmul, bool set)
{
    if(!allow_transforms && sp_lpe_item) {
        SP_ITEM(sp_lpe_item)->transform *= postmul.inverse();
    }
}

} // namespace LivePathEffect
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
