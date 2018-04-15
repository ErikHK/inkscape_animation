/*
 * SVG <path> implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   David Turner <novalis@gnu.org>
 *   Abhishek Sharma
 *   Johan Engelen
 *
 * Copyright (C) 2004 David Turner
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 1999-2012 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glibmm/i18n.h>

#include "live_effects/effect.h"
#include "live_effects/lpeobject.h"
#include "live_effects/lpeobject-reference.h"
#include "sp-lpe-item.h"

#include "display/curve.h"
#include <2geom/curves.h>
#include "helper/geom-curves.h"

#include "svg/svg.h"
#include "xml/repr.h"
#include "attributes.h"

#include "sp-path.h"
#include "sp-guide.h"

#include "document.h"
#include "desktop.h"

#include "desktop-style.h"
#include "ui/tools/tool-base.h"
#include "inkscape.h"
#include "style.h"

#define noPATH_VERBOSE

gint SPPath::nodesInPath() const
{
    return _curve ? _curve->nodes_in_path() : 0;
}

const char* SPPath::displayName() const {
    return _("Path");
}

gchar* SPPath::description() const {
    int count = this->nodesInPath();
    char *lpe_desc = g_strdup("");
    
    if (hasPathEffect()) {
        Glib::ustring s;
        PathEffectList effect_list =  this->getEffectList();
        
        for (PathEffectList::iterator it = effect_list.begin(); it != effect_list.end(); ++it)
        {
            LivePathEffectObject *lpeobj = (*it)->lpeobject;
            
            if (!lpeobj || !lpeobj->get_lpe()) {
                break;
            }
            
            if (s.empty()) {
                s = lpeobj->get_lpe()->getName();
            } else {
                s = s + ", " + lpeobj->get_lpe()->getName();
            }
        }
        lpe_desc = g_strdup_printf(_(", path effect: %s"), s.c_str());
    }
    char *ret = g_strdup_printf(ngettext(
                _("%i node%s"), _("%i nodes%s"), count), count, lpe_desc);
    g_free(lpe_desc);
    return ret;
}

void SPPath::convert_to_guides() const {
    if (!this->_curve) {
        return;
    }

    std::list<std::pair<Geom::Point, Geom::Point> > pts;

    Geom::Affine const i2dt(this->i2dt_affine());
    Geom::PathVector const & pv = this->_curve->get_pathvector();
    
    for(Geom::PathVector::const_iterator pit = pv.begin(); pit != pv.end(); ++pit) {
        for(Geom::Path::const_iterator cit = pit->begin(); cit != pit->end_default(); ++cit) {
            // only add curves for straight line segments
            if( is_straight_curve(*cit) )
            {
                pts.push_back(std::make_pair(cit->initialPoint() * i2dt, cit->finalPoint() * i2dt));
            }
        }
    }

    sp_guide_pt_pairs_to_guides(this->document, pts);
}

SPPath::SPPath() : SPShape(), connEndPair(this) {
}

SPPath::~SPPath() {
}

void SPPath::build(SPDocument *document, Inkscape::XML::Node *repr) {
    /* Are these calls actually necessary? */
    this->readAttr( "marker" );
    this->readAttr( "marker-start" );
    this->readAttr( "marker-mid" );
    this->readAttr( "marker-end" );

    sp_conn_end_pair_build(this);

    SPShape::build(document, repr);

    // this->readAttr( "inkscape:original-d" ); // bug #1299948
    // Why we take the long way of doing this probably needs some explaining:
    //
    // Normally upon being built, reading the inkscape:original-d attribute
    // will cause the path to actually _write to its repr_ in response to this.
    // This is bad, bad news if the attached effect refers to a path which
    // hasn't been constructed yet.
    // 
    // What will happen is the effect parameter will cause the effect to
    // recalculate with a completely different value due to the parameter being
    // "empty" -- even worse, an undo event might be created with the bad value,
    // and undoing the current action could cause it to revert to the "bad"
    // state. (After that, the referred object will be constructed and the
    // reference will trigger the path effect to update and commit the right
    // value to "d".)
    //
    // This mild nastiness here (don't recalculate effects on build) prevents a
    // plethora of issues with effects with linked parameters doing wild and
    // stupid things on new documents upon a mere undo.

    if (gchar const* s = this->getRepr()->attribute("inkscape:original-d"))
    {
        // Write the value to _curve_before_lpe, do not recalculate effects
        Geom::PathVector pv = sp_svg_read_pathv(s);
        SPCurve *curve = new SPCurve(pv);
        
        if (_curve_before_lpe) {
            _curve_before_lpe = _curve_before_lpe->unref();
        }

        if (curve) {
            _curve_before_lpe = curve->ref();
        }
    }
    this->readAttr( "d" );

    /* d is a required attribute */
    char const *d = this->getAttribute("d", NULL);

    if (d == NULL) {
        // First see if calculating the path effect will generate "d":
        this->update_patheffect(true);
        d = this->getAttribute("d", NULL);

        // I guess that didn't work, now we have nothing useful to write ("")
        if (d == NULL) {
            this->setKeyValue( sp_attribute_lookup("d"), "");
        }
    }
}

void SPPath::release() {
    this->connEndPair.release();

    SPShape::release();
}

