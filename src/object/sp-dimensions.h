#ifndef SP_DIMENSIONS_H__
#define SP_DIMENSIONS_H__

/*
 * dimensions helper class, common code used by root, image and others
 *
 * Authors:
 *  Shlomi Fish
 * Copyright (C) 2017 Shlomi Fish, authors
 *
 * Released under dual Expat and GNU GPL, read the file 'COPYING' for more information
 *
 */

#include "svg/svg-length.h"

class SPItemCtx;

class SPDimensions {

public:
    SVGLength x;
    SVGLength y;
    SVGLength width;
    SVGLength height;
    void calcDimsFromParentViewport(const SPItemCtx *ictx, bool assign_to_set = false);
};

#endif

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-basic-offset:2
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=2:tabstop=8:softtabstop=2:fileencoding=utf-8:textwidth=99 :
