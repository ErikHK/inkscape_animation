/*
 * The reference corresponding to href of LPE Item parameter.
 *
 * Copyright (C) 2008 Johan Engelen
 *
 * Released under GNU GPL, read the file 'COPYING' for more information.
 */

#include "live_effects/parameter/item-reference.h"

#include "object/sp-shape.h"
#include "object/sp-text.h"
#include "object/sp-item-group.h"

namespace Inkscape {
namespace LivePathEffect {

bool ItemReference::_acceptObject(SPObject * const obj) const
{
    if (SP_IS_SHAPE(obj) || SP_IS_TEXT(obj) || SP_IS_GROUP(obj)) {
        /* Refuse references to lpeobject */
        if (obj == getOwner()) {
            return false;
        }
        // TODO: check whether the referred item has this LPE applied, if so: deny deny deny!
        return URIReference::_acceptObject(obj);
    } else {
        return false;
    }
}

} // namespace LivePathEffect
} // namespace Inkscape

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
