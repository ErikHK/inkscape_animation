/*
 *  splivarot.cpp
 *  Inkscape
 *
 *  Created by fred on Fri Dec 05 2003.
 *  tweaked endlessly by bulia byak <buliabyak@users.sf.net>
 *  public domain
 *
 */

/*
 * contains lots of stitched pieces of path-chemistry.c
 */

#ifdef HAVE_CONFIG_H
#endif

#include <cstring>
#include <string>
#include <vector>

#include <glib.h>
#include <glibmm/i18n.h>

#include <2geom/svg-path-parser.h> // to get from SVG on boolean to Geom::Path
#include <2geom/svg-path-writer.h>

#include "splivarot.h"

#include "document-undo.h"
#include "document.h"
#include "layer-model.h"
#include "message-stack.h"
#include "selection.h"
#include "text-editing.h"
#include "verbs.h"

#include "helper/geom.h"

#include "livarot/Path.h"
#include "livarot/Shape.h"

#include "object/sp-flowtext.h"
#include "object/sp-image.h"
#include "object/sp-marker.h"
#include "object/sp-path.h"
#include "object/sp-text.h"
#include "style.h"

#include "svg/svg.h"

#include "util/units.h"            // to get abbr for document units

#include "xml/repr-sorting.h"
#include "xml/repr.h"

using Inkscape::DocumentUndo;

bool   Ancetre(Inkscape::XML::Node *a, Inkscape::XML::Node *who);

void sp_selected_path_do_offset(SPDesktop *desktop, bool expand, double prefOffset);
void sp_selected_path_create_offset_object(SPDesktop *desktop, int expand, bool updating);

bool Inkscape::ObjectSet::pathUnion(const bool skip_undo) {
    BoolOpErrors result = pathBoolOp(bool_op_union, skip_undo, false, SP_VERB_SELECTION_UNION, _("Union"));
    return DONE == result;
}

bool
Inkscape::ObjectSet::pathIntersect(const bool skip_undo)
{
    BoolOpErrors result = pathBoolOp(bool_op_inters, skip_undo, false, SP_VERB_SELECTION_INTERSECT, _("Intersection"));
    return DONE == result;
}

bool
Inkscape::ObjectSet::pathDiff(const bool skip_undo)
{
    BoolOpErrors result = pathBoolOp(bool_op_diff, skip_undo, false, SP_VERB_SELECTION_DIFF, _("Difference"));
    return DONE == result;
}

bool
Inkscape::ObjectSet::pathSymDiff(const bool skip_undo)
{
    BoolOpErrors result = pathBoolOp(bool_op_symdiff, skip_undo, false, SP_VERB_SELECTION_SYMDIFF, _("Exclusion"));
    return DONE == result;
}

bool
Inkscape::ObjectSet::pathCut(const bool skip_undo)
{
    BoolOpErrors result = pathBoolOp(bool_op_cut, skip_undo, false, SP_VERB_SELECTION_CUT, _("Division"));
    return DONE == result;
}

bool
Inkscape::ObjectSet::pathSlice(const bool skip_undo)
{
    BoolOpErrors result = pathBoolOp(bool_op_slice, skip_undo, false, SP_VERB_SELECTION_SLICE, _("Cut path"));
    return DONE == result;
}

// helper for printing error messages, regardless of whether we have a GUI or not
// If desktop == NULL, errors will be shown on stderr
static void
boolop_display_error_message(SPDesktop *desktop, Glib::ustring const &msg)
{
    if (desktop) {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, msg);
    } else {
        g_printerr("%s\n", msg.c_str());
    }
}

// boolean operations PathVectors A,B -> PathVector result.
// This is derived from sp_selected_path_boolop
// take the source paths from the file, do the operation, delete the originals and add the results
// fra,fra are fill_rules for PathVectors a,b
Geom::PathVector 
sp_pathvector_boolop(Geom::PathVector const &pathva, Geom::PathVector const &pathvb, bool_op bop, fill_typ fra, fill_typ frb)
{        

    // extract the livarot Paths from the source objects
    // also get the winding rule specified in the style
    int nbOriginaux = 2;
    std::vector<Path *> originaux(nbOriginaux);
    std::vector<FillRule> origWind(nbOriginaux);
    origWind[0]=fra;
    origWind[1]=frb;
    Geom::PathVector patht;
    // Livarot's outline of arcs is broken. So convert the path to linear and cubics only, for which the outline is created correctly. 
    originaux[0] = Path_for_pathvector(pathv_to_linear_and_cubic_beziers( pathva));
    originaux[1] = Path_for_pathvector(pathv_to_linear_and_cubic_beziers( pathvb));

    // some temporary instances, first
    Shape *theShapeA = new Shape;
    Shape *theShapeB = new Shape;
    Shape *theShape = new Shape;
    Path *res = new Path;
    res->SetBackData(false);
    Path::cut_position  *toCut=NULL;
    int                  nbToCut=0;

    if ( bop == bool_op_inters || bop == bool_op_union || bop == bool_op_diff || bop == bool_op_symdiff ) {
        // true boolean op
        // get the polygons of each path, with the winding rule specified, and apply the operation iteratively
        originaux[0]->ConvertWithBackData(0.1);

        originaux[0]->Fill(theShape, 0);

        theShapeA->ConvertToShape(theShape, origWind[0]);

        originaux[1]->ConvertWithBackData(0.1);

        originaux[1]->Fill(theShape, 1);

        theShapeB->ConvertToShape(theShape, origWind[1]);
        
        theShape->Booleen(theShapeB, theShapeA, bop);

    } else if ( bop == bool_op_cut ) {
        // cuts= sort of a bastard boolean operation, thus not the axact same modus operandi
        // technically, the cut path is not necessarily a polygon (thus has no winding rule)
        // it is just uncrossed, and cleaned from duplicate edges and points
        // then it's fed to Booleen() which will uncross it against the other path
        // then comes the trick: each edge of the cut path is duplicated (one in each direction),
        // thus making a polygon. the weight of the edges of the cut are all 0, but
        // the Booleen need to invert the ones inside the source polygon (for the subsequent
        // ConvertToForme)

        // the cut path needs to have the highest pathID in the back data
        // that's how the Booleen() function knows it's an edge of the cut

        // FIXME: this gives poor results, the final paths are full of extraneous nodes. Decreasing
        // ConvertWithBackData parameter below simply increases the number of nodes, so for now I
        // left it at 1.0. Investigate replacing this by a combination of difference and
        // intersection of the same two paths. -- bb
        {
            Path* swap=originaux[0];originaux[0]=originaux[1];originaux[1]=swap;
            int   swai=origWind[0];origWind[0]=origWind[1];origWind[1]=(fill_typ)swai;
        }
        originaux[0]->ConvertWithBackData(1.0);

        originaux[0]->Fill(theShape, 0);

        theShapeA->ConvertToShape(theShape, origWind[0]);

        originaux[1]->ConvertWithBackData(1.0);

        originaux[1]->Fill(theShape, 1,false,false,false); //do not closeIfNeeded

        theShapeB->ConvertToShape(theShape, fill_justDont); // fill_justDont doesn't computes winding numbers

        // les elements arrivent en ordre inverse dans la liste
        theShape->Booleen(theShapeB, theShapeA, bool_op_cut, 1);

    } else if ( bop == bool_op_slice ) {
        // slice is not really a boolean operation
        // you just put the 2 shapes in a single polygon, uncross it
        // the points where the degree is > 2 are intersections
        // just check it's an intersection on the path you want to cut, and keep it
        // the intersections you have found are then fed to ConvertPositionsToMoveTo() which will
        // make new subpath at each one of these positions
        // inversion pour l'opration
        {
            Path* swap=originaux[0];originaux[0]=originaux[1];originaux[1]=swap;
            int   swai=origWind[0];origWind[0]=origWind[1];origWind[1]=(fill_typ)swai;
        }
        originaux[0]->ConvertWithBackData(1.0);

        originaux[0]->Fill(theShapeA, 0,false,false,false); // don't closeIfNeeded

        originaux[1]->ConvertWithBackData(1.0);

        originaux[1]->Fill(theShapeA, 1,true,false,false);// don't closeIfNeeded and just dump in the shape, don't reset it

        theShape->ConvertToShape(theShapeA, fill_justDont);

        if ( theShape->hasBackData() ) {
            // should always be the case, but ya never know
            {
                for (int i = 0; i < theShape->numberOfPoints(); i++) {
                    if ( theShape->getPoint(i).totalDegree() > 2 ) {
                        // possibly an intersection
                        // we need to check that at least one edge from the source path is incident to it
                        // before we declare it's an intersection
                        int cb = theShape->getPoint(i).incidentEdge[FIRST];
                        int   nbOrig=0;
                        int   nbOther=0;
                        int   piece=-1;
                        float t=0.0;
                        while ( cb >= 0 && cb < theShape->numberOfEdges() ) {
                            if ( theShape->ebData[cb].pathID == 0 ) {
                                // the source has an edge incident to the point, get its position on the path
                                piece=theShape->ebData[cb].pieceID;
                                if ( theShape->getEdge(cb).st == i ) {
                                    t=theShape->ebData[cb].tSt;
                                } else {
                                    t=theShape->ebData[cb].tEn;
                                }
                                nbOrig++;
                            }
                            if ( theShape->ebData[cb].pathID == 1 ) nbOther++; // the cut is incident to this point
                            cb=theShape->NextAt(i, cb);
                        }
                        if ( nbOrig > 0 && nbOther > 0 ) {
                            // point incident to both path and cut: an intersection
                            // note that you only keep one position on the source; you could have degenerate
                            // cases where the source crosses itself at this point, and you wouyld miss an intersection
                            toCut=(Path::cut_position*)realloc(toCut, (nbToCut+1)*sizeof(Path::cut_position));
                            toCut[nbToCut].piece=piece;
                            toCut[nbToCut].t=t;
                            nbToCut++;
                        }
                    }
                }
            }
            {
                // i think it's useless now
                int i = theShape->numberOfEdges() - 1;
                for (;i>=0;i--) {
                    if ( theShape->ebData[i].pathID == 1 ) {
                        theShape->SubEdge(i);
                    }
                }
            }

        }
    }

    int*    nesting=NULL;
    int*    conts=NULL;
    int     nbNest=0;
    // pour compenser le swap juste avant
    if ( bop == bool_op_slice ) {
//    theShape->ConvertToForme(res, nbOriginaux, originaux, true);
//    res->ConvertForcedToMoveTo();
        res->Copy(originaux[0]);
        res->ConvertPositionsToMoveTo(nbToCut, toCut); // cut where you found intersections
        free(toCut);
    } else if ( bop == bool_op_cut ) {
        // il faut appeler pour desallouer PointData (pas vital, mais bon)
        // the Booleen() function did not deallocate the point_data array in theShape, because this
        // function needs it.
        // this function uses the point_data to get the winding number of each path (ie: is a hole or not)
        // for later reconstruction in objects, you also need to extract which path is parent of holes (nesting info)
        theShape->ConvertToFormeNested(res, nbOriginaux, &originaux[0], 1, nbNest, nesting, conts);
    } else {
        theShape->ConvertToForme(res, nbOriginaux, &originaux[0]);
    }

    delete theShape;
    delete theShapeA;
    delete theShapeB;
    delete originaux[0];
    delete originaux[1];

    gchar *result_str = res->svg_dump_path();
    Geom::PathVector outres =  Geom::parse_svg_path(result_str);
    g_free(result_str);

    delete res;
    return outres;
}


