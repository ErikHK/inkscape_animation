/*
 * Authors:
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <gtkmm.h>
#include "ui/widget/registered-widget.h"
#include "live_effects/parameter/fontbutton.h"
#include "live_effects/effect.h"
#include "ui/widget/font-button.h"
#include "svg/svg.h"
#include "svg/stringstream.h"
#include "verbs.h"

#include <glibmm/i18n.h>

namespace Inkscape {

namespace LivePathEffect {

FontButtonParam::FontButtonParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect, const Glib::ustring default_value )
    : Parameter(label, tip, key, wr, effect),
      value(default_value),
      defvalue(default_value)
{
}

void
FontButtonParam::param_set_default()
{
    param_setValue(defvalue);
}

void 
FontButtonParam::param_update_default(const gchar * default_value)
{
    defvalue = (Glib::ustring)strdup(default_value);
}

bool
FontButtonParam::param_readSVGValue(const gchar * strvalue)
{
    Inkscape::SVGOStringStream os;
    os << strvalue;
    param_setValue((Glib::ustring)os.str());
    return true;
}

gchar *
FontButtonParam::param_getSVGValue() const
{
    return g_strdup(value.c_str());
}

gchar *
FontButtonParam::param_getDefaultSVGValue() const
{
    return g_strdup(defvalue.c_str());
}



Gtk::Widget *
FontButtonParam::param_newWidget()
{
    Inkscape::UI::Widget::RegisteredFontButton * fontbuttonwdg = Gtk::manage(
        new Inkscape::UI::Widget::RegisteredFontButton( param_label,
                                                        param_tooltip,
                                                        param_key,
                                                        *param_wr,
                                                        param_effect->getRepr(),
                                                        param_effect->getSPDoc() ) );
    Glib::ustring fontspec = param_getSVGValue();
    fontbuttonwdg->setValue( fontspec);
    fontbuttonwdg->set_undo_parameters(SP_VERB_DIALOG_LIVE_PATH_EFFECT, _("Change font button parameter"));
    return dynamic_cast<Gtk::Widget *> (fontbuttonwdg);
}

void
FontButtonParam::param_setValue(const Glib::ustring newvalue)
{
    if (value != newvalue) {
        param_effect->upd_params = true;
    }
    value = newvalue;
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
