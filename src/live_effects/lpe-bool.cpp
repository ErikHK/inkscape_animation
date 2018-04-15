/*
 * Boolean operation live path effect
 *
 * Copyright (C) 2016-2017 Michael Soegtrop
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <glibmm/i18n.h>
#include <math.h>
#include <string.h>
#include <algorithm>

#include "live_effects/lpe-bool.h"

#include "display/curve.h"

#include "2geom/path.h"
#include "2geom/bezier-curve.h"
#include "2geom/path-sink.h"
#include "2geom/affine.h"
#include "2geom/svg-path-parser.h"

#include "helper/geom.h"

#include "splivarot.h"
#include "livarot/Path.h"
#include "livarot/Shape.h"
#include "livarot/path-description.h"

namespace Inkscape {
namespace LivePathEffect {

// Define an extended boolean operation type

static const Util::EnumData<LPEBool::bool_op_ex> BoolOpData[LPEBool::bool_op_ex_count] = {
    { LPEBool::bool_op_ex_union, N_("union"), "union" },
    { LPEBool::bool_op_ex_inters, N_("intersection"), "inters" },
    { LPEBool::bool_op_ex_diff, N_("difference"), "diff" },
    { LPEBool::bool_op_ex_symdiff, N_("symmetric difference"), "symdiff" },
    { LPEBool::bool_op_ex_cut, N_("division"), "cut" },
    // Note on naming of operations:
    // bool_op_cut is called "Division" in the manu, see sp_selected_path_cut
    // bool_op_slice is called "Cut path" in the menu, see sp_selected_path_slice
    { LPEBool::bool_op_ex_slice, N_("cut"), "slice" },
    { LPEBool::bool_op_ex_slice_inside, N_("cut inside"), "slice-inside" },
    { LPEBool::bool_op_ex_slice_outside, N_("cut outside"), "slice-outside" },
};

static const Util::EnumDataConverter<LPEBool::bool_op_ex> BoolOpConverter(BoolOpData, sizeof(BoolOpData) / sizeof(*BoolOpData));

static const Util::EnumData<fill_typ> FillTypeData[] = {
    { fill_oddEven, N_("odd-even"), "oddeven" },
    { fill_nonZero, N_("non-zero"), "nonzero" },
    { fill_positive, N_("positive"), "positive" },
    { fill_justDont, N_("from curve"), "from-curve" }
};

static const Util::EnumDataConverter<fill_typ> FillTypeConverter(FillTypeData, sizeof(FillTypeData) / sizeof(*FillTypeData));

static const Util::EnumData<fill_typ> FillTypeDataThis[] = {
    { fill_oddEven, N_("odd-even"), "oddeven" },
    { fill_nonZero, N_("non-zero"), "nonzero" },
    { fill_positive, N_("positive"), "positive" }
};

static const Util::EnumDataConverter<fill_typ> FillTypeConverterThis(FillTypeDataThis, sizeof(FillTypeDataThis) / sizeof(*FillTypeDataThis));

LPEBool::LPEBool(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    operand_path(_("Operand path:"), _("Operand for the boolean operation"), "operand-path", &wr, this),
    bool_operation(_("Operation:"), _("Boolean Operation"), "operation", BoolOpConverter, &wr, this, bool_op_ex_union),
    swap_operands(_("Swap operands:"), _("Swap operands (useful e.g. for difference)"), "swap-operands", &wr, this),
    rmv_inner(_("Remove inner:"), _("For cut operations: remove inner (non-contour) lines of cutting path to avoid invisible extra points"), "rmv-inner", &wr, this),
    fill_type_this(_("Fill type this:"), _("Fill type (winding mode) for this path"), "filltype-this", FillTypeConverterThis, &wr, this, fill_oddEven),
    fill_type_operand(_("Fill type operand:"), _("Fill type (winding mode) for operand path"), "filltype-operand", FillTypeConverter, &wr, this, fill_justDont)
{
    registerParameter(&operand_path);
    registerParameter(&bool_operation);
    registerParameter(&swap_operands);
    registerParameter(&rmv_inner);
    registerParameter(&fill_type_this);
    registerParameter(&fill_type_operand);

    show_orig_path = true;
}

LPEBool::~LPEBool()
{

}

void LPEBool::resetDefaults(SPItem const * /*item*/)
{
}