/* Convert from a livarot path to a 2geom PathVector */
Geom::PathVector pathliv_to_pathvector(Path *pathliv){
    Geom::PathVector outres =  Geom::parse_svg_path(pathliv->svg_dump_path());
    return outres;
}


// boolean operations on the desktop
// take the source paths from the file, do the operation, delete the originals and add the results
BoolOpErrors Inkscape::ObjectSet::pathBoolOp(bool_op bop, const bool skip_undo, const bool checked, const unsigned int verb, const Glib::ustring description)
{
    if (nullptr != desktop() && !checked) {
        SPDocument *doc = desktop()->getDocument();
        BoolOpErrors returnCode = ObjectSet::pathBoolOp(bop, true, true);
        switch(returnCode) {
        case ERR_TOO_LESS_PATHS_1:
            boolop_display_error_message(desktop(), _("Select <b>at least 1 path</b> to perform a boolean union."));
            break;
        case ERR_TOO_LESS_PATHS_2:
            boolop_display_error_message(desktop(), _("Select <b>at least 2 paths</b> to perform a boolean operation."));
            break;
        case ERR_NO_PATHS:
            boolop_display_error_message(desktop(), _("One of the objects is <b>not a path</b>, cannot perform boolean operation."));
            break;
        case ERR_Z_ORDER:
            boolop_display_error_message(desktop(), _("Unable to determine the <b>z-order</b> of the objects selected for difference, XOR, division, or path cut."));
            break;
        case DONE_NO_PATH:
            if (!skip_undo) { 
                DocumentUndo::done(doc, SP_VERB_NONE, description);
            }
            break;
        case DONE:
            if (!skip_undo) { 
                DocumentUndo::done(doc, verb, description);
            }
            break;
        case DONE_NO_ACTION:
            // Do nothing (?)
            break;
        }
        return returnCode;
    }

    SPDocument *doc = document();
    std::vector<SPItem*> il(items().begin(), items().end());

    // allow union on a single object for the purpose of removing self overlapse (svn log, revision 13334)
    if (il.size() < 2 && bop != bool_op_union) {
        return ERR_TOO_LESS_PATHS_2;
    }
    else if (il.size() < 1) {
        return ERR_TOO_LESS_PATHS_1;
    }

    g_assert(!il.empty());

    // reverseOrderForOp marks whether the order of the list is the top->down order
    // it's only used when there are 2 objects, and for operations who need to know the
    // topmost object (differences, cuts)
    bool reverseOrderForOp = false;

    if (bop == bool_op_diff || bop == bool_op_cut || bop == bool_op_slice) {
        // check in the tree to find which element of the selection list is topmost (for 2-operand commands only)
        Inkscape::XML::Node *a = il.front()->getRepr();
        Inkscape::XML::Node *b = il.back()->getRepr();

        if (a == NULL || b == NULL) {
            return ERR_Z_ORDER;
        }

        if (Ancetre(a, b)) {
            // a is the parent of b, already in the proper order
        } else if (Ancetre(b, a)) {
            // reverse order
            reverseOrderForOp = true;
        } else {

            // objects are not in parent/child relationship;
            // find their lowest common ancestor
            Inkscape::XML::Node *parent = LCA(a, b);
            if (parent == NULL) {
                return ERR_Z_ORDER;
            }

            // find the children of the LCA that lead from it to the a and b
            Inkscape::XML::Node *as = AncetreFils(a, parent);
            Inkscape::XML::Node *bs = AncetreFils(b, parent);

            // find out which comes first
            for (Inkscape::XML::Node *child = parent->firstChild(); child; child = child->next()) {
                if (child == as) {
                    /* a first, so reverse. */
                    reverseOrderForOp = true;
                    break;
                }
                if (child == bs)
                    break;
            }
        }
    }

    g_assert(!il.empty());

    // first check if all the input objects have shapes
    // otherwise bail out
    for (std::vector<SPItem*>::const_iterator l = il.begin(); l != il.end(); l++)
    {
        SPItem *item = *l;
        if (!SP_IS_SHAPE(item) && !SP_IS_TEXT(item) && !SP_IS_FLOWTEXT(item))
        {
            return ERR_NO_PATHS;
        }
    }

    // extract the livarot Paths from the source objects
    // also get the winding rule specified in the style
    int nbOriginaux = il.size();
    std::vector<Path *> originaux(nbOriginaux);
    std::vector<FillRule> origWind(nbOriginaux);
    int curOrig;
    {
        curOrig = 0;
        for (std::vector<SPItem*>::const_iterator l = il.begin(); l != il.end(); l++)
        {
            // apply live path effects prior to performing boolean operation
            if (SP_IS_LPE_ITEM(*l)) {
                SP_LPE_ITEM(*l)->removeAllPathEffects(true);
            }

            SPCSSAttr *css = sp_repr_css_attr(reinterpret_cast<SPObject *>(il[0])->getRepr(), "style");
            gchar const *val = sp_repr_css_property(css, "fill-rule", NULL);
            if (val && strcmp(val, "nonzero") == 0) {
                origWind[curOrig]= fill_nonZero;
            } else if (val && strcmp(val, "evenodd") == 0) {
                origWind[curOrig]= fill_oddEven;
            } else {
                origWind[curOrig]= fill_nonZero;
            }

            originaux[curOrig] = Path_for_item(*l, true, true);
            if (originaux[curOrig] == NULL || originaux[curOrig]->descr_cmd.size() <= 1)
            {
                for (int i = curOrig; i >= 0; i--) delete originaux[i];
                return DONE_NO_ACTION;
            }
            curOrig++;
        }
    }
    // reverse if needed
    // note that the selection list keeps its order
    if ( reverseOrderForOp ) {
        using std::swap;
        swap(originaux[0], originaux[1]);
        swap(origWind[0], origWind[1]);
    }

    // and work
    // some temporary instances, first
    Shape *theShapeA = new Shape;
    Shape *theShapeB = new Shape;
    Shape *theShape = new Shape;
    Path *res = new Path;
    res->SetBackData(false);
    Path::cut_position  *toCut=NULL;
    int                  nbToCut=0;

    if ( bop == bool_op_inters || bop == bool_op_union || bop == bool_op_diff || bop == bool_op_symdiff ) {
        // true boolean op
        // get the polygons of each path, with the winding rule specified, and apply the operation iteratively
        originaux[0]->ConvertWithBackData(0.1);

        originaux[0]->Fill(theShape, 0);

        theShapeA->ConvertToShape(theShape, origWind[0]);

        curOrig = 1;
        for (std::vector<SPItem*>::const_iterator l = il.begin(); l != il.end(); l++){
            if(*l==il[0])continue;
            originaux[curOrig]->ConvertWithBackData(0.1);

            originaux[curOrig]->Fill(theShape, curOrig);

            theShapeB->ConvertToShape(theShape, origWind[curOrig]);

            /* Due to quantization of the input shape coordinates, we may end up with A or B being empty.
             * If this is a union or symdiff operation, we just use the non-empty shape as the result:
             *   A=0  =>  (0 or B) == B
             *   B=0  =>  (A or 0) == A
             *   A=0  =>  (0 xor B) == B
             *   B=0  =>  (A xor 0) == A
             * If this is an intersection operation, we just use the empty shape as the result:
             *   A=0  =>  (0 and B) == 0 == A
             *   B=0  =>  (A and 0) == 0 == B
             * If this a difference operation, and the upper shape (A) is empty, we keep B.
             * If the lower shape (B) is empty, we still keep B, as it's empty:
             *   A=0  =>  (B - 0) == B
             *   B=0  =>  (0 - A) == 0 == B
             *
             * In any case, the output from this operation is stored in shape A, so we may apply
             * the above rules simply by judicious use of swapping A and B where necessary.
             */
            bool zeroA = theShapeA->numberOfEdges() == 0;
            bool zeroB = theShapeB->numberOfEdges() == 0;
            if (zeroA || zeroB) {
                // We might need to do a swap. Apply the above rules depending on operation type.
                bool resultIsB =   ((bop == bool_op_union || bop == bool_op_symdiff) && zeroA)
                                   || ((bop == bool_op_inters) && zeroB)
                                   ||  (bop == bool_op_diff);
                if (resultIsB) {
                    // Swap A and B to use B as the result
                    Shape *swap = theShapeB;
                    theShapeB = theShapeA;
                    theShapeA = swap;
                }
            } else {
                // Just do the Boolean operation as usual
                // les elements arrivent en ordre inverse dans la liste
                theShape->Booleen(theShapeB, theShapeA, bop);
                Shape *swap = theShape;
                theShape = theShapeA;
                theShapeA = swap;
            }
            curOrig++;
        }

        {
            Shape *swap = theShape;
            theShape = theShapeA;
            theShapeA = swap;
        }

    } else if ( bop == bool_op_cut ) {
        // cuts= sort of a bastard boolean operation, thus not the axact same modus operandi
        // technically, the cut path is not necessarily a polygon (thus has no winding rule)
        // it is just uncrossed, and cleaned from duplicate edges and points
        // then it's fed to Booleen() which will uncross it against the other path
        // then comes the trick: each edge of the cut path is duplicated (one in each direction),
        // thus making a polygon. the weight of the edges of the cut are all 0, but
        // the Booleen need to invert the ones inside the source polygon (for the subsequent
        // ConvertToForme)

        // the cut path needs to have the highest pathID in the back data
        // that's how the Booleen() function knows it's an edge of the cut

        // FIXME: this gives poor results, the final paths are full of extraneous nodes. Decreasing
        // ConvertWithBackData parameter below simply increases the number of nodes, so for now I
        // left it at 1.0. Investigate replacing this by a combination of difference and
        // intersection of the same two paths. -- bb
        {
            Path* swap=originaux[0];originaux[0]=originaux[1];originaux[1]=swap;
            int   swai=origWind[0];origWind[0]=origWind[1];origWind[1]=(fill_typ)swai;
        }
        originaux[0]->ConvertWithBackData(1.0);

        originaux[0]->Fill(theShape, 0);

        theShapeA->ConvertToShape(theShape, origWind[0]);

        originaux[1]->ConvertWithBackData(1.0);

        if ((originaux[1]->pts.size() == 2) && originaux[1]->pts[0].isMoveTo && !originaux[1]->pts[1].isMoveTo)
            originaux[1]->Fill(theShape, 1,false,true,false); // see LP Bug 177956
        else
            originaux[1]->Fill(theShape, 1,false,false,false); //do not closeIfNeeded

        theShapeB->ConvertToShape(theShape, fill_justDont); // fill_justDont doesn't computes winding numbers

        // les elements arrivent en ordre inverse dans la liste
        theShape->Booleen(theShapeB, theShapeA, bool_op_cut, 1);

    } else if ( bop == bool_op_slice ) {
        // slice is not really a boolean operation
        // you just put the 2 shapes in a single polygon, uncross it
        // the points where the degree is > 2 are intersections
        // just check it's an intersection on the path you want to cut, and keep it
        // the intersections you have found are then fed to ConvertPositionsToMoveTo() which will
        // make new subpath at each one of these positions
        // inversion pour l'opration
        {
            Path* swap=originaux[0];originaux[0]=originaux[1];originaux[1]=swap;
            int   swai=origWind[0];origWind[0]=origWind[1];origWind[1]=(fill_typ)swai;
        }
        originaux[0]->ConvertWithBackData(1.0);

        originaux[0]->Fill(theShapeA, 0,false,false,false); // don't closeIfNeeded

        originaux[1]->ConvertWithBackData(1.0);

        originaux[1]->Fill(theShapeA, 1,true,false,false);// don't closeIfNeeded and just dump in the shape, don't reset it

        theShape->ConvertToShape(theShapeA, fill_justDont);

        if ( theShape->hasBackData() ) {
            // should always be the case, but ya never know
            {
                for (int i = 0; i < theShape->numberOfPoints(); i++) {
                    if ( theShape->getPoint(i).totalDegree() > 2 ) {
                        // possibly an intersection
                        // we need to check that at least one edge from the source path is incident to it
                        // before we declare it's an intersection
                        int cb = theShape->getPoint(i).incidentEdge[FIRST];
                        int   nbOrig=0;
                        int   nbOther=0;
                        int   piece=-1;
                        float t=0.0;
                        while ( cb >= 0 && cb < theShape->numberOfEdges() ) {
                            if ( theShape->ebData[cb].pathID == 0 ) {
                                // the source has an edge incident to the point, get its position on the path
                                piece=theShape->ebData[cb].pieceID;
                                if ( theShape->getEdge(cb).st == i ) {
                                    t=theShape->ebData[cb].tSt;
                                } else {
                                    t=theShape->ebData[cb].tEn;
                                }
                                nbOrig++;
                            }
                            if ( theShape->ebData[cb].pathID == 1 ) nbOther++; // the cut is incident to this point
                            cb=theShape->NextAt(i, cb);
                        }
                        if ( nbOrig > 0 && nbOther > 0 ) {
                            // point incident to both path and cut: an intersection
                            // note that you only keep one position on the source; you could have degenerate
                            // cases where the source crosses itself at this point, and you wouyld miss an intersection
                            toCut=(Path::cut_position*)realloc(toCut, (nbToCut+1)*sizeof(Path::cut_position));
                            toCut[nbToCut].piece=piece;
                            toCut[nbToCut].t=t;
                            nbToCut++;
                        }
                    }
                }
            }
            {
                // i think it's useless now
                int i = theShape->numberOfEdges() - 1;
                for (;i>=0;i--) {
                    if ( theShape->ebData[i].pathID == 1 ) {
                        theShape->SubEdge(i);
                    }
                }
            }

        }
    }

    int*    nesting=NULL;
    int*    conts=NULL;
    int     nbNest=0;
    // pour compenser le swap juste avant
    if ( bop == bool_op_slice ) {
//    theShape->ConvertToForme(res, nbOriginaux, originaux, true);
//    res->ConvertForcedToMoveTo();
        res->Copy(originaux[0]);
        res->ConvertPositionsToMoveTo(nbToCut, toCut); // cut where you found intersections
        free(toCut);
    } else if ( bop == bool_op_cut ) {
        // il faut appeler pour desallouer PointData (pas vital, mais bon)
        // the Booleen() function did not deallocate the point_data array in theShape, because this
        // function needs it.
        // this function uses the point_data to get the winding number of each path (ie: is a hole or not)
        // for later reconstruction in objects, you also need to extract which path is parent of holes (nesting info)
        theShape->ConvertToFormeNested(res, nbOriginaux, &originaux[0], 1, nbNest, nesting, conts);
    } else {
        theShape->ConvertToForme(res, nbOriginaux, &originaux[0]);
    }

    delete theShape;
    delete theShapeA;
    delete theShapeB;
    for (int i = 0; i < nbOriginaux; i++)  delete originaux[i];

    if (res->descr_cmd.size() <= 1)
    {
        // only one command, presumably a moveto: it isn't a path
        for (std::vector<SPItem*>::const_iterator l = il.begin(); l != il.end(); l++){
            (*l)->deleteObject();
        }
        clear();

        delete res;
        return DONE_NO_PATH;
    }

    // get the source path object
    SPObject *source;
    if ( bop == bool_op_diff || bop == bool_op_cut || bop == bool_op_slice ) {
        if (reverseOrderForOp) {
            source = il[0];
        } else {
            source = il.back();
        }
    } else {
        // find out the bottom object
        std::vector<Inkscape::XML::Node*> sorted(xmlNodes().begin(), xmlNodes().end());

        sort(sorted.begin(),sorted.end(),sp_repr_compare_position_bool);

        source = doc->getObjectByRepr(sorted.front());
    }

    // adjust style properties that depend on a possible transform in the source object in order
    // to get a correct style attribute for the new path
    SPItem* item_source = SP_ITEM(source);
    Geom::Affine i2doc(item_source->i2doc_affine());
    item_source->adjust_stroke(i2doc.descrim());
    item_source->adjust_pattern(i2doc);
    item_source->adjust_gradient(i2doc);
    item_source->adjust_livepatheffect(i2doc);

    Inkscape::XML::Node *repr_source = source->getRepr();

    // remember important aspects of the source path, to be restored
    gint pos = repr_source->position();
    Inkscape::XML::Node *parent = repr_source->parent();
    gchar const *id = repr_source->attribute("id");
    gchar const *style = repr_source->attribute("style");
    gchar const *mask = repr_source->attribute("mask");
    gchar const *clip_path = repr_source->attribute("clip-path");
    gchar *title = source->title();
    gchar *desc = source->desc();
    // remove source paths
    clear();
    for (std::vector<SPItem*>::const_iterator l = il.begin(); l != il.end(); l++){
        // if this is the bottommost object,
        if (!strcmp(reinterpret_cast<SPObject *>(*l)->getRepr()->attribute("id"), id)) {
            // delete it so that its clones don't get alerted; this object will be restored shortly, with the same id
            (*l)->deleteObject(false);
        } else {
            // delete the object for real, so that its clones can take appropriate action
            (*l)->deleteObject();
        }
    }

    // premultiply by the inverse of parent's repr
    SPItem *parent_item = SP_ITEM(doc->getObjectByRepr(parent));
    Geom::Affine local (parent_item->i2doc_affine());
    gchar *transform = sp_svg_transform_write(local.inverse());

    // now that we have the result, add it on the canvas
    if ( bop == bool_op_cut || bop == bool_op_slice ) {
        int    nbRP=0;
        Path** resPath;
        if ( bop == bool_op_slice ) {
            // there are moveto's at each intersection, but it's still one unique path
            // so break it down and add each subpath independently
            // we could call break_apart to do this, but while we have the description...
            resPath=res->SubPaths(nbRP, false);
        } else {
            // cut operation is a bit wicked: you need to keep holes
            // that's why you needed the nesting
            // ConvertToFormeNested() dumped all the subpath in a single Path "res", so we need
            // to get the path for each part of the polygon. that's why you need the nesting info:
            // to know in which subpath to add a subpath
            resPath=res->SubPathsWithNesting(nbRP, true, nbNest, nesting, conts);

            // cleaning
            if ( conts ) free(conts);
            if ( nesting ) free(nesting);
        }

        // add all the pieces resulting from cut or slice
        for (int i=0;i<nbRP;i++) {
            gchar *d = resPath[i]->svg_dump_path();

            Inkscape::XML::Document *xml_doc = doc->getReprDoc();
            Inkscape::XML::Node *repr = xml_doc->createElement("svg:path");
            repr->setAttribute("style", style);
            if (mask)
                repr->setAttribute("mask", mask);
            if (clip_path)
                repr->setAttribute("clip-path", clip_path);

            repr->setAttribute("d", d);
            g_free(d);

            // for slice, remove fill
            if (bop == bool_op_slice) {
                SPCSSAttr *css;

                css = sp_repr_css_attr_new();
                sp_repr_css_set_property(css, "fill", "none");

                sp_repr_css_change(repr, css, "style");

                sp_repr_css_attr_unref(css);
            }

            // we assign the same id on all pieces, but it on adding to document, it will be changed on all except one
            // this means it's basically random which of the pieces inherits the original's id and clones
            // a better algorithm might figure out e.g. the biggest piece
            repr->setAttribute("id", id);

            repr->setAttribute("transform", transform);

            // add the new repr to the parent
            parent->appendChild(repr);

            // move to the saved position
            repr->setPosition(pos > 0 ? pos : 0);

            add(doc->getObjectByRepr(repr));
            Inkscape::GC::release(repr);

            delete resPath[i];
        }
        if ( resPath ) free(resPath);

    } else {
        gchar *d = res->svg_dump_path();

        Inkscape::XML::Document *xml_doc = doc->getReprDoc();
        Inkscape::XML::Node *repr = xml_doc->createElement("svg:path");
        repr->setAttribute("style", style);

        if ( mask )
            repr->setAttribute("mask", mask);

        if ( clip_path )
            repr->setAttribute("clip-path", clip_path);

        repr->setAttribute("d", d);
        g_free(d);

        repr->setAttribute("transform", transform);

        repr->setAttribute("id", id);
        parent->appendChild(repr);
        if (title) {
            doc->getObjectByRepr(repr)->setTitle(title);
        }
        if (desc) {
            doc->getObjectByRepr(repr)->setDesc(desc);
        }
        repr->setPosition(pos > 0 ? pos : 0);

        add(doc->getObjectByRepr(repr));
        Inkscape::GC::release(repr);
    }

    g_free(transform);
    if (title) g_free(title);
    if (desc) g_free(desc);

    delete res;

    return DONE;
}

