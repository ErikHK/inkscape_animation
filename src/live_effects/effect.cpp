/*
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *   Abhishek Sharma
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

//#define LPE_ENABLE_TEST_EFFECTS //uncomment for toy effects

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// include effects:
#include "live_effects/lpe-patternalongpath.h"
#include "live_effects/lpe-angle_bisector.h"
#include "live_effects/lpe-attach-path.h"
#include "live_effects/lpe-bendpath.h"
#include "live_effects/lpe-bounding-box.h"
#include "live_effects/lpe-bspline.h"
#include "live_effects/lpe-circle_3pts.h"
#include "live_effects/lpe-circle_with_radius.h"
#include "live_effects/lpe-clone-original.h"
#include "live_effects/lpe-constructgrid.h"
#include "live_effects/lpe-copy_rotate.h"
#include "live_effects/lpe-curvestitch.h"
#include "live_effects/lpe-dash-stroke.h"
#include "live_effects/lpe-dynastroke.h"
#include "live_effects/lpe-ellipse_5pts.h"
#include "live_effects/lpe-envelope.h"
#include "live_effects/lpe-extrude.h"
#include "live_effects/lpe-fill-between-many.h"
#include "live_effects/lpe-fill-between-strokes.h"
#include "live_effects/lpe-fillet-chamfer.h"
#include "live_effects/lpe-gears.h"
#include "live_effects/lpe-interpolate.h"
#include "live_effects/lpe-interpolate_points.h"
#include "live_effects/lpe-jointype.h"
#include "live_effects/lpe-knot.h"
#include "live_effects/lpe-lattice2.h"
#include "live_effects/lpe-lattice.h"
#include "live_effects/lpe-line_segment.h"
#include "live_effects/lpe-measure-segments.h"
#include "live_effects/lpe-mirror_symmetry.h"
#include "live_effects/lpe-offset.h"
#include "live_effects/lpe-parallel.h"
#include "live_effects/lpe-path_length.h"
#include "live_effects/lpe-perp_bisector.h"
#include "live_effects/lpe-perspective-envelope.h"
#include "live_effects/lpe-perspective_path.h"
#include "live_effects/lpe-powerclip.h"
#include "live_effects/lpe-powermask.h"
#include "live_effects/lpe-powerstroke.h"
#include "live_effects/lpe-recursiveskeleton.h"
#include "live_effects/lpe-roughen.h"
#include "live_effects/lpe-rough-hatches.h"
#include "live_effects/lpe-ruler.h"
#include "live_effects/lpe-show_handles.h"
#include "live_effects/lpe-simplify.h"
#include "live_effects/lpe-sketch.h"
#include "live_effects/lpe-spiro.h"
#include "live_effects/lpe-tangent_to_curve.h"
#include "live_effects/lpe-transform_2pts.h"
#include "live_effects/lpe-taperstroke.h"
#include "live_effects/lpe-test-doEffect-stack.h"
#include "live_effects/lpe-text_label.h"
#include "live_effects/lpe-vonkoch.h"
#include "live_effects/lpe-embrodery-stitch.h"
#include "live_effects/lpe-bool.h"
#include "live_effects/lpe-pts2ellipse.h"

#include "live_effects/lpeobject.h"

#include "xml/node-event-vector.h"
#include "xml/sp-css-attr.h"

#include "message-stack.h"
#include "document-private.h"
#include "ui/tools/pen-tool.h"
#include "ui/tools/node-tool.h"
#include "ui/tools-switch.h"
#include "knotholder.h"
#include "path-chemistry.h"
#include "display/curve.h"

#include "object/sp-defs.h"
#include "object/sp-shape.h"

#include <stdio.h>
#include <string.h>
#include <pangomm/layout.h>
#include <gtkmm/expander.h>

namespace Inkscape {

namespace LivePathEffect {

const Util::EnumData<EffectType> LPETypeData[] = {
    // {constant defined in effect-enum.h, N_("name of your effect"), "name of your effect in SVG"}
/* 0.46 */
    {BEND_PATH,             N_("Bend"),                            "bend_path"},
    {GEARS,                 N_("Gears"),                           "gears"},
    {PATTERN_ALONG_PATH,    N_("Pattern Along Path"),              "skeletal"},   // for historic reasons, this effect is called skeletal(strokes) in Inkscape:SVG
    {CURVE_STITCH,          N_("Stitch Sub-Paths"),                "curvestitching"},
/* 0.47 */
    {VONKOCH,               N_("VonKoch"),                         "vonkoch"},
    {KNOT,                  N_("Knot"),                            "knot"},
    {CONSTRUCT_GRID,        N_("Construct grid"),                  "construct_grid"},
    {SPIRO,                 N_("Spiro spline"),                    "spiro"},
    {ENVELOPE,              N_("Envelope Deformation"),            "envelope"},
    {INTERPOLATE,           N_("Interpolate Sub-Paths"),           "interpolate"},
    {ROUGH_HATCHES,         N_("Hatches (rough)"),                 "rough_hatches"},
    {SKETCH,                N_("Sketch"),                          "sketch"},
    {RULER,                 N_("Ruler"),                           "ruler"},
/* 0.91 */
    {POWERSTROKE,           N_("Power stroke"),                    "powerstroke"},
    {CLONE_ORIGINAL,        N_("Clone original"),                  "clone_original"},