bool cmp_cut_position(const Path::cut_position &a, const Path::cut_position &b)
{
    return a.piece == b.piece ? a.t < b.t : a.piece < b.piece;
}

Geom::PathVector
sp_pathvector_boolop_slice_intersect(Geom::PathVector const &pathva, Geom::PathVector const &pathvb, bool inside, fill_typ fra, fill_typ frb)
{
    // This is similar to sp_pathvector_boolop/bool_op_slice, but keeps only edges inside the cutter area.
    // The code is also based on sp_pathvector_boolop_slice.
    //
    // We have two paths on input
    // - a closed area which is used to cut out pieces from a contour (called area below)
    // - a contour which is cut into pieces by the border of thr area (called contour below)
    //
    // The code below works in the following steps
    // (a) Convert the area to a shape, so that we can ask the winding number for any point
    // (b) Add both, the contour and the area to a single shape and intersect them
    // (c) Find the intersection points between area border and contour (vector toCut)
    // (d) Split the original contour at the intersection points
    // (e) check for each contour edge in combined shape if its center is inside the area - if not discard it
    // (f) create a vector of all inside edges
    // (g) convert the piece numbers to the piece numbers after applying the cuts
    // (h) fill a bool vector with information which pieces are in
    // (i) filter the descr_cmd of the result path with this bool vector
    //
    // The main inefficiency here is step (e) because I use a winding function of the area-shape which goes
    // through the complete edge list for each point I ask for, so effort is n-edges-contour * n-edges-area.
    // It is tricky to improve this without building into the livarot code.
    // One way might be to decide at the intersection points which edges touching the intersection points are
    // in by making a loop through all edges on the intersection vertex. Since this is a directed non intersecting
    // graph, this should provide sufficient information.
    // But since I anyway will change this to the new mechanism some time speed is fairly ok, I didn't look into this.


    // extract the livarot Paths from the source objects
    // also get the winding rule specified in the style
    // Livarot's outline of arcs is broken. So convert the path to linear and cubics only, for which the outline is created correctly.
    Path *contour_path = Path_for_pathvector(pathv_to_linear_and_cubic_beziers(pathva));
    Path *area_path = Path_for_pathvector(pathv_to_linear_and_cubic_beziers(pathvb));

    // Shapes from above paths
    Shape *area_shape = new Shape;
    Shape *combined_shape = new Shape;
    Shape *combined_inters = new Shape;

    // Add the area (process to intersection free shape)
    area_path->ConvertWithBackData(1.0);
    area_path->Fill(combined_shape, 1);

    // Convert this to a shape with full winding information
    area_shape->ConvertToShape(combined_shape, frb);

    // Add the contour to the combined path (just add, no winding processing)
    contour_path->ConvertWithBackData(1.0);
    contour_path->Fill(combined_shape, 0, true, false, false);

    // Intersect the area and the contour - no fill processing
    combined_inters->ConvertToShape(combined_shape, fill_justDont);

    // Result path
    Path *result_path = new Path;
    result_path->SetBackData(false);

    // Cutting positions for contour
    std::vector<Path::cut_position> toCut;

    if (combined_inters->hasBackData()) {
        // should always be the case, but ya never know
        {
            for (int i = 0; i < combined_inters->numberOfPoints(); i++) {
                if (combined_inters->getPoint(i).totalDegree() > 2) {
                    // possibly an intersection
                    // we need to check that at least one edge from the source path is incident to it
                    // before we declare it's an intersection
                    int cb = combined_inters->getPoint(i).incidentEdge[FIRST];
                    int   nbOrig = 0;
                    int   nbOther = 0;
                    int   piece = -1;
                    float t = 0.0;
                    while (cb >= 0 && cb < combined_inters->numberOfEdges()) {
                        if (combined_inters->ebData[cb].pathID == 0) {
                            // the source has an edge incident to the point, get its position on the path
                            piece = combined_inters->ebData[cb].pieceID;
                            if (combined_inters->getEdge(cb).st == i) {
                                t = combined_inters->ebData[cb].tSt;
                            } else {
                                t = combined_inters->ebData[cb].tEn;
                            }
                            nbOrig++;
                        }
                        if (combined_inters->ebData[cb].pathID == 1) {
                            nbOther++;    // the cut is incident to this point
                        }
                        cb = combined_inters->NextAt(i, cb);
                    }
                    if (nbOrig > 0 && nbOther > 0) {
                        // point incident to both path and cut: an intersection
                        // note that you only keep one position on the source; you could have degenerate
                        // cases where the source crosses itself at this point, and you wouyld miss an intersection
                        Path::cut_position cutpos;
                        cutpos.piece = piece;
                        cutpos.t = t;
                        toCut.push_back(cutpos);
                    }
                }
            }
        }
        {
            // remove the edges from the intersection polygon
            int i = combined_inters->numberOfEdges() - 1;
            for (; i >= 0; i--) {
                if (combined_inters->ebData[i].pathID == 1) {
                    combined_inters->SubEdge(i);
                } else {
                    const Shape::dg_arete &edge = combined_inters->getEdge(i);
                    const Shape::dg_point &start = combined_inters->getPoint(edge.st);
                    const Shape::dg_point &end = combined_inters->getPoint(edge.en);
                    Geom::Point mid = 0.5 * (start.x + end.x);
                    int wind = area_shape->PtWinding(mid);
                    if (wind == 0) {
                        combined_inters->SubEdge(i);
                    }
                }
            }
        }
    }

    // create a vector of pieces, which are in the intersection
    std::vector<Path::cut_position> inside_pieces(combined_inters->numberOfEdges());
    for (int i = 0; i < combined_inters->numberOfEdges(); i++) {
        inside_pieces[i].piece = combined_inters->ebData[i].pieceID;
        // Use the t middle point, this is safe to compare with values from toCut in the presence of roundoff errors
        inside_pieces[i].t = 0.5 * (combined_inters->ebData[i].tSt + combined_inters->ebData[i].tEn);
    }
    std::sort(inside_pieces.begin(), inside_pieces.end(), cmp_cut_position);

    // sort cut positions
    std::sort(toCut.begin(), toCut.end(), cmp_cut_position);

    // Compute piece ids after ConvertPositionsToMoveTo
    {
        int idIncr = 0;
        std::vector<Path::cut_position>::iterator itPiece = inside_pieces.begin();
        std::vector<Path::cut_position>::iterator itCut = toCut.begin();
        while (itPiece != inside_pieces.end()) {
            while (itCut != toCut.end() && cmp_cut_position(*itCut, *itPiece)) {
                ++itCut;
                idIncr += 2;
            }
            itPiece->piece += idIncr;
            ++itPiece;
        }
    }

    // Copy the original path to result and cut at the intersection points
    result_path->Copy(contour_path);
    result_path->ConvertPositionsToMoveTo(toCut.size(), toCut.data());   // cut where you found intersections

    // Create an array of bools which states which pieces are in
    std::vector<bool> inside_flags(result_path->descr_cmd.size(), false);
    for (std::vector<Path::cut_position>::iterator itPiece = inside_pieces.begin(); itPiece != inside_pieces.end(); ++itPiece) {
        inside_flags[ itPiece->piece ] = true;
        // also enable the element -1 to get the MoveTo
        if (itPiece->piece >= 1) {
            inside_flags[ itPiece->piece - 1 ] = true;
        }
    }

#if 0 // CONCEPT TESTING
    //Check if the inside/outside verdict is consistent - just for testing the concept
    // Retrieve the pieces
    int nParts = 0;
    Path **parts = result_path->SubPaths(nParts, false);

    // Each piece should be either fully in or fully out
    int iPiece = 0;
    for (int iPart = 0; iPart < nParts; iPart++) {
        bool andsum = true;
        bool orsum = false;
        for (int iCmd = 0; iCmd < parts[iPart]->descr_cmd.size(); iCmd++, iPiece++) {
            andsum = andsum && inside_flags[ iPiece ];
            orsum = andsum || inside_flags[ iPiece ];
        }

        if (andsum != orsum) {
            g_warning("Inconsistent inside/outside verdict for part=%d", iPart);
        }
    }
    g_free(parts);
#endif

    // iterate over the commands of a path and keep those which are inside
    int iDest = 0;
    for (int iSrc = 0; iSrc < result_path->descr_cmd.size(); iSrc++) {
        if (inside_flags[iSrc] == inside) {
            result_path->descr_cmd[iDest++] = result_path->descr_cmd[iSrc];
        } else {
            delete result_path->descr_cmd[iSrc];
        }
    }
    result_path->descr_cmd.resize(iDest);

    delete combined_inters;
    delete combined_shape;
    delete area_shape;
    delete contour_path;
    delete area_path;

    gchar *result_str = result_path->svg_dump_path();
    Geom::PathVector outres =  Geom::parse_svg_path(result_str);
    // CONCEPT TESTING g_warning( "%s", result_str );
    g_free(result_str);
    delete result_path;

    return outres;
}