static
void sp_selected_path_outline_add_marker( SPObject *marker_object, Geom::Affine marker_transform,
                                          Geom::Scale stroke_scale, Geom::Affine transform,
                                          Inkscape::XML::Node *g_repr, Inkscape::XML::Document *xml_doc, SPDocument * doc,
                                          SPDesktop *desktop , bool legacy)
{
    SPMarker* marker = SP_MARKER (marker_object);
    SPItem* marker_item = sp_item_first_item_child(marker_object);
    if (!marker_item) {
        return;
    }

    Geom::Affine tr(marker_transform);

    if (marker->markerUnits == SP_MARKER_UNITS_STROKEWIDTH) {
        tr = stroke_scale * tr;
    }
    
    // total marker transform
    tr = marker_item->transform * marker->c2p * tr * transform;

    if (marker_item->getRepr()) {
        Inkscape::XML::Node *m_repr = marker_item->getRepr()->duplicate(xml_doc);
        g_repr->appendChild(m_repr);
        //There is a special group to markers with this reverse the order in clussion
        m_repr->setPosition(0);
        SPItem *marker_item = (SPItem *) doc->getObjectByRepr(m_repr);
        marker_item->doWriteTransform(tr);
        if (!legacy) {
            sp_item_path_outline(marker_item, desktop, legacy);
        }
    }
}