/* 0.92 */
    {SIMPLIFY,              N_("Simplify"),                        "simplify"},
    {LATTICE2,              N_("Lattice Deformation 2"),           "lattice2"},
    {PERSPECTIVE_ENVELOPE,  N_("Perspective/Envelope"),            "perspective-envelope"}, //TODO:Wrong name with "-"
    {INTERPOLATE_POINTS,    N_("Interpolate points"),              "interpolate_points"},
    {TRANSFORM_2PTS,        N_("Transform by 2 points"),           "transform_2pts"},
    {SHOW_HANDLES,          N_("Show handles"),                    "show_handles"},
    {ROUGHEN,               N_("Roughen"),                         "roughen"},
    {BSPLINE,               N_("BSpline"),                         "bspline"},
    {JOIN_TYPE,             N_("Join type"),                       "join_type"},
    {TAPER_STROKE,          N_("Taper stroke"),                    "taper_stroke"},
    {MIRROR_SYMMETRY,       N_("Mirror symmetry"),                 "mirror_symmetry"},
    {COPY_ROTATE,           N_("Rotate copies"),                   "copy_rotate"},
/* Ponyscape -> Inkscape 0.92*/
    {ATTACH_PATH,           N_("Attach path"),                     "attach_path"},
    {FILL_BETWEEN_STROKES,  N_("Fill between strokes"),            "fill_between_strokes"},
    {FILL_BETWEEN_MANY,     N_("Fill between many"),               "fill_between_many"},
    {ELLIPSE_5PTS,          N_("Ellipse by 5 points"),             "ellipse_5pts"},
    {BOUNDING_BOX,          N_("Bounding Box"),                    "bounding_box"},
/* 0.93 */
    {MEASURE_SEGMENTS,      N_("Measure Segments"),                "measure_segments"},
    {FILLET_CHAMFER,        N_("Fillet/Chamfer"),                  "fillet_chamfer"},
    {BOOL_OP,               N_("Boolean operation"),               "bool_op"},
    {EMBRODERY_STITCH,      N_("Embroidery stitch"),               "embrodery_stitch"},
    {POWERCLIP,             N_("Power clip"),                      "powerclip"},
    {POWERMASK,             N_("Power mask"),                      "powermask"},
    {PTS2ELLIPSE,           N_("Ellipse from points"),             "pts2ellipse"},
    {OFFSET,                N_("Offset"),                          "offset"},
    {DASH_STROKE,           N_("Dash Stroke"),                     "dash_stroke"},
#ifdef LPE_ENABLE_TEST_EFFECTS
    {DOEFFECTSTACK_TEST,    N_("doEffect stack test"),             "doeffectstacktest"},
    {ANGLE_BISECTOR,        N_("Angle bisector"),                  "angle_bisector"},
    {CIRCLE_WITH_RADIUS,    N_("Circle (by center and radius)"),   "circle_with_radius"},
    {CIRCLE_3PTS,           N_("Circle by 3 points"),              "circle_3pts"},
    {DYNASTROKE,            N_("Dynamic stroke"),                  "dynastroke"},
    {EXTRUDE,               N_("Extrude"),                         "extrude"},
    {LATTICE,               N_("Lattice Deformation"),             "lattice"},
    {LINE_SEGMENT,          N_("Line Segment"),                    "line_segment"},
    {PARALLEL,              N_("Parallel"),                        "parallel"},
    {PATH_LENGTH,           N_("Path length"),                     "path_length"},
    {PERP_BISECTOR,         N_("Perpendicular bisector"),          "perp_bisector"},
    {PERSPECTIVE_PATH,      N_("Perspective path"),                "perspective_path"},
    {RECURSIVE_SKELETON,    N_("Recursive skeleton"),              "recursive_skeleton"},
    {TANGENT_TO_CURVE,      N_("Tangent to curve"),                "tangent_to_curve"},
    {TEXT_LABEL,            N_("Text label"),                      "text_label"},
#endif

};
const Util::EnumDataConverter<EffectType> LPETypeConverter(LPETypeData, sizeof(LPETypeData)/sizeof(*LPETypeData));

int
Effect::acceptsNumClicks(EffectType type) {
    switch (type) {
        case INVALID_LPE: return -1; // in case we want to distinguish between invalid LPE and valid ones that expect zero clicks
        case ANGLE_BISECTOR: return 3;
        case CIRCLE_3PTS: return 3;
        case CIRCLE_WITH_RADIUS: return 2;
        case LINE_SEGMENT: return 2;
        case PERP_BISECTOR: return 2;
        default: return 0;
    }
}