void SPPath::set(unsigned int key, const gchar* value) {
    switch (key) {
        case SP_ATTR_INKSCAPE_ORIGINAL_D:
            if (value) {
                Geom::PathVector pv = sp_svg_read_pathv(value);
                SPCurve *curve = new SPCurve(pv);

                if (curve) {
                    this->setCurveBeforeLPE(curve);
                    curve->unref();
                }
            } else {
                this->setCurveBeforeLPE(NULL);
                
            }
            sp_lpe_item_update_patheffect(this, true, true);
            break;

       case SP_ATTR_D:
            if (value) {
                Geom::PathVector pv = sp_svg_read_pathv(value);
                SPCurve *curve = new SPCurve(pv);

                if (curve) {
                    this->setCurve(curve);
                    curve->unref();
                }
            } else {
                this->setCurve(NULL);
            }

            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SP_PROP_MARKER:
        case SP_PROP_MARKER_START:
        case SP_PROP_MARKER_MID:
        case SP_PROP_MARKER_END:
            sp_shape_set_marker(this, key, value);
            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SP_ATTR_CONNECTOR_TYPE:
        case SP_ATTR_CONNECTOR_CURVATURE:
        case SP_ATTR_CONNECTION_START:
        case SP_ATTR_CONNECTION_END:
        case SP_ATTR_CONNECTION_START_POINT:
        case SP_ATTR_CONNECTION_END_POINT:
            this->connEndPair.setAttr(key, value);
            break;

        default:
            SPShape::set(key, value);
            break;
    }
}

Inkscape::XML::Node* SPPath::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:path");
    }

#ifdef PATH_VERBOSE
g_message("sp_path_write writes 'd' attribute");
#endif

    if ( this->_curve != NULL ) {
        gchar *str = sp_svg_write_path(this->_curve->get_pathvector());
        repr->setAttribute("d", str);
        g_free(str);
    } else {
        repr->setAttribute("d", NULL);
    }

    if (flags & SP_OBJECT_WRITE_EXT) {
        if ( this->_curve_before_lpe != NULL ) {
            gchar *str = sp_svg_write_path(this->_curve_before_lpe->get_pathvector());
            repr->setAttribute("inkscape:original-d", str);
            g_free(str);
        } else {
            repr->setAttribute("inkscape:original-d", NULL);
        }
    }

    this->connEndPair.writeRepr(repr);

    SPShape::write(xml_doc, repr, flags);

    return repr;
}

void SPPath::update(SPCtx *ctx, guint flags) {
    if (flags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG | SP_OBJECT_VIEWPORT_MODIFIED_FLAG)) {
        flags &= ~SP_OBJECT_USER_MODIFIED_FLAG_B; // since we change the description, it's not a "just translation" anymore
    }

    // Our code depends on 'd' being an attribute (LPE's, etc.). To support 'd' as a property, we
    // check it here (after the style property has been evaluated, this allows us to properly
    // handled precedence of property vs attribute). If we read in a 'd' set by styling, convert it
    // to an attribute. We'll convert it back on output.

    d_source = style->d.style_src;

    if (style->d.set &&

        (d_source == SP_STYLE_SRC_STYLE_PROP || d_source == SP_STYLE_SRC_STYLE_SHEET) ) {

        if (style->d.value) {

            Geom::PathVector pv = sp_svg_read_pathv(style->d.value);
            SPCurve *curve = new SPCurve(pv);
            if (curve) {

                // Update curve
                this->setCurveInsync(curve, TRUE);
                curve->unref();

                // Convert from property to attribute (convert back on write)
                getRepr()->setAttribute("d", style->d.value);

                SPCSSAttr *css = sp_repr_css_attr( getRepr(), "style");
                sp_repr_css_unset_property ( css, "d");
                sp_repr_css_set ( getRepr(), css, "style" );
                sp_repr_css_attr_unref ( css );

                style->d.style_src = SP_STYLE_SRC_ATTRIBUTE;
            } else {
                // Do nothing... don't overwrite 'd' from attribute
            }
        }
    }

    SPShape::update(ctx, flags);

    this->connEndPair.update();
}

Geom::Affine SPPath::set_transform(Geom::Affine const &transform) {
    if (!_curve) { // 0 nodes, nothing to transform
        return Geom::identity();
    }
    // Transform the original-d path if this is a valid LPE this, other else the (ordinary) path
    if (_curve_before_lpe && hasPathEffectRecursive()) {
        if (this->hasPathEffectOfType(Inkscape::LivePathEffect::CLONE_ORIGINAL) || 
            this->hasPathEffectOfType(Inkscape::LivePathEffect::BEND_PATH) || 
            this->hasPathEffectOfType(Inkscape::LivePathEffect::FILL_BETWEEN_MANY) ||
            this->hasPathEffectOfType(Inkscape::LivePathEffect::FILL_BETWEEN_STROKES) ) 
        {
            // if path has this LPE applied, don't write the transform to the pathdata, but write it 'unoptimized'
            // also if the effect is type BEND PATH to fix bug #179842
            this->adjust_livepatheffect(transform);
            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
            return transform;
        } else {
            _curve_before_lpe->transform(transform);
        }
    } else {
        _curve->transform(transform);
    }

    // Adjust stroke
    this->adjust_stroke(transform.descrim());

    // Adjust pattern fill
    this->adjust_pattern(transform);

    // Adjust gradient fill
    this->adjust_gradient(transform);

    // Adjust LPE
    this->adjust_livepatheffect(transform);

    this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);

    // nothing remains - we've written all of the transform, so return identity
    return Geom::identity();
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
