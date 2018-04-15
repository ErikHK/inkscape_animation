/*
 * SVG dimensions implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Edward Flick (EAF)
 *   Abhishek Sharma
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 1999-2005 Authors
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "sp-dimensions.h"
#include "sp-item.h"

void SPDimensions::calcDimsFromParentViewport(const SPItemCtx *ictx, bool assign_to_set)
{
#define ASSIGN(field) { if (assign_to_set) { field._set = true; } }
    if (this->x.unit == SVGLength::PERCENT) {
        ASSIGN(x);
        this->x.computed = this->x.value * ictx->viewport.width();
    }

    if (this->y.unit == SVGLength::PERCENT) {
        ASSIGN(y);
        this->y.computed = this->y.value * ictx->viewport.height();
    }

    if (this->width.unit == SVGLength::PERCENT) {
        ASSIGN(width);
        this->width.computed = this->width.value * ictx->viewport.width();
    }

    if (this->height.unit == SVGLength::PERCENT) {
        ASSIGN(height);
        this->height.computed = this->height.value * ictx->viewport.height();
    }
}

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
