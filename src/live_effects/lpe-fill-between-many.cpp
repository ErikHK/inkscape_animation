/*
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */


#include "live_effects/lpe-fill-between-many.h"
#include "live_effects/lpeobject.h"
#include "xml/node.h"
#include "display/curve.h"
#include "inkscape.h"
#include "selection.h"

#include "object/sp-defs.h"
#include "object/sp-shape.h"

#include "svg/svg.h"

// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>

namespace Inkscape {
namespace LivePathEffect {

static const Util::EnumData<Filllpemethod> FilllpemethodData[] = {
    { FLM_ORIGINALD, N_("Without LPE's"), "originald" }, 
    { FLM_BSPLINESPIRO, N_("With Spiro or BSpline"), "bsplinespiro" },
    { FLM_D, N_("With LPE's"), "d" }
};
static const Util::EnumDataConverter<Filllpemethod> FLMConverter(FilllpemethodData, FLM_END);

LPEFillBetweenMany::LPEFillBetweenMany(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    linked_paths(_("Linked path:"), _("Paths from which to take the original path data"), "linkedpaths", &wr, this),
    method(_("LPE's on linked:"), _("LPE's on linked"), "method", FLMConverter, &wr, this, FLM_BSPLINESPIRO),
    fuse(_("Fuse coincident points"), _("Fuse coincident points"), "fuse", &wr, this, false),
    allow_transforms(_("Allow transforms"), _("Allow transforms"), "allow_transforms", &wr, this, false),
    join(_("Join subpaths"), _("Join subpaths"), "join", &wr, this, true),
    close(_("Close"), _("Close path"), "close", &wr, this, true),
    applied("Store the first apply", "", "applied", &wr, this, "false", false)
{
    registerParameter(&linked_paths);
    registerParameter(&method);
    registerParameter(&fuse);
    registerParameter(&allow_transforms);
    registerParameter(&join);
    registerParameter(&close);
    registerParameter(&applied);
    previous_method = FLM_END;
}

LPEFillBetweenMany::~LPEFillBetweenMany()
{

}

void LPEFillBetweenMany::doOnApply (SPLPEItem const* lpeitem)
{
    SPDocument * document = SP_ACTIVE_DOCUMENT;
    if (!document) {
        return;
    }
    SPLPEItem *lpe_item = const_cast<SPLPEItem *>(lpeitem);
    SPObject * parent = lpe_item->parent; 
    if (lpe_item) {
        SPShape *shape = dynamic_cast<SPShape *>(lpe_item);
        if (shape) {
            Inkscape::SVGOStringStream os;
            if (strcmp(this->lpeobj->getRepr()->attribute("applied"), "false") == 0) {
                os << '#' << SP_ITEM(lpe_item)->getId() << ",0,1";

                Inkscape::XML::Document *xml_doc = document->getReprDoc();
                // create the LPE
                Inkscape::XML::Node *lpe_repr = xml_doc->createElement("inkscape:path-effect");
                {
                    lpe_repr->setAttribute("effect", "fill_between_many");
                    lpe_repr->setAttribute("linkedpaths", os.str());
                    lpe_repr->setAttribute("applied", "true");
                    lpe_repr->setAttribute("method", "partial");
                    lpe_repr->setAttribute("allow_transforms", "false");
                    document->getDefs()->getRepr()->addChild(lpe_repr, NULL); // adds to <defs> and assigns the 'id' attribute
                }
                std::string lpe_id_href = std::string("#") + lpe_repr->attribute("id");
                Inkscape::GC::release(lpe_repr);
                Inkscape::XML::Node *clone = xml_doc->createElement("svg:path");
                {
                    clone->setAttribute("d", "M 0 0", false);
                    // add the new clone to the top of the original's parent
                    parent->appendChildRepr(clone);
                    SPObject *clone_obj = document->getObjectById(clone->attribute("id"));
                    SPLPEItem *clone_lpeitem = dynamic_cast<SPLPEItem *>(clone_obj);
                    if (clone_lpeitem) {
                        clone_lpeitem->addPathEffect(lpe_id_href, false);
                    }
                }
                Inkscape::Selection * sel = SP_ACTIVE_DESKTOP->getSelection();
                sel->set(clone);
                Inkscape::GC::release(clone);
                lpe_item->removeCurrentPathEffect(false);
            }
        }
    }
}

void LPEFillBetweenMany::doEffect (SPCurve * curve)
{
    if (previous_method != method) {
        if (method == FLM_BSPLINESPIRO) {
            linked_paths.allowOnlyBsplineSpiro(true);
            linked_paths.setFromOriginalD(false);
        } else if(method == FLM_ORIGINALD) {
            linked_paths.allowOnlyBsplineSpiro(false);
            linked_paths.setFromOriginalD(true);
        } else {
            linked_paths.allowOnlyBsplineSpiro(false);
            linked_paths.setFromOriginalD(false);
        }
        previous_method = method;
    }
    Geom::PathVector res_pathv;
    for (std::vector<PathAndDirectionAndVisible*>::iterator iter = linked_paths._vector.begin(); iter != linked_paths._vector.end(); ++iter) {
        SPObject *obj;
        if ((*iter)->ref.isAttached() && (obj = (*iter)->ref.getObject()) && SP_IS_ITEM(obj) && !(*iter)->_pathvector.empty() && (*iter)->visibled) {
            Geom::Path linked_path;
            if ((*iter)->reversed) {
                linked_path = (*iter)->_pathvector.front().reversed();
            } else {
                linked_path = (*iter)->_pathvector.front();
            }
            
            if (!res_pathv.empty() && join) {
                if (!are_near(res_pathv.front().finalPoint(), linked_path.initialPoint(), 0.01) || !fuse) {
                    res_pathv.front().appendNew<Geom::LineSegment>(linked_path.initialPoint());
                } else {
                    linked_path.setInitial(res_pathv.front().finalPoint());
                }
                if(!allow_transforms) {
                    Geom::Affine affine = Geom::identity();
                    sp_svg_transform_read(SP_ITEM(obj)->getAttribute("transform"), &affine);
                    linked_path *= affine;
                }
                res_pathv.front().append(linked_path);
            } else {
                if (close && !join) {
                    linked_path.close();
                }
                if(!allow_transforms) {
                    Geom::Affine affine = Geom::identity();
                    sp_svg_transform_read(SP_ITEM(obj)->getAttribute("transform"), &affine);
                    linked_path *= affine;
                }
                res_pathv.push_back(linked_path);
            }
        }
    }
    
    if(!allow_transforms && sp_lpe_item) {
        SP_ITEM(sp_lpe_item)->transform = Geom::identity();
    }
    
    if (!res_pathv.empty() && close) {
        res_pathv.front().close();
    }
    
    if (res_pathv.empty()) {
        res_pathv = curve->get_pathvector();
    }

    curve->set_pathvector(res_pathv);
}

void
LPEFillBetweenMany::transform_multiply(Geom::Affine const& postmul, bool set)
{
    if(!allow_transforms && sp_lpe_item) {
        SP_ITEM(sp_lpe_item)->transform *= postmul.inverse();
    }
}

} // namespace LivePathEffect
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