static
void item_outline_add_marker_child( SPItem const *item, Geom::Affine marker_transform, Geom::PathVector* pathv_in )
{
    Geom::Affine tr(marker_transform);
    tr = item->transform * tr;

    // note: a marker child item can be an item group!
    if (SP_IS_GROUP(item)) {
        // recurse through all childs:
        for (auto& o: item->children) {
            if ( SP_IS_ITEM(&o) ) {
                item_outline_add_marker_child(SP_ITEM(&o), tr, pathv_in);
            }
        }
    } else {
        Geom::PathVector* marker_pathv = item_outline(item);

        if (marker_pathv) {
            for (unsigned int j=0; j < marker_pathv->size(); j++) {
                pathv_in->push_back((*marker_pathv)[j] * tr);
            }
            delete marker_pathv;
        }
    }
}

static
void item_outline_add_marker( SPObject const *marker_object, Geom::Affine marker_transform,
                              Geom::Scale stroke_scale, Geom::PathVector* pathv_in )
{
    SPMarker const * marker = SP_MARKER(marker_object);

    Geom::Affine tr(marker_transform);
    if (marker->markerUnits == SP_MARKER_UNITS_STROKEWIDTH) {
        tr = stroke_scale * tr;
    }
    // total marker transform
    tr = marker->c2p * tr;

    SPItem const * marker_item = sp_item_first_item_child(marker_object); // why only consider the first item? can a marker only consist of a single item (that may be a group)?
    if (marker_item) {
        item_outline_add_marker_child(marker_item, tr, pathv_in);
    }
}

/**
 *  Returns a pathvector that is the outline of the stroked item, with markers.
 *  item must be SPShape or SPText.
 */
Geom::PathVector* item_outline(SPItem const *item, bool bbox_only)
{
    Geom::PathVector *ret_pathv = NULL;

    if (!SP_IS_SHAPE(item) && !SP_IS_TEXT(item)) {
        return ret_pathv;
    }

    // no stroke: no outline
    if (!item->style || item->style->stroke.noneSet) {
        return ret_pathv;
    }

    SPCurve *curve = NULL;
    if (SP_IS_SHAPE(item)) {
        curve = SP_SHAPE(item)->getCurve();
    } else if (SP_IS_TEXT(item)) {
        curve = SP_TEXT(item)->getNormalizedBpath();
    }
    if (curve == NULL) {
        return ret_pathv;
    }

    if (curve->get_pathvector().empty()) {
        return ret_pathv;
    }

    // remember old stroke style, to be set on fill
    SPStyle *i_style = item->style;

    Geom::Affine const transform(item->transform);
    float const scale = transform.descrim();

    float o_width = i_style->stroke_width.computed;
    if (o_width < Geom::EPSILON) {
        // This may result in rounding errors for very small stroke widths (happens e.g. when user unit is large).
        // See bug lp:1244861
        o_width = Geom::EPSILON;
    }
    float o_miter = i_style->stroke_miterlimit.value * o_width;

    JoinType o_join;
    ButtType o_butt;
    {
        switch (i_style->stroke_linejoin.computed) {
            case SP_STROKE_LINEJOIN_MITER:
                o_join = join_pointy;
                break;
            case SP_STROKE_LINEJOIN_ROUND:
                o_join = join_round;
                break;
            default:
                o_join = join_straight;
                break;
        }

        switch (i_style->stroke_linecap.computed) {
            case SP_STROKE_LINECAP_SQUARE:
                o_butt = butt_square;
                break;
            case SP_STROKE_LINECAP_ROUND:
                o_butt = butt_round;
                break;
            default:
                o_butt = butt_straight;
                break;
        }
    }

    // Livarot's outline of arcs is broken. So convert the path to linear and cubics only, for which the outline is created correctly.
    Geom::PathVector pathv = pathv_to_linear_and_cubic_beziers( curve->get_pathvector() );

    Path *orig = new Path;
    orig->LoadPathVector(pathv);

    Path *res = new Path;
    res->SetBackData(false);

    if (!i_style->stroke_dasharray.values.empty()) {
        // For dashed strokes, use Stroke method, because Outline can't do dashes
        // However Stroke adds lots of extra nodes _or_ makes the path crooked, so consider this a temporary workaround

        orig->ConvertWithBackData(0.1);

        orig->DashPolylineFromStyle(i_style, scale, 0);

        Shape* theShape = new Shape;
        orig->Stroke(theShape, false, 0.5*o_width, o_join, o_butt,
                     0.5 * o_miter);
        orig->Outline(res, 0.5 * o_width, o_join, o_butt, 0.5 * o_miter);

        if (!bbox_only) {
            Shape *theRes = new Shape;
            theRes->ConvertToShape(theShape, fill_positive);

            Path *originaux[1];
            originaux[0] = res;
            theRes->ConvertToForme(orig, 1, originaux);
            res->Coalesce(5.0);
            delete theRes;
        }
        delete theShape;
    } else {
        orig->Outline(res, 0.5 * o_width, o_join, o_butt, 0.5 * o_miter);

        if (!bbox_only) {
            orig->Coalesce(0.5 * o_width);
            Shape *theShape = new Shape;
            Shape *theRes = new Shape;

            res->ConvertWithBackData(1.0);
            res->Fill(theShape, 0);
            theRes->ConvertToShape(theShape, fill_positive);

            Path *originaux[1];
            originaux[0] = res;
            theRes->ConvertToForme(orig, 1, originaux);

            delete theShape;
            delete theRes;
        }
    }

    if (orig->descr_cmd.size() <= 1) {
        // ca a merd, ou bien le resultat est vide
        delete res;
        delete orig;
        curve->unref();
        return ret_pathv;
    }


    if (res->descr_cmd.size() > 1) { // if there's 0 or 1 node left, drop this path altogether
        ret_pathv = bbox_only ? res->MakePathVector() : orig->MakePathVector();

        if (SP_IS_SHAPE(item) && SP_SHAPE(item)->hasMarkers() && !bbox_only) {
            SPShape *shape = SP_SHAPE(item);

            Geom::PathVector const & pathv = curve->get_pathvector();

            // START marker
            for (int i = 0; i < 2; i++) {  // SP_MARKER_LOC and SP_MARKER_LOC_START
                if ( SPObject *marker_obj = shape->_marker[i] ) {
                    Geom::Affine const m (sp_shape_marker_get_transform_at_start(pathv.front().front()));
                    item_outline_add_marker( marker_obj, m,
                                             Geom::Scale(i_style->stroke_width.computed),
                                             ret_pathv );
                }
            }
            // MID marker
            for (int i = 0; i < 3; i += 2) {  // SP_MARKER_LOC and SP_MARKER_LOC_MID
                SPObject *midmarker_obj = shape->_marker[i];
                if (!midmarker_obj) continue;
                for(Geom::PathVector::const_iterator path_it = pathv.begin(); path_it != pathv.end(); ++path_it) {
                    // START position
                    if ( path_it != pathv.begin() 
                         && ! ((path_it == (pathv.end()-1)) && (path_it->size_default() == 0)) ) // if this is the last path and it is a moveto-only, there is no mid marker there
                    {
                        Geom::Affine const m (sp_shape_marker_get_transform_at_start(path_it->front()));
                        item_outline_add_marker( midmarker_obj, m,
                                                 Geom::Scale(i_style->stroke_width.computed),
                                                 ret_pathv );
                    }
                    // MID position
                   if (path_it->size_default() > 1) {
                        Geom::Path::const_iterator curve_it1 = path_it->begin();      // incoming curve
                        Geom::Path::const_iterator curve_it2 = ++(path_it->begin());  // outgoing curve
                        while (curve_it2 != path_it->end_default())
                        {
                            /* Put marker between curve_it1 and curve_it2.
                             * Loop to end_default (so including closing segment), because when a path is closed,
                             * there should be a midpoint marker between last segment and closing straight line segment
                             */
                            Geom::Affine const m (sp_shape_marker_get_transform(*curve_it1, *curve_it2));
                            item_outline_add_marker( midmarker_obj, m,
                                                     Geom::Scale(i_style->stroke_width.computed),
                                                     ret_pathv);

                            ++curve_it1;
                            ++curve_it2;
                        }
                    }
                    // END position
                    if ( path_it != (pathv.end()-1) && !path_it->empty()) {
                        Geom::Curve const &lastcurve = path_it->back_default();
                        Geom::Affine const m = sp_shape_marker_get_transform_at_end(lastcurve);
                        item_outline_add_marker( midmarker_obj, m,
                                                 Geom::Scale(i_style->stroke_width.computed),
                                                 ret_pathv );
                    }
                }
            }
            // END marker
            for (int i = 0; i < 4; i += 3) {  // SP_MARKER_LOC and SP_MARKER_LOC_END
                if ( SPObject *marker_obj = shape->_marker[i] ) {
                    /* Get reference to last curve in the path.
                     * For moveto-only path, this returns the "closing line segment". */
                    Geom::Path const &path_last = pathv.back();
                    unsigned int index = path_last.size_default();
                    if (index > 0) {
                        index--;
                    }
                    Geom::Curve const &lastcurve = path_last[index];

                    Geom::Affine const m = sp_shape_marker_get_transform_at_end(lastcurve);
                    item_outline_add_marker( marker_obj, m,
                                             Geom::Scale(i_style->stroke_width.computed),
                                             ret_pathv );
                }
            }
        }

        curve->unref();
    }

    delete res;
    delete orig;

    return ret_pathv;
}

