/*
 * Authors:
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <gtkmm.h>
#include "ui/widget/registered-widget.h"
#include "live_effects/parameter/colorpicker.h"
#include "live_effects/effect.h"
#include "ui/widget/color-picker.h"
#include "svg/svg.h"
#include "svg/svg-color.h"
#include "color.h"
#include "inkscape.h"
#include "svg/stringstream.h"
#include "verbs.h"
#include "document.h"
#include "document-undo.h"

#include <glibmm/i18n.h>

namespace Inkscape {

namespace LivePathEffect {

ColorPickerParam::ColorPickerParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect, const guint32 default_color )
    : Parameter(label, tip, key, wr, effect),
      value(default_color),
      defvalue(default_color)
{

}

void
ColorPickerParam::param_set_default()
{
    param_setValue(defvalue);
}

static guint32 sp_read_color_alpha(gchar const *str, guint32 def)
{
    guint32 val = 0;
    if (str == NULL) return def;
    while ((*str <= ' ') && *str) str++;
    if (!*str) return def;

    if (str[0] == '#') {
        gint i;
        for (i = 1; str[i]; i++) {
            int hexval;
            if (str[i] >= '0' && str[i] <= '9')
                hexval = str[i] - '0';
            else if (str[i] >= 'A' && str[i] <= 'F')
                hexval = str[i] - 'A' + 10;
            else if (str[i] >= 'a' && str[i] <= 'f')
                hexval = str[i] - 'a' + 10;
            else
                break;
            val = (val << 4) + hexval;
        }
        if (i != 1 + 8) {
            return def;
        }
    }
    return val;
}

void 
ColorPickerParam::param_update_default(const gchar * default_value)
{
    defvalue = sp_read_color_alpha(default_value, 0x000000ff);
}

bool
ColorPickerParam::param_readSVGValue(const gchar * strvalue)
{
    param_setValue(sp_read_color_alpha(strvalue, 0x000000ff));
    return true;
}

gchar *
ColorPickerParam::param_getSVGValue() const
{
    gchar c[32];
    sprintf(c, "#%08x", value);
    return strdup(c);
}

gchar *
ColorPickerParam::param_getDefaultSVGValue() const
{
    gchar c[32];
    sprintf(c, "#%08x", defvalue);
    return strdup(c);
}

Gtk::Widget *
ColorPickerParam::param_newWidget()
{
    Gtk::HBox *hbox = Gtk::manage(new Gtk::HBox());

    hbox->set_border_width(5);
    hbox->set_homogeneous(false);
    hbox->set_spacing(2);
    Inkscape::UI::Widget::RegisteredColorPicker * colorpickerwdg = Gtk::manage(
        new Inkscape::UI::Widget::RegisteredColorPicker( param_label,
                                                         param_label,
                                                         param_tooltip,
                                                         param_key,
                                                         param_key + "_opacity_LPE",
                                                        *param_wr,
                                                         param_effect->getRepr(),
                                                         param_effect->getSPDoc() ) );
    Gtk::Label * label = new Gtk::Label (param_label, Gtk::ALIGN_END);
    label->set_use_underline (true);
    label->set_mnemonic_widget (*colorpickerwdg);
    SPDocument *document = SP_ACTIVE_DOCUMENT;
    bool saved = DocumentUndo::getUndoSensitive(document);
    DocumentUndo::setUndoSensitive(document, false);
    colorpickerwdg->setRgba32(value);
    DocumentUndo::setUndoSensitive(document, saved);
    colorpickerwdg->set_undo_parameters(SP_VERB_DIALOG_LIVE_PATH_EFFECT, _("Change color button parameter"));
    hbox->pack_start(*dynamic_cast<Gtk::Widget *> (label), true, true);
    hbox->pack_start(*dynamic_cast<Gtk::Widget *> (colorpickerwdg), true, true);
    return dynamic_cast<Gtk::Widget *> (hbox);
}

void
ColorPickerParam::param_setValue(const guint32 newvalue)
{
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
