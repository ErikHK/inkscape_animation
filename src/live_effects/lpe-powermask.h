#ifndef INKSCAPE_LPE_POWERMASK_H
#define INKSCAPE_LPE_POWERMASK_H

/*
 * Inkscape::LPEPowerMask
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
#include "live_effects/effect.h"
#include "live_effects/parameter/bool.h"
#include "live_effects/parameter/text.h"
#include "live_effects/parameter/hidden.h"
#include "live_effects/parameter/colorpicker.h"

namespace Inkscape {
namespace LivePathEffect {

class LPEPowerMask : public Effect {
public:
    LPEPowerMask(LivePathEffectObject *lpeobject);
    virtual ~LPEPowerMask();
    virtual void doBeforeEffect (SPLPEItem const* lpeitem);
    virtual void doEffect (SPCurve * curve);
    virtual void doOnRemove (SPLPEItem const* /*lpeitem*/);
    virtual void doOnVisibilityToggled(SPLPEItem const* lpeitem);
    void toggleMaskVisibility();
    void setMask();
private:
    HiddenParam uri;
    BoolParam invert;
    //BoolParam wrap;
    BoolParam hide_mask;
    BoolParam background;
    ColorPickerParam background_color;
    Geom::Path mask_box;
    guint32 previous_color;
};

void sp_inverse_powermask(Inkscape::Selection *sel);

} //namespace LivePathEffect
} //namespace Inkscape

#endif