bool
sp_item_path_outline(SPItem *item, SPDesktop *desktop, bool legacy)
{
    bool did = false;
    Inkscape::Selection *selection = desktop->getSelection();
    SPDocument * doc = desktop->getDocument();
    Inkscape::XML::Document *xml_doc = doc->getReprDoc();
    SPLPEItem *lpeitem = SP_LPE_ITEM(item);
    if (lpeitem) {
        lpeitem->removeAllPathEffects(true);
    }

    SPGroup *group = dynamic_cast<SPGroup *>(item);
    if (group) {
        if (legacy) {
            return false;
        }
        std::vector<SPItem*> const item_list = sp_item_group_item_list(group);
        for ( std::vector<SPItem*>::const_iterator iter=item_list.begin();iter!=item_list.end();++iter) {
            SPItem *subitem = *iter;
            sp_item_path_outline(subitem, desktop, legacy);
        }
    } else {
        if (!SP_IS_SHAPE(item) && !SP_IS_TEXT(item))
            return did;

        SPCurve *curve = NULL;
        if (SP_IS_SHAPE(item)) {
            curve = SP_SHAPE(item)->getCurve();
            if (curve == NULL)
                return did;
        }
        if (SP_IS_TEXT(item)) {
            curve = SP_TEXT(item)->getNormalizedBpath();
            if (curve == NULL)
                return did;
        }

        g_assert(curve != NULL);

        if (curve->get_pathvector().empty()) {
            return did;
        }

        // pas de stroke pas de chocolat
        if (!item->style) {
            curve->unref();
            return did;
        }

        // remember old stroke style, to be set on fill
        SPStyle *i_style = item->style;
        //Stroke - and markers
        gchar const *opacity;
        gchar const *filter;

        // Copying stroke style to fill will fail for properties not defined by style attribute
        // (i.e., properties defined in style sheet or by attributes).

        // Stroke
        SPCSSAttr *ncss = 0;
        {
            ncss = sp_css_attr_from_style(i_style, SP_STYLE_FLAG_ALWAYS | SP_STYLE_FLAG_IFSRC);
            gchar const *s_val = sp_repr_css_property(ncss, "stroke", NULL);
            gchar const *s_opac = sp_repr_css_property(ncss, "stroke-opacity", NULL);
            opacity = sp_repr_css_property(ncss, "opacity", NULL);
            filter = sp_repr_css_property(ncss, "filter", NULL);
            sp_repr_css_set_property(ncss, "stroke", "none");
            sp_repr_css_set_property(ncss, "filter", NULL);
            sp_repr_css_set_property(ncss, "opacity", NULL);
            sp_repr_css_set_property(ncss, "stroke-opacity", "1.0");
            sp_repr_css_set_property(ncss, "fill", s_val);
            if ( s_opac ) {
                sp_repr_css_set_property(ncss, "fill-opacity", s_opac);
            } else {
                sp_repr_css_set_property(ncss, "fill-opacity", "1.0");
            }
            sp_repr_css_unset_property(ncss, "marker-start");
            sp_repr_css_unset_property(ncss, "marker-mid");
            sp_repr_css_unset_property(ncss, "marker-end");
        }

        // Fill
        SPCSSAttr *ncsf = 0;
        {
            ncsf = sp_css_attr_from_style(i_style, SP_STYLE_FLAG_ALWAYS | SP_STYLE_FLAG_IFSRC);
            sp_repr_css_set_property(ncsf, "stroke", "none");
            sp_repr_css_set_property(ncsf, "stroke-opacity", "1.0");
            sp_repr_css_set_property(ncsf, "filter", NULL);
            sp_repr_css_set_property(ncsf, "opacity", NULL);
            sp_repr_css_unset_property(ncsf, "marker-start");
            sp_repr_css_unset_property(ncsf, "marker-mid");
            sp_repr_css_unset_property(ncsf, "marker-end");
        }

        Geom::Affine const transform(item->transform);
        float const scale = transform.descrim();
        gchar const *mask = item->getRepr()->attribute("mask");
        gchar const *clip_path = item->getRepr()->attribute("clip-path");

        Path *orig = new Path;
        Path *res = new Path;
        SPCurve *curvetemp = curve_for_item(item);
        if (curvetemp == NULL) {
            curve->unref();
            return did;
        }
        // Livarot's outline of arcs is broken. So convert the path to linear and cubics only, for which the outline is created correctly.
        Geom::PathVector pathv = pathv_to_linear_and_cubic_beziers( curvetemp->get_pathvector() );
        curvetemp->unref();
        if ( !item->style->stroke.noneSet ) {
            float o_width, o_miter;
            JoinType o_join;
            ButtType o_butt;

            {
                int jointype, captype;

                jointype = i_style->stroke_linejoin.computed;
                captype = i_style->stroke_linecap.computed;
                o_width = i_style->stroke_width.computed;

                switch (jointype) {
                    case SP_STROKE_LINEJOIN_MITER:
                        o_join = join_pointy;
                        break;
                    case SP_STROKE_LINEJOIN_ROUND:
                        o_join = join_round;
                        break;
                    default:
                        o_join = join_straight;
                        break;
                }

                switch (captype) {
                    case SP_STROKE_LINECAP_SQUARE:
                        o_butt = butt_square;
                        break;
                    case SP_STROKE_LINECAP_ROUND:
                        o_butt = butt_round;
                        break;
                    default:
                        o_butt = butt_straight;
                        break;
                }

                if (o_width < 0.032)
                    o_width = 0.032;
                o_miter = i_style->stroke_miterlimit.value * o_width;
            }


            orig->LoadPathVector(pathv);
            res->SetBackData(false);

            if (!i_style->stroke_dasharray.values.empty()) {
                // For dashed strokes, use Stroke method, because Outline can't do dashes
                // However Stroke adds lots of extra nodes _or_ makes the path crooked, so consider this a temporary workaround

                orig->ConvertWithBackData(0.1);

                orig->DashPolylineFromStyle(i_style, scale, 0);

                Shape* theShape = new Shape;
                orig->Stroke(theShape, false, 0.5*o_width, o_join, o_butt,
                             0.5 * o_miter);
                orig->Outline(res, 0.5 * o_width, o_join, o_butt, 0.5 * o_miter);

                Shape *theRes = new Shape;

                theRes->ConvertToShape(theShape, fill_positive);

                Path *originaux[1];
                originaux[0] = res;
                theRes->ConvertToForme(orig, 1, originaux);

                res->Coalesce(5.0);

                delete theShape;
                delete theRes;

            } else {
                orig->Outline(res, 0.5 * o_width, o_join, o_butt, 0.5 * o_miter);

                orig->Coalesce(0.5 * o_width);

                Shape *theShape = new Shape;
                Shape *theRes = new Shape;

                res->ConvertWithBackData(1.0);
                res->Fill(theShape, 0);
                theRes->ConvertToShape(theShape, fill_positive);

                Path *originaux[1];
                originaux[0] = res;
                theRes->ConvertToForme(orig, 1, originaux);

                delete theShape;
                delete theRes;
            }

            if (orig->descr_cmd.size() <= 1) {
                // ca a merd, ou bien le resultat est vide
                delete res;
                delete orig;
                return did;
            }
        }
        // remember the position of the item
        gint pos = item->getRepr()->position();
        // remember parent
        Inkscape::XML::Node *parent = item->getRepr()->parent();
        // remember id
        char const *id = item->getRepr()->attribute("id");
        // remember title
        gchar *title = item->title();
        // remember description
        gchar *desc = item->desc();
        
        if (res->descr_cmd.size() > 1) { // if there's 0 or 1 node left, drop this path altogether

            //The stroke
            Inkscape::XML::Node *stroke = NULL;
            if( !item->style->stroke.noneSet ){
                SPDocument * doc = desktop->getDocument();
                Inkscape::XML::Document *xml_doc = doc->getReprDoc();
                stroke = xml_doc->createElement("svg:path");

                // restore old style, but set old stroke style on fill
                sp_repr_css_change(stroke, ncss, "style");

                sp_repr_css_attr_unref(ncss);

                gchar *str = orig->svg_dump_path();
                stroke->setAttribute("d", str);
                g_free(str);

                if (mask)
                    stroke->setAttribute("mask", mask);
                if (clip_path)
                    stroke->setAttribute("clip-path", clip_path);
            }

            if (SP_IS_SHAPE(item)) {
                Inkscape::XML::Node *g_repr = xml_doc->createElement("svg:g");

                // add the group to the parent
                parent->appendChild(g_repr);
                // move to the saved position
                g_repr->setPosition(pos > 0 ? pos : 0);

                //The fill
                Inkscape::XML::Node *fill = NULL;
                if (!legacy) {
//                    gchar const *f_val = sp_repr_css_property(ncsf, "fill", NULL);
                    if( !item->style->fill.noneSet ){
                        fill = xml_doc->createElement("svg:path");
                        sp_repr_css_change(fill, ncsf, "style");

                        sp_repr_css_attr_unref(ncsf);

                        gchar *str = sp_svg_write_path( pathv );
                        fill->setAttribute("d", str);
                        g_free(str);

                        if (mask)
                            fill->setAttribute("mask", mask);
                        if (clip_path)
                            fill->setAttribute("clip-path", clip_path);
                    }
                }
                // restore title, description, id, transform
                g_repr->setAttribute("id", id);
                SPItem *newitem = (SPItem *) doc->getObjectByRepr(g_repr);
                newitem->doWriteTransform(transform);
                if (title) {
                	newitem->setTitle(title);
                }
                if (desc) {
                	newitem->setDesc(desc);
                }
                SPShape *shape = SP_SHAPE(item);

                Geom::PathVector const & pathv = curve->get_pathvector();
                Inkscape::XML::Node *markers = NULL;
                if(SP_SHAPE(item)->hasMarkers ()) {
                    if (!legacy) {
                        markers = xml_doc->createElement("svg:g");
                        g_repr->appendChild(markers);
                        markers->setPosition(pos > 0 ? pos : 0);
                    } else {
                        markers = g_repr;
                    }
                    // START marker
                    for (int i = 0; i < 2; i++) {  // SP_MARKER_LOC and SP_MARKER_LOC_START
                        if ( SPObject *marker_obj = shape->_marker[i] ) {
                            Geom::Affine const m (sp_shape_marker_get_transform_at_start(pathv.front().front()));
                            sp_selected_path_outline_add_marker( marker_obj, m,
                                                                 Geom::Scale(i_style->stroke_width.computed), transform,
                                                                 markers, xml_doc, doc, desktop, legacy);
                        }
                    }
                    // MID marker
                    for (int i = 0; i < 3; i += 2) {  // SP_MARKER_LOC and SP_MARKER_LOC_MID
                        SPObject *midmarker_obj = shape->_marker[i];
                        if (!midmarker_obj) continue;
                        for(Geom::PathVector::const_iterator path_it = pathv.begin(); path_it != pathv.end(); ++path_it) {
                            // START position
                            if ( path_it != pathv.begin() 
                                 && ! ((path_it == (pathv.end()-1)) && (path_it->size_default() == 0)) ) // if this is the last path and it is a moveto-only, there is no mid marker there
                            {
                                Geom::Affine const m (sp_shape_marker_get_transform_at_start(path_it->front()));
                                sp_selected_path_outline_add_marker( midmarker_obj, m,
                                                                     Geom::Scale(i_style->stroke_width.computed), transform,
                                                                     markers, xml_doc, doc, desktop, legacy);
                            }
                            // MID position
                           if (path_it->size_default() > 1) {
                                Geom::Path::const_iterator curve_it1 = path_it->begin();      // incoming curve
                                Geom::Path::const_iterator curve_it2 = ++(path_it->begin());  // outgoing curve
                                while (curve_it2 != path_it->end_default())
                                {
                                    /* Put marker between curve_it1 and curve_it2.
                                     * Loop to end_default (so including closing segment), because when a path is closed,
                                     * there should be a midpoint marker between last segment and closing straight line segment
                                     */
                                    Geom::Affine const m (sp_shape_marker_get_transform(*curve_it1, *curve_it2));
                                    sp_selected_path_outline_add_marker( midmarker_obj, m,
                                                                         Geom::Scale(i_style->stroke_width.computed), transform,
                                                                         markers, xml_doc, doc, desktop, legacy);

                                    ++curve_it1;
                                    ++curve_it2;
                                }
                            }
                            // END position
                            if ( path_it != (pathv.end()-1) && !path_it->empty()) {
                                Geom::Curve const &lastcurve = path_it->back_default();
                                Geom::Affine const m = sp_shape_marker_get_transform_at_end(lastcurve);
                                sp_selected_path_outline_add_marker( midmarker_obj, m,
                                                                     Geom::Scale(i_style->stroke_width.computed), transform,
                                                                     markers, xml_doc, doc, desktop, legacy);
                            }
                        }
                    }
                    // END marker
                    for (int i = 0; i < 4; i += 3) {  // SP_MARKER_LOC and SP_MARKER_LOC_END
                        if ( SPObject *marker_obj = shape->_marker[i] ) {
                            /* Get reference to last curve in the path.
                             * For moveto-only path, this returns the "closing line segment". */
                            Geom::Path const &path_last = pathv.back();
                            unsigned int index = path_last.size_default();
                            if (index > 0) {
                                index--;
                            }
                            Geom::Curve const &lastcurve = path_last[index];

                            Geom::Affine const m = sp_shape_marker_get_transform_at_end(lastcurve);
                            sp_selected_path_outline_add_marker( marker_obj, m,
                                                                 Geom::Scale(i_style->stroke_width.computed), transform,
                                                                 markers, xml_doc, doc, desktop, legacy);
                        }
                    }
                    if (!legacy) {
                        if (mask)
                            markers->setAttribute("mask", mask);
                        if (clip_path)
                            markers->setAttribute("clip-path", clip_path);
                    }
                }

                gchar const *paint_order = sp_repr_css_property(ncss, "paint-order", NULL);
                SPIPaintOrder temp;
                temp.read( paint_order );
                bool unique = false;
                if ((!fill && !markers) || (!fill && !stroke) || (!markers && !stroke)) {
                    unique = true;
                }
                if (temp.layer[0] != SP_CSS_PAINT_ORDER_NORMAL && !legacy && !unique) {

                    if (temp.layer[0] == SP_CSS_PAINT_ORDER_FILL) {
                        if (temp.layer[1] == SP_CSS_PAINT_ORDER_STROKE) {
                            if ( fill ) {
                                g_repr->appendChild(fill);
                            }
                            if ( stroke ) {
                                g_repr->appendChild(stroke);
                            }
                            if ( markers ) {
                                markers->setPosition(2);
                            }
                        } else {
                            if ( fill ) {
                                g_repr->appendChild(fill);
                            }
                            if ( markers ) {
                                markers->setPosition(1);
                            }
                            if ( stroke ) {
                                g_repr->appendChild(stroke);
                            }
                        }
                    } else if (temp.layer[0] == SP_CSS_PAINT_ORDER_STROKE) {
                        if (temp.layer[1] == SP_CSS_PAINT_ORDER_FILL) {
                            if ( stroke ) {
                                g_repr->appendChild(stroke);
                            }
                            if ( fill ) {
                                g_repr->appendChild(fill);
                            }
                            if ( markers ) {
                                markers->setPosition(2);
                            }
                        } else {
                            if ( stroke ) {
                                g_repr->appendChild(stroke);
                            }
                            if ( markers ) {
                                markers->setPosition(1);
                            }
                            if ( fill ) {
                                g_repr->appendChild(fill);
                            }
                        }
                    } else {
                        if (temp.layer[1] == SP_CSS_PAINT_ORDER_STROKE) {
                            if ( markers ) {
                                markers->setPosition(0);
                            }
                            if ( stroke ) {
                                g_repr->appendChild(stroke);
                            }
                            if ( fill ) {
                                g_repr->appendChild(fill);
                            }
                        } else {
                            if ( markers ) {
                                markers->setPosition(0);
                            }
                            if ( fill ) {
                                g_repr->appendChild(fill);
                            }
                            if ( stroke ) {
                                g_repr->appendChild(stroke);
                            }
                        }
                    }

                } else if (!unique) {
                    if ( fill ) {
                        g_repr->appendChild(fill);
                    }
                    if ( stroke ) {
                        g_repr->appendChild(stroke);
                    }
                    if ( markers ) {
                        markers->setPosition(2);
                    }
                }
                if( fill || stroke || markers ) {
                    did = true;
                }
                
                Inkscape::XML::Node *out = NULL;
                if (!fill && !markers && did) {
                    out = stroke;
                } else if (!fill && !stroke  && did) {
                    out = markers;
                } else if (!markers && !stroke  && did) {
                    out = fill;
                } else if(did) {
                    out = g_repr;
                }

                SPCSSAttr *r_style = sp_repr_css_attr_new();
                sp_repr_css_set_property(r_style, "opacity", opacity);
                sp_repr_css_set_property(r_style, "filter", filter);
                sp_repr_css_change(out, r_style, "style");

                sp_repr_css_attr_unref(r_style);
                if (unique) {
                    parent->appendChild(out);
                    parent->removeChild(g_repr);
                    out->setPosition(pos > 0 ? pos : 0);
                }
                out->setAttribute("transform", item->getRepr()->attribute("transform"));
                //bug lp:1290573 : completely destroy the old object first
                curve->unref();
                //Check for recursive markers to path
                if (did) {
                    if( selection->includes(item) ){
                        selection->remove(item);
                        item->deleteObject(false);
                        selection->add(out);
                    } else {
                        item->deleteObject(false);
                    }
                    Inkscape::GC::release(g_repr);
                }
            }
        }

        if (title) {
            g_free(title);
            title = 0;
        }
        if (desc) {
            g_free(desc);
            desc = 0;
        }

        delete res;
        delete orig;
    }
    return did;
}

