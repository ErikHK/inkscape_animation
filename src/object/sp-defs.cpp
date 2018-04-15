/*
 * SVG <defs> implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2000-2002 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

/*
 * fixme: We should really check childrens validity - currently everything
 * flips in
 */

#include "sp-defs.h"
#include "xml/repr.h"
#include "document.h"

SPDefs::SPDefs() : SPObject() {
}

SPDefs::~SPDefs() {
}

void SPDefs::release() {
	SPObject::release();
}

void SPDefs::update(SPCtx *ctx, guint flags) {
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }

    flags &= SP_OBJECT_MODIFIED_CASCADE;
    std::vector<SPObject*> l(this->childList(true));
    for(std::vector<SPObject*>::const_iterator i=l.begin();i!=l.end();++i){
        SPObject *child = *i;
        if (flags || (child->uflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            child->updateDisplay(ctx, flags);
        }
        sp_object_unref(child);
    }
}

void SPDefs::modified(unsigned int flags) {
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }

    flags &= SP_OBJECT_MODIFIED_CASCADE;
    std::vector<SPObject *> l;
    for (auto& child: children) {
        sp_object_ref(&child);
        l.push_back(&child);
    }

    for (auto child:l) {
        if (flags || (child->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            child->emitModified(flags);
        }
        sp_object_unref(child);
    }
}

Inkscape::XML::Node* SPDefs::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if (flags & SP_OBJECT_WRITE_BUILD) {

        if (!repr) {
            repr = xml_doc->createElement("svg:defs");
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
