/*
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "ui/widget/registered-widget.h"
#include "live_effects/parameter/bool.h"
#include "live_effects/effect.h"
#include "svg/svg.h"
#include "svg/stringstream.h"
#include "inkscape.h"
#include "verbs.h"
#include "helper-fns.h"
#include <glibmm/i18n.h>

namespace Inkscape {

namespace LivePathEffect {

BoolParam::BoolParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect, bool default_value)
    : Parameter(label, tip, key, wr, effect), value(default_value), defvalue(default_value)
{
}

BoolParam::~BoolParam()
{
}

void
BoolParam::param_set_default()
{
    param_setValue(defvalue);
}

void 
BoolParam::param_update_default(bool const default_value)
{
    defvalue = default_value;
}

void 
BoolParam::param_update_default(const gchar * default_value)
{
    param_update_default(helperfns_read_bool(default_value, defvalue));
}

bool
BoolParam::param_readSVGValue(const gchar * strvalue)
{
    param_setValue(helperfns_read_bool(strvalue, defvalue));
    return true; // not correct: if value is unacceptable, should return false!
}

gchar *
BoolParam::param_getSVGValue() const
{
    return g_strdup(value ? "true" : "false");
}

gchar *
BoolParam::param_getDefaultSVGValue() const
{
    return g_strdup(defvalue ? "true" : "false");
}

Gtk::Widget *
BoolParam::param_newWidget()
{
    if(widget_is_visible){
        Inkscape::UI::Widget::RegisteredCheckButton * checkwdg = Gtk::manage(
            new Inkscape::UI::Widget::RegisteredCheckButton( param_label,
                                                             param_tooltip,
                                                             param_key,
                                                             *param_wr,
                                                             false,
                                                             param_effect->getRepr(),
                                                             param_effect->getSPDoc()) );

        checkwdg->setActive(value);
        checkwdg->setProgrammatically = false;
        checkwdg->set_undo_parameters(SP_VERB_DIALOG_LIVE_PATH_EFFECT, _("Change bool parameter"));
        return dynamic_cast<Gtk::Widget *> (checkwdg);
    } else {
        return NULL;
    }
}

void
BoolParam::param_setValue(bool newvalue)
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
