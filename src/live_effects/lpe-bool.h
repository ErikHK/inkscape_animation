/*
 * Boolean operation live path effect
 *
 * Copyright (C) 2016 Michael Soegtrop
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifndef INKSCAPE_LPE_BOOL_H
#define INKSCAPE_LPE_BOOL_H

#include "live_effects/effect.h"
#include "live_effects/parameter/parameter.h"
#include "live_effects/parameter/originalpath.h"
#include "live_effects/parameter/bool.h"
#include "live_effects/parameter/enum.h"
#include "livarot/LivarotDefs.h"

namespace Inkscape {
namespace LivePathEffect {

class LPEBool : public Effect {
public:
    LPEBool(LivePathEffectObject *lpeobject);
    virtual ~LPEBool();

    void doEffect(SPCurve *curve);
    virtual void resetDefaults(SPItem const *item);

    enum bool_op_ex {
        bool_op_ex_union     = bool_op_union,
        bool_op_ex_inters    = bool_op_inters,
        bool_op_ex_diff      = bool_op_diff,
        bool_op_ex_symdiff   = bool_op_symdiff,
        bool_op_ex_cut       = bool_op_cut,
        bool_op_ex_slice     = bool_op_slice,
        bool_op_ex_slice_inside,            // like bool_op_slice, but leaves only the contour pieces inside of the cut path
        bool_op_ex_slice_outside,           // like bool_op_slice, but leaves only the contour pieces outside of the cut path
        bool_op_ex_count
    };

    inline friend bool_op to_bool_op(bool_op_ex val)
    {
        assert(val <= bool_op_ex_slice);
        return (bool_op) val;
    }

private:
    LPEBool(const LPEBool &);
    LPEBool &operator=(const LPEBool &);

    OriginalPathParam operand_path;
    EnumParam<bool_op_ex> bool_operation;
    EnumParam<fill_typ> fill_type_this;
    EnumParam<fill_typ> fill_type_operand;
    BoolParam swap_operands;
    BoolParam rmv_inner;
};

}; //namespace LivePathEffect
}; //namespace Inkscape

#endif
