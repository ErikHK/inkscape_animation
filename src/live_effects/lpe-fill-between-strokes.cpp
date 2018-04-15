/*
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
#include "live_effects/lpe-fill-between-strokes.h"

#include "display/curve.h"
#include "svg/svg.h"

// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>

namespace Inkscape {
namespace LivePathEffect {

LPEFillBetweenStrokes::LPEFillBetweenStrokes(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    linked_path(_("Linked path:"), _("Path from which to take the original path data"), "linkedpath", &wr, this),
    second_path(_("Second path:"), _("Second path from which to take the original path data"), "secondpath", &wr, this),
    reverse_second(_("Reverse Second"), _("Reverses the second path order"), "reversesecond", &wr, this),
    fuse(_("Fuse coincident points"), _("Fuse coincident points"), "fuse", &wr, this, false),
    allow_transforms(_("Allow transforms"), _("Allow transforms"), "allow_transforms", &wr, this, false),
    join(_("Join subpaths"), _("Join subpaths"), "join", &wr, this, true),
    close(_("Close"), _("Close path"), "close", &wr, this, true)
{
    registerParameter(&linked_path);
    registerParameter(&second_path);
    registerParameter(&reverse_second);
    registerParameter(&fuse);
    registerParameter(&allow_transforms);
    registerParameter(&join);
    registerParameter(&close);
    transformmultiply = false;
}

LPEFillBetweenStrokes::~LPEFillBetweenStrokes()
{

}

void LPEFillBetweenStrokes::doEffect (SPCurve * curve)
{
    if (curve) {
        Geom::Affine affine = Geom::identity();
        if(!allow_transforms && !transformmultiply) {
            sp_svg_transform_read(SP_ITEM(sp_lpe_item)->getAttribute("transform"), &affine);
        }
        if(transformmultiply) {
            transformmultiply = false;
        }
        if ( linked_path.linksToPath() && second_path.linksToPath() && linked_path.getObject() && second_path.getObject() ) {
            Geom::PathVector linked_pathv = linked_path.get_pathvector();
            Geom::PathVector second_pathv = second_path.get_pathvector();
            Geom::PathVector result_linked_pathv;
            Geom::PathVector result_second_pathv;
            Geom::Affine second_transform = second_path.getObject()->getRelativeTransform(linked_path.getObject());

            for (Geom::PathVector::iterator iter = linked_pathv.begin(); iter != linked_pathv.end(); ++iter)
            {
                result_linked_pathv.push_back((*iter));
            }
            for (Geom::PathVector::iterator iter = second_pathv.begin(); iter != second_pathv.end(); ++iter)
            {
                result_second_pathv.push_back((*iter) * second_transform);
            }

            if ( !result_linked_pathv.empty() && !result_second_pathv.empty() && !result_linked_pathv.front().closed() ) {
                if (reverse_second.get_value()) {
                    result_second_pathv.front() = result_second_pathv.front().reversed();
                }
                if (join) {
                    if (!are_near(result_linked_pathv.front().finalPoint(), result_second_pathv.front().initialPoint(),0.01) || !fuse) {
                        result_linked_pathv.front().appendNew<Geom::LineSegment>(result_second_pathv.front().initialPoint());
                    } else {
                        result_second_pathv.front().setInitial(result_linked_pathv.front().finalPoint());
                    }
                    result_linked_pathv.front().append(result_second_pathv.front());
                    if (close) {
                        result_linked_pathv.front().close();
                    }
                } else {
                    if (close) {
                        result_linked_pathv.front().close();
                        result_second_pathv.front().close();
                    }
                    result_linked_pathv.push_back(result_second_pathv.front());
                }
                result_linked_pathv *= affine.inverse();
                curve->set_pathvector(result_linked_pathv);
            } else if ( !result_linked_pathv.empty() ) {
                result_linked_pathv *= affine.inverse();
                curve->set_pathvector(result_linked_pathv);
            } else if ( !result_second_pathv.empty() ) {
                result_second_pathv *= affine.inverse();
                curve->set_pathvector(result_second_pathv);
            }
        }
        else if ( linked_path.linksToPath() && linked_path.getObject() ) {
            Geom::PathVector linked_pathv = linked_path.get_pathvector();
            Geom::PathVector result_pathv;

            for (Geom::PathVector::iterator iter = linked_pathv.begin(); iter != linked_pathv.end(); ++iter)
            {
                result_pathv.push_back((*iter));
            }
            if ( !result_pathv.empty() ) {
                result_pathv *= affine.inverse();
                if (close) {
                    result_pathv.front().close();
                }
                curve->set_pathvector(result_pathv);
            }
        }
        else if ( second_path.linksToPath() && second_path.getObject() ) {
            Geom::PathVector second_pathv = second_path.get_pathvector();
            Geom::PathVector result_pathv;

            for (Geom::PathVector::iterator iter = second_pathv.begin(); iter != second_pathv.end(); ++iter)
            {
                result_pathv.push_back((*iter));
            }
            if ( !result_pathv.empty() ) {
                result_pathv *= affine.inverse();
                if (close) {
                    result_pathv.front().close();
                }
                curve->set_pathvector(result_pathv);
            }
        }
    }
}

void
LPEFillBetweenStrokes::transform_multiply(Geom::Affine const& postmul, bool set)
{
    if(!allow_transforms && sp_lpe_item) {
        SP_ITEM(sp_lpe_item)->transform *= postmul.inverse();
        transformmultiply = true;
        sp_lpe_item_update_patheffect(sp_lpe_item, false, false);
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
