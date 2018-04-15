/** @file
 * Parameters for extensions.
 */
/* Author:
 *   Ted Gould <ted@gould.cx>
 *   Johan Engelen <johan@shouraizou.nl>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2005-2007 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef linux  // does the dollar sign need escaping when passed as string parameter?
# define ESCAPE_DOLLAR_COMMANDLINE
#endif

#include <cstring>

#include "ui/widget/color-notebook.h"
#include <xml/node.h>

#include <extension/extension.h>
#include "document-private.h"
#include "object/sp-defs.h"
#include <color.h>

#include "parameter.h"
#include "bool.h"
#include "color.h"
#include "description.h"
#include "enum.h"
#include "float.h"
#include "int.h"
#include "notebook.h"
#include "radiobutton.h"
#include "string.h"

#include <glibmm/i18n.h>

namespace Inkscape {
namespace Extension {

Parameter *Parameter::make(Inkscape::XML::Node *in_repr, Inkscape::Extension::Extension *in_ext)
{
    const char *name = in_repr->attribute("name");
    const char *type = in_repr->attribute("type");

    // we can't create a parameter without type
    if (!type) {
        return NULL;
    }
    // also require name unless it's a pure UI element that does not store its value
    if (!name) {
        static std::vector<std::string> ui_elements = {"description"};
        if (std::find(ui_elements.begin(), ui_elements.end(), type) == ui_elements.end()) {
            return NULL;
        }
    }

    const char *text = in_repr->attribute("gui-text");
    if (text == NULL) {
        text = in_repr->attribute("_gui-text");
        if (text == NULL) {
            // text = ""; // propably better to require devs to explicitly set an empty gui-text if this is what they want
        } else {
            const char *context = in_repr->attribute("msgctxt");
            if (context != NULL) {
                text = g_dpgettext2(NULL, context, text);
            } else {
                text = _(text);
            }
        }
    }
    const char *description = in_repr->attribute("gui-description");
    if (description == NULL) {
        description = in_repr->attribute("_gui-description");
        if (description != NULL) {
            const char *context = in_repr->attribute("msgctxt");
            if (context != NULL) {
                description = g_dpgettext2(NULL, context, description);
            } else {
                description = _(description);
            }
        }
    }
    bool hidden = false;
    {
        const char *gui_hide = in_repr->attribute("gui-hidden");
        if (gui_hide != NULL) {
            if (strcmp(gui_hide, "1") == 0 ||
                strcmp(gui_hide, "true") == 0) {
                hidden = true;
            }
            /* else stays false */
        }
    }
    int indent = 0;
    {
        const char *indent_attr = in_repr->attribute("indent");
        if (indent_attr != NULL) {
            if (strcmp(indent_attr, "true") == 0) {
                indent = 1;
            } else {
                indent = atoi(indent_attr);
            }
        }
    }
    const gchar* appearance = in_repr->attribute("appearance");

    Parameter * param = NULL;
    if (!strcmp(type, "boolean")) {
        param = new ParamBool(name, text, description, hidden, indent, in_ext, in_repr);
    } else if (!strcmp(type, "int")) {
        if (appearance && !strcmp(appearance, "full")) {
            param = new ParamInt(name, text, description, hidden, indent, in_ext, in_repr, ParamInt::FULL);
        } else {
            param = new ParamInt(name, text, description, hidden, indent, in_ext, in_repr, ParamInt::MINIMAL);
        }
    } else if (!strcmp(type, "float")) {
        if (appearance && !strcmp(appearance, "full")) {
            param = new ParamFloat(name, text, description, hidden, indent, in_ext, in_repr, ParamFloat::FULL);
        } else {
            param = new ParamFloat(name, text, description, hidden, indent, in_ext, in_repr, ParamFloat::MINIMAL);
        }
    } else if (!strcmp(type, "string")) {
        param = new ParamString(name, text, description, hidden, indent, in_ext, in_repr);
        gchar const * max_length = in_repr->attribute("max_length");
        if (max_length != NULL) {
            ParamString * ps = dynamic_cast<ParamString *>(param);
            ps->setMaxLength(atoi(max_length));
        }
    } else if (!strcmp(type, "description")) {
        ParamDescription::AppearanceMode appearance_mode = ParamDescription::DESCRIPTION;
        if (appearance) {
            if (!strcmp(appearance, "header")) {
                appearance_mode = ParamDescription::HEADER;
            } else if (!strcmp(appearance, "url")) {
                appearance_mode = ParamDescription::URL;
            }
        }
        param = new ParamDescription(name, text, description, hidden, indent, in_ext, in_repr, appearance_mode);
    } else if (!strcmp(type, "enum")) {
        param = new ParamComboBox(name, text, description, hidden, indent, in_ext, in_repr);
    } else if (!strcmp(type, "notebook")) {
        param = new ParamNotebook(name, text, description, hidden, indent, in_ext, in_repr);
    } else if (!strcmp(type, "optiongroup")) {
        if (appearance && !strcmp(appearance, "minimal")) {
            param = new ParamRadioButton(name, text, description, hidden, indent, in_ext, in_repr, ParamRadioButton::MINIMAL);
        } else {
            param = new ParamRadioButton(name, text, description, hidden, indent, in_ext, in_repr, ParamRadioButton::FULL);
        }
    } else if (!strcmp(type, "color")) {
        param = new ParamColor(name, text, description, hidden, indent, in_ext, in_repr);
    }

    // Note: param could equal NULL
    return param;
}