Effect*
Effect::New(EffectType lpenr, LivePathEffectObject *lpeobj)
{
    Effect* neweffect = NULL;
    switch (lpenr) {
        case EMBRODERY_STITCH:
            neweffect = static_cast<Effect*> ( new LPEEmbroderyStitch(lpeobj) );
            break;
        case BOOL_OP:
            neweffect = static_cast<Effect*> ( new LPEBool(lpeobj) );
            break;
        case PATTERN_ALONG_PATH:
            neweffect = static_cast<Effect*> ( new LPEPatternAlongPath(lpeobj) );
            break;
        case BEND_PATH:
            neweffect = static_cast<Effect*> ( new LPEBendPath(lpeobj) );
            break;
        case SKETCH:
            neweffect = static_cast<Effect*> ( new LPESketch(lpeobj) );
            break;
        case ROUGH_HATCHES:
            neweffect = static_cast<Effect*> ( new LPERoughHatches(lpeobj) );
            break;
        case VONKOCH:
            neweffect = static_cast<Effect*> ( new LPEVonKoch(lpeobj) );
            break;
        case KNOT:
            neweffect = static_cast<Effect*> ( new LPEKnot(lpeobj) );
            break;
        case GEARS:
            neweffect = static_cast<Effect*> ( new LPEGears(lpeobj) );
            break;
        case CURVE_STITCH:
            neweffect = static_cast<Effect*> ( new LPECurveStitch(lpeobj) );
            break;
        case LATTICE:
            neweffect = static_cast<Effect*> ( new LPELattice(lpeobj) );
            break;
        case ENVELOPE:
            neweffect = static_cast<Effect*> ( new LPEEnvelope(lpeobj) );
            break;
        case CIRCLE_WITH_RADIUS:
            neweffect = static_cast<Effect*> ( new LPECircleWithRadius(lpeobj) );
            break;
        case PERSPECTIVE_PATH:
            neweffect = static_cast<Effect*> ( new LPEPerspectivePath(lpeobj) );
            break;
        case SPIRO:
            neweffect = static_cast<Effect*> ( new LPESpiro(lpeobj) );
            break;
        case CONSTRUCT_GRID:
            neweffect = static_cast<Effect*> ( new LPEConstructGrid(lpeobj) );
            break;
        case PERP_BISECTOR:
            neweffect = static_cast<Effect*> ( new LPEPerpBisector(lpeobj) );
            break;
        case TANGENT_TO_CURVE:
            neweffect = static_cast<Effect*> ( new LPETangentToCurve(lpeobj) );
            break;
        case MIRROR_SYMMETRY:
            neweffect = static_cast<Effect*> ( new LPEMirrorSymmetry(lpeobj) );
            break;
        case CIRCLE_3PTS:
            neweffect = static_cast<Effect*> ( new LPECircle3Pts(lpeobj) );
            break;
        case ANGLE_BISECTOR:
            neweffect = static_cast<Effect*> ( new LPEAngleBisector(lpeobj) );
            break;
        case PARALLEL:
            neweffect = static_cast<Effect*> ( new LPEParallel(lpeobj) );
            break;
        case COPY_ROTATE:
            neweffect = static_cast<Effect*> ( new LPECopyRotate(lpeobj) );
            break;
        case OFFSET:
            neweffect = static_cast<Effect*> ( new LPEOffset(lpeobj) );
            break;
        case RULER:
            neweffect = static_cast<Effect*> ( new LPERuler(lpeobj) );
            break;
        case INTERPOLATE:
            neweffect = static_cast<Effect*> ( new LPEInterpolate(lpeobj) );
            break;
        case INTERPOLATE_POINTS:
            neweffect = static_cast<Effect*> ( new LPEInterpolatePoints(lpeobj) );
            break;
        case TEXT_LABEL:
            neweffect = static_cast<Effect*> ( new LPETextLabel(lpeobj) );
            break;
        case PATH_LENGTH:
            neweffect = static_cast<Effect*> ( new LPEPathLength(lpeobj) );
            break;
        case LINE_SEGMENT:
            neweffect = static_cast<Effect*> ( new LPELineSegment(lpeobj) );
            break;
        case DOEFFECTSTACK_TEST:
            neweffect = static_cast<Effect*> ( new LPEdoEffectStackTest(lpeobj) );
            break;
        case BSPLINE:
            neweffect = static_cast<Effect*> ( new LPEBSpline(lpeobj) );
            break;
        case DYNASTROKE:
            neweffect = static_cast<Effect*> ( new LPEDynastroke(lpeobj) );
            break;
        case RECURSIVE_SKELETON:
            neweffect = static_cast<Effect*> ( new LPERecursiveSkeleton(lpeobj) );
            break;
        case EXTRUDE:
            neweffect = static_cast<Effect*> ( new LPEExtrude(lpeobj) );
            break;
        case POWERSTROKE:
            neweffect = static_cast<Effect*> ( new LPEPowerStroke(lpeobj) );
            break;
        case CLONE_ORIGINAL:
            neweffect = static_cast<Effect*> ( new LPECloneOriginal(lpeobj) );
            break;
        case ATTACH_PATH:
            neweffect = static_cast<Effect*> ( new LPEAttachPath(lpeobj) );
            break;
        case FILL_BETWEEN_STROKES:
            neweffect = static_cast<Effect*> ( new LPEFillBetweenStrokes(lpeobj) );
            break;
        case FILL_BETWEEN_MANY:
            neweffect = static_cast<Effect*> ( new LPEFillBetweenMany(lpeobj) );
            break;
        case ELLIPSE_5PTS:
            neweffect = static_cast<Effect*> ( new LPEEllipse5Pts(lpeobj) );
            break;
        case BOUNDING_BOX:
            neweffect = static_cast<Effect*> ( new LPEBoundingBox(lpeobj) );
            break;
        case JOIN_TYPE:
            neweffect = static_cast<Effect*> ( new LPEJoinType(lpeobj) );
            break;
        case TAPER_STROKE:
            neweffect = static_cast<Effect*> ( new LPETaperStroke(lpeobj) );
            break;
        case SIMPLIFY:
            neweffect = static_cast<Effect*> ( new LPESimplify(lpeobj) );
            break;
        case LATTICE2:
            neweffect = static_cast<Effect*> ( new LPELattice2(lpeobj) );
            break;
        case PERSPECTIVE_ENVELOPE:
            neweffect = static_cast<Effect*> ( new LPEPerspectiveEnvelope(lpeobj) );
            break;
        case FILLET_CHAMFER:
            neweffect = static_cast<Effect*> ( new LPEFilletChamfer(lpeobj) );
            break;
        case POWERCLIP:
            neweffect = static_cast<Effect*> ( new LPEPowerClip(lpeobj) );
            break;
        case POWERMASK:
            neweffect = static_cast<Effect*> ( new LPEPowerMask(lpeobj) );
            break;
        case ROUGHEN:
            neweffect = static_cast<Effect*> ( new LPERoughen(lpeobj) );
            break;
        case SHOW_HANDLES:
            neweffect = static_cast<Effect*> ( new LPEShowHandles(lpeobj) );
            break;
        case TRANSFORM_2PTS:
            neweffect = static_cast<Effect*> ( new LPETransform2Pts(lpeobj) );
            break;
        case MEASURE_SEGMENTS:
            neweffect = static_cast<Effect*> ( new LPEMeasureSegments(lpeobj) );
            break;
        case PTS2ELLIPSE:
            neweffect = static_cast<Effect*> ( new LPEPts2Ellipse(lpeobj) );
            break;
        case DASH_STROKE:
            neweffect = static_cast<Effect*> ( new LPEDashStroke(lpeobj) );
            break;
        default:
            g_warning("LivePathEffect::Effect::New called with invalid patheffect type (%d)", lpenr);
            neweffect = NULL;
            break;
    }

    if (neweffect) {
        neweffect->readallParameters(lpeobj->getRepr());
    }

    return neweffect;
}

