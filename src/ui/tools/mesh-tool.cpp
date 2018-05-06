/*
 * Mesh drawing and editing tool
 *
 * Authors:
 *   bulia byak <buliabyak@users.sf.net>
 *   Johan Engelen <j.b.c.engelen@ewi.utwente.nl>
 *   Abhishek Sharma
 *   Tavmjong Bah <tavmjong@free.fr>
 *
 * Copyright (C) 2012 Tavmjong Bah
 * Copyright (C) 2007 Johan Engelen
 * Copyright (C) 2005 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

//#define DEBUG_MESH


// Libraries
#include <gdk/gdkkeysyms.h>
#include <glibmm/i18n.h>

// General
#include "desktop.h"

#include "document.h"
#include "document-undo.h"
#include "macros.h"
#include "message-context.h"
#include "message-stack.h"
#include "preferences.h"
#include "rubberband.h"
#include "selection.h"
#include "snap.h"
#include "sp-namedview.h"
#include "verbs.h"
#include "sp-text.h"
#include "sp-defs.h"
#include "style.h"
#include "ui/control-manager.h"

// Gradient specific
#include "gradient-drag.h"
#include "gradient-chemistry.h"
#include "pixmaps/cursor-gradient.xpm"
#include "pixmaps/cursor-gradient-add.xpm"

// Mesh specific
#include "ui/tools/mesh-tool.h"
#include "sp-mesh-gradient.h"
#include "display/sp-ctrlcurve.h"
#include "display/curve.h"

using Inkscape::DocumentUndo;

namespace Inkscape {
namespace UI {
namespace Tools {

static void sp_mesh_new_default(MeshTool &rc);

const std::string& MeshTool::getPrefsPath() {
	return MeshTool::prefsPath;
}

const std::string MeshTool::prefsPath = "/tools/mesh";

// TODO: The gradient tool class looks like a 1:1 copy.

MeshTool::MeshTool()
    : ToolBase(cursor_gradient_xpm, 4, 4)
    , cursor_addnode(false)
    , node_added(false)
    , show_handles(true)
    , edit_fill(true)
    , edit_stroke(true)
// TODO: Why are these connections stored as pointers?
    , selcon(NULL)
    , subselcon(NULL)
{
    // TODO: This value is overwritten in the root handler
    this->tolerance = 6;
}

MeshTool::~MeshTool() {
    this->enableGrDrag(false);

    this->selcon->disconnect();
    delete this->selcon;
    
    this->subselcon->disconnect();
    delete this->subselcon;
}

// This must match GrPointType enum sp-gradient.h
// We should move this to a shared header (can't simply move to gradient.h since that would require
// including <glibmm/i18n.h> which messes up "N_" in extensions... argh!).
const gchar *ms_handle_descr [] = {
    N_("Linear gradient <b>start</b>"), //POINT_LG_BEGIN
    N_("Linear gradient <b>end</b>"),
    N_("Linear gradient <b>mid stop</b>"),
    N_("Radial gradient <b>center</b>"),
    N_("Radial gradient <b>radius</b>"),
    N_("Radial gradient <b>radius</b>"),
    N_("Radial gradient <b>focus</b>"), // POINT_RG_FOCUS
    N_("Radial gradient <b>mid stop</b>"),
    N_("Radial gradient <b>mid stop</b>"),
    N_("Mesh gradient <b>corner</b>"),
    N_("Mesh gradient <b>handle</b>"),
    N_("Mesh gradient <b>tensor</b>")
};

void MeshTool::selection_changed(Inkscape::Selection* /*sel*/) {
    GrDrag *drag = this->_grdrag;
    Inkscape::Selection *selection = this->desktop->getSelection();

    if (selection == NULL) {
        return;
    }

    guint n_obj = selection->itemList().size();

    if (!drag->isNonEmpty() || selection->isEmpty()) {
        return;
    }

    guint n_tot = drag->numDraggers();
    guint n_sel = drag->numSelected();

    //The use of ngettext in the following code is intentional even if the English singular form would never be used
    if (n_sel == 1) {
        if (drag->singleSelectedDraggerNumDraggables() == 1) {
            gchar * message = g_strconcat(
                //TRANSLATORS: %s will be substituted with the point name (see previous messages); This is part of a compound message
                _("%s selected"),
                //TRANSLATORS: Mind the space in front. This is part of a compound message
                ngettext(" out of %d mesh handle"," out of %d mesh handles",n_tot),
                ngettext(" on %d selected object"," on %d selected objects",n_obj),NULL);
            this->message_context->setF(Inkscape::NORMAL_MESSAGE,
                                       message,_(ms_handle_descr[drag->singleSelectedDraggerSingleDraggableType()]), n_tot, n_obj);
        } else {
            gchar * message =
                g_strconcat(
                    //TRANSLATORS: This is a part of a compound message (out of two more indicating: grandint handle count & object count)
                    ngettext("One handle merging %d stop (drag with <b>Shift</b> to separate) selected",
                             "One handle merging %d stops (drag with <b>Shift</b> to separate) selected",
                             drag->singleSelectedDraggerNumDraggables()),
                    ngettext(" out of %d mesh handle"," out of %d mesh handles",n_tot),
                    ngettext(" on %d selected object"," on %d selected objects",n_obj),NULL);
            this->message_context->setF(Inkscape::NORMAL_MESSAGE,message,drag->singleSelectedDraggerNumDraggables(), n_tot, n_obj);
        }
    } else if (n_sel > 1) {
        //TRANSLATORS: The plural refers to number of selected mesh handles. This is part of a compound message (part two indicates selected object count)
        gchar * message =
            g_strconcat(ngettext("<b>%d</b> mesh handle selected out of %d","<b>%d</b> mesh handles selected out of %d",n_sel),
                        //TRANSLATORS: Mind the space in front. (Refers to gradient handles selected). This is part of a compound message
                        ngettext(" on %d selected object"," on %d selected objects",n_obj),NULL);
        this->message_context->setF(Inkscape::NORMAL_MESSAGE,message, n_sel, n_tot, n_obj);
    } else if (n_sel == 0) {
        this->message_context->setF(Inkscape::NORMAL_MESSAGE,
                                   //TRANSLATORS: The plural refers to number of selected objects
                                   ngettext("<b>No</b> mesh handles selected out of %d on %d selected object",
                                            "<b>No</b> mesh handles selected out of %d on %d selected objects",n_obj), n_tot, n_obj);
    }

    // FIXME
    // We need to update mesh gradient handles.
    // Get gradient this drag belongs too..
    // std::cout << "mesh_selection_changed: selection: objects: " << n_obj << std::endl;
    // GSList *itemList = (GSList *) selection->itemList();
    // while( itemList ) {

    //     SPItem *item = SP_ITEM( itemList->data );
    //     // std::cout << "  item: " << SP_OBJECT(item)->getId() << std::endl;

    //     SPStyle *style = item->style;
    //     if (style && (style->fill.isPaintserver())) {

    //         SPPaintServer *server = item->style->getFillPaintServer();
    //         if ( SP_IS_MESHGRADIENT(server) ) {

    //             SPMeshGradient *mg = SP_MESHGRADIENT(server);

    //             guint rows    = 0;//mg->array.patches.size();
    //             for ( guint i = 0; i < rows; ++i ) {
    //                 guint columns = 0;//mg->array.patches[0].size();
    //                 for ( guint j = 0; j < columns; ++j ) {
    //                 }
    //             }
    //         }
    //     }
    //     itemList = itemList->next;
    // }

    // GList* dragger_ptr = drag->draggers;  // Points to GrDragger class (group of GrDraggable)
    // guint count = 0;
    // while( dragger_ptr ) {

    //     std::cout << "mesh_selection_changed: dragger: " << ++count << std::endl;
    //     GSList* draggable_ptr = ((GrDragger *) dragger_ptr->data)->draggables;

    //     while( draggable_ptr ) {

    //         std::cout << "mesh_selection_changed:  draggable: " << draggable_ptr << std::endl;
    //         GrDraggable *draggable = (GrDraggable *) draggable_ptr->data;

    //         gint point_type     = draggable->point_type;
    //         gint point_i        = draggable->point_i;
    //         bool fill_or_stroke = draggable->fill_or_stroke;

    //         if( point_type == POINT_MG_CORNER ) {

    //             //std::cout << "mesh_selection_changed:   POINT_MG_CORNER: " << point_i << std::endl;
    //             // Now we must create or destroy corresponding handles.

    //             if( g_list_find( drag->selected, dragger_ptr->data ) ) {
    //                 //std::cout << "gradient_selection_changed:    Selected: " << point_i << std::endl;
    //                 // Which meshes does this point belong to?

    //             } else {
    //                 //std::cout << "mesh_selection_changed:    Not Selected: " << point_i << std::endl;
    //             }
    //         }

    //         draggable_ptr = draggable_ptr->next;

    //     }

    //     dragger_ptr = dragger_ptr->next;
    // }
}