bool Parameter::get_bool(SPDocument const *doc, Inkscape::XML::Node const *node) const
{
    ParamBool const *boolpntr = dynamic_cast<ParamBool const *>(this);
    if (!boolpntr) {
        throw Extension::param_not_bool_param();
    }
    return boolpntr->get(doc, node);
}

int Parameter::get_int(SPDocument const *doc, Inkscape::XML::Node const *node) const
{
    ParamInt const *intpntr = dynamic_cast<ParamInt const *>(this);
    if (!intpntr) {
        throw Extension::param_not_int_param();
    }
    return intpntr->get(doc, node);
}

float Parameter::get_float(SPDocument const *doc, Inkscape::XML::Node const *node) const
{
    ParamFloat const *floatpntr = dynamic_cast<ParamFloat const *>(this);
    if (!floatpntr) {
        throw Extension::param_not_float_param();
    }
    return floatpntr->get(doc, node);
}

gchar const *Parameter::get_string(SPDocument const *doc, Inkscape::XML::Node const *node) const
{
    ParamString const *stringpntr = dynamic_cast<ParamString const *>(this);
    if (!stringpntr) {
        throw Extension::param_not_string_param();
    }
    return stringpntr->get(doc, node);
}

gchar const *Parameter::get_enum(SPDocument const *doc, Inkscape::XML::Node const *node) const
{
    ParamComboBox const *param = dynamic_cast<ParamComboBox const *>(this);
    if (!param) {
        throw Extension::param_not_enum_param();
    }
    return param->get(doc, node);
}

bool Parameter::get_enum_contains(gchar const * value, SPDocument const *doc, Inkscape::XML::Node const *node) const
{
    ParamComboBox const *param = dynamic_cast<ParamComboBox const *>(this);
    if (!param) {
        throw Extension::param_not_enum_param();
    }
    return param->contains(value, doc, node);
}

gchar const *Parameter::get_optiongroup(SPDocument const *doc, Inkscape::XML::Node const * node) const
{
    ParamRadioButton const *param = dynamic_cast<ParamRadioButton const *>(this);
    if (!param) {
        throw Extension::param_not_optiongroup_param();
    }
    return param->get(doc, node);
}

guint32 Parameter::get_color(const SPDocument* doc, Inkscape::XML::Node const *node) const
{
    ParamColor const *param = dynamic_cast<ParamColor const *>(this);
    if (!param) {
        throw Extension::param_not_color_param();
    }
    return param->get(doc, node);
}

bool Parameter::set_bool(bool in, SPDocument * doc, Inkscape::XML::Node * node)
{
    ParamBool * boolpntr = dynamic_cast<ParamBool *>(this);
    if (boolpntr == NULL)
        throw Extension::param_not_bool_param();
    return boolpntr->set(in, doc, node);
}

int Parameter::set_int(int in, SPDocument * doc, Inkscape::XML::Node * node)
{
    ParamInt * intpntr = dynamic_cast<ParamInt *>(this);
    if (intpntr == NULL)
        throw Extension::param_not_int_param();
    return intpntr->set(in, doc, node);
}

/** Wrapper to cast to the object and use it's function. */
float
Parameter::set_float (float in, SPDocument * doc, Inkscape::XML::Node * node)
{
    ParamFloat * floatpntr;
    floatpntr = dynamic_cast<ParamFloat *>(this);
    if (floatpntr == NULL)
        throw Extension::param_not_float_param();
    return floatpntr->set(in, doc, node);
}

/** Wrapper to cast to the object and use it's function. */
gchar const *
Parameter::set_string (gchar const * in, SPDocument * doc, Inkscape::XML::Node * node)
{
    ParamString * stringpntr = dynamic_cast<ParamString *>(this);
    if (stringpntr == NULL)
        throw Extension::param_not_string_param();
    return stringpntr->set(in, doc, node);
}

