#ifndef SEEN_INKSCAPE_EXTENSION_INTERNAL_FILTER_PROTRUSIONS_H__
#define SEEN_INKSCAPE_EXTENSION_INTERNAL_FILTER_PROTRUSIONS_H__
/* Change the 'PROTRUSIONS' above to be your file name */

/*
 * Copyright (C) 2008 Authors:
 *   Ted Gould <ted@gould.cx>
 * Copyright (C) 2011 Authors:
 *   Ivan Louette (filters)
 *   Nicolas Dufour (UI) <nicoduf@yahoo.fr>
 *
 * Protrusion filters
 *   Snow
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
/* ^^^ Change the copyright to be you and your e-mail address ^^^ */

#include "filter.h"

#include "extension/internal/clear-n_.h"
#include "extension/system.h"
#include "extension/extension.h"

namespace Inkscape {
namespace Extension {
namespace Internal {
namespace Filter {


/**
    \brief    Custom predefined Snow filter.
    
    Snow has fallen on object.
*/
class Snow : public Inkscape::Extension::Internal::Filter::Filter {
protected:
	virtual gchar const * get_filter_text (Inkscape::Extension::Extension * ext);

public:
	Snow ( ) : Filter() { };
	virtual ~Snow ( ) { if (_filter != NULL) g_free((void *)_filter); return; }

public:
	static void init (void) {
		Inkscape::Extension::build_from_mem(
			"<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
				"<name>" N_("Snow Crest") "</name>\n"
				"<id>org.inkscape.effect.filter.snow</id>\n"
				"<param name=\"drift\" _gui-text=\"" N_("Drift Size") "\" type=\"float\" appearance=\"full\" min=\"0.0\" max=\"20.0\">3.5</param>\n"
				"<effect>\n"
					"<object-type>all</object-type>\n"
					"<effects-menu>\n"
						"<submenu name=\"" N_("Filters") "\">\n"
   						"<submenu name=\"Protrusions\"/>\n"
			      "</submenu>\n"
					"</effects-menu>\n"
					"<menu-tip>" N_("Snow has fallen on object") "</menu-tip>\n"
				"</effect>\n"
			"</inkscape-extension>\n", new Snow());
	};

};

gchar const *
Snow::get_filter_text (Inkscape::Extension::Extension * ext)
{
	if (_filter != NULL) g_free((void *)_filter);

        std::ostringstream drift;
        drift << ext->get_param_float("drift");

	_filter = g_strdup_printf(
				"<filter xmlns:inkscape=\"http://www.inkscape.org/namespaces/inkscape\" style=\"color-interpolation-filters:sRGB;\" inkscape:label=\"Snow\">\n"
					"<feConvolveMatrix order=\"3 3\" kernelMatrix=\"1 1 1 0 0 0 -1 -1 -1\" preserveAlpha=\"false\" divisor=\"3\"/>\n"
					"<feMorphology operator=\"dilate\" radius=\"1 %s\"/>\n"
					"<feGaussianBlur stdDeviation=\"1.6270889487870621\" result=\"result0\"/>\n"
					"<feColorMatrix values=\"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 10 0\" result=\"result1\"/>\n"
					"<feOffset dx=\"0\" dy=\"1\" result=\"result5\"/>\n"
					"<feDiffuseLighting in=\"result0\" diffuseConstant=\"2.2613065326633168\" surfaceScale=\"1\">\n"
					  "<feDistantLight azimuth=\"225\" elevation=\"32\"/>\n"
					"</feDiffuseLighting>\n"
					"<feComposite in2=\"result1\" operator=\"in\" result=\"result2\"/>\n"
					"<feColorMatrix values=\"0.4 0 0 0 0.6 0 0.4 0 0 0.6 0 0 0 0 1 0 0 0 1 0\" result=\"result4\"/>\n"
					"<feComposite in2=\"result5\" in=\"result4\"/>\n"
					"<feComposite in2=\"SourceGraphic\"/>\n"
				"</filter>\n", drift.str().c_str());

	return _filter;
}; /* Snow filter */


}; /* namespace Filter */
}; /* namespace Internal */
}; /* namespace Extension */
}; /* namespace Inkscape */

/* Change the 'PROTRUSIONS' below to be your file name */
#endif /* SEEN_INKSCAPE_EXTENSION_INTERNAL_FILTER_PROTRUSIONS_H__ */
