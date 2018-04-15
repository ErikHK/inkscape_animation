#ifndef SEEN_LPE_ITEM_REFERENCE_H
#define SEEN_LPE_ITEM_REFERENCE_H

/*
 * Copyright (C) 2008-2012 Authors
 * Authors: Johan Engelen
 *          Abhishek Sharma
 *
 * Released under GNU GPL, read the file 'COPYING' for more information.
 */

#include "object/uri-references.h"

class SPItem;
namespace Inkscape {
namespace XML { class Node; }

namespace LivePathEffect {

/**
 * The reference corresponding to href of LPE ItemParam.
 */
class ItemReference : public Inkscape::URIReference {
public:
    ItemReference(SPObject *owner) : URIReference(owner) {}

    SPItem *getObject() const {
        return (SPItem *)URIReference::getObject();
    }

protected:
    virtual bool _acceptObject(SPObject * const obj) const;

private:
    ItemReference(const ItemReference&);
    ItemReference& operator=(const ItemReference&);
};

} // namespace LivePathEffect

} // namespace Inkscape



#endif /* !SEEN_LPE_PATH_REFERENCE_H */

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
