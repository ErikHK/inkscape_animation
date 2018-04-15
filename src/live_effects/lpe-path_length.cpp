/** \file
 * LPE <path_length> implementation.
 */
/*
 * Authors:
 *   Maximilian Albert <maximilian.albert@gmail.com>
 *   Johan Engelen
 *
 * Copyright (C) 2007-2008 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "live_effects/lpe-path_length.h"
#include "util/units.h"
// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>

namespace Inkscape {
namespace LivePathEffect {

LPEPathLength::LPEPathLength(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    scale(_("Scale:"), _("Scaling factor"), "scale", &wr, this, 1.0),
    info_text(this),
    unit(_("Unit:"), _("Unit"), "unit", &wr, this),
    display_unit(_("Display unit"), _("Print unit after path length"), "display_unit", &wr, this, true)
{
    registerParameter(&scale);
    registerParameter(&info_text);
    registerParameter(&unit);
    registerParameter(&display_unit);
}

LPEPathLength::~LPEPathLength()
{

}

Geom::Piecewise<Geom::D2<Geom::SBasis> >
LPEPathLength::doEffect_pwd2 (Geom::Piecewise<Geom::D2<Geom::SBasis> > const & pwd2_in)
{
    using namespace Geom;

    /* convert the measured length to the correct unit ... */
    double lengthval = Geom::length(pwd2_in) * scale;
    lengthval = Inkscape::Util::Quantity::convert(lengthval, "px", unit.get_abbreviation());

    /* ... set it as the canvas text ... */
    gchar *arc_length = g_strdup_printf("%.2f %s", lengthval,
                                        display_unit ? unit.get_abbreviation() : "");
    info_text.param_setValue(arc_length);
    g_free(arc_length);

    info_text.setPosAndAnchor(pwd2_in, 0.5, 10);

    // TODO: how can we compute the area (such that cw turns don't count negative)?
    //       should we display the area here, too, or write a new LPE for this?
    Piecewise<D2<SBasis> > A = integral(pwd2_in);
    Point c;
    double area;
    if (centroid(pwd2_in, c, area)) {
        //g_print ("Area is zero\n");
    }
    //g_print ("Area: %f\n", area);
    if (!this->isVisible()) {
        info_text.param_setValue("");
    }
    return pwd2_in;
}

} //namespace LivePathEffect
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