void
sp_selected_path_outline(SPDesktop *desktop, bool legacy)
{
    Inkscape::Selection *selection = desktop->getSelection();

    if (selection->isEmpty()) {
        desktop->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Select <b>stroked path(s)</b> to convert stroke to path."));
        return;
    }
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool scale_stroke = prefs->getBool("/options/transform/stroke", true);
    prefs->setBool("/options/transform/stroke", true);
    bool did = false;
    std::vector<SPItem*> il(selection->items().begin(), selection->items().end());
    for (std::vector<SPItem*>::const_iterator l = il.begin(); l != il.end(); l++){
        SPItem *item = *l;
        did = sp_item_path_outline(item, desktop, legacy);
    }

    prefs->setBool("/options/transform/stroke", scale_stroke);
    if (did) {
        DocumentUndo::done(desktop->getDocument(), SP_VERB_SELECTION_OUTLINE, 
                           _("Convert stroke to path"));
    } else {
        // TRANSLATORS: "to outline" means "to convert stroke to path"
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("<b>No stroked paths</b> in the selection."));
        return;
    }
}


void
sp_selected_path_offset(SPDesktop *desktop)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    double prefOffset = prefs->getDouble("/options/defaultoffsetwidth/value", 1.0, desktop->getDocument()->getDisplayUnit()->abbr);

    sp_selected_path_do_offset(desktop, true, prefOffset);
}
void
sp_selected_path_inset(SPDesktop *desktop)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    double prefOffset = prefs->getDouble("/options/defaultoffsetwidth/value", 1.0, desktop->getDocument()->getDisplayUnit()->abbr);

    sp_selected_path_do_offset(desktop, false, prefOffset);
}

void
sp_selected_path_offset_screen(SPDesktop *desktop, double pixels)
{
    sp_selected_path_do_offset(desktop, true,  pixels / desktop->current_zoom());
}

void
sp_selected_path_inset_screen(SPDesktop *desktop, double pixels)
{
    sp_selected_path_do_offset(desktop, false,  pixels / desktop->current_zoom());
}


void sp_selected_path_create_offset_object_zero(SPDesktop *desktop)
{
    sp_selected_path_create_offset_object(desktop, 0, false);
}

void sp_selected_path_create_offset(SPDesktop *desktop)
{
    sp_selected_path_create_offset_object(desktop, 1, false);
}
void sp_selected_path_create_inset(SPDesktop *desktop)
{
    sp_selected_path_create_offset_object(desktop, -1, false);
}

void sp_selected_path_create_updating_offset_object_zero(SPDesktop *desktop)
{
    sp_selected_path_create_offset_object(desktop, 0, true);
}

void sp_selected_path_create_updating_offset(SPDesktop *desktop)
{
    sp_selected_path_create_offset_object(desktop, 1, true);
}
void sp_selected_path_create_updating_inset(SPDesktop *desktop)
{
    sp_selected_path_create_offset_object(desktop, -1, true);
}