void MeshTool::setup() {
    ToolBase::setup();

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    if (prefs->getBool("/tools/mesh/selcue", true)) {
        this->enableSelectionCue();
    }

    this->enableGrDrag();
    Inkscape::Selection *selection = this->desktop->getSelection();

    this->selcon = new sigc::connection(selection->connectChanged(
    	sigc::mem_fun(this, &MeshTool::selection_changed)
    ));

    this->subselcon = new sigc::connection(this->desktop->connectToolSubselectionChanged(
    	sigc::hide(sigc::bind(
    		sigc::mem_fun(*this, &MeshTool::selection_changed),
    		(Inkscape::Selection*)NULL)
    	)
    ));

    sp_event_context_read(this, "show_handles");
    sp_event_context_read(this, "edit_fill");
    sp_event_context_read(this, "edit_stroke");

    this->selection_changed(selection);

}

void MeshTool::set(const Inkscape::Preferences::Entry& value) {
    Glib::ustring entry_name = value.getEntryName();
    if (entry_name == "show_handles") {
        this->show_handles = value.getBool(true);
    } else if (entry_name == "edit_fill") {
        this->edit_fill = value.getBool(true);
    } else if (entry_name == "edit_stroke") {
        this->edit_stroke = value.getBool(true);
    } else {
        ToolBase::set(value);
    }
}

void
sp_mesh_context_select_next (ToolBase *event_context)
{
    GrDrag *drag = event_context->_grdrag;
    g_assert (drag);

    GrDragger *d = drag->select_next();

    event_context->desktop->scroll_to_point(d->point, 1.0);
}

void
sp_mesh_context_select_prev (ToolBase *event_context)
{
    GrDrag *drag = event_context->_grdrag;
    g_assert (drag);

    GrDragger *d = drag->select_prev();

    event_context->desktop->scroll_to_point(d->point, 1.0);
}

