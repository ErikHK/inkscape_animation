#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_MESSAGE_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_MESSAGE_H

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

class MessageParam : public Parameter {
public:
    MessageParam( const Glib::ustring& label,
               const Glib::ustring& tip,
               const Glib::ustring& key,
               Inkscape::UI::Widget::Registry* wr,
               Effect* effect,
               const gchar * default_message = "Default message");
    virtual ~MessageParam() {}

    virtual Gtk::Widget * param_newWidget();
    virtual bool param_readSVGValue(const gchar * strvalue);
    void param_update_default(const gchar * default_value);
    virtual gchar * param_getSVGValue() const;
    virtual gchar * param_getDefaultSVGValue() const;

    void param_setValue(const gchar * message);
    
    virtual void param_set_default();
    void param_set_min_height(int height);
    const gchar *  get_value() const { return message; };

private:
    Gtk::Label * _label;
    int _min_height;
    MessageParam(const MessageParam&);
    MessageParam& operator=(const MessageParam&);
    const gchar *  message;
    const gchar *  defmessage;
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
