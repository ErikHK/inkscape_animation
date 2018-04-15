/*
 * SVG <script> implementation
 *
 * Authors:
 *   Felipe Corrêa da Silva Sanches <juca@members.fsf.org>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2008 authors
 *
 * Released under GNU GPL version 2 or later, read the file 'COPYING' for more information
 */

#include "sp-script.h"
#include "attributes.h"

SPScript::SPScript() : SPObject() {
	this->xlinkhref = NULL;
}

SPScript::~SPScript() {
}

void SPScript::build(SPDocument* doc, Inkscape::XML::Node* repr) {
    SPObject::build(doc, repr);

    //Read values of key attributes from XML nodes into object.
    this->readAttr( "xlink:href" );

    doc->addResource("script", this);
}

/**
 * Reads the Inkscape::XML::Node, and initializes SPScript variables.  For this to get called,
 * our name must be associated with a repr via "sp_object_type_register".  Best done through
 * sp-object-repr.cpp's repr_name_entries array.
 */

void SPScript::release() {
    if (this->document) {
        // Unregister ourselves
        this->document->removeResource("script", this);
    }

    SPObject::release();
}

void SPScript::update(SPCtx* /*ctx*/, unsigned int /*flags*/) {
}


void SPScript::modified(unsigned int /*flags*/) {
}


void SPScript::set(unsigned int key, const gchar* value) {
    switch (key) {
	case SP_ATTR_XLINK_HREF:
            if (this->xlinkhref) {
            	g_free(this->xlinkhref);
            }

            this->xlinkhref = g_strdup(value);
            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
	default:
            SPObject::set(key, value);
            break;
    }
}

Inkscape::XML::Node* SPScript::write(Inkscape::XML::Document* /*doc*/, Inkscape::XML::Node* repr, guint /*flags*/) {
	return repr;
}

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