/**
Returns vector of control lines mouse is over. Returns only first if 'first' is true.
*/
static std::vector<SPCtrlCurve *>
sp_mesh_context_over_line (MeshTool *rc, Geom::Point event_p, bool first = true)
{
    SPDesktop *desktop = SP_EVENT_CONTEXT (rc)->desktop;

    //Translate mouse point into proper coord system
    rc->mousepoint_doc = desktop->w2d(event_p);

    double tolerance = (double) SP_EVENT_CONTEXT(rc)->tolerance;

    GrDrag *drag = rc->_grdrag;

    std::vector<SPCtrlCurve *> selected;

    for (std::vector<SPCtrlLine *>::const_iterator l = drag->lines.begin(); l != drag->lines.end(); ++l) {
        if (!SP_IS_CTRLCURVE(*l)) continue;

        SPCtrlCurve *curve = SP_CTRLCURVE(*l);
        Geom::BezierCurveN<3> b( curve->p0, curve->p1, curve->p2, curve->p3 );
        Geom::Coord coord = b.nearestTime( rc->mousepoint_doc ); // Coord == double
        Geom::Point nearest = b( coord );

        double dist_screen = Geom::L2 (rc->mousepoint_doc - nearest) * desktop->current_zoom();
        if (dist_screen < tolerance) {
            selected.push_back(curve);
            if (first) {
                break;
            }
        }
    }
    return selected;
}


/**
Split row/column near the mouse point.
*/
static void sp_mesh_context_split_near_point(MeshTool *rc, SPItem *item,  Geom::Point mouse_p, guint32 /*etime*/)
{

#ifdef DEBUG_MESH
    std::cout << "sp_mesh_context_split_near_point: entrance: " << mouse_p << std::endl;
#endif

    // item is the selected item. mouse_p the location in doc coordinates of where to add the stop

    ToolBase *ec = SP_EVENT_CONTEXT(rc);
    SPDesktop *desktop = SP_EVENT_CONTEXT (rc)->desktop;

    double tolerance = (double) ec->tolerance;

    ec->get_drag()->addStopNearPoint (item, mouse_p, tolerance/desktop->current_zoom());

    DocumentUndo::done(desktop->getDocument(), SP_VERB_CONTEXT_MESH,
                       _("Split mesh row/column"));

    ec->get_drag()->updateDraggers();
}

/**
Wrapper for various mesh operations that require a list of selected corner nodes.
 */
void
sp_mesh_context_corner_operation (MeshTool *rc, MeshCornerOperation operation )
{

#ifdef DEBUG_MESH
    std::cout << "sp_mesh_corner_operation: entrance: " << operation << std::endl;
#endif

    SPDocument *doc = NULL;
    GrDrag *drag = rc->_grdrag;

    std::map<SPMeshGradient*, std::vector<guint> > points;
    std::map<SPMeshGradient*, SPItem*> items;
    std::map<SPMeshGradient*, Inkscape::PaintTarget> fill_or_stroke;

    // Get list of selected draggers for each mesh.
    // For all selected draggers (a dragger may include draggerables from different meshes).
    for (std::set<GrDragger *>::const_iterator i = drag->selected.begin(); i != drag->selected.end(); ++i) {
        GrDragger *dragger = *i;
        // For all draggables of dragger (a draggable corresponds to a unique mesh).
        for (std::vector<GrDraggable *>::const_iterator j = dragger->draggables.begin(); j != dragger->draggables.end() ; ++j) { 
            GrDraggable *d = *j;

            // Only mesh corners
            if( d->point_type != POINT_MG_CORNER ) continue;

            // Find the gradient
            SPMeshGradient *gradient = SP_MESHGRADIENT( getGradient (d->item, d->fill_or_stroke) );

            // Collect points together for same gradient
            points[gradient].push_back( d->point_i );
            items[gradient] = d->item;
            fill_or_stroke[gradient] = d->fill_or_stroke ? Inkscape::FOR_FILL: Inkscape::FOR_STROKE;
        }
    }

    // Loop over meshes.
    for( std::map<SPMeshGradient*, std::vector<guint> >::const_iterator iter = points.begin(); iter != points.end(); ++iter) {
        SPMeshGradient *mg = SP_MESHGRADIENT( iter->first );
        if( iter->second.size() > 0 ) {
            guint noperation = 0;
            switch (operation) {

                case MG_CORNER_SIDE_TOGGLE:
                    // std::cout << "SIDE_TOGGLE" << std::endl;
                    noperation += mg->array.side_toggle( iter->second );
                    break;

                case MG_CORNER_SIDE_ARC:
                    // std::cout << "SIDE_ARC" << std::endl;
                    noperation += mg->array.side_arc( iter->second );
                    break;

                case MG_CORNER_TENSOR_TOGGLE:
                    // std::cout << "TENSOR_TOGGLE" << std::endl;
                    noperation += mg->array.tensor_toggle( iter->second );
                    break;

                case MG_CORNER_COLOR_SMOOTH:
                    // std::cout << "COLOR_SMOOTH" << std::endl;
                    noperation += mg->array.color_smooth( iter->second );
                    break;

                case MG_CORNER_COLOR_PICK:
                    // std::cout << "COLOR_PICK" << std::endl;
                    noperation += mg->array.color_pick( iter->second, items[iter->first] );
                    break;

                case MG_CORNER_INSERT:
                    // std::cout << "INSERT" << std::endl;
                    noperation += mg->array.insert( iter->second );
                    break;

                default:
                    std::cout << "sp_mesh_corner_operation: unknown operation" << std::endl;
            }                    

            if( noperation > 0 ) {
                mg->array.write( mg );
                mg->requestModified(SP_OBJECT_MODIFIED_FLAG);
                doc = mg->document;

                switch (operation) {

                    case MG_CORNER_SIDE_TOGGLE:
                        DocumentUndo::done(doc, SP_VERB_CONTEXT_MESH, _("Toggled mesh path type."));
                        drag->local_change = true; // Don't create new draggers.
                        break;

                    case MG_CORNER_SIDE_ARC:
                        DocumentUndo::done(doc, SP_VERB_CONTEXT_MESH, _("Approximated arc for mesh side."));
                        drag->local_change = true; // Don't create new draggers.
                        break;

                    case MG_CORNER_TENSOR_TOGGLE:
                        DocumentUndo::done(doc, SP_VERB_CONTEXT_MESH, _("Toggled mesh tensors."));
                        drag->local_change = true; // Don't create new draggers.
                        break;

                    case MG_CORNER_COLOR_SMOOTH:
                        DocumentUndo::done(doc, SP_VERB_CONTEXT_MESH, _("Smoothed mesh corner color."));
                        drag->local_change = true; // Don't create new draggers.
                        break;

                    case MG_CORNER_COLOR_PICK:
                        DocumentUndo::done(doc, SP_VERB_CONTEXT_MESH, _("Picked mesh corner color."));
                        drag->local_change = true; // Don't create new draggers.
                        break;

                    case MG_CORNER_INSERT:
                        DocumentUndo::done(doc, SP_VERB_CONTEXT_MESH, _("Inserted new row or column."));
                        break;

                    default:
                        std::cout << "sp_mesh_corner_operation: unknown operation" << std::endl;
                }
            }
        }
    }

    // Not needed. Update is done via gr_drag_sel_modified().
    // drag->updateDraggers();

}