void Effect::createAndApply(const char* name, SPDocument *doc, SPItem *item)
{
    // Path effect definition
    Inkscape::XML::Document *xml_doc = doc->getReprDoc();
    Inkscape::XML::Node *repr = xml_doc->createElement("inkscape:path-effect");
    repr->setAttribute("effect", name);

    doc->getDefs()->getRepr()->addChild(repr, NULL); // adds to <defs> and assigns the 'id' attribute
    const gchar * repr_id = repr->attribute("id");
    Inkscape::GC::release(repr);

    gchar *href = g_strdup_printf("#%s", repr_id);
    SP_LPE_ITEM(item)->addPathEffect(href, true);
    g_free(href);
}

void
Effect::createAndApply(EffectType type, SPDocument *doc, SPItem *item)
{
    createAndApply(LPETypeConverter.get_key(type).c_str(), doc, item);
}

Effect::Effect(LivePathEffectObject *lpeobject)
    : apply_to_clippath_and_mask(false),
      _provides_knotholder_entities(false),
      oncanvasedit_it(0),
      is_visible(_("Is visible?"), _("If unchecked, the effect remains applied to the object but is temporarily disabled on canvas"), "is_visible", &wr, this, true),
      show_orig_path(false),
      keep_paths(false),
      is_load(true),
      lpeobj(lpeobject),
      concatenate_before_pwd2(false),
      sp_lpe_item(NULL),
      current_zoom(1),
      upd_params(true),
      current_shape(NULL),
      provides_own_flash_paths(true), // is automatically set to false if providesOwnFlashPaths() is not overridden
      defaultsopen(false),
      is_ready(false) // is automatically set to false if providesOwnFlashPaths() is not overridden
{
    registerParameter( dynamic_cast<Parameter *>(&is_visible) );
    is_visible.widget_is_visible = false;
    current_zoom = 0.0;
}

Effect::~Effect()
{
}

Glib::ustring
Effect::getName() const
{
    if (lpeobj->effecttype_set && LPETypeConverter.is_valid_id(lpeobj->effecttype) )
        return Glib::ustring( _(LPETypeConverter.get_label(lpeobj->effecttype).c_str()) );
    else
        return Glib::ustring( _("No effect") );
}

EffectType
Effect::effectType() const {
    return lpeobj->effecttype;
}

/**
 * Is performed a single time when the effect is freshly applied to a path
 */
void
Effect::doOnApply (SPLPEItem const*/*lpeitem*/)
{
}

void
Effect::setCurrentZoom(double cZ)
{
    current_zoom = cZ;
}

void
Effect::setSelectedNodePoints(std::vector<Geom::Point> sNP)
{
    selectedNodesPoints = sNP;
}

bool
Effect::isNodePointSelected(Geom::Point const &nodePoint) const
{
    if (selectedNodesPoints.size() > 0) {
        using Geom::X;
        using Geom::Y; 
        for (std::vector<Geom::Point>::const_iterator i = selectedNodesPoints.begin();
                i != selectedNodesPoints.end(); ++i) {
            Geom::Point p = *i;
            Geom::Affine transformCoordinate = sp_lpe_item->i2dt_affine();
            Geom::Point p2(nodePoint[X],nodePoint[Y]);
            p2 *= transformCoordinate;
            if (Geom::are_near(p, p2, 0.01)) {
                return true;
            }
        }
    }
    return false;
}

