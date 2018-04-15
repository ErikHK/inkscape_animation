#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_TEXT_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_TEXT_H

/*
 * Inkscape::LivePathEffectParameters
 *
 * Authors:
 *   Maximilian Albert
 *   Johan Engelen
 *
 * Copyright (C) Maximilian Albert 2008 <maximilian.albert@gmail.com>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <glib.h>

#include "display/canvas-bpath.h"
#include "live_effects/parameter/parameter.h"

struct SPCanvasText;

namespace Inkscape {

namespace LivePathEffect {

class TextParam : public Parameter {
public:
    TextParam( const Glib::ustring& label,
               const Glib::ustring& tip,
               const Glib::ustring& key,
               Inkscape::UI::Widget::Registry* wr,
               Effect* effect,
               const Glib::ustring default_value = "");
    virtual ~TextParam() {}

    virtual Gtk::Widget * param_newWidget();

    virtual bool param_readSVGValue(const gchar * strvalue);
    virtual gchar * param_getSVGValue() const;
    virtual gchar * param_getDefaultSVGValue() const;

    void param_setValue(Glib::ustring newvalue);
    void param_hide_canvas_text();
    void setTextParam(Inkscape::UI::Widget::RegisteredText *rsu);
    virtual void param_set_default();
    virtual void param_update_default(const gchar * default_value);
    void setPos(Geom::Point pos);
    void setPosAndAnchor(const Geom::Piecewise<Geom::D2<Geom::SBasis> > &pwd2,
			 const double t, const double length, bool use_curvature = false);
    void setAnchor(double x_value, double y_value);

    const Glib::ustring get_value() const { return value; };

private:
    TextParam(const TextParam&);
    TextParam& operator=(const TextParam&);
    double anchor_x;
    double anchor_y;
    bool _hide_canvas_text;
    Glib::ustring value;
    Glib::ustring defvalue;

    SPCanvasText *canvas_text;
};

/*
 * This parameter does not display a widget in the LPE dialog; LPEs can use it to display on-canvas
 * text that should not be settable by the user. Note that since no widget is provided, the
 * parameter must be initialized differently than usual (only with a pointer to the parent effect;
 * no label, no tooltip, etc.).
 */
class TextParamInternal : public TextParam {
public:
    TextParamInternal(Effect* effect) :
        TextParam("", "", "", NULL, effect) {}

    virtual Gtk::Widget * param_newWidget() { return NULL; }
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