gchar const * Parameter::set_optiongroup( gchar const * in, SPDocument * doc, Inkscape::XML::Node * node )
{
    ParamRadioButton *param = dynamic_cast<ParamRadioButton *>(this);
    if (!param) {
        throw Extension::param_not_optiongroup_param();
    }
    return param->set(in, doc, node);
}

gchar const *Parameter::set_enum( gchar const * in, SPDocument * doc, Inkscape::XML::Node * node )
{
    ParamComboBox *param = dynamic_cast<ParamComboBox *>(this);
    if (!param) {
        throw Extension::param_not_enum_param();
    }
    return param->set(in, doc, node);
}


/** Wrapper to cast to the object and use it's function. */
guint32
Parameter::set_color (guint32 in, SPDocument * doc, Inkscape::XML::Node * node)
{
    ParamColor* param = dynamic_cast<ParamColor *>(this);
    if (param == NULL)
        throw Extension::param_not_color_param();
    return param->set(in, doc, node);
}


/** Oop, now that we need a parameter, we need it's name. */
Parameter::Parameter(gchar const * name, gchar const * text, gchar const * description, bool hidden, int indent, Inkscape::Extension::Extension * ext) :
    _description(0),
    _text(0),
    _hidden(hidden),
    _indent(indent),
    _extension(ext),
    _name(0)
{
    if (name != NULL) {
        _name = g_strdup(name);
    }

    if (description != NULL) {
        _description = g_strdup(description);
    }

    if (text != NULL) {
        _text = g_strdup(text);
    } else {
        _text = g_strdup(name);
    }

    return;
}

/** Oop, now that we need a parameter, we need it's name. */
Parameter::Parameter (gchar const * name, gchar const * text, Inkscape::Extension::Extension * ext) :
    _description(0),
    _text(0),
    _hidden(false),
    _indent(0),
    _extension(ext),
    _name(0)
{
    if (name != NULL) {
        _name = g_strdup(name);
    }
    if (text != NULL) {
        _text = g_strdup(text);
    } else {
        _text = g_strdup(name);
    }

    return;
}

Parameter::~Parameter(void)
{
    g_free(_name);
    _name = 0;

    g_free(_text);
    _text = 0;

    g_free(_description);
    _description = 0;
}

gchar *Parameter::pref_name(void) const
{
    return g_strdup_printf("%s.%s", _extension->get_id(), _name);
}

Inkscape::XML::Node *
Parameter::find_child (Inkscape::XML::Node * adult)
{
    return sp_repr_lookup_child(adult, "name", _name);
}

Inkscape::XML::Node *
Parameter::new_child (Inkscape::XML::Node * parent)
{
    Inkscape::XML::Node * retval;
    retval = parent->document()->createElement("inkscape:extension-param");
    retval->setAttribute("name", _name);

    parent->appendChild(retval);
    Inkscape::GC::release(retval);
    return retval;
}

Inkscape::XML::Node *Parameter::document_param_node(SPDocument * doc)
{
    Inkscape::XML::Document *xml_doc = doc->getReprDoc();
    Inkscape::XML::Node * defs = doc->getDefs()->getRepr();
    Inkscape::XML::Node * params = NULL;

    GQuark const name_quark = g_quark_from_string("inkscape:extension-params");

    for (Inkscape::XML::Node * child = defs->firstChild();
            child != NULL;
            child = child->next()) {
        if ((GQuark)child->code() == name_quark &&
                !strcmp(child->attribute("extension"), _extension->get_id())) {
            params = child;
            break;
        }
    }

    if (params == NULL) {
        params = xml_doc->createElement("inkscape:extension-param");
        params->setAttribute("extension", _extension->get_id());
        defs->appendChild(params);
        Inkscape::GC::release(params);
    }

    return params;
}

/** Basically, if there is no widget pass a NULL. */
Gtk::Widget *
Parameter::get_widget (SPDocument * /*doc*/, Inkscape::XML::Node * /*node*/, sigc::signal<void> * /*changeSignal*/)
{
    return NULL;
}

/** If I'm not sure which it is, just don't return a value. */
void Parameter::string(std::string &/*string*/) const
{
    // TODO investigate clearing the target string.
}

void Parameter::string(std::list <std::string> &list) const
{
    std::string value;
    string(value);
    if (!value.empty()) {
        std::string final;
        final += "--";
        final += name();
        final += "=";
        final += value;

        list.insert(list.end(), final);
    }
}

Parameter *Parameter::get_param(gchar const * /*name*/)
{
    return NULL;
}

Glib::ustring const extension_pref_root = "/extensions/";

}  // namespace Extension
}  // namespace Inkscape

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