void 
Effect::processObjects(LpeAction lpe_action)
{
    SPDocument * document = SP_ACTIVE_DOCUMENT;
    if (!document) {
        return;
    }
    for (std::vector<Glib::ustring>::iterator el_it = items.begin(); 
         el_it != items.end(); ++el_it) {
        Glib::ustring id = *el_it;
        if (id.empty()) {
            return;
        }
        SPObject *elemref = NULL;
        if ((elemref = document->getObjectById(id.c_str()))) {
            Inkscape::XML::Node * elemnode = elemref->getRepr();
            std::vector<SPItem*> item_list;
            item_list.push_back(SP_ITEM(elemref));
            std::vector<Inkscape::XML::Node*> item_to_select;
            std::vector<SPItem*> item_selected;
            SPCSSAttr *css;
            Glib::ustring css_str;
            SPItem *item = SP_ITEM(elemref);
            switch (lpe_action){
            case LPE_TO_OBJECTS:
                if (item->isHidden()) {
                    item->deleteObject(true);
                } else {
                    if (elemnode->attribute("inkscape:path-effect")) {
                        sp_item_list_to_curves(item_list, item_selected, item_to_select);
                    }
                    elemnode->setAttribute("sodipodi:insensitive", NULL);
                }
                break;

            case LPE_ERASE:
                item->deleteObject(true);
                break;

            case LPE_VISIBILITY:
                css = sp_repr_css_attr_new();
                sp_repr_css_attr_add_from_string(css, elemref->getRepr()->attribute("style"));
                if (!this->isVisible()/* && std::strcmp(elemref->getId(),sp_lpe_item->getId()) != 0*/) {
                    css->setAttribute("display", "none");
                } else {
                    css->setAttribute("display", NULL);
                }
                sp_repr_css_write_string(css,css_str);
                elemnode->setAttribute("style", css_str.c_str());
                break;

            default:
                break;
            }
        }
    }
    if (lpe_action == LPE_ERASE || lpe_action == LPE_TO_OBJECTS) {
        items.clear();
    }
}

/**
 * Is performed each time before the effect is updated.
 */
void
Effect::doBeforeEffect (SPLPEItem const*/*lpeitem*/)
{
    //Do nothing for simple effects
}

void Effect::doAfterEffect (SPLPEItem const* /*lpeitem*/)
{
    is_load = false;
}

void Effect::doOnRemove (SPLPEItem const* /*lpeitem*/)
{
}
void Effect::doOnVisibilityToggled(SPLPEItem const* /*lpeitem*/)
{
}
//secret impl methods (shhhh!)
void Effect::doOnApply_impl(SPLPEItem const* lpeitem)
{
    sp_lpe_item = const_cast<SPLPEItem *>(lpeitem);
    doOnApply(lpeitem);
}

void Effect::doBeforeEffect_impl(SPLPEItem const* lpeitem)
{
    sp_lpe_item = const_cast<SPLPEItem *>(lpeitem);
    doBeforeEffect(lpeitem);
    update_helperpath();
}

/**
 * Effects can have a parameter path set before they are applied by accepting a nonzero number of
 * mouse clicks. This method activates the pen context, which waits for the specified number of
 * clicks. Override Effect::acceptsNumClicks() to return the number of expected mouse clicks.
 */
void
Effect::doAcceptPathPreparations(SPLPEItem *lpeitem)
{
    // switch to pen context
    SPDesktop *desktop = SP_ACTIVE_DESKTOP; // TODO: Is there a better method to find the item's desktop?
    if (!tools_isactive(desktop, TOOLS_FREEHAND_PEN)) {
        tools_switch(desktop, TOOLS_FREEHAND_PEN);
    }

    Inkscape::UI::Tools::ToolBase *ec = desktop->event_context;
    Inkscape::UI::Tools::PenTool *pc = SP_PEN_CONTEXT(ec);
    pc->expecting_clicks_for_LPE = this->acceptsNumClicks();
    pc->waiting_LPE = this;
    pc->waiting_item = lpeitem;
    pc->polylines_only = true;

    ec->desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE,
        g_strdup_printf(_("Please specify a parameter path for the LPE '%s' with %d mouse clicks"),
                        getName().c_str(), acceptsNumClicks()));
}

void
Effect::writeParamsToSVG() {
    std::vector<Inkscape::LivePathEffect::Parameter *>::iterator p;
    for (p = param_vector.begin(); p != param_vector.end(); ++p) {
        (*p)->write_to_SVG();
    }
}

/**
 * If the effect expects a path parameter (specified by a number of mouse clicks) before it is
 * applied, this is the method that processes the resulting path. Override it to customize it for
 * your LPE. But don't forget to call the parent method so that is_ready is set to true!
 */
void
Effect::acceptParamPath (SPPath const*/*param_path*/) {
    setReady();
}

/*
 *  Here be the doEffect function chain:
 */
void
Effect::doEffect (SPCurve * curve)
{
    Geom::PathVector orig_pathv = curve->get_pathvector();

    Geom::PathVector result_pathv = doEffect_path(orig_pathv);

    curve->set_pathvector(result_pathv);
}

