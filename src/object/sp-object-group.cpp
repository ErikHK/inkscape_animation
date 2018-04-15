/*
 * Abstract base class for non-item groups
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 1999-2003 Authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "sp-object-group.h"
#include "xml/repr.h"
#include "document.h"

SPObjectGroup::SPObjectGroup() : SPObject() {
}

SPObjectGroup::~SPObjectGroup() {
}

void SPObjectGroup::child_added(Inkscape::XML::Node *child, Inkscape::XML::Node *ref) {
	SPObject::child_added(child, ref);

	this->requestModified(SP_OBJECT_MODIFIED_FLAG);
}


void SPObjectGroup::remove_child(Inkscape::XML::Node *child) {
	SPObject::remove_child(child);

	this->requestModified(SP_OBJECT_MODIFIED_FLAG);
}


void SPObjectGroup::order_changed(Inkscape::XML::Node *child, Inkscape::XML::Node *old_ref, Inkscape::XML::Node *new_ref) {
	SPObject::order_changed(child, old_ref, new_ref);

	this->requestModified(SP_OBJECT_MODIFIED_FLAG);
}


Inkscape::XML::Node *SPObjectGroup::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if (flags & SP_OBJECT_WRITE_BUILD) {
        if (!repr) {
            repr = xml_doc->createElement("svg:g");
        }

        std::vector<Inkscape::XML::Node *> l;
        for (auto& child: children) {
            Inkscape::XML::Node *crepr = child.updateRepr(xml_doc, NULL, flags);

            if (crepr) {
                l.push_back(crepr);
            }
        }
        for (auto i=l.rbegin();i!=l.rend();++i) {
            repr->addChild(*i, NULL);
            Inkscape::GC::release(*i);
        }
    } else {
        for (auto& child: children) {
            child.updateRepr(flags);
        }
    }

    SPObject::write(xml_doc, repr, flags);

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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