void sp_selected_path_create_offset_object(SPDesktop *desktop, int expand, bool updating)
{
    SPCurve *curve = NULL;
    Inkscape::Selection *selection = desktop->getSelection();
    SPItem *item = selection->singleItem();

    if (item == NULL || ( !SP_IS_SHAPE(item) && !SP_IS_TEXT(item) ) ) {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("Selected object is <b>not a path</b>, cannot inset/outset."));
        return;
    }
    else if (SP_IS_SHAPE(item))
    {
        curve = SP_SHAPE(item)->getCurve();
    }
    else // Item must be SP_TEXT
    {
        curve = SP_TEXT(item)->getNormalizedBpath();
    }
        
    if (curve == NULL) {
        return;
    }

    Geom::Affine const transform(item->transform);

    item->doWriteTransform(Geom::identity());

    //XML Tree being used directly here while it shouldn't be...
    gchar *style = g_strdup(item->getRepr()->attribute("style"));

    // remember the position of the item
    gint pos = item->getRepr()->position();

    // remember parent
    Inkscape::XML::Node *parent = item->getRepr()->parent();

    float o_width = 0;
    {
        {
            Inkscape::Preferences *prefs = Inkscape::Preferences::get();
            o_width = prefs->getDouble("/options/defaultoffsetwidth/value", 1.0, desktop->getDocument()->getDisplayUnit()->abbr);
        }

        if (o_width < 0.01){
            o_width = 0.01;
        }
    }

    Path *orig = Path_for_item(item, true, false);
    if (orig == NULL)
    {
        g_free(style);
        curve->unref();
        return;
    }

    Path *res = new Path;
    res->SetBackData(false);

    {
        Shape *theShape = new Shape;
        Shape *theRes = new Shape;

        orig->ConvertWithBackData(1.0);
        orig->Fill(theShape, 0);

        SPCSSAttr *css = sp_repr_css_attr(item->getRepr(), "style");
        gchar const *val = sp_repr_css_property(css, "fill-rule", NULL);
        if (val && strcmp(val, "nonzero") == 0)
        {
            theRes->ConvertToShape(theShape, fill_nonZero);
        }
        else if (val && strcmp(val, "evenodd") == 0)
        {
            theRes->ConvertToShape(theShape, fill_oddEven);
        }
        else
        {
            theRes->ConvertToShape(theShape, fill_nonZero);
        }

        Path *originaux[1];
        originaux[0] = orig;
        theRes->ConvertToForme(res, 1, originaux);

        delete theShape;
        delete theRes;
    }

    curve->unref();

    if (res->descr_cmd.size() <= 1)
    {
        // pas vraiment de points sur le resultat
        // donc il ne reste rien
        DocumentUndo::done(desktop->getDocument(), 
                           (updating ? SP_VERB_SELECTION_LINKED_OFFSET 
                            : SP_VERB_SELECTION_DYNAMIC_OFFSET),
                           (updating ? _("Create linked offset")
                            : _("Create dynamic offset")));
        selection->clear();

        delete res;
        delete orig;
        g_free(style);
        return;
    }

    {
        Inkscape::XML::Document *xml_doc = desktop->doc()->getReprDoc();
        Inkscape::XML::Node *repr = xml_doc->createElement("svg:path");
        repr->setAttribute("sodipodi:type", "inkscape:offset");
        sp_repr_set_svg_double(repr, "inkscape:radius", ( expand > 0
                                                          ? o_width
                                                          : expand < 0
                                                          ? -o_width
                                                          : 0 ));

        gchar *str = res->svg_dump_path();
        repr->setAttribute("inkscape:original", str);
        g_free(str);
        str = 0;

        if ( updating ) {

			//XML Tree being used directly here while it shouldn't be
            item->doWriteTransform(transform);
            char const *id = item->getRepr()->attribute("id");
            char const *uri = g_strdup_printf("#%s", id);
            repr->setAttribute("xlink:href", uri);
            g_free((void *) uri);
        } else {
            repr->setAttribute("inkscape:href", NULL);
        }

        repr->setAttribute("style", style);

        // add the new repr to the parent
        parent->appendChild(repr);

        // move to the saved position
        repr->setPosition(pos > 0 ? pos : 0);

        SPItem *nitem = reinterpret_cast<SPItem *>(desktop->getDocument()->getObjectByRepr(repr));

        if ( !updating ) {
            // delete original, apply the transform to the offset
            const char *n_id = item->getRepr()->attribute("id");
            item->deleteObject(false);
            repr->setAttribute("id", n_id);
            nitem->doWriteTransform(transform);
        }

        // The object just created from a temporary repr is only a seed.
        // We need to invoke its write which will update its real repr (in particular adding d=)
        nitem->updateRepr();

        Inkscape::GC::release(repr);

        selection->set(nitem);
    }

    DocumentUndo::done(desktop->getDocument(), 
                       (updating ? SP_VERB_SELECTION_LINKED_OFFSET 
                        : SP_VERB_SELECTION_DYNAMIC_OFFSET),
                       (updating ? _("Create linked offset")
                        : _("Create dynamic offset")));

    delete res;
    delete orig;

    g_free(style);
}












void
sp_selected_path_do_offset(SPDesktop *desktop, bool expand, double prefOffset)
{
    Inkscape::Selection *selection = desktop->getSelection();

    if (selection->isEmpty()) {
        desktop->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Select <b>path(s)</b> to inset/outset."));
        return;
    }

    bool did = false;
    std::vector<SPItem*> il(selection->items().begin(), selection->items().end());
    for (std::vector<SPItem*>::const_iterator l = il.begin(); l != il.end(); l++){
        SPItem *item = *l;
        SPCurve *curve = NULL;

        if (!SP_IS_SHAPE(item) && !SP_IS_TEXT(item) && !SP_IS_FLOWTEXT(item))
            continue;
        else if (SP_IS_SHAPE(item)) {
            curve = SP_SHAPE(item)->getCurve();
        }
        else if (SP_IS_FLOWTEXT(item)) {
            curve = SP_FLOWTEXT(item)->getNormalizedBpath();
        }
        else { // Item must be SP_TEXT
            curve = SP_TEXT(item)->getNormalizedBpath();
        }

        if (curve == NULL)
            continue;

        Geom::Affine const transform(item->transform);

        item->doWriteTransform(Geom::identity());

        gchar *style = g_strdup(item->getRepr()->attribute("style"));

        float o_width = 0;
        float o_miter = 0;
        JoinType o_join = join_straight;
        //ButtType o_butt = butt_straight;

        {
            SPStyle *i_style = item->style;
            int jointype = i_style->stroke_linejoin.value;

            switch (jointype) {
                case SP_STROKE_LINEJOIN_MITER:
                    o_join = join_pointy;
                    break;
                case SP_STROKE_LINEJOIN_ROUND:
                    o_join = join_round;
                    break;
                default:
                    o_join = join_straight;
                    break;
            }

            o_width = prefOffset;

            if (o_width < 0.1)
                o_width = 0.1;
            o_miter = i_style->stroke_miterlimit.value * o_width;
        }

        Path *orig = Path_for_item(item, false);
        if (orig == NULL) {
            g_free(style);
            curve->unref();
            continue;
        }

        Path *res = new Path;
        res->SetBackData(false);

        {
            Shape *theShape = new Shape;
            Shape *theRes = new Shape;

            orig->ConvertWithBackData(0.03);
            orig->Fill(theShape, 0);

            SPCSSAttr *css = sp_repr_css_attr(item->getRepr(), "style");
            gchar const *val = sp_repr_css_property(css, "fill-rule", NULL);
            if (val && strcmp(val, "nonzero") == 0)
            {
                theRes->ConvertToShape(theShape, fill_nonZero);
            }
            else if (val && strcmp(val, "evenodd") == 0)
            {
                theRes->ConvertToShape(theShape, fill_oddEven);
            }
            else
            {
                theRes->ConvertToShape(theShape, fill_nonZero);
            }

            // et maintenant: offset
            // methode inexacte
/*			Path *originaux[1];
			originaux[0] = orig;
			theRes->ConvertToForme(res, 1, originaux);

			if (expand) {
                        res->OutsideOutline(orig, 0.5 * o_width, o_join, o_butt, o_miter);
			} else {
                        res->OutsideOutline(orig, -0.5 * o_width, o_join, o_butt, o_miter);
			}

			orig->ConvertWithBackData(1.0);
			orig->Fill(theShape, 0);
			theRes->ConvertToShape(theShape, fill_positive);
			originaux[0] = orig;
			theRes->ConvertToForme(res, 1, originaux);

			if (o_width >= 0.5) {
                        //     res->Coalesce(1.0);
                        res->ConvertEvenLines(1.0);
                        res->Simplify(1.0);
			} else {
                        //      res->Coalesce(o_width);
                        res->ConvertEvenLines(1.0*o_width);
                        res->Simplify(1.0 * o_width);
			}    */
            // methode par makeoffset

            if (expand)
            {
                theShape->MakeOffset(theRes, o_width, o_join, o_miter);
            }
            else
            {
                theShape->MakeOffset(theRes, -o_width, o_join, o_miter);
            }
            theRes->ConvertToShape(theShape, fill_positive);

            res->Reset();
            theRes->ConvertToForme(res);

            delete theShape;
            delete theRes;
        }

        did = true;

        curve->unref();
        // remember the position of the item
        gint pos = item->getRepr()->position();
        // remember parent
        Inkscape::XML::Node *parent = item->getRepr()->parent();
        // remember id
        char const *id = item->getRepr()->attribute("id");

        selection->remove(item);
        item->deleteObject(false);

        if (res->descr_cmd.size() > 1) { // if there's 0 or 1 node left, drop this path altogether
            Inkscape::XML::Document *xml_doc = desktop->doc()->getReprDoc();
            Inkscape::XML::Node *repr = xml_doc->createElement("svg:path");

            repr->setAttribute("style", style);

            gchar *str = res->svg_dump_path();
            repr->setAttribute("d", str);
            g_free(str);

            // add the new repr to the parent
            parent->appendChild(repr);

            // move to the saved position
            repr->setPosition(pos > 0 ? pos : 0);

            SPItem *newitem = (SPItem *) desktop->getDocument()->getObjectByRepr(repr);

            // reapply the transform
            newitem->doWriteTransform(transform);

            repr->setAttribute("id", id);

            selection->add(repr);

            Inkscape::GC::release(repr);
        }

        delete orig;
        delete res;
    }

    if (did) {
        DocumentUndo::done(desktop->getDocument(), 
                           (expand ? SP_VERB_SELECTION_OFFSET : SP_VERB_SELECTION_INSET),
                           (expand ? _("Outset path") : _("Inset path")));
    } else {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("<b>No paths</b> to inset/outset in the selection."));
        return;
    }
}


static bool
sp_selected_path_simplify_items(SPDesktop *desktop,
                                Inkscape::Selection *selection, std::vector<SPItem*> &items,
                                float threshold,  bool justCoalesce,
                                float angleLimit, bool breakableAngles,
                                bool modifySelection);


//return true if we changed something, else false
static bool
sp_selected_path_simplify_item(SPDesktop *desktop,
                 Inkscape::Selection *selection, SPItem *item,
                 float threshold,  bool justCoalesce,
                 float angleLimit, bool breakableAngles,
                 gdouble size,     bool modifySelection)
{
    if (!(SP_IS_GROUP(item) || SP_IS_SHAPE(item) || SP_IS_TEXT(item)))
        return false;

    //If this is a group, do the children instead
    if (SP_IS_GROUP(item)) {
    	std::vector<SPItem*> items = sp_item_group_item_list(SP_GROUP(item));
        
        return sp_selected_path_simplify_items(desktop, selection, items,
                                               threshold, justCoalesce,
                                               angleLimit, breakableAngles,
                                               false);
    }

    // get path to simplify (note that the path *before* LPE calculation is needed)
    Path *orig = Path_for_item_before_LPE(item, false);
    if (orig == NULL) {
        return false;
    }

    // correct virtual size by full transform (bug #166937)
    size /= item->i2doc_affine().descrim();

    // save the transform, to re-apply it after simplification
    Geom::Affine const transform(item->transform);

    /*
       reset the transform, effectively transforming the item by transform.inverse();
       this is necessary so that the item is transformed twice back and forth,
       allowing all compensations to cancel out regardless of the preferences
    */
    item->doWriteTransform(Geom::identity());

    gchar *style = g_strdup(item->getRepr()->attribute("style"));
    gchar *mask = g_strdup(item->getRepr()->attribute("mask"));
    gchar *clip_path = g_strdup(item->getRepr()->attribute("clip-path"));

    // remember the position of the item
    gint pos = item->getRepr()->position();
    // remember parent
    Inkscape::XML::Node *parent = item->getRepr()->parent();
    // remember id
    char const *id = item->getRepr()->attribute("id");
    // remember path effect
    char const *patheffect = item->getRepr()->attribute("inkscape:path-effect");
    // remember title
    gchar *title = item->title();
    // remember description
    gchar *desc = item->desc();
    
    //If a group was selected, to not change the selection list
    if (modifySelection) {
        selection->remove(item);
    }

    item->deleteObject(false);

    if ( justCoalesce ) {
        orig->Coalesce(threshold * size);
    } else {
        orig->ConvertEvenLines(threshold * size);
        orig->Simplify(threshold * size);
    }

    Inkscape::XML::Document *xml_doc = desktop->doc()->getReprDoc();
    Inkscape::XML::Node *repr = xml_doc->createElement("svg:path");

    // restore style, mask and clip-path
    repr->setAttribute("style", style);
    g_free(style);

    if ( mask ) {
        repr->setAttribute("mask", mask);
        g_free(mask);
    }

    if ( clip_path ) {
        repr->setAttribute("clip-path", clip_path);
        g_free(clip_path);
    }

    // restore path effect
    repr->setAttribute("inkscape:path-effect", patheffect);

    // path
    gchar *str = orig->svg_dump_path();
    if (patheffect)
        repr->setAttribute("inkscape:original-d", str);
    else 
        repr->setAttribute("d", str);
    g_free(str);

    // restore id
    repr->setAttribute("id", id);

    // add the new repr to the parent
    parent->appendChild(repr);

    // move to the saved position
    repr->setPosition(pos > 0 ? pos : 0);

    SPItem *newitem = (SPItem *) desktop->getDocument()->getObjectByRepr(repr);

    // reapply the transform
    newitem->doWriteTransform(transform);

    // restore title & description
    if (title) {
    	newitem->setTitle(title);
        g_free(title);
    }
    if (desc) {
    	newitem->setDesc(desc);
        g_free(desc);
    }
    
    //If we are not in a selected group
    if (modifySelection)
        selection->add(repr);

    Inkscape::GC::release(repr);

    // clean up
    if (orig) delete orig;

    return true;
}