Geom::PathVector
Effect::doEffect_path (Geom::PathVector const & path_in)
{
    Geom::PathVector path_out;

    if ( !concatenate_before_pwd2 ) {
        // default behavior
        for (unsigned int i=0; i < path_in.size(); i++) {
            Geom::Piecewise<Geom::D2<Geom::SBasis> > pwd2_in = path_in[i].toPwSb();
            Geom::Piecewise<Geom::D2<Geom::SBasis> > pwd2_out = doEffect_pwd2(pwd2_in);
            Geom::PathVector path = Geom::path_from_piecewise( pwd2_out, LPE_CONVERSION_TOLERANCE);
            // add the output path vector to the already accumulated vector:
            for (unsigned int j=0; j < path.size(); j++) {
                path_out.push_back(path[j]);
            }
        }
    } else {
      // concatenate the path into possibly discontinuous pwd2
        Geom::Piecewise<Geom::D2<Geom::SBasis> > pwd2_in;
        for (unsigned int i=0; i < path_in.size(); i++) {
            pwd2_in.concat( path_in[i].toPwSb() );
        }
        Geom::Piecewise<Geom::D2<Geom::SBasis> > pwd2_out = doEffect_pwd2(pwd2_in);
        path_out = Geom::path_from_piecewise( pwd2_out, LPE_CONVERSION_TOLERANCE);
    }

    return path_out;
}

Geom::Piecewise<Geom::D2<Geom::SBasis> >
Effect::doEffect_pwd2 (Geom::Piecewise<Geom::D2<Geom::SBasis> > const & pwd2_in)
{
    g_warning("Effect has no doEffect implementation");
    return pwd2_in;
}

void
Effect::readallParameters(Inkscape::XML::Node const* repr)
{
    std::vector<Parameter *>::iterator it = param_vector.begin();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    while (it != param_vector.end()) {
        Parameter * param = *it;
        const gchar * key = param->param_key.c_str();
        const gchar * value = repr->attribute(key);
        if (value) {
            bool accepted = param->param_readSVGValue(value);
            if (!accepted) {
                g_warning("Effect::readallParameters - '%s' not accepted for %s", value, key);
            }
        } else {
            Glib::ustring pref_path = (Glib::ustring)"/live_effects/" +
                                       (Glib::ustring)LPETypeConverter.get_key(effectType()).c_str() +
                                       (Glib::ustring)"/" + 
                                       (Glib::ustring)key;
            bool valid = prefs->getEntry(pref_path).isValid();
            if(valid){
                param->param_update_default(prefs->getString(pref_path).c_str());
            } else {
                param->param_set_default();
            }
        }
        ++it;
    }
}

/* This function does not and SHOULD NOT write to XML */
void
Effect::setParameter(const gchar * key, const gchar * new_value)
{
    Parameter * param = getParameter(key);
    if (param) {
        if (new_value) {
            bool accepted = param->param_readSVGValue(new_value);
            if (!accepted) {
                g_warning("Effect::setParameter - '%s' not accepted for %s", new_value, key);
            }
        } else {
            // set default value
            param->param_set_default();
        }
    }
}

void
Effect::registerParameter(Parameter * param)
{
    param_vector.push_back(param);
}


/**
 * Add all registered LPE knotholder handles to the knotholder
 */
void
Effect::addHandles(KnotHolder *knotholder, SPItem *item) {
    using namespace Inkscape::LivePathEffect;

    // add handles provided by the effect itself
    addKnotHolderEntities(knotholder, item);

    // add handles provided by the effect's parameters (if any)
    for (std::vector<Parameter *>::iterator p = param_vector.begin(); p != param_vector.end(); ++p) {
        (*p)->addKnotHolderEntities(knotholder, item);
    }
}

/**
 * Return a vector of PathVectors which contain all canvas indicators for this effect.
 * This is the function called by external code to get all canvas indicators (effect and its parameters)
 * lpeitem = the item onto which this effect is applied
 * @todo change return type to one pathvector, add all paths to one pathvector instead of maintaining a vector of pathvectors
 */
std::vector<Geom::PathVector>
Effect::getCanvasIndicators(SPLPEItem const* lpeitem)
{
    std::vector<Geom::PathVector> hp_vec;

    // add indicators provided by the effect itself
    addCanvasIndicators(lpeitem, hp_vec);

    // add indicators provided by the effect's parameters
    for (std::vector<Parameter *>::iterator p = param_vector.begin(); p != param_vector.end(); ++p) {
        (*p)->addCanvasIndicators(lpeitem, hp_vec);
    }

    return hp_vec;
}

/**
 * Add possible canvas indicators (i.e., helperpaths other than the original path) to \a hp_vec
 * This function should be overwritten by derived effects if they want to provide their own helperpaths.
 */
void
Effect::addCanvasIndicators(SPLPEItem const*/*lpeitem*/, std::vector<Geom::PathVector> &/*hp_vec*/)
{
}

/**
 * Call to a method on nodetool to update the helper path from the effect
 */
void
Effect::update_helperpath() {
    Inkscape::UI::Tools::sp_update_helperpath();
}

/**
 * This *creates* a new widget, management of deletion should be done by the caller
 */
