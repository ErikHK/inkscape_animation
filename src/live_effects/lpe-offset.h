#ifndef INKSCAPE_LPE_OFFSET_H
#define INKSCAPE_LPE_OFFSET_H

/** \file
 * LPE <offset> implementation, see lpe-offset.cpp.
 */

/*
 * Authors:
 *   Maximilian Albert
 *   Jabiertxo Arraiza
 *
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 * Copyright (C) Maximilian Albert 2008 <maximilian.albert@gmail.com>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "live_effects/lpegroupbbox.h"
#include "live_effects/effect.h"
#include "live_effects/parameter/parameter.h"

namespace Inkscape {
namespace LivePathEffect {

namespace OfS {
// we need a separate namespace to avoid clashes with other LPEs
class KnotHolderEntityOffsetPoint;
}

class LPEOffset : public Effect, GroupBBoxEffect {
public:
    LPEOffset(LivePathEffectObject *lpeobject);
    virtual ~LPEOffset();
    virtual void doBeforeEffect (SPLPEItem const* lpeitem);
    virtual Geom::PathVector doEffect_path (Geom::PathVector const & path_in);
    virtual void doOnApply(SPLPEItem const* lpeitem);
    virtual Gtk::Widget *newWidget();
    void calculateOffset (Geom::PathVector const & path_in);
    void drawHandle(Geom::Point p);
    virtual void addKnotHolderEntities(KnotHolder * knotholder, SPItem * item);
    friend class OfS::KnotHolderEntityOffsetPoint;
protected:
    void addCanvasIndicators(SPLPEItem const */*lpeitem*/, std::vector<Geom::PathVector> &hp_vec);

private:
    ScalarParam offset;
    EnumParam<unsigned> linejoin_type;
    ScalarParam miter_limit;
    BoolParam attempt_force_join;
    BoolParam update_on_knot_move;
    Geom::PathVector hp;
    Geom::Point offset_pt;
    Geom::Point origin;
    bool evenodd;
    KnotHolderEntity * _knot_entity;
    Geom::PathVector original_pathv;
    Inkscape::UI::Widget::Scalar *offset_widget;

    LPEOffset(const LPEOffset&);
    LPEOffset& operator=(const LPEOffset&);
};

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
