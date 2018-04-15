/*
 * Copyright (C) jabiertxof 2017 <jabier.arraiza@marker.es>
 * Copyright (C) Maximilian Albert 2008 <maximilian.albert@gmail.com>
 *
 * Authors:
 *   Jabiertxof
 *   Maximilian Albert
 *   Johan Engelen
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */


#include "live_effects/parameter/hidden.h"
#include "live_effects/effect.h"
#include "svg/svg.h"
#include "svg/stringstream.h"

namespace Inkscape {

namespace LivePathEffect {

HiddenParam::HiddenParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect, const Glib::ustring default_value, bool is_visible)
    : Parameter(label, tip, key, wr, effect),
      value(default_value),
      defvalue(default_value)
{
    param_widget_is_visible(is_visible);
}

void
HiddenParam::param_set_default()
{
    param_setValue(defvalue);
}

void
HiddenParam::param_update_default(const gchar * default_value)
{
    defvalue = (Glib::ustring)default_value;
}


bool
HiddenParam::param_readSVGValue(const gchar * strvalue)
{
    param_setValue(strvalue);
    return true;
}

gchar *
HiddenParam::param_getSVGValue() const
{
    Inkscape::SVGOStringStream os;
    os << value;
    return g_strdup(os.str().c_str());
}

gchar *
HiddenParam::param_getDefaultSVGValue() const
{
    Inkscape::SVGOStringStream os;
    os << defvalue;
    return g_strdup(os.str().c_str());
}

Gtk::Widget *
HiddenParam::param_newWidget()
{
    return NULL;
}

void
HiddenParam::param_setValue(const Glib::ustring newvalue, bool write)
{
    value = newvalue;
    if (write) {
        param_write_to_repr(value.c_str());
    }
}

} /* namespace LivePathEffect */

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