Gtk::Widget *
Effect::newWidget()
{
    // use manage here, because after deletion of Effect object, others might still be pointing to this widget.
    Gtk::VBox * vbox = Gtk::manage( new Gtk::VBox() );

    vbox->set_border_width(5);

    std::vector<Parameter *>::iterator it = param_vector.begin();
    while (it != param_vector.end()) {
        if ((*it)->widget_is_visible) {
            Parameter * param = *it;
            Gtk::Widget * widg = param->param_newWidget();
            Glib::ustring * tip = param->param_getTooltip();
            if (widg) {
                vbox->pack_start(*widg, true, true, 2);
                if (tip) {
                    widg->set_tooltip_text(*tip);
                } else {
                    widg->set_tooltip_text("");
                    widg->set_has_tooltip(false);
                }
            }
        }

        ++it;
    }
    if(Gtk::Widget* widg = defaultParamSet()) {
        vbox->pack_start(*widg, true, true, 2);
    }
    return dynamic_cast<Gtk::Widget *>(vbox);
}

/**
 * This *creates* a new widget, with default values setter
 */
Gtk::Widget *
Effect::defaultParamSet()
{
    // use manage here, because after deletion of Effect object, others might still be pointing to this widget.
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    Gtk::VBox * vbox_expander = Gtk::manage( new Gtk::VBox() );
    Glib::ustring effectname = (Glib::ustring)Inkscape::LivePathEffect::LPETypeConverter.get_label(effectType());
    Glib::ustring effectkey = (Glib::ustring)Inkscape::LivePathEffect::LPETypeConverter.get_key(effectType());
    std::vector<Parameter *>::iterator it = param_vector.begin();
    Inkscape::UI::Widget::Registry * wr;
    bool has_params = false;
    while (it != param_vector.end()) {
        if ((*it)->widget_is_visible) {
            has_params = true;
            Parameter * param = *it;
            Glib::ustring * tip = param->param_getTooltip();
            const gchar * key   = param->param_key.c_str();
            const gchar * label = param->param_label.c_str();
            Glib::ustring value = param->param_getSVGValue();
            Glib::ustring defvalue  = param->param_getDefaultSVGValue();
            Glib::ustring pref_path = "/live_effects/";
            pref_path += effectkey;
            pref_path +="/";
            pref_path += key;
            bool valid = prefs->getEntry(pref_path).isValid();
            const gchar * set_or_upd;
            Glib::ustring def = Glib::ustring(_("<b>Default value:</b> ")) + defvalue + Glib::ustring("\n");
            Glib::ustring ove = Glib::ustring(_("<b>Default value overridden:</b> ")) + Glib::ustring(prefs->getString(pref_path)) + Glib::ustring("\n");
            if (valid) {
                set_or_upd = _("Update");
                def = Glib::ustring(_("<b>Default value:</b> <s>")) + defvalue + Glib::ustring("</s>\n");
            } else {
                set_or_upd = _("Set");
                ove = Glib::ustring(_("<b>Default value overridden:</b> None\n"));
            }
            Glib::ustring cur = Glib::ustring(_("<b>Current parameter value:</b> ")) + value;
            Gtk::HBox * vbox_param = Gtk::manage( new Gtk::HBox(true) );
            Gtk::Label *parameter_label = Gtk::manage(new Gtk::Label(label, Gtk::ALIGN_START));
            parameter_label->set_use_markup(true);
            parameter_label->set_use_underline(true);
            Glib::ustring tooltip = Glib::ustring("<b>") + parameter_label->get_text () + Glib::ustring("</b>\n") + param->param_tooltip + Glib::ustring("\n\n");
            parameter_label->set_ellipsize(Pango::ELLIPSIZE_END);
            parameter_label->set_tooltip_markup((tooltip + def + ove + cur).c_str());
            vbox_param->pack_start(*parameter_label, true, true, 2);
            Gtk::Button *set = Gtk::manage(new Gtk::Button((Glib::ustring)set_or_upd));
            Gtk::Button *unset = Gtk::manage(new Gtk::Button(Glib::ustring(_("Unset"))));
            unset->signal_clicked().connect(sigc::bind<Glib::ustring, Glib::ustring, Glib::ustring, Glib::ustring, Gtk::Label *,Gtk::Button *, Gtk::Button *>(sigc::mem_fun(*this, &Effect::unsetDefaultParam), pref_path, tooltip, value, defvalue, parameter_label, set, unset));
            
            set->signal_clicked().connect(sigc::bind<Glib::ustring, Glib::ustring, Glib::ustring, Glib::ustring, Gtk::Label *,Gtk::Button *, Gtk::Button *>(sigc::mem_fun(*this, &Effect::setDefaultParam), pref_path, tooltip, value, defvalue, parameter_label, set, unset));
            if (!valid) {
                unset->set_sensitive(false);
            }
            vbox_param->pack_start(*set, true, true, 2);
            vbox_param->pack_start(*unset, true, true, 2);

            vbox_expander->pack_start(*vbox_param, true, true, 2);
        }
        ++it;
    }
    Glib::ustring tip = "<b>" + effectname + (Glib::ustring)_("</b>: Set default parameters");
    Gtk::Expander * expander = Gtk::manage(new Gtk::Expander(tip));
    expander->set_use_markup(true);
    expander->add(*vbox_expander);
    expander->set_expanded(defaultsopen);
    //expander->set_size_request(-1, 90);
    expander->property_expanded().signal_changed().connect(sigc::bind<0>(sigc::mem_fun(*this, &Effect::onDefaultsExpanderChanged), expander ));
    if (has_params) {
        Gtk::Widget *vboxwidg = dynamic_cast<Gtk::Widget *>(expander);
        vboxwidg->set_margin_bottom(5);
        vboxwidg->set_margin_top(5);
        return vboxwidg;
    } else {
        return NULL;
    }
}