bool
sp_selected_path_simplify_items(SPDesktop *desktop,
                                Inkscape::Selection *selection, std::vector<SPItem*> &items,
                                float threshold,  bool justCoalesce,
                                float angleLimit, bool breakableAngles,
                                bool modifySelection)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool simplifyIndividualPaths = prefs->getBool("/options/simplifyindividualpaths/value");

    gchar *simplificationType;
    if (simplifyIndividualPaths) {
        simplificationType = _("Simplifying paths (separately):");
    } else {
        simplificationType = _("Simplifying paths:");
    }

    bool didSomething = false;

    Geom::OptRect selectionBbox = selection->visualBounds();
    if (!selectionBbox) {
        return false;
    }
    gdouble selectionSize  = L2(selectionBbox->dimensions());

    gdouble simplifySize  = selectionSize;

    int pathsSimplified = 0;
    int totalPathCount  = items.size();

    // set "busy" cursor
    desktop->setWaitingCursor();

    for (std::vector<SPItem*>::const_iterator l = items.begin(); l != items.end(); l++){
        SPItem *item = *l;

        if (!(SP_IS_GROUP(item) || SP_IS_SHAPE(item) || SP_IS_TEXT(item)))
          continue;

        if (simplifyIndividualPaths) {
            Geom::OptRect itemBbox = item->desktopVisualBounds();
            if (itemBbox) {
                simplifySize      = L2(itemBbox->dimensions());
            } else {
                simplifySize      = 0;
            }
        }

        pathsSimplified++;

        if (pathsSimplified % 20 == 0) {
            gchar *message = g_strdup_printf(_("%s <b>%d</b> of <b>%d</b> paths simplified..."),
                simplificationType, pathsSimplified, totalPathCount);
            desktop->messageStack()->flash(Inkscape::IMMEDIATE_MESSAGE, message);
            g_free(message);
        }

        didSomething |= sp_selected_path_simplify_item(desktop, selection, item,
            threshold, justCoalesce, angleLimit, breakableAngles, simplifySize, modifySelection);
    }

    desktop->clearWaitingCursor();

    if (pathsSimplified > 20) {
        desktop->messageStack()->flashF(Inkscape::NORMAL_MESSAGE, _("<b>%d</b> paths simplified."), pathsSimplified);
    }

    return didSomething;
}

static void
sp_selected_path_simplify_selection(SPDesktop *desktop, float threshold, bool justCoalesce,
                                    float angleLimit, bool breakableAngles)
{
    Inkscape::Selection *selection = desktop->getSelection();

    if (selection->isEmpty()) {
        desktop->messageStack()->flash(Inkscape::WARNING_MESSAGE,
                         _("Select <b>path(s)</b> to simplify."));
        return;
    }

    std::vector<SPItem*> items(selection->items().begin(), selection->items().end());

    bool didSomething = sp_selected_path_simplify_items(desktop, selection,
                                                        items, threshold,
                                                        justCoalesce,
                                                        angleLimit,
                                                        breakableAngles, true);

    if (didSomething)
        DocumentUndo::done(desktop->getDocument(), SP_VERB_SELECTION_SIMPLIFY, 
                           _("Simplify"));
    else
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("<b>No paths</b> to simplify in the selection."));

}


// globals for keeping track of accelerated simplify
static double previousTime      = 0.0;
static gdouble simplifyMultiply = 1.0;

void
sp_selected_path_simplify(SPDesktop *desktop)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    gdouble simplifyThreshold =
        prefs->getDouble("/options/simplifythreshold/value", 0.003);
    bool simplifyJustCoalesce = prefs->getBool("/options/simplifyjustcoalesce/value", 0);

    //Get the current time
    GTimeVal currentTimeVal;
    g_get_current_time(&currentTimeVal);
    double currentTime = currentTimeVal.tv_sec * 1000000 +
                currentTimeVal.tv_usec;

    //Was the previous call to this function recent? (<0.5 sec)
    if (previousTime > 0.0 && currentTime - previousTime < 500000.0) {

        // add to the threshold 1/2 of its original value
        simplifyMultiply  += 0.5;
        simplifyThreshold *= simplifyMultiply;

    } else {
        // reset to the default
        simplifyMultiply = 1;
    }

    //remember time for next call
    previousTime = currentTime;

    //g_print("%g\n", simplify_threshold);

    //Make the actual call
    sp_selected_path_simplify_selection(desktop, simplifyThreshold,
                                        simplifyJustCoalesce, 0.0, false);
}



// fonctions utilitaires

bool
Ancetre(Inkscape::XML::Node *a, Inkscape::XML::Node *who)
{
    if (who == NULL || a == NULL)
        return false;
    if (who == a)
        return true;
    return Ancetre(a->parent(), who);
}

// derived from Path_for_item
Path *
Path_for_pathvector(Geom::PathVector const &epathv)
{
    /*std::cout << "converting to Livarot path" << std::endl;

    Geom::SVGPathWriter wr;
    wr.feed(epathv);
    std::cout << wr.str() << std::endl;*/

    Path *dest = new Path;
    dest->LoadPathVector(epathv);
    return dest;
}

Path *
Path_for_item(SPItem *item, bool doTransformation, bool transformFull)
{
    SPCurve *curve = curve_for_item(item);

    if (curve == NULL)
        return NULL;

    Geom::PathVector *pathv = pathvector_for_curve(item, curve, doTransformation, transformFull, Geom::identity(), Geom::identity());
    curve->unref();

    /*std::cout << "converting to Livarot path" << std::endl;

    Geom::SVGPathWriter wr;
    if (pathv) {
        wr.feed(*pathv);
    }
    std::cout << wr.str() << std::endl;*/

    Path *dest = new Path;
    dest->LoadPathVector(*pathv);    
    delete pathv;

    /*gchar *str = dest->svg_dump_path();
    std::cout << "After conversion:\n" << str << std::endl;
    g_free(str);*/

    return dest;
}

/**
 * Obtains an item's Path before the LPE stack has been applied.
 */
Path *
Path_for_item_before_LPE(SPItem *item, bool doTransformation, bool transformFull)
{
    SPCurve *curve = curve_for_item_before_LPE(item);

    if (curve == NULL)
        return NULL;
    
    Geom::PathVector *pathv = pathvector_for_curve(item, curve, doTransformation, transformFull, Geom::identity(), Geom::identity());
    curve->unref();
    
    Path *dest = new Path;
    dest->LoadPathVector(*pathv);
    delete pathv;

    return dest;
}

/* 
 * NOTE: Returns empty pathvector if curve == NULL
 * TODO: see if calling this method can be optimized. All the pathvector copying might be slow.
 */
Geom::PathVector*
pathvector_for_curve(SPItem *item, SPCurve *curve, bool doTransformation, bool transformFull, Geom::Affine extraPreAffine, Geom::Affine extraPostAffine)
{
    if (curve == NULL)
        return NULL;

    Geom::PathVector *dest = new Geom::PathVector;    
    *dest = curve->get_pathvector(); // Make a copy; must be freed by the caller!
    
    if (doTransformation) {
        if (transformFull) {
            *dest *= extraPreAffine * item->i2doc_affine() * extraPostAffine;
        } else {
            *dest *= extraPreAffine * (Geom::Affine)item->transform * extraPostAffine;
        }
    } else {
        *dest *= extraPreAffine * extraPostAffine;
    }

    return dest;
}

/**
 * Obtains an item's curve. For SPPath, it is the path *before* LPE. For SPShapes other than path, it is the path *after* LPE.
 * So the result is somewhat ill-defined, and probably this method should not be used... See curve_for_item_before_LPE.
 */
SPCurve* curve_for_item(SPItem *item)
{
    if (!item) 
        return NULL;
    
    SPCurve *curve = NULL;
    if (SP_IS_SHAPE(item)) {
        if (SP_IS_PATH(item)) {
            curve = SP_PATH(item)->getCurveForEdit();
        } else {
            curve = SP_SHAPE(item)->getCurve();
        }
    }
    else if (SP_IS_TEXT(item) || SP_IS_FLOWTEXT(item))
    {
        curve = te_get_layout(item)->convertToCurves();
    }
    else if (SP_IS_IMAGE(item))
    {
    curve = sp_image_get_curve(SP_IMAGE(item));
    }
    
    return curve; // do not forget to unref the curve at some point!
}

/**
 * Obtains an item's curve *before* LPE.
 * The returned SPCurve should be unreffed by the caller.
 */
SPCurve* curve_for_item_before_LPE(SPItem *item)
{
    if (!item) 
        return NULL;
    
    SPCurve *curve = NULL;
    if (SP_IS_SHAPE(item)) {
        curve = SP_SHAPE(item)->getCurveForEdit();
    }
    else if (SP_IS_TEXT(item) || SP_IS_FLOWTEXT(item))
    {
        curve = te_get_layout(item)->convertToCurves();
    }
    else if (SP_IS_IMAGE(item))
    {
        curve = sp_image_get_curve(SP_IMAGE(item));
    }
    
    return curve; // do not forget to unref the curve at some point!
}

boost::optional<Path::cut_position> get_nearest_position_on_Path(Path *path, Geom::Point p, unsigned seg)
{
    //get nearest position on path
    Path::cut_position pos = path->PointToCurvilignPosition(p, seg);
    return pos;
}

Geom::Point get_point_on_Path(Path *path, int piece, double t)
{
    Geom::Point p;
    path->PointAt(piece, t, p);
    return p;
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