// remove inner contours
Geom::PathVector
sp_pathvector_boolop_remove_inner(Geom::PathVector const &pathva, fill_typ fra)
{
    Geom::PathVector patht;
    Path *patha = Path_for_pathvector(pathv_to_linear_and_cubic_beziers(pathva));

    Shape *shape = new Shape;
    Shape *shapeshape = new Shape;
    Path *resultp = new Path;
    resultp->SetBackData(false);

    patha->ConvertWithBackData(0.1);
    patha->Fill(shape, 0);
    shapeshape->ConvertToShape(shape, fra);
    shapeshape->ConvertToForme(resultp, 1, &patha);

    delete shape;
    delete shapeshape;
    delete patha;

    gchar *result_str = resultp->svg_dump_path();
    Geom::PathVector resultpv =  Geom::parse_svg_path(result_str);
    g_free(result_str);

    delete resultp;
    return resultpv;
}

static fill_typ GetFillTyp(SPItem *item)
{
    SPCSSAttr *css = sp_repr_css_attr(item->getRepr(), "style");
    gchar const *val = sp_repr_css_property(css, "fill-rule", NULL);
    if (val && strcmp(val, "nonzero") == 0) {
        return fill_nonZero;
    } else if (val && strcmp(val, "evenodd") == 0) {
        return fill_oddEven;
    } else {
        return fill_nonZero;
    }
}