void
Effect::onDefaultsExpanderChanged(Gtk::Expander * expander)
{
    defaultsopen = expander->get_expanded();
}

void
Effect::setDefaultParam(Glib::ustring pref_path, Glib::ustring tooltip, Glib::ustring value, Glib::ustring defvalue, Gtk::Label *parameter_label, Gtk::Button *set , Gtk::Button *unset)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setString(pref_path, value);
    gchar * label = _("Update");
    set->set_label((Glib::ustring)label);
    unset->set_sensitive(true);
    Glib::ustring def = Glib::ustring(_("<b>Default value:</b> <s>")) + defvalue + Glib::ustring("</s>\n");
    Glib::ustring ove = Glib::ustring(_("<b>Default value overridden:</b> ")) + value + Glib::ustring("\n");
    Glib::ustring cur = Glib::ustring(_("<b>Current parameter value:</b> ")) + value;
    parameter_label->set_tooltip_markup((tooltip + def + ove + cur).c_str());
}

void
Effect::unsetDefaultParam(Glib::ustring pref_path, Glib::ustring tooltip, Glib::ustring value, Glib::ustring defvalue, Gtk::Label *parameter_label, Gtk::Button *set , Gtk::Button *unset)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->remove(pref_path);
    gchar * label = _("Set");
    set->set_label((Glib::ustring)label);
    unset->set_sensitive(false);
    Glib::ustring def = Glib::ustring(_("<b>Default value:</b> ")) + defvalue + Glib::ustring("\n");
    Glib::ustring ove = Glib::ustring(_("<b>Default value overridden:</b> None\n"));
    Glib::ustring cur = Glib::ustring(_("<b>Current parameter value:</b> ")) +value;
    parameter_label->set_tooltip_markup((tooltip + def + ove + cur).c_str());
}

Inkscape::XML::Node *Effect::getRepr()
{
    return lpeobj->getRepr();
}

SPDocument *Effect::getSPDoc()
{
    if (lpeobj->document == NULL) {
        g_message("Effect::getSPDoc() returns NULL");
    }
    return lpeobj->document;
}

Parameter *
Effect::getParameter(const char * key)
{
    Glib::ustring stringkey(key);

    if (param_vector.empty()) return NULL;
    std::vector<Parameter *>::iterator it = param_vector.begin();
    while (it != param_vector.end()) {
        Parameter * param = *it;
        if ( param->param_key == key) {
            return param;
        }

        ++it;
    }

    return NULL;
}

Parameter *
Effect::getNextOncanvasEditableParam()
{
    if (param_vector.size() == 0) // no parameters
        return NULL;

    oncanvasedit_it++;
    if (oncanvasedit_it >= static_cast<int>(param_vector.size())) {
        oncanvasedit_it = 0;
    }
    int old_it = oncanvasedit_it;

    do {
        Parameter * param = param_vector[oncanvasedit_it];
        if(param && param->oncanvas_editable) {
            return param;
        } else {
            oncanvasedit_it++;
            if (oncanvasedit_it == static_cast<int>(param_vector.size())) {  // loop round the map
                oncanvasedit_it = 0;
            }
        }
    } while (oncanvasedit_it != old_it); // iterate until complete loop through map has been made

    return NULL;
}

void
Effect::editNextParamOncanvas(SPItem * item, SPDesktop * desktop)
{
    if (!desktop) return;

    Parameter * param = getNextOncanvasEditableParam();
    if (param) {
        param->param_editOncanvas(item, desktop);
        gchar *message = g_strdup_printf(_("Editing parameter <b>%s</b>."), param->param_label.c_str());
        desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, message);
        g_free(message);
    } else {
        desktop->messageStack()->flash( Inkscape::WARNING_MESSAGE,
                                        _("None of the applied path effect's parameters can be edited on-canvas.") );
    }
}

/* This function should reset the defaults and is used for example to initialize an effect right after it has been applied to a path
* The nice thing about this is that this function can use knowledge of the original path and set things accordingly for example to the size or origin of the original path!
*/
void
Effect::resetDefaults(SPItem const* /*item*/)
{
    std::vector<Inkscape::LivePathEffect::Parameter *>::iterator p;
    for (p = param_vector.begin(); p != param_vector.end(); ++p) {
        (*p)->param_set_default();
        (*p)->write_to_SVG();
    }
}

//Activate handle your transform filling your effect on SPPath.cpp
void
Effect::transform_multiply(Geom::Affine const& postmul, bool set)
{
    // cycle through all parameters. Most parameters will not need transformation, but path and point params do.
    for (std::vector<Parameter *>::iterator it = param_vector.begin(); it != param_vector.end(); ++it) {
        Parameter * param = *it;
        param->param_transform_multiply(postmul, set);
    }
}

bool
Effect::providesKnotholder() const
{
    // does the effect actively provide any knotholder entities of its own?
    if (_provides_knotholder_entities) {
        return true;
    }

    // otherwise: are there any parameters that have knotholderentities?
    for (std::vector<Parameter *>::const_iterator p = param_vector.begin(); p != param_vector.end(); ++p) {
        if ((*p)->providesKnotHolderEntities()) {
            return true;
        }
    }

    return false;
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
