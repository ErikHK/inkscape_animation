#ifndef INKSCAPE_LPE_MEASURE_SEGMENTS_H
#define INKSCAPE_LPE_MEASURE_SEGMENTS_H

/*
 * Author(s):
 *     Jabiertxo Arraiza Cenoz <jabier.arraiza@marker.es>
 *
 * Copyright (C) 2014 Author(s)
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "live_effects/effect.h"
#include "live_effects/lpeobject.h"
#include "live_effects/lpeobject-reference.h"
#include "live_effects/parameter/bool.h"
#include "live_effects/parameter/colorpicker.h"
#include "live_effects/parameter/enum.h"
#include "live_effects/parameter/fontbutton.h"
#include "live_effects/parameter/message.h"
#include "live_effects/parameter/text.h"
#include "live_effects/parameter/unit.h"


namespace Inkscape {
namespace LivePathEffect {

enum OrientationMethod {
    OM_HORIZONTAL,
    OM_VERTICAL,
    OM_PARALLEL,
    OM_END
};

class LPEMeasureSegments : public Effect {
public:
    LPEMeasureSegments(LivePathEffectObject *lpeobject);
    virtual ~LPEMeasureSegments();
    virtual void doOnApply(SPLPEItem const* lpeitem);
    virtual void doBeforeEffect (SPLPEItem const* lpeitem);
    virtual void doOnRemove(SPLPEItem const* /*lpeitem*/);
    virtual void doEffect(SPCurve * curve){}; //stop the chain
    virtual void doOnVisibilityToggled(SPLPEItem const* /*lpeitem*/);
    virtual void transform_multiply(Geom::Affine const& postmul, bool set);
    void createLine(Geom::Point start,Geom::Point end, Glib::ustring name, size_t counter, bool main, bool remove, bool arrows = false);
    void createTextLabel(Geom::Point pos, size_t counter, double length, Geom::Coord angle, bool remove, bool valid);
    void createArrowMarker(Glib::ustring mode);
    bool hasMeassure (size_t i);
private:
    UnitParam unit;
    EnumParam<OrientationMethod> orientation;
    ColorPickerParam coloropacity;
    FontButtonParam fontbutton;
    ScalarParam precision;
    ScalarParam fix_overlaps;
    ScalarParam position;
    ScalarParam text_top_bottom;
    ScalarParam helpline_distance;
    ScalarParam helpline_overlap;
    ScalarParam line_width;
    ScalarParam scale;
    TextParam format;
    TextParam blacklist;
    BoolParam whitelist;
    BoolParam arrows_outside;
    BoolParam flip_side;
    BoolParam scale_sensitive;
    BoolParam local_locale;
    BoolParam rotate_anotation;
    BoolParam hide_back;
    MessageParam message;
    Glib::ustring display_unit;
    double doc_scale;
    double fontsize;
    double anotation_width;
    double previous_size;
    unsigned rgb24;
    double arrow_gap;
    gchar const* locale_base;
    Geom::Affine star_ellipse_fix;
    LPEMeasureSegments(const LPEMeasureSegments &);
    LPEMeasureSegments &operator=(const LPEMeasureSegments &);

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