/**
 * Scale mesh to just fit into bbox of selected items.
 */
void
sp_mesh_context_fit_mesh_in_bbox (MeshTool *rc)
{

#ifdef DEBUG_MESH
    std::cout << "sp_mesh_context_fit_mesh_in_bbox: entrance: Entrance"<< std::endl;
#endif

    SPDesktop *desktop = SP_EVENT_CONTEXT (rc)->desktop;

    Inkscape::Selection *selection = desktop->getSelection();
    if (selection->isEmpty()) {
        return;
    }

    bool changed = false;
    std::vector<SPItem*> items(selection->itemList());
    for (std::vector<SPItem*>::const_iterator i = items.begin(); i != items.end(); i++){
        SPItem *item = *i;
        SPStyle *style = item->style;

        if (style) {

            if (style->fill.isPaintserver()) {
                SPPaintServer *server = item->style->getFillPaintServer();
                if ( SP_IS_MESHGRADIENT(server) ) {

                    Geom::OptRect item_bbox = item->geometricBounds();
                    SPMeshGradient *gradient = SP_MESHGRADIENT(server);
                    if (gradient->array.fill_box( item_bbox )) {
                        changed = true;
                    }
                }
            }

            if (style->stroke.isPaintserver()) {
                SPPaintServer *server = item->style->getStrokePaintServer();
                if ( SP_IS_MESHGRADIENT(server) ) {

                    Geom::OptRect item_bbox = item->visualBounds();
                    SPMeshGradient *gradient = SP_MESHGRADIENT(server);
                    if (gradient->array.fill_box( item_bbox )) {
                        changed = true;
                    }
                }
            }

        }
    }
    if (changed) {
        DocumentUndo::done(desktop->getDocument(), SP_VERB_CONTEXT_MESH,
                           _("Fit mesh inside bounding box."));
    }
}


