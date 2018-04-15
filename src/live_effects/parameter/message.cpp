/*
 * Authors:
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <gtkmm.h>
#include "live_effects/parameter/message.h"
#include "live_effects/effect.h"
#include <glibmm/i18n.h>

namespace Inkscape {

namespace LivePathEffect {

MessageParam::MessageParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect, const gchar * default_message )
    : Parameter(label, tip, key, wr, effect),
      message(default_message),
      defmessage(default_message)
{
    _label  = NULL;
    _min_height = -1;
}

void
MessageParam::param_set_default()
{
    param_setValue(defmessage);
}

void 
MessageParam::param_update_default(const gchar * default_message)
{
    defmessage = default_message;
}

bool
MessageParam::param_readSVGValue(const gchar * strvalue)
{
    param_setValue(strvalue);
    return true;
}

gchar *
MessageParam::param_getSVGValue() const
{
    return g_strdup(message);
}

gchar *
MessageParam::param_getDefaultSVGValue() const
{
    return g_strdup(defmessage);
}

void
MessageParam::param_set_min_height(int height)
{
    _min_height = height;
    if (_label) {
        _label->set_size_request(-1, _min_height);
    }
}


Gtk::Widget *
MessageParam::param_newWidget()
{
    Gtk::Frame * frame = new Gtk::Frame (param_label);
    Gtk::Widget * widg_frame = frame->get_label_widget();


    _label = new Gtk::Label (message, Gtk::ALIGN_END);
    _label->set_use_underline (true);
    _label->set_use_markup();
    _label->set_line_wrap(true);
    _label->set_size_request(-1, _min_height);
    Gtk::Widget* widg_label = dynamic_cast<Gtk::Widget *> (_label);

    frame->add(*widg_label);
    return dynamic_cast<Gtk::Widget *> (frame);
}

void
MessageParam::param_setValue(const gchar * strvalue)
{
    if (strcmp(strvalue, message) != 0) {
        param_effect->upd_params = true;
    }
    message = strvalue;
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
