#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_COLOR_BUTTON_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_COLOR_BUTTON_H

/*
 * Inkscape::LivePathEffectParameters
 *
 * Authors:
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
#include <glib.h>
#include "live_effects/parameter/parameter.h"

namespace Inkscape {

namespace LivePathEffect {

class ColorPickerParam : public Parameter {
public:
    ColorPickerParam( const Glib::ustring& label,
               const Glib::ustring& tip,
               const Glib::ustring& key,
               Inkscape::UI::Widget::Registry* wr,
               Effect* effect,
               const guint32 default_color = 0x000000ff);
    virtual ~ColorPickerParam() {}

    virtual Gtk::Widget * param_newWidget();
    virtual bool param_readSVGValue(const gchar * strvalue);
    void param_update_default(const gchar * default_value);
    virtual gchar * param_getSVGValue() const;
    virtual gchar * param_getDefaultSVGValue() const;

    void param_setValue(guint32 newvalue);
    
    virtual void param_set_default();

    const guint32 get_value() const { return value; };

private:
    ColorPickerParam(const ColorPickerParam&);
    ColorPickerParam& operator=(const ColorPickerParam&);
    guint32 value;
    guint32 defvalue;
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