/**
Handles all keyboard and mouse input for meshs.
Note: node/handle events are take care of elsewhere.
*/
bool MeshTool::root_handler(GdkEvent* event) {
    static bool dragging;

    Inkscape::Selection *selection = desktop->getSelection();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    this->tolerance = prefs->getIntLimited("/options/dragtolerance/value", 0, 0, 100);
    double const nudge = prefs->getDoubleLimited("/options/nudgedistance/value", 2, 0, 1000, "px"); // in px

    GrDrag *drag = this->_grdrag;
    g_assert (drag);

    gint ret = FALSE;

    switch (event->type) {
    case GDK_2BUTTON_PRESS:

#ifdef DEBUG_MESH
        std::cout << "sp_mesh_context_root_handler: GDK_2BUTTON_PRESS" << std::endl;
#endif

        // Double click:
        //  If over a mesh line, divide mesh row/column
        //  If not over a line and no mesh, create new mesh for top selected object.

        if ( event->button.button == 1 ) {

            // Are we over a mesh line?
            std::vector<SPCtrlCurve *> over_line =
                sp_mesh_context_over_line(this, Geom::Point(event->motion.x, event->motion.y));

            if (!over_line.empty()) {
                // We take the first item in selection, because with doubleclick, the first click
                // always resets selection to the single object under cursor
                sp_mesh_context_split_near_point(this, selection->itemList()[0], this->mousepoint_doc, event->button.time);
            } else {
                // Create a new gradient with default coordinates.

                // Check if object already has mesh... if it does,
                // don't create new mesh with click-drag.
                bool has_mesh = false;
                if (!selection->isEmpty()) {
                    SPStyle *style = selection->itemList()[0]->style;
                    if (style) {
                        Inkscape::PaintTarget fill_or_stroke =
                            (prefs->getInt("/tools/gradient/newfillorstroke", 1) != 0) ?
                            Inkscape::FOR_FILL : Inkscape::FOR_STROKE;
                        SPPaintServer *server =
                            (fill_or_stroke == Inkscape::FOR_FILL) ?
                            style->getFillPaintServer():
                            style->getStrokePaintServer();
                        if (server && SP_IS_MESHGRADIENT(server)) 
                            has_mesh = true;
                    }
                }

                if (!has_mesh) {
                    sp_mesh_new_default(*this);
                }
            }

            ret = TRUE;
        }
        break;

    case GDK_BUTTON_PRESS:

#ifdef DEBUG_MESH
        std::cout << "sp_mesh_context_root_handler: GDK_BUTTON_PRESS" << std::endl;
#endif

        // Button down
        //  If mesh already exists, do rubber band selection.
        //  Else set origin for drag which will create a new gradient.
         if ( event->button.button == 1 && !this->space_panning ) {

            // Are we over a mesh line?
            std::vector<SPCtrlCurve *> over_line =
                sp_mesh_context_over_line(this, Geom::Point(event->motion.x, event->motion.y), false);

            if (!over_line.empty()) {
                for (std::vector<SPCtrlCurve *>::const_iterator it = over_line.begin();
                     it != over_line.end(); ++it ) {
                    SPItem *item = (*it)->item;
                    Inkscape::PaintTarget fill_or_stroke =
                        (*it)->is_fill ? Inkscape::FOR_FILL : Inkscape::FOR_STROKE;
                    GrDragger* dragger0 = drag->getDraggerFor(item, POINT_MG_CORNER, (*it)->corner0, fill_or_stroke);
                    GrDragger* dragger1 = drag->getDraggerFor(item, POINT_MG_CORNER, (*it)->corner1, fill_or_stroke);
                    bool add    = (event->button.state & GDK_SHIFT_MASK);
                    bool toggle = (event->button.state & GDK_CONTROL_MASK);
                    if ( !add && !toggle ) {
                        drag->deselectAll();
                    }
                    drag->setSelected( dragger0, true, !toggle );
                    drag->setSelected( dragger1, true, !toggle );
                }
                ret = true;
                break; // To avoid putting the following code in an else block.
            }

            Geom::Point button_w(event->button.x, event->button.y);

            // save drag origin
            this->xp = (gint) button_w[Geom::X];
            this->yp = (gint) button_w[Geom::Y];
            this->within_tolerance = true;

            dragging = true;

            Geom::Point button_dt = desktop->w2d(button_w);
            // Check if object already has mesh... if it does,
            // don't create new mesh with click-drag.
            bool has_mesh = false;
            if (!selection->isEmpty()) {
                SPStyle *style = selection->itemList()[0]->style;
                if (style) {
                    Inkscape::PaintTarget fill_or_stroke =
                        (prefs->getInt("/tools/gradient/newfillorstroke", 1) != 0) ?
                        Inkscape::FOR_FILL : Inkscape::FOR_STROKE;
                    SPPaintServer *server =
                        (fill_or_stroke == Inkscape::FOR_FILL) ?
                        style->getFillPaintServer():
                        style->getStrokePaintServer();
                    if (server && SP_IS_MESHGRADIENT(server)) 
                        has_mesh = true;
                }
            }

            if (has_mesh) {
                Inkscape::Rubberband::get(desktop)->start(desktop, button_dt);
            }

            // remember clicked item, disregarding groups, honoring Alt; do nothing with Crtl to
            // enable Ctrl+doubleclick of exactly the selected item(s)
            if (!(event->button.state & GDK_CONTROL_MASK)) {
                this->item_to_select = sp_event_context_find_item (desktop, button_w, event->button.state & GDK_MOD1_MASK, TRUE);
            }

            if (!selection->isEmpty()) {
                SnapManager &m = desktop->namedview->snap_manager;
                m.setup(desktop);
                m.freeSnapReturnByRef(button_dt, Inkscape::SNAPSOURCE_NODE_HANDLE);
                m.unSetup();
            }

            this->origin = button_dt;

            ret = TRUE;
        }
        break;

    case GDK_MOTION_NOTIFY:
        // Mouse move
        if ( dragging && ( event->motion.state & GDK_BUTTON1_MASK ) && !this->space_panning ) {
 
#ifdef DEBUG_MESH
            std::cout << "sp_mesh_context_root_handler: GDK_MOTION_NOTIFY: Dragging" << std::endl;
#endif
            if ( this->within_tolerance
                 && ( abs( (gint) event->motion.x - this->xp ) < this->tolerance )
                 && ( abs( (gint) event->motion.y - this->yp ) < this->tolerance ) ) {
                break; // do not drag if we're within tolerance from origin
            }
            // Once the user has moved farther than tolerance from the original location
            // (indicating they intend to draw, not click), then always process the
            // motion notify coordinates as given (no snapping back to origin)
            this->within_tolerance = false;

            Geom::Point const motion_w(event->motion.x,
                                     event->motion.y);
            Geom::Point const motion_dt = this->desktop->w2d(motion_w);

            if (Inkscape::Rubberband::get(desktop)->is_started()) {
                Inkscape::Rubberband::get(desktop)->move(motion_dt);
                this->defaultMessageContext()->set(Inkscape::NORMAL_MESSAGE, _("<b>Draw around</b> handles to select them"));
            } else {
                // Do nothing. For a linear/radial gradient we follow the drag, updating the
                // gradient as the end node is dragged. For a mesh gradient, the gradient is always
                // created to fill the object when the drag ends.
            }

            gobble_motion_events(GDK_BUTTON1_MASK);

            ret = TRUE;
        } else {
            // Not dragging

            // Do snapping
            if (!drag->mouseOver() && !selection->isEmpty()) {
                SnapManager &m = desktop->namedview->snap_manager;
                m.setup(desktop);

                Geom::Point const motion_w(event->motion.x, event->motion.y);
                Geom::Point const motion_dt = this->desktop->w2d(motion_w);

                m.preSnap(Inkscape::SnapCandidatePoint(motion_dt, Inkscape::SNAPSOURCE_OTHER_HANDLE));
                m.unSetup();
            }

            // Highlight corner node corresponding to side or tensor node
            if( drag->mouseOver() ) {
                // MESH FIXME: Light up corresponding corner node corresponding to node we are over.
                // See "pathflash" in ui/tools/node-tool.cpp for ideas.
                // Use desktop->add_temporary_canvasitem( SPCanvasItem, milliseconds );
            }

            // Change cursor shape if over line
            std::vector<SPCtrlCurve *> over_line =
                sp_mesh_context_over_line(this, Geom::Point(event->motion.x, event->motion.y));

            if (this->cursor_addnode && over_line.empty()) {
                this->cursor_shape = cursor_gradient_xpm;
                this->sp_event_context_update_cursor();
                this->cursor_addnode = false;
            } else if (!this->cursor_addnode && !over_line.empty()) {
                this->cursor_shape = cursor_gradient_add_xpm;
                this->sp_event_context_update_cursor();
                this->cursor_addnode = true;
            }
        }
        break;

    case GDK_BUTTON_RELEASE:

#ifdef DEBUG_MESH
        std::cout << "sp_mesh_context_root_handler: GDK_BUTTON_RELEASE" << std::endl;
#endif

        this->xp = this->yp = 0;

        if ( event->button.button == 1 && !this->space_panning ) {

            // Check if over line
            std::vector<SPCtrlCurve *> over_line =
                sp_mesh_context_over_line(this, Geom::Point(event->motion.x, event->motion.y));

            if ( (event->button.state & GDK_CONTROL_MASK) && (event->button.state & GDK_MOD1_MASK ) ) {
                if (!over_line.empty()) {
                    sp_mesh_context_split_near_point(this, over_line[0]->item,
                                                     this->mousepoint_doc, 0);
                    ret = TRUE;
                }
            } else {
                dragging = false;

                // unless clicked with Ctrl (to enable Ctrl+doubleclick).
                if (event->button.state & GDK_CONTROL_MASK) {
                    ret = TRUE;
                    break;
                }

                if (!this->within_tolerance) {

                    // Check if object already has mesh... if it does,
                    // don't create new mesh with click-drag.
                    bool has_mesh = false;
                    if (!selection->isEmpty()) {
                        SPStyle *style = selection->itemList()[0]->style;
                        if (style) {
                            Inkscape::PaintTarget fill_or_stroke =
                                (prefs->getInt("/tools/gradient/newfillorstroke", 1) != 0) ?
                                Inkscape::FOR_FILL : Inkscape::FOR_STROKE;
                            SPPaintServer *server =
                                (fill_or_stroke == Inkscape::FOR_FILL) ?
                                style->getFillPaintServer():
                                style->getStrokePaintServer();
                            if (server && SP_IS_MESHGRADIENT(server)) 
                                has_mesh = true;
                        }
                    }

                    if (!has_mesh) {
                        sp_mesh_new_default(*this);
                    } else {

                        // we've been dragging, either create a new gradient
                        // or rubberband-select if we have rubberband
                        Inkscape::Rubberband *r = Inkscape::Rubberband::get(desktop);

                        if (r->is_started() && !this->within_tolerance) {
                            // this was a rubberband drag
                            if (r->getMode() == RUBBERBAND_MODE_RECT) {
                                Geom::OptRect const b = r->getRectangle();
                                if (!(event->button.state & GDK_SHIFT_MASK)) {
                                    drag->deselectAll();
                                }
                                drag->selectRect(*b);
                            }
                        }
                    }

                } else if (this->item_to_select) {
                    if (!over_line.empty()) {
                        // Clicked on an existing mesh line, don't change selection. This stops
                        // possible change in selection during a double click with overlapping objects
                    } else {
                        // no dragging, select clicked item if any
                        if (event->button.state & GDK_SHIFT_MASK) {
                            selection->toggle(this->item_to_select);
                        } else {
                            drag->deselectAll();
                            selection->set(this->item_to_select);
                        }
                    }
                } else {
                    if (!over_line.empty()) {
                        // Clicked on an existing mesh line, don't change selection. This stops
                        // possible change in selection during a double click with overlapping objects
                    } else {
                        // click in an empty space; do the same as Esc
                        if (!drag->selected.empty()) {
                            drag->deselectAll();
                        } else {
                            selection->clear();
                        }
                    }
                }

                this->item_to_select = NULL;
                ret = TRUE;
            }

            Inkscape::Rubberband::get(desktop)->stop();
        }
        break;

    case GDK_KEY_PRESS:

#ifdef DEBUG_MESH
        std::cout << "sp_mesh_context_root_handler: GDK_KEY_PRESS" << std::endl;
#endif

        // FIXME: tip
        switch (get_latin_keyval (&event->key)) {
        case GDK_KEY_Alt_L:
        case GDK_KEY_Alt_R:
        case GDK_KEY_Control_L:
        case GDK_KEY_Control_R:
        case GDK_KEY_Shift_L:
        case GDK_KEY_Shift_R:
        case GDK_KEY_Meta_L:  // Meta is when you press Shift+Alt (at least on my machine)
        case GDK_KEY_Meta_R:

            // sp_event_show_modifier_tip (this->defaultMessageContext(), event,
            //                             _("FIXME<b>Ctrl</b>: snap mesh angle"),
            //                             _("FIXME<b>Shift</b>: draw mesh around the starting point"),
            //                             NULL);
            break;

        case GDK_KEY_A:
        case GDK_KEY_a:
            if (MOD__CTRL_ONLY(event) && drag->isNonEmpty()) {
                drag->selectAll();
                ret = TRUE;
            }
            break;

        case GDK_KEY_Escape:
            if (!drag->selected.empty()) {
                drag->deselectAll();
            } else {
                selection->clear();
            }

            ret = TRUE;
            //TODO: make dragging escapable by Esc
            break;

        case GDK_KEY_Left: // move handle left
        case GDK_KEY_KP_Left:
        case GDK_KEY_KP_4:
            if (!MOD__CTRL(event)) { // not ctrl
                gint mul = 1 + gobble_key_events(get_latin_keyval(&event->key), 0); // with any mask

                if (MOD__ALT(event)) { // alt
                    if (MOD__SHIFT(event)) {
                    	drag->selected_move_screen(mul*-10, 0); // shift
                    } else {
                    	drag->selected_move_screen(mul*-1, 0); // no shift
                    }
                } else { // no alt
                    if (MOD__SHIFT(event)) {
                    	drag->selected_move(mul*-10*nudge, 0); // shift
                    } else {
                    	drag->selected_move(mul*-nudge, 0); // no shift
                    }
                }

                ret = TRUE;
            }
            break;

        case GDK_KEY_Up: // move handle up
        case GDK_KEY_KP_Up:
        case GDK_KEY_KP_8:
            if (!MOD__CTRL(event)) { // not ctrl
                gint mul = 1 + gobble_key_events(get_latin_keyval(&event->key), 0); // with any mask

                if (MOD__ALT(event)) { // alt
                    if (MOD__SHIFT(event)) {
                    	drag->selected_move_screen(0, mul*10); // shift
                    } else {
                    	drag->selected_move_screen(0, mul*1); // no shift
                    }
                } else { // no alt
                    if (MOD__SHIFT(event)) {
                    	drag->selected_move(0, mul*10*nudge); // shift
                    } else {
                    	drag->selected_move(0, mul*nudge); // no shift
                    }
                }

                ret = TRUE;
            }
            break;

        case GDK_KEY_Right: // move handle right
        case GDK_KEY_KP_Right:
        case GDK_KEY_KP_6:
            if (!MOD__CTRL(event)) { // not ctrl
                gint mul = 1 + gobble_key_events(get_latin_keyval(&event->key), 0); // with any mask

                if (MOD__ALT(event)) { // alt
                    if (MOD__SHIFT(event)) {
                    	drag->selected_move_screen(mul*10, 0); // shift
                    } else {
                    	drag->selected_move_screen(mul*1, 0); // no shift
                    }
                } else { // no alt
                    if (MOD__SHIFT(event)) {
                    	drag->selected_move(mul*10*nudge, 0); // shift
                    } else {
                    	drag->selected_move(mul*nudge, 0); // no shift
                    }
                }

                ret = TRUE;
            }
            break;

        case GDK_KEY_Down: // move handle down
        case GDK_KEY_KP_Down:
        case GDK_KEY_KP_2:
            if (!MOD__CTRL(event)) { // not ctrl
                gint mul = 1 + gobble_key_events(get_latin_keyval(&event->key), 0); // with any mask

                if (MOD__ALT(event)) { // alt
                    if (MOD__SHIFT(event)) {
                    	drag->selected_move_screen(0, mul*-10); // shift
                    } else {
                    	drag->selected_move_screen(0, mul*-1); // no shift
                    }
                } else { // no alt
                    if (MOD__SHIFT(event)) {
                    	drag->selected_move(0, mul*-10*nudge); // shift
                    } else {
                    	drag->selected_move(0, mul*-nudge); // no shift
                    }
                }

                ret = TRUE;
            }
            break;

        // Mesh Operations --------------------------------------------

        case GDK_KEY_Insert:
        case GDK_KEY_KP_Insert:
            // with any modifiers:
            sp_mesh_context_corner_operation ( this, MG_CORNER_INSERT );
            ret = TRUE;
            break;

        case GDK_KEY_i:
        case GDK_KEY_I:
            if (MOD__SHIFT_ONLY(event)) {
                // Shift+I - insert corners (alternate keybinding for keyboards
                //           that don't have the Insert key)
                sp_mesh_context_corner_operation ( this, MG_CORNER_INSERT );
                ret = TRUE;
            }
            break;

        case GDK_KEY_Delete:
        case GDK_KEY_KP_Delete:
        case GDK_KEY_BackSpace:
            if ( !drag->selected.empty() ) {
                ret = TRUE;
            }
            break;

        case GDK_KEY_b:  // Toggle mesh side between lineto and curveto.
        case GDK_KEY_B: 
            if (MOD__ALT(event) && drag->isNonEmpty() && drag->hasSelection()) {
                sp_mesh_context_corner_operation ( this, MG_CORNER_SIDE_TOGGLE );
                ret = TRUE;
            }
            break;

        case GDK_KEY_c:  // Convert mesh side from generic Bezier to Bezier approximating arc,
        case GDK_KEY_C:  // preserving handle direction.
            if (MOD__ALT(event) && drag->isNonEmpty() && drag->hasSelection()) {
                sp_mesh_context_corner_operation ( this, MG_CORNER_SIDE_ARC );
                ret = TRUE;
            }
            break;

        case GDK_KEY_g:  // Toggle mesh tensor points on/off
        case GDK_KEY_G: 
            if (MOD__ALT(event) && drag->isNonEmpty() && drag->hasSelection()) {
                sp_mesh_context_corner_operation ( this, MG_CORNER_TENSOR_TOGGLE );
                ret = TRUE;
            }
            break;

        case GDK_KEY_j:  // Smooth corner color
        case GDK_KEY_J:
            if (MOD__ALT(event) && drag->isNonEmpty() && drag->hasSelection()) {
                sp_mesh_context_corner_operation ( this, MG_CORNER_COLOR_SMOOTH );
                ret = TRUE;
            }
            break;

        case GDK_KEY_k:  // Pick corner color
        case GDK_KEY_K:
            if (MOD__ALT(event) && drag->isNonEmpty() && drag->hasSelection()) {
                sp_mesh_context_corner_operation ( this, MG_CORNER_COLOR_PICK );
                ret = TRUE;
            }
            break;

        default:
            break;
        }

        break;

    case GDK_KEY_RELEASE:

#ifdef DEBUG_MESH
        std::cout << "sp_mesh_context_root_handler: GDK_KEY_RELEASE" << std::endl;
#endif
        switch (get_latin_keyval (&event->key)) {
        case GDK_KEY_Alt_L:
        case GDK_KEY_Alt_R:
        case GDK_KEY_Control_L:
        case GDK_KEY_Control_R:
        case GDK_KEY_Shift_L:
        case GDK_KEY_Shift_R:
        case GDK_KEY_Meta_L:  // Meta is when you press Shift+Alt
        case GDK_KEY_Meta_R:
            this->defaultMessageContext()->clear();
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    if (!ret) {
    	ret = ToolBase::root_handler(event);
    }

    return ret;
}

// Creates a new mesh gradient.
static void sp_mesh_new_default(MeshTool &rc) {
    SPDesktop *desktop = SP_EVENT_CONTEXT(&rc)->desktop;
    Inkscape::Selection *selection = desktop->getSelection();
    SPDocument *document = desktop->getDocument();
    ToolBase *ec = SP_EVENT_CONTEXT(&rc);

    if (!selection->isEmpty()) {

        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        Inkscape::PaintTarget fill_or_stroke =
            (prefs->getInt("/tools/gradient/newfillorstroke", 1) != 0) ?
            Inkscape::FOR_FILL : Inkscape::FOR_STROKE;

        // Ensure mesh is immediately editable.
        // Editting both fill and stroke at same time doesn't work well so avoid.
        if (fill_or_stroke == Inkscape::FOR_FILL) {
            prefs->setBool("/tools/mesh/edit_fill",   true );
            prefs->setBool("/tools/mesh/edit_stroke", false);
        } else {
            prefs->setBool("/tools/mesh/edit_fill",   false);
            prefs->setBool("/tools/mesh/edit_stroke", true );
        }

// HACK: reset fill-opacity - that 0.75 is annoying; BUT remove this when we have an opacity slider for all tabs
        SPCSSAttr *css = sp_repr_css_attr_new();
        sp_repr_css_set_property(css, "fill-opacity", "1.0");

        Inkscape::XML::Document *xml_doc = document->getReprDoc();
        SPDefs *defs = document->getDefs();

        std::vector<SPItem*> items=selection->itemList();
        for(std::vector<SPItem*>::const_iterator i=items.begin();i!=items.end();++i){

            //FIXME: see above
            sp_repr_css_change_recursive((*i)->getRepr(), css, "style");

            // Create mesh element
            Inkscape::XML::Node *repr = xml_doc->createElement("svg:meshgradient");

            // privates are garbage-collectable
            repr->setAttribute("inkscape:collect", "always");

            // Attach to document
            defs->getRepr()->appendChild(repr);
            Inkscape::GC::release(repr);

            // Get corresponding object
            SPMeshGradient *mg = static_cast<SPMeshGradient *>(document->getObjectByRepr(repr));
            mg->array.create(mg, *i, (fill_or_stroke == Inkscape::FOR_FILL) ?
                             (*i)->geometricBounds() : (*i)->visualBounds());

            bool isText = SP_IS_TEXT(*i);
            sp_style_set_property_url (*i, ((fill_or_stroke == Inkscape::FOR_FILL) ? "fill":"stroke"),
                                   mg, isText);

            (*i)->requestModified(SP_OBJECT_MODIFIED_FLAG|SP_OBJECT_STYLE_MODIFIED_FLAG);
        }

        if (css) {
            sp_repr_css_attr_unref(css);
            css = 0;
        }

        DocumentUndo::done(desktop->getDocument(), SP_VERB_CONTEXT_MESH, _("Create mesh"));

        // status text; we do not track coords because this branch is run once, not all the time
        // during drag
        int n_objects = selection->itemList().size();
        rc.message_context->setF(Inkscape::NORMAL_MESSAGE,
                                  ngettext("<b>Gradient</b> for %d object; with <b>Ctrl</b> to snap angle",
                                           "<b>Gradient</b> for %d objects; with <b>Ctrl</b> to snap angle", n_objects),
                                  n_objects);
    } else {
        desktop->getMessageStack()->flash(Inkscape::WARNING_MESSAGE, _("Select <b>objects</b> on which to create gradient."));
    }

}
}
}
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