void LPEBool::doEffect(SPCurve *curve)
{
    Geom::PathVector path_in = curve->get_pathvector();

    if (operand_path.linksToPath() && operand_path.getObject()) {
        bool_op_ex op = bool_operation.get_value();
        bool swap =  swap_operands.get_value();

        Geom::PathVector path_a = swap ? operand_path.get_pathvector() : path_in;
        Geom::PathVector path_b = swap ? path_in : operand_path.get_pathvector();

        // TODO: I would like to use the original objects fill rule if the UI selected rule is fill_justDont.
        // But it doesn't seem possible to access them from here, because SPCurve is not derived from SPItem.
        // The nearest function in the call stack, where this is available is SPLPEItem::performPathEffect (this is then an SPItem)
        // For the parameter curve, this is possible.
        // fill_typ fill_this    = fill_type_this.   get_value()!=fill_justDont ? fill_type_this.get_value()    : GetFillTyp( curve ) ;
        fill_typ fill_this    = fill_type_this.get_value();
        fill_typ fill_operand = fill_type_operand.get_value() != fill_justDont ? fill_type_operand.get_value() : GetFillTyp(operand_path.getObject());

        fill_typ fill_a = swap ? fill_operand : fill_this;
        fill_typ fill_b = swap ? fill_this : fill_operand;

        if (rmv_inner.get_value()) {
            path_b = sp_pathvector_boolop_remove_inner(path_b, fill_b);
        }

        Geom::PathVector path_out;

        if (op == bool_op_ex_slice) {
            // For slicing, the bool op is added to the line group which is sliced, not the cut path. This swapped order is correct.
            path_out = sp_pathvector_boolop(path_b, path_a, to_bool_op(op), fill_b, fill_a);
        } else if (op == bool_op_ex_slice_inside) {
            path_out = sp_pathvector_boolop_slice_intersect(path_a, path_b, true, fill_a, fill_b);
        } else if (op == bool_op_ex_slice_outside) {
            path_out = sp_pathvector_boolop_slice_intersect(path_a, path_b, false, fill_a, fill_b);
        } else {
            path_out = sp_pathvector_boolop(path_a, path_b, to_bool_op(op), fill_a, fill_b);
        }
        curve->set_pathvector(path_out);
    }
}

} // namespace LivePathEffect
} /* namespace Inkscape */
