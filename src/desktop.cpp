/*
 * Editable view implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   MenTaLguY <mental@rydia.net>
 *   bulia byak <buliabyak@users.sf.net>
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *   John Bintz <jcoswell@coswellproductions.org>
 *   Johan Engelen <j.b.c.engelen@ewi.utwente.nl>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2007 Jon A. Cruz
 * Copyright (C) 2006-2008 Johan Engelen
 * Copyright (C) 2006 John Bintz
 * Copyright (C) 2004 MenTaLguY
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glibmm/i18n.h>
#include <2geom/transforms.h>

#include <2geom/rect.h>

#include "desktop.h"

#include "color.h"
#include "desktop-events.h"
#include "desktop-style.h"
#include "device-manager.h"
#include "document-undo.h"
#include "event-log.h"
#include "layer-fns.h"
#include "layer-manager.h"
#include "message-context.h"
#include "message-stack.h"
#include "resource-manager.h"

#include "display/canvas-arena.h"
#include "display/canvas-debug.h"
#include "display/canvas-grid.h"
#include "display/canvas-rotate.h"
#include "display/canvas-temporary-item-list.h"
#include "display/drawing-group.h"
#include "display/gnome-canvas-acetate.h"
#include "display/snap-indicator.h"
#include "display/sodipodi-ctrlrect.h"
#include "display/sp-canvas-group.h"
#include "display/sp-canvas-util.h"

#include "helper/action-context.h"
#include "helper/action.h" //sp_action_perform

#include "object/sp-namedview.h"
#include "object/sp-root.h"

#include "ui/dialog/dialog-manager.h"
#include "ui/interface.h"
#include "ui/tool-factory.h"
#include "ui/tools/box3d-tool.h"
#include "ui/tools/select-tool.h"

#include "widgets/desktop-widget.h"

// TODO those includes are only for node tool quick zoom. Remove them after fixing it.
#include "ui/tools/node-tool.h"
#include "ui/tool/control-point-selection.h"

namespace Inkscape { namespace XML { class Node; }}

// Callback declarations
static void _onSelectionChanged (Inkscape::Selection *selection, SPDesktop *desktop);
static gint _arena_handler (SPCanvasArena *arena, Inkscape::DrawingItem *ai, GdkEvent *event, SPDesktop *desktop);
static void _layer_activated(SPObject *layer, SPDesktop *desktop);
static void _layer_deactivated(SPObject *layer, SPDesktop *desktop);
static void _layer_hierarchy_changed(SPObject *top, SPObject *bottom, SPDesktop *desktop);
static void _reconstruction_start(SPDesktop * desktop);
static void _reconstruction_finish(SPDesktop * desktop);
static void _namedview_modified (SPObject *obj, guint flags, SPDesktop *desktop);

SPDesktop::SPDesktop() :
    _dlg_mgr( NULL ),
    namedview( NULL ),
    canvas( NULL ),
    layers( NULL ),
    selection( NULL ),
    event_context( NULL ),
    layer_manager( NULL ),
    event_log( NULL ),
    temporary_item_list( NULL ),
    snapindicator( NULL ),
    acetate( NULL ),
    main( NULL ),
    gridgroup( NULL ),
    guides( NULL ),
    drawing( NULL ),
    sketch( NULL ),
    controls( NULL ),
    tempgroup ( NULL ),
    page( NULL ),
    page_border( NULL ),
    current( NULL ),
    _focusMode(false),
    dkey( 0 ),
    number( 0 ),
    window_state(0),
    interaction_disabled_counter( 0 ),
    waiting_cursor( false ),
    showing_dialogs ( false ),
    guides_active( false ),
    gr_item( NULL ),
    gr_point_type( POINT_LG_BEGIN ),
    gr_point_i( 0 ),
    gr_fill_or_stroke( Inkscape::FOR_FILL ),
    _reconstruction_old_layer_id(), // an id attribute is not allowed to be the empty string
    _display_mode(Inkscape::RENDERMODE_NORMAL),
    _display_color_mode(Inkscape::COLORMODE_NORMAL),
    _widget( NULL ),
    _guides_message_context( NULL ),
    _active( false ),
    _doc2dt( Geom::Scale(1, -1) ),
    _image_render_observer(this, "/options/rendering/imageinoutlinemode"),
    grids_visible( false )
{
    layers = new Inkscape::LayerModel();
    layers->_layer_activated_signal.connect(sigc::bind(sigc::ptr_fun(_layer_activated), this));
    layers->_layer_deactivated_signal.connect(sigc::bind(sigc::ptr_fun(_layer_deactivated), this));
    layers->_layer_changed_signal.connect(sigc::bind(sigc::ptr_fun(_layer_hierarchy_changed), this));
    selection = Inkscape::GC::release( new Inkscape::Selection(layers, this) );
}

void
SPDesktop::init (SPNamedView *nv, SPCanvas *aCanvas, Inkscape::UI::View::EditWidgetInterface *widget)
{
    _widget = widget;

    // Temporary workaround for link order issues:
    Inkscape::DeviceManager::getManager().getDevices();
    Inkscape::ResourceManager::getManager();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    _guides_message_context = new Inkscape::MessageContext(const_cast<Inkscape::MessageStack*>(messageStack()));

    current = prefs->getStyle("/desktop/style");

    namedview = nv;
    canvas = aCanvas;

    SPDocument *document = namedview->document;
    /* XXX:
     * ensureUpToDate() sends a 'modified' signal to the root element.
     * This is reportedly required to prevent flickering after the document
     * loads. However, many SPObjects write to their repr in response
     * to this signal. This is apparently done to support live path effects,
     * which rewrite their result paths after each modification of the base object.
     * This causes the generation of an incomplete undo transaction,
     * which causes problems down the line, including crashes in the
     * Undo History dialog.
     *
     * For now, this is handled by disabling undo tracking during this call.
     * A proper fix would involve modifying the way ensureUpToDate() works,
     * so that the LPE results are not rewritten.
     */
    Inkscape::DocumentUndo::setUndoSensitive(document, false);
    document->ensureUpToDate();
    Inkscape::DocumentUndo::setUndoSensitive(document, true);

    /* Setup Dialog Manager */
    _dlg_mgr = &Inkscape::UI::Dialog::DialogManager::getInstance();

    dkey = SPItem::display_key_new(1);

    /* Connect display key to layer model */
    layers->setDisplayKey(dkey);

    /* Connect document */
    setDocument (document);

    number = namedview->getViewCount();


    /* Setup Canvas */
    g_object_set_data (G_OBJECT (canvas), "SPDesktop", this);

    SPCanvasGroup *root = canvas->getRoot();

    /* Setup administrative layers */
    acetate = sp_canvas_item_new (root, GNOME_TYPE_CANVAS_ACETATE, NULL);
    g_signal_connect (G_OBJECT (acetate), "event", G_CALLBACK (sp_desktop_root_handler), this);
    main = (SPCanvasGroup *) sp_canvas_item_new (root, SP_TYPE_CANVAS_GROUP, NULL);
    g_signal_connect (G_OBJECT (main), "event", G_CALLBACK (sp_desktop_root_handler), this);

    /* This is the background the page sits on. */
    canvas->setBackgroundColor(0xffffff00);

    page = sp_canvas_item_new (main, SP_TYPE_CTRLRECT, NULL);
    ((CtrlRect *) page)->setColor(0x00000000, FALSE, 0x00000000);
    page_border = sp_canvas_item_new (main, SP_TYPE_CTRLRECT, NULL);

    drawing = sp_canvas_item_new (main, SP_TYPE_CANVAS_ARENA, NULL);
    g_signal_connect (G_OBJECT (drawing), "arena_event", G_CALLBACK (_arena_handler), this);

    SP_CANVAS_ARENA (drawing)->drawing.delta = prefs->getDouble("/options/cursortolerance/value", 1.0); // default is 1 px

    if (prefs->getBool("/options/startmode/outline")) {
        // Start in outline mode
        setDisplayModeOutline();
    } else {
        // Start in normal mode, default
        setDisplayModeNormal();
    }

    // The order in which these canvas items are added determines the z-order. It's therefore
    // important to add the tempgroup (which will contain the snapindicator) before adding the
    // controls. Only this way one will be able to quickly (before the snap indicator has
    // disappeared) reselect a node after snapping it. If the z-order is wrong however, this
    // will not work (the snap indicator is on top of the node handler; is the snapindicator
    // being selected? or does it intercept some of the events that should have gone to the
    // node handler? see bug https://bugs.launchpad.net/inkscape/+bug/414142)
    gridgroup = (SPCanvasGroup *) sp_canvas_item_new (main, SP_TYPE_CANVAS_GROUP, NULL);
    guides    = (SPCanvasGroup *) sp_canvas_item_new (main, SP_TYPE_CANVAS_GROUP, NULL);
    sketch    = (SPCanvasGroup *) sp_canvas_item_new (main, SP_TYPE_CANVAS_GROUP, NULL);
    tempgroup = (SPCanvasGroup *) sp_canvas_item_new (main, SP_TYPE_CANVAS_GROUP, NULL);
    controls  = (SPCanvasGroup *) sp_canvas_item_new (main, SP_TYPE_CANVAS_GROUP, NULL);

    // Set the select tool as the active tool.
    setEventContext("/tools/select");

    // display rect and zoom are now handled in sp_desktop_widget_realize()

    Geom::Rect const d(Geom::Point(0.0, 0.0),
                       Geom::Point(document->getWidth().value("px"), document->getHeight().value("px")));

    SP_CTRLRECT(page)->setRectangle(d);
    SP_CTRLRECT(page_border)->setRectangle(d);

    /* the following sets the page shadow on the canvas
       It was originally set to 5, which is really cheesy!
       It now is an attribute in the document's namedview. If a value of
       0 is used, then the constructor for a shadow is not initialized.
    */

    if ( namedview->pageshadow != 0 && namedview->showpageshadow ) {
        SP_CTRLRECT(page_border)->setShadow(namedview->pageshadow, 0x3f3f3fff);
    }


    /* Connect event for page resize */
    _doc2dt[5] = document->getHeight().value("px");
    sp_canvas_item_affine_absolute (SP_CANVAS_ITEM (drawing), _doc2dt);

    _modified_connection =
        namedview->connectModified(sigc::bind<2>(sigc::ptr_fun(&_namedview_modified), this));

    Inkscape::DrawingItem *ai = document->getRoot()->invoke_show(
            SP_CANVAS_ARENA (drawing)->drawing,
            dkey,
            SP_ITEM_SHOW_DISPLAY);
    if (ai) {
        SP_CANVAS_ARENA (drawing)->drawing.root()->prependChild(ai);
    }

    namedview->show(this);
    /* Ugly hack */
    activate_guides (true);
    /* Ugly hack */
    _namedview_modified (namedview, SP_OBJECT_MODIFIED_FLAG, this);

/* Set up notification of rebuilding the document, this allows
       for saving object related settings in the document. */
    _reconstruction_start_connection =
        document->connectReconstructionStart(sigc::bind(sigc::ptr_fun(_reconstruction_start), this));
    _reconstruction_finish_connection =
        document->connectReconstructionFinish(sigc::bind(sigc::ptr_fun(_reconstruction_finish), this));
    _reconstruction_old_layer_id.clear();

    // ?
    // sp_active_desktop_set (desktop);

    _activate_connection = _activate_signal.connect(
        sigc::bind(
            sigc::ptr_fun(_onActivate),
            this
        )
    );
     _deactivate_connection = _deactivate_signal.connect(
        sigc::bind(
            sigc::ptr_fun(_onDeactivate),
            this
        )
    );

    _sel_modified_connection = selection->connectModified(
        sigc::bind(
            sigc::ptr_fun(&_onSelectionModified),
            this
        )
    );
    _sel_changed_connection = selection->connectChanged(
        sigc::bind(
            sigc::ptr_fun(&_onSelectionChanged),
            this
        )
    );


    /* setup LayerManager */
    //   (Setting up after the connections are all in place, as it may use some of them)
    layer_manager = new Inkscape::LayerManager( this );

    showGrids(namedview->grids_visible, false);

    temporary_item_list = new Inkscape::Display::TemporaryItemList( this );
    snapindicator = new Inkscape::Display::SnapIndicator ( this );

    canvas_rotate = sp_canvas_item_new (root, SP_TYPE_CANVAS_ROTATE, NULL);
    sp_canvas_item_hide( canvas_rotate );
    // canvas_debug = sp_canvas_item_new (main, SP_TYPE_CANVAS_DEBUG, NULL);
}

void SPDesktop::destroy()
{
    _destroy_signal.emit(this);

    if (snapindicator) {
        delete snapindicator;
        snapindicator = NULL;
    }

    if (temporary_item_list) {
        delete temporary_item_list;
        temporary_item_list = NULL;
    }

    if (selection) {
        delete selection;
        selection = NULL;
    }

    namedview->hide(this);

    _activate_connection.disconnect();
    _deactivate_connection.disconnect();
    _sel_modified_connection.disconnect();
    _sel_changed_connection.disconnect();
    _modified_connection.disconnect();
    _commit_connection.disconnect();
    _reconstruction_start_connection.disconnect();
    _reconstruction_finish_connection.disconnect();

    g_signal_handlers_disconnect_by_func(G_OBJECT (acetate), (gpointer) G_CALLBACK(sp_desktop_root_handler), this);
    g_signal_handlers_disconnect_by_func(G_OBJECT (main), (gpointer) G_CALLBACK(sp_desktop_root_handler), this);
    g_signal_handlers_disconnect_by_func(G_OBJECT (drawing), (gpointer) G_CALLBACK(_arena_handler), this);

    delete layers;

    if (layer_manager) {
        delete layer_manager;
        layer_manager = NULL;
    }

    if (drawing) {
        doc()->getRoot()->invoke_hide(dkey);
        g_object_unref(drawing);
        drawing = NULL;
    }

    delete _guides_message_context;
    _guides_message_context = NULL;
}

SPDesktop::~SPDesktop()
{
}


Inkscape::UI::Tools::ToolBase* SPDesktop::getEventContext() const {
	return event_context;
}

Inkscape::Selection* SPDesktop::getSelection() const {
	return selection;
}

SPDocument* SPDesktop::getDocument() const {
	return doc();
}

SPCanvas* SPDesktop::getCanvas() const {
	return SP_CANVAS_ITEM(main)->canvas;
}

SPCanvasItem* SPDesktop::getAcetate() const {
	return acetate;
}

SPCanvasGroup* SPDesktop::getMain() const {
	return main;
}

SPCanvasGroup* SPDesktop::getGridGroup() const {
	return gridgroup;
}

SPCanvasGroup* SPDesktop::getGuides() const {
	return guides;
}

SPCanvasItem* SPDesktop::getDrawing() const {
	return drawing;
}

SPCanvasGroup* SPDesktop::getSketch() const {
	return sketch;
}

SPCanvasGroup* SPDesktop::getControls() const {
	return controls;
}

SPCanvasGroup* SPDesktop::getTempGroup() const {
	return tempgroup;
}

Inkscape::MessageStack* SPDesktop::getMessageStack() const {
	return messageStack();
}

SPNamedView* SPDesktop::getNamedView() const {
	return namedview;
}



//--------------------------------------------------------------------
/* Public methods */


/* These methods help for temporarily showing things on-canvas.
 * The *only* valid use of the TemporaryItem* that you get from add_temporary_canvasitem
 * is when you want to prematurely remove the item from the canvas, by calling
 * desktop->remove_temporary_canvasitem(tempitem).
 */
/** Note that lifetime is measured in milliseconds
 * One should *not* keep a reference to the SPCanvasItem, the temporary item code will
 * delete the object for you and the reference will become invalid without you knowing it.
 * It is perfectly safe to ignore the returned pointer: the object is deleted by itself, so don't delete it elsewhere!
 * The *only* valid use of the returned TemporaryItem* is as argument for SPDesktop::remove_temporary_canvasitem,
 * because the object might be deleted already without you knowing it.
 * move_to_bottom = true by default so the item does not interfere with handling of other items on the canvas like nodes.
 */
Inkscape::Display::TemporaryItem *
SPDesktop::add_temporary_canvasitem (SPCanvasItem *item, guint lifetime, bool move_to_bottom)
{
    if (move_to_bottom) {
        sp_canvas_item_move_to_z(item, 0);
    }

    return temporary_item_list->add_item(item, lifetime);
}

/** It is perfectly safe to call this function while the object has already been deleted due to a timeout.
*/
void
SPDesktop::remove_temporary_canvasitem (Inkscape::Display::TemporaryItem * tempitem)
{
    // check for non-null temporary_item_list, because during destruction of desktop, some destructor might try to access this list!
    if (tempitem && temporary_item_list) {
        temporary_item_list->delete_item(tempitem);
    }
}

void SPDesktop::redrawDesktop() {
    sp_canvas_item_affine_absolute (SP_CANVAS_ITEM (main), _current_affine.d2w()); // redraw
}

void SPDesktop::_setDisplayMode(Inkscape::RenderMode mode) {
    SP_CANVAS_ARENA (drawing)->drawing.setRenderMode(mode);
    canvas->_rendermode = mode;
    _display_mode = mode;
    redrawDesktop();
    _widget->setTitle( this->getDocument()->getName() );
}
void SPDesktop::_setDisplayColorMode(Inkscape::ColorMode mode) {
    // reload grayscale matrix from prefs
    if (mode == Inkscape::COLORMODE_GRAYSCALE) {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        gdouble r = prefs->getDoubleLimited("/options/rendering/grayscale/red-factor",0.21,0.,1.);
        gdouble g = prefs->getDoubleLimited("/options/rendering/grayscale/green-factor",0.72,0.,1.);
        gdouble b = prefs->getDoubleLimited("/options/rendering/grayscale/blue-factor",0.072,0.,1.);
        gdouble grayscale_value_matrix[20] = { r, g, b, 0, 0,
                                               r, g, b, 0, 0,
                                               r, g, b, 0, 0,
                                               0, 0, 0, 1, 0 };
        g_message("%g",grayscale_value_matrix[0]);
        SP_CANVAS_ARENA (drawing)->drawing.setGrayscaleMatrix(grayscale_value_matrix);
    }

    SP_CANVAS_ARENA (drawing)->drawing.setColorMode(mode);
    canvas->_colorrendermode = mode;
    _display_color_mode = mode;
    redrawDesktop();
    _widget->setTitle( this->getDocument()->getName() );
}

void SPDesktop::displayModeToggle() {
    switch (_display_mode) {
    case Inkscape::RENDERMODE_NORMAL:
        _setDisplayMode(Inkscape::RENDERMODE_NO_FILTERS);
        break;
    case Inkscape::RENDERMODE_NO_FILTERS:
        _setDisplayMode(Inkscape::RENDERMODE_OUTLINE);
        break;
    case Inkscape::RENDERMODE_OUTLINE:
        _setDisplayMode(Inkscape::RENDERMODE_NORMAL);
        break;
    default:
        _setDisplayMode(Inkscape::RENDERMODE_NORMAL);
    }
}
void SPDesktop::displayColorModeToggle() {
    switch (_display_color_mode) {
    case Inkscape::COLORMODE_NORMAL:
        _setDisplayColorMode(Inkscape::COLORMODE_GRAYSCALE);
        break;
    case Inkscape::COLORMODE_GRAYSCALE:
        _setDisplayColorMode(Inkscape::COLORMODE_NORMAL);
        break;
//    case Inkscape::COLORMODE_PRINT_COLORS_PREVIEW:
    default:
        _setDisplayColorMode(Inkscape::COLORMODE_NORMAL);
    }
}

// Pass-through LayerModel functions
SPObject *SPDesktop::currentRoot() const
{
    return layers->currentRoot();
}

SPObject *SPDesktop::currentLayer() const
{
    return layers->currentLayer();
}

void SPDesktop::setCurrentLayer(SPObject *object)
{
    layers->setCurrentLayer(object);
}

void SPDesktop::toggleLayerSolo(SPObject *object)
{
    layers->toggleLayerSolo(object);
}

void SPDesktop::toggleHideAllLayers(bool hide)
{
    layers->toggleHideAllLayers(hide);
}

void SPDesktop::toggleLockAllLayers(bool lock)
{
    layers->toggleLockAllLayers(lock);
}

void SPDesktop::toggleLockOtherLayers(SPObject *object)
{
    layers->toggleLockOtherLayers(object);
}

bool SPDesktop::isLayer(SPObject *object) const
{
    return layers->isLayer(object);
}

/**
 * True if desktop viewport intersects \a item's bbox.
 */
bool SPDesktop::isWithinViewport (SPItem *item) const
{
    Geom::Rect const viewport = get_display_area();
    Geom::OptRect const bbox = item->desktopVisualBounds();
    if (bbox) {
        return viewport.intersects(*bbox);
    } else {
        return false;
    }
}

///
bool SPDesktop::itemIsHidden(SPItem const *item) const {
    return item->isHidden(this->dkey);
}

/**
 * Set activate property of desktop; emit signal if changed.
 */
void
SPDesktop::set_active (bool new_active)
{
    if (new_active != _active) {
        _active = new_active;
        if (new_active) {
            _activate_signal.emit();
        } else {
            _deactivate_signal.emit();
        }
    }
}

/**
 * Set activate status of current desktop's named view.
 */
void
SPDesktop::activate_guides(bool activate)
{
    guides_active = activate;
    namedview->activateGuides(this, activate);
}

/**
 * Make desktop switch documents.
 */
void
SPDesktop::change_document (SPDocument *theDocument)
{
    g_return_if_fail (theDocument != NULL);

    /* unselect everything before switching documents */
    selection->clear();

    setDocument (theDocument);

    /* update the rulers, connect the desktop widget's signal to the new namedview etc.
       (this can probably be done in a better way) */
    Gtk::Window *parent = this->getToplevel();
    g_assert(parent != NULL);
    SPDesktopWidget *dtw = (SPDesktopWidget *) parent->get_data("desktopwidget");
    if (dtw) {
        dtw->desktop = this;
        dtw->updateNamedview();
    }

    _namedview_modified (namedview, SP_OBJECT_MODIFIED_FLAG, this);
    _document_replaced_signal.emit (this, theDocument);
}

/**
 * Replaces the currently active tool with a new one. Pass the empty string to
 * unset and free the current tool.
 */
void SPDesktop::setEventContext(const std::string& toolName)
{
    if (event_context) {
        if (toolName.compare(event_context->pref_observer->observed_path) != 0) {
            event_context->finish();
            delete event_context;
        } else {
            _event_context_changed_signal.emit(this, event_context);
            return;
        }
    }

    if (toolName.empty()) {
        event_context = nullptr;
    } else {
        event_context = ToolFactory::createObject(toolName);
        event_context->desktop = this;
        event_context->message_context = new Inkscape::MessageContext(this->messageStack());
        event_context->setup();

        // Make sure no delayed snapping events are carried over after switching tools
        // (this is only an additional safety measure against sloppy coding, because each
        // tool should take care of this by itself)
        sp_event_context_discard_delayed_snap_event(event_context);
    }

    _event_context_changed_signal.emit(this, event_context);
}

/**
 * Sets the coordinate status to a given point
 */
void
SPDesktop::set_coordinate_status (Geom::Point p) {
    _widget->setCoordinateStatus(p);
}

Inkscape::UI::Widget::Dock* SPDesktop::getDock() {
	return _widget->getDock();
}

/**
 * \see SPDocument::getItemFromListAtPointBottom()
 */
SPItem *SPDesktop::getItemFromListAtPointBottom(const std::vector<SPItem*> &list, Geom::Point const &p) const
{
    g_return_val_if_fail (doc() != NULL, NULL);
    return SPDocument::getItemFromListAtPointBottom(dkey, doc()->getRoot(), list, p);
}

/**
 * \see SPDocument::getItemAtPoint()
 */
SPItem *SPDesktop::getItemAtPoint(Geom::Point const &p, bool into_groups, SPItem *upto) const
{
    g_return_val_if_fail (doc() != NULL, NULL);
    return doc()->getItemAtPoint( dkey, p, into_groups, upto);
}

/**
 * \see SPDocument::getGroupAtPoint()
 */
SPItem *SPDesktop::getGroupAtPoint(Geom::Point const &p) const
{
    g_return_val_if_fail (doc() != NULL, NULL);
    return doc()->getGroupAtPoint(dkey, p);
}

/**
 * Returns the mouse point in document coordinates; if mouse is
 * outside the canvas, returns the center of canvas viewpoint.
 */
Geom::Point SPDesktop::point() const
{
    Geom::Point p = _widget->getPointer();
    Geom::Point pw = sp_canvas_window_to_world (canvas, p);
    Geom::Rect const r = canvas->getViewbox();

    if (r.interiorContains(pw)) {
        p = w2d(pw);
        return p;
    }

    // Shouldn't happen
    std::cerr << "SPDesktop::point(): point outside of canvas!" << std::endl;
    Geom::Point r0 = w2d(r.min());
    Geom::Point r1 = w2d(r.max());
    return (r0 + r1) / 2.0;
}


/**
 * Revert back to previous transform if possible. Note: current transform is
 * always at front of stack.
 */
void
SPDesktop::prev_transform()
{
    if (transforms_past.empty()) {
        std::cerr << "SPDesktop::prev_transform: current transform missing!" << std::endl;
        return;
    }

    if (transforms_past.size() == 1) {
        messageStack()->flash(Inkscape::WARNING_MESSAGE, _("No previous transform."));
        return;
    }

    // Push current transform into future transforms list.
    transforms_future.push_front( _current_affine );

    // Remove the current transform from the past transforms list.
    transforms_past.pop_front();

    // restore previous transform
    _current_affine = transforms_past.front();
    set_display_area (false);

}


/**
 * Set transform to next in list.
 */
void SPDesktop::next_transform()
{
    if (transforms_future.empty()) {
        this->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("No next transform."));
        return;
    }

    // restore next transform
    _current_affine = transforms_future.front();
    set_display_area (false);

    // remove the just-used transform from the future transforms list
    transforms_future.pop_front();

    // push current transform into past transforms list
    transforms_past.push_front( _current_affine );
}


/**
 * Clear transform lists.
 */
void
SPDesktop::clear_transform_history()
{
    transforms_past.clear();
    transforms_future.clear();
}


/**
 * Does all the dirty work in setting the display area.
 * _current_affine must already be full updated (including offset).
 * log: if true, save transform in transform stack for reuse.
 */
void
SPDesktop::set_display_area (bool log)
{
    // Save the transform
    if (log) {
        transforms_past.push_front( _current_affine );
        // if we do a logged transform, our transform-forward list is invalidated, so delete it
        transforms_future.clear();
    }

    redrawDesktop();

    // Scroll
    Geom::Point offset = _current_affine.getOffset();
    canvas->scrollTo(offset, true);
    // To do: if transform unchanged call with 'false' (redraw only newly exposed areas).

    /* Update perspective lines if we are in the 3D box tool (so that infinite ones are shown
     * correctly) */
    if (SP_IS_BOX3D_CONTEXT(event_context)) {
    	SP_BOX3D_CONTEXT(event_context)->_vpdrag->updateLines();
    }

    _widget->updateRulers();
    _widget->updateScrollbars(_current_affine.getZoom());
    _widget->updateZoom();
    _widget->updateRotation();

    signal_zoom_changed.emit(_current_affine.getZoom());
}


/**
 * Map the drawing to the window so that 'c' lies at 'w' where where 'c'
 * is a point on the canvas and 'w' is position in window in screen pixels.
 */
void
SPDesktop::set_display_area (Geom::Point const &c, Geom::Point const &w, bool log)
{
    // The relative offset needed to keep c at w.
    Geom::Point offset = d2w(c) - w;
    _current_affine.addOffset( offset );
    set_display_area( log );
}


/**
 * Map the center of rectangle 'r' (which specifies a non-rotated region of the
 * drawing) to lie at the center of the window. The zoom factor is calculated such that
 * the edges of 'r' closest to 'w' are 'border' length inside of the window (if
 * there is no rotation). 'r' is in document pixel units, 'border' is in screen pixels.
 */
void
SPDesktop::set_display_area( Geom::Rect const &r, double border, bool log)
{
    // Create a rectangle the size of the window aligned with origin.
    Geom::Rect w( Geom::Point(), canvas->getViewbox().dimensions() ); // Not the SVG 'viewBox'.

    // Shrink window to account for border padding.
    w.expandBy( -border );

    double zoom = 1.0;
    // Determine which direction limits scale:
    //   if (r.width/w.width > r.height/w.height) then zoom using width.
    //   Avoiding division in test:
    if ( r.width()*w.height() > r.height()*w.width() ) {
        zoom = w.width() / r.width();
    } else {
        zoom = w.height() / r.height();
    }
    _current_affine.setScale( zoom );

    // Zero offset, actual offset calculated later.
    _current_affine.setOffset( Geom::Point( 0, 0 ) );

    set_display_area( r.midpoint(), w.midpoint(), log );
}


/**
 * Return viewbox dimensions. FixMe: Doesn't handle rotation. FixMe InvertedY
 */
Geom::Rect SPDesktop::get_display_area() const
{
    Geom::Rect const viewbox = canvas->getViewbox();
    double const scale = _current_affine.getZoom();

    /// @fixme hardcoded desktop transform
    return Geom::Rect(Geom::Point(viewbox.min()[Geom::X] / scale, viewbox.max()[Geom::Y] / -scale),
                      Geom::Point(viewbox.max()[Geom::X] / scale, viewbox.min()[Geom::Y] / -scale));
}


/**
 * Zoom keeping the point 'c' fixed in the desktop window.
 */
void
SPDesktop::zoom_absolute_keep_point (Geom::Point const &c, double zoom)
{
    zoom = CLAMP (zoom, SP_DESKTOP_ZOOM_MIN, SP_DESKTOP_ZOOM_MAX);    
    Geom::Point w = d2w( c ); // Must be before zoom changed.
    _current_affine.setScale( zoom );
    set_display_area( c, w );
}


void 
SPDesktop::zoom_relative_keep_point (Geom::Point const &c, double zoom)
{
    double new_zoom = _current_affine.getZoom() * zoom;
    zoom_absolute_keep_point( c, new_zoom );
}


/**
 * Zoom aligning the point 'c' to the center of desktop window.
 */
void 
SPDesktop::zoom_absolute_center_point (Geom::Point const &c, double zoom)
{
    zoom = CLAMP (zoom, SP_DESKTOP_ZOOM_MIN, SP_DESKTOP_ZOOM_MAX);
    _current_affine.setScale( zoom );
    Geom::Rect viewbox = canvas->getViewbox();
    set_display_area( c, viewbox.midpoint() );
}


void 
SPDesktop::zoom_relative_center_point (Geom::Point const &c, double zoom)
{
    double new_zoom = _current_affine.getZoom() * zoom;
    zoom_absolute_center_point( c, new_zoom );
}


/**
 * Set display area to origin and current document dimensions.
 */
void
SPDesktop::zoom_page()
{
    Geom::Rect d(Geom::Point(0, 0),
                 Geom::Point(doc()->getWidth().value("px"), doc()->getHeight().value("px")));

    if (d.minExtent() < 1.0) {
        return;
    }

    set_display_area(d, 10);
}

/**
 * Set display area to current document width.
 */
void
SPDesktop::zoom_page_width()
{
    Geom::Rect const a = get_display_area();

    if (doc()->getWidth().value("px") < 1.0) {
        return;
    }

    Geom::Rect d(Geom::Point(0, a.midpoint()[Geom::Y]),
                 Geom::Point(doc()->getWidth().value("px"), a.midpoint()[Geom::Y]));

    set_display_area(d, 10);
}


/**
 * Zoom to whole drawing.
 */
void
SPDesktop::zoom_drawing()
{
    g_return_if_fail (doc() != NULL);
    SPItem *docitem = doc()->getRoot();
    g_return_if_fail (docitem != NULL);

    docitem->bbox_valid = FALSE;
    Geom::OptRect d = docitem->desktopVisualBounds();

    /* Note that the second condition here indicates that
    ** there are no items in the drawing.
    */
    if ( !d || d->minExtent() < 0.1 ) {
        return;
    }

    set_display_area(*d, 10);
}


/**
 * Zoom to selection.
 */
void
SPDesktop::zoom_selection()
{
    Geom::OptRect const d = selection->visualBounds();

    if ( !d || d->minExtent() < 0.1 ) {
        return;
    }

    set_display_area(*d, 10);
}


/**
 * Performs a quick zoom into what the user is working on.
 *
 * @param  enable  Whether we're going in or out of quick zoom.
 */
void SPDesktop::zoom_quick(bool enable)
{
    if (enable == _quick_zoom_enabled) {
        return;
    }

    if (enable) {
        _quick_zoom_affine = _current_affine;
        bool zoomed = false;

        // TODO This needs to migrate into the node tool, but currently the design
        // of this method is sufficiently wrong to prevent this.
        if (!zoomed && INK_IS_NODE_TOOL(event_context)) {
            Inkscape::UI::Tools::NodeTool *nt = static_cast<Inkscape::UI::Tools::NodeTool*>(event_context);
            if (!nt->_selected_nodes->empty()) {
                Geom::Rect nodes = *nt->_selected_nodes->bounds();
                double area = nodes.area();
                // do not zoom if a single cusp node is selected aand the bounds
                // have zero area.
                if (!Geom::are_near(area, 0)) {
                    set_display_area(nodes, true);
                    zoomed = true;
                }
            }
        }

        if (!zoomed) {
            Geom::OptRect const d = selection->visualBounds();
            if (d) {
                set_display_area(*d, true);
                zoomed = true;
            }
        }

        if (!zoomed) {
            Geom::Rect const d_canvas = canvas->getViewbox(); // Not SVG 'viewBox'
            Geom::Point midpoint = w2d(d_canvas.midpoint()); // Midpoint of drawing on canvas.
            zoom_relative_center_point(midpoint, 2.0);
        }
    } else {
        _current_affine = _quick_zoom_affine;
        set_display_area( false );
    }

    _quick_zoom_enabled = enable;
    return;
}


/**
 * Tell widget to let zoom widget grab keyboard focus.
 */
void
SPDesktop::zoom_grab_focus()
{
    _widget->letZoomGrabFocus();
}


/**
 * Rotate keeping the point 'c' fixed in the desktop window.
 */
void
SPDesktop::rotate_absolute_keep_point (Geom::Point const &c, double rotate)
{
    Geom::Point w = d2w( c ); // Must be before rotate changed.
    _current_affine.setRotate( rotate );
    set_display_area( c, w );
}


void 
SPDesktop::rotate_relative_keep_point (Geom::Point const &c, double rotate)
{
    Geom::Point w = d2w( c ); // Must be before rotate changed.
    _current_affine.addRotate( rotate );
    set_display_area( c, w );
}


/**
 * Rotate aligning the point 'c' to the center of desktop window.
 */
void 
SPDesktop::rotate_absolute_center_point (Geom::Point const &c, double rotate)
{
    _current_affine.setRotate( rotate );
    Geom::Rect viewbox = canvas->getViewbox();
    set_display_area( c, viewbox.midpoint() );
}


void 
SPDesktop::rotate_relative_center_point (Geom::Point const &c, double rotate)
{
    _current_affine.addRotate( rotate );
    Geom::Rect viewbox = canvas->getViewbox();
    set_display_area( c, viewbox.midpoint() );
}


/**
 * Flip keeping the point 'c' fixed in the desktop window.
 */
void
SPDesktop::flip_absolute_keep_point (Geom::Point const &c, CanvasFlip flip)
{
    Geom::Point w = d2w( c ); // Must be before flip.
    _current_affine.setFlip( flip );
    set_display_area( c, w );
}


void
SPDesktop::flip_relative_keep_point (Geom::Point const &c, CanvasFlip flip)
{
    Geom::Point w = d2w( c ); // Must be before flip.
    _current_affine.addFlip( flip );
    set_display_area( c, w );
}


/**
 * Flip aligning the point 'c' to the center of desktop window.
 */
void 
SPDesktop::flip_absolute_center_point (Geom::Point const &c, CanvasFlip flip)
{
    _current_affine.setFlip( flip );
    Geom::Rect viewbox = canvas->getViewbox();
    set_display_area( c, viewbox.midpoint() );
}


void 
SPDesktop::flip_relative_center_point (Geom::Point const &c, CanvasFlip flip)
{
    _current_affine.addFlip( flip );
    Geom::Rect viewbox = canvas->getViewbox();
    set_display_area( c, viewbox.midpoint() );
}


/**
 * Scroll canvas by to a particular point (window coordinates).
 */
void
SPDesktop::scroll_absolute (Geom::Point const &point, bool is_scrolling)
{
    canvas->scrollTo(point, FALSE, is_scrolling);
    _current_affine.setOffset( point );

    /*  update perspective lines if we are in the 3D box tool (so that infinite ones are shown correctly) */
    //sp_box3d_context_update_lines(event_context);
    if (SP_IS_BOX3D_CONTEXT(event_context)) {
		SP_BOX3D_CONTEXT(event_context)->_vpdrag->updateLines();
	}

    _widget->updateRulers();
    _widget->updateScrollbars(_current_affine.getZoom());
}


/**
 * Scroll canvas by specific coordinate amount (window coordinates).
 */
void
SPDesktop::scroll_relative (Geom::Point const &delta, bool is_scrolling)
{
    Geom::Rect const viewbox = canvas->getViewbox();
    scroll_absolute( viewbox.min() - delta, is_scrolling );
}


/**
 * Scroll canvas by specific coordinate amount in svg coordinates.
 */
void
SPDesktop::scroll_relative_in_svg_coords (double dx, double dy, bool is_scrolling)
{
    double scale = _current_affine.getZoom();
    scroll_relative(Geom::Point(dx*scale, dy*scale), is_scrolling);
}


/**
 * Scroll screen so as to keep point 'p' visible in window.
 * (Used, for example, when a node is being dragged.)
 * 'p': The point in desktop coordinates.
 * 'autoscrollspeed': The scroll speed (or zero to use preferences' value).
 */
bool
SPDesktop::scroll_to_point (Geom::Point const &p, gdouble autoscrollspeed)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    // autoscrolldistance is in screen pixels.
    gdouble autoscrolldistance = (gdouble) prefs->getIntLimited("/options/autoscrolldistance/value", 0, -1000, 10000);

    Geom::Rect w = canvas->getViewbox(); // Window in screen coordinates.
    w.expandBy(-autoscrolldistance);  // Shrink window

    Geom::Point c = d2w(p);  // Point 'p' in screen coordinates.
    if (!w.contains(c)) {

        Geom::Point c2 = w.clamp(c); // Constrain c to window.

        if (autoscrollspeed == 0)
            autoscrollspeed = prefs->getDoubleLimited("/options/autoscrollspeed/value", 1, 0, 10);

        if (autoscrollspeed != 0)
            scroll_relative (autoscrollspeed * (c2 - c) );

        return true;
    }
    return false;
}

bool
SPDesktop::is_iconified()
{
    return 0!=(window_state & GDK_WINDOW_STATE_ICONIFIED);
}

void
SPDesktop::iconify()
{
    _widget->setIconified();
}

bool
SPDesktop::is_maximized()
{
    return 0!=(window_state & GDK_WINDOW_STATE_MAXIMIZED);
}

void
SPDesktop::maximize()
{
    _widget->setMaximized();
}

bool
SPDesktop::is_fullscreen()
{
    return 0!=(window_state & GDK_WINDOW_STATE_FULLSCREEN);
}

void
SPDesktop::fullscreen()
{
    _widget->setFullscreen();
}

/**
 * Checks to see if the user is working in focused mode.
 *
 * @return  the value of \c _focusMode.
 */
bool SPDesktop::is_focusMode()
{
    return _focusMode;
}

/**
 * Changes whether the user is in focus mode or not.
 *
 * @param  mode  Which mode the view should be in.
 */
void SPDesktop::focusMode(bool mode)
{
    if (mode == _focusMode) { return; }

    _focusMode = mode;

    layoutWidget();
    //sp_desktop_widget_layout(SPDesktopWidget);

    return;
}

void
SPDesktop::getWindowGeometry (gint &x, gint &y, gint &w, gint &h)
{
    _widget->getGeometry (x, y, w, h);
}

void
SPDesktop::setWindowPosition (Geom::Point p)
{
    _widget->setPosition (p);
}

void
SPDesktop::setWindowSize (gint w, gint h)
{
    _widget->setSize (w, h);
}

void
SPDesktop::setWindowTransient (void *p, int transient_policy)
{
    _widget->setTransient (p, transient_policy);
}

Gtk::Window*
SPDesktop::getToplevel( )
{
    return _widget->getWindow();
}

void
SPDesktop::presentWindow()
{
    _widget->present();
}

bool SPDesktop::showInfoDialog( Glib::ustring const & message )
{
    return _widget->showInfoDialog( message );
}

bool
SPDesktop::warnDialog (Glib::ustring const &text)
{
    return _widget->warnDialog (text);
}

void
SPDesktop::toggleRulers()
{
    _widget->toggleRulers();
}

void
SPDesktop::toggleScrollbars()
{
    _widget->toggleScrollbars();
}


void SPDesktop::toggleToolbar(gchar const *toolbar_name)
{
    Glib::ustring pref_path = getLayoutPrefPath(this) + toolbar_name + "/state";
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    gboolean visible = prefs->getBool(pref_path, true);
    prefs->setBool(pref_path, !visible);

    layoutWidget();
}

void
SPDesktop::layoutWidget()
{
    _widget->layout();
}

void
SPDesktop::destroyWidget()
{
    _widget->destroy();
}

bool
SPDesktop::shutdown()
{
    return _widget->shutdown();
}

bool SPDesktop::onDeleteUI (GdkEventAny*)
{
    if(shutdown())
        return true;

    destroyWidget();
    return false;
}

/**
 *  onWindowStateEvent
 *
 *  Called when the window changes its maximize/fullscreen/iconify/pinned state.
 *  Since GTK doesn't have a way to query this state information directly, we
 *  record it for the desktop here, and also possibly trigger a layout.
 */
bool
SPDesktop::onWindowStateEvent (GdkEventWindowState* event)
{
    // Record the desktop window's state
    window_state = event->new_window_state;

    // Layout may differ depending on full-screen mode or not
    GdkWindowState changed = event->changed_mask;
    if (changed & (GDK_WINDOW_STATE_FULLSCREEN|GDK_WINDOW_STATE_MAXIMIZED)) {
        layoutWidget();
    }

    return false;
}


/**
  * Apply the desktop's current style or the tool style to the object.
  */
void SPDesktop::applyCurrentOrToolStyle(SPObject *obj, Glib::ustring const &tool_path, bool with_text)
{
    SPCSSAttr *css_current = sp_desktop_get_style(this, with_text);
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    if (prefs->getBool(tool_path + "/usecurrent") && css_current) {
        obj->setCSS(css_current,"style");
    } else {
        SPCSSAttr *css = prefs->getInheritedStyle(tool_path + "/style");
        obj->setCSS(css,"style");
        sp_repr_css_attr_unref(css);
    }
    if (css_current) {
        sp_repr_css_attr_unref(css_current);
    }
}


void
SPDesktop::setToolboxFocusTo (gchar const *label)
{
    _widget->setToolboxFocusTo (label);
}

void
SPDesktop::setToolboxAdjustmentValue (gchar const* id, double val)
{
    _widget->setToolboxAdjustmentValue (id, val);
}

void
SPDesktop::setToolboxSelectOneValue (gchar const* id, gint val)
{
    _widget->setToolboxSelectOneValue (id, val);
}

bool
SPDesktop::isToolboxButtonActive (gchar const *id)
{
    return _widget->isToolboxButtonActive (id);
}

void
SPDesktop::emitToolSubselectionChanged(gpointer data)
{
    _tool_subselection_changed.emit(data);
    INKSCAPE.subselection_changed (this);
}

void SPDesktop::updateNow()
{
    canvas->updateNow();
}

void
SPDesktop::enableInteraction()
{
  _widget->enableInteraction();
}

void SPDesktop::disableInteraction()
{
  _widget->disableInteraction();
}

void SPDesktop::setWaitingCursor()
{
    GdkDisplay *display = gdk_display_get_default();
    GdkCursor  *waiting = gdk_cursor_new_for_display(display, GDK_WATCH);
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(getCanvas())), waiting);
    gdk_cursor_unref(waiting);
    // GDK needs the flush for the cursor change to take effect
    gdk_flush();
    waiting_cursor = true;
}

void SPDesktop::clearWaitingCursor() {
  if (waiting_cursor) {
      this->event_context->sp_event_context_update_cursor();
  }
}

void SPDesktop::toggleColorProfAdjust()
{
    _widget->toggleColorProfAdjust();
}

void SPDesktop::toggleGuidesLock()
{
    _widget->toggleGuidesLock();
}

bool SPDesktop::colorProfAdjustEnabled()
{
    return _widget->colorProfAdjustEnabled();
}

void SPDesktop::toggleGrids()
{
    if (! namedview->grids.empty()) {
        if(gridgroup) {
            showGrids(!grids_visible);
        }
    } else {
        //there is no grid present at the moment. add a rectangular grid and make it visible
        namedview->writeNewGrid(this->getDocument(), Inkscape::GRID_RECTANGULAR);
        showGrids(true);
    }
}

void SPDesktop::showGrids(bool show, bool dirty_document)
{
    grids_visible = show;
    sp_namedview_show_grids(namedview, grids_visible, dirty_document);
    if (show) {
        sp_canvas_item_show(SP_CANVAS_ITEM(gridgroup));
    } else {
        sp_canvas_item_hide(SP_CANVAS_ITEM(gridgroup));
    }
}

void SPDesktop::toggleSnapGlobal()
{
    bool v = namedview->getSnapGlobal();
    namedview->setSnapGlobal(!v);
}

//----------------------------------------------------------------------
// Callback implementations. The virtual ones are connected by the view.

void
SPDesktop::onResized (double /*x*/, double /*y*/)
{
   // Nothing called here
}

/**
 * Redraw callback; queues Gtk redraw; connected by View.
 */
void
SPDesktop::onRedrawRequested ()
{
    if (main) {
        _widget->requestCanvasUpdate();
    }
}

void
SPDesktop::updateCanvasNow()
{
  _widget->requestCanvasUpdateAndWait();
}

/**
 * Associate document with desktop.
 */
void
SPDesktop::setDocument (SPDocument *doc)
{
    if (!doc) return;

    if (this->doc()) {
        namedview->hide(this);
        this->doc()->getRoot()->invoke_hide(dkey);
    }

    layers->setDocument(doc);
    selection->setDocument(doc);

    if (event_log) {
        // Remove it from the replaced document. This prevents Inkscape from
        // crashing since we access it in the replaced document's destructor
        // which results in an undefined behavior. (See also: bug #1670688)
        if (this->doc()) {
            this->doc()->removeUndoObserver(*event_log);
        }
        delete event_log;
        event_log = 0;
    }

    /* setup EventLog */
    event_log = new Inkscape::EventLog(doc);
    doc->addUndoObserver(*event_log);

    _commit_connection.disconnect();
    _commit_connection = doc->connectCommit(sigc::mem_fun(*this, &SPDesktop::updateNow));

    /// \todo fixme: This condition exists to make sure the code
    /// inside is NOT called on initialization, only on replacement. But there
    /// are surely more safe methods to accomplish this.
    // TODO since the comment had reversed logic, check the intent of this block of code:
    if (drawing) {
        Inkscape::DrawingItem *ai = 0;

        namedview = sp_document_namedview (doc, NULL);
        _modified_connection = namedview->connectModified(sigc::bind<2>(sigc::ptr_fun(&_namedview_modified), this));
        number = namedview->getViewCount();

        ai = doc->getRoot()->invoke_show(
                SP_CANVAS_ARENA (drawing)->drawing,
                dkey,
                SP_ITEM_SHOW_DISPLAY);
        if (ai) {
            SP_CANVAS_ARENA (drawing)->drawing.root()->prependChild(ai);
        }
        namedview->show(this);
        /* Ugly hack */
        activate_guides (true);
        /* Ugly hack */
        _namedview_modified (namedview, SP_OBJECT_MODIFIED_FLAG, this);
    }

    _document_replaced_signal.emit (this, doc);

    View::setDocument (doc);
}

void
SPDesktop::onStatusMessage
(Inkscape::MessageType type, gchar const *message)
{
    if (_widget) {
        _widget->setMessage(type, message);
    }
}

void
SPDesktop::onDocumentURISet (gchar const* uri)
{
    _widget->setTitle(uri);
}

/**
 * Resized callback.
 */
void
SPDesktop::onDocumentResized (gdouble width, gdouble height)
{
    _doc2dt[5] = height;
    sp_canvas_item_affine_absolute (SP_CANVAS_ITEM (drawing), _doc2dt);
    Geom::Rect const a(Geom::Point(0, 0), Geom::Point(width, height));
    SP_CTRLRECT(page)->setRectangle(a);
    SP_CTRLRECT(page_border)->setRectangle(a);
}


void
SPDesktop::_onActivate (SPDesktop* dt)
{
    if (!dt->_widget) return;
    dt->_widget->activateDesktop();
}

void
SPDesktop::_onDeactivate (SPDesktop* dt)
{
    if (!dt->_widget) return;
    dt->_widget->deactivateDesktop();
}

void
SPDesktop::_onSelectionModified
(Inkscape::Selection */*selection*/, guint /*flags*/, SPDesktop *dt)
{
    if (!dt->_widget) return;
    dt->_widget->updateScrollbars (dt->_current_affine.getZoom());
}

static void
_onSelectionChanged
(Inkscape::Selection *selection, SPDesktop *desktop)
{
    /** \todo
     * only change the layer for single selections, or what?
     * This seems reasonable -- for multiple selections there can be many
     * different layers involved.
     */
    SPItem *item=selection->singleItem();
    if (item) {
        SPObject *layer=desktop->layers->layerForObject(item);
        if ( layer && layer != desktop->currentLayer() ) {
            desktop->layers->setCurrentLayer(layer);
        }
    }
}

/**
 * Calls event handler of current event context.
 * \param arena Unused
 * \todo fixme
 */
static gint
_arena_handler (SPCanvasArena */*arena*/, Inkscape::DrawingItem *ai, GdkEvent *event, SPDesktop *desktop)
{
    if (ai) {
        SPItem *spi = (SPItem*) ai->data();
        return sp_event_context_item_handler (desktop->event_context, spi, event);
    } else {
        return sp_event_context_root_handler (desktop->event_context, event);
    }
}

static void
_layer_activated(SPObject *layer, SPDesktop *desktop) {
    g_return_if_fail(SP_IS_GROUP(layer));
    SP_GROUP(layer)->setLayerDisplayMode(desktop->dkey, SPGroup::LAYER);
}

/// Callback
static void
_layer_deactivated(SPObject *layer, SPDesktop *desktop) {
    g_return_if_fail(SP_IS_GROUP(layer));
    SP_GROUP(layer)->setLayerDisplayMode(desktop->dkey, SPGroup::GROUP);
}

/// Callback
static void
_layer_hierarchy_changed(SPObject */*top*/, SPObject *bottom,
                                         SPDesktop *desktop)
{
    desktop->_layer_changed_signal.emit (bottom);
}

/// Called when document is starting to be rebuilt.
static void _reconstruction_start(SPDesktop * desktop)
{
    desktop->_reconstruction_old_layer_id = desktop->currentLayer()->getId() ? desktop->currentLayer()->getId() : "";
    desktop->layers->reset();

    desktop->selection->clear();
}

/// Called when document rebuild is finished.
static void _reconstruction_finish(SPDesktop * desktop)
{
    g_debug("Desktop, finishing reconstruction\n");
    if ( !desktop->_reconstruction_old_layer_id.empty() ) {
        SPObject * newLayer = desktop->namedview->document->getObjectById(desktop->_reconstruction_old_layer_id);
        if (newLayer != NULL) {
            desktop->layers->setCurrentLayer(newLayer);
        }

        desktop->_reconstruction_old_layer_id.clear();
    }
    g_debug("Desktop, finishing reconstruction end\n");
}

/**
 * Namedview_modified callback.
 */
static void _namedview_modified (SPObject *obj, guint flags, SPDesktop *desktop)
{
    SPNamedView *nv=SP_NAMEDVIEW(obj);

    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        if (nv->pagecheckerboard) {
            desktop->canvas->setBackgroundCheckerboard();
        } else {
            desktop->canvas->setBackgroundColor(nv->pagecolor);
        }

        /* Show/hide page border */
        if (nv->showborder) {
            // show
            sp_canvas_item_show (desktop->page_border);
            // set color and shadow
            ((CtrlRect *) desktop->page_border)->setColor(nv->bordercolor, false, 0x00000000);
            if (nv->pageshadow) {
                ((CtrlRect *) desktop->page_border)->setShadow(nv->pageshadow, nv->bordercolor);
            }
            // place in the z-order stack
            if (nv->borderlayer == SP_BORDER_LAYER_BOTTOM) {
                 sp_canvas_item_move_to_z (desktop->page_border, 1);
            } else {
                int order = sp_canvas_item_order (desktop->page_border);
                int morder = sp_canvas_item_order (desktop->drawing);
                if (morder > order) sp_canvas_item_raise (desktop->page_border,
                    morder - order);
            }
        } else {
                sp_canvas_item_hide (desktop->page_border);
                if (nv->pageshadow) {
                    ((CtrlRect *) desktop->page)->setShadow(0, 0x00000000);
                }
        }

        /* Show/hide page shadow */
        if (nv->showpageshadow && nv->pageshadow) {
            ((CtrlRect *) desktop->page_border)->setShadow(nv->pageshadow, nv->bordercolor);
        } else {
            ((CtrlRect *) desktop->page_border)->setShadow(0, 0x00000000);
        }

        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        if (SP_RGBA32_R_U(nv->pagecolor) +
            SP_RGBA32_G_U(nv->pagecolor) +
            SP_RGBA32_B_U(nv->pagecolor) >= 384) {
            // the background color is light, use black outline
            SP_CANVAS_ARENA (desktop->drawing)->drawing.outlinecolor = prefs->getInt("/options/wireframecolors/onlight", 0xff);
        } else { // use white outline
            SP_CANVAS_ARENA (desktop->drawing)->drawing.outlinecolor = prefs->getInt("/options/wireframecolors/ondark", 0xffffffff);
        }
    }
}

Geom::Affine SPDesktop::w2d() const
{
    return _current_affine.w2d();
}

Geom::Point SPDesktop::w2d(Geom::Point const &p) const
{
    return p * _current_affine.w2d();
}

Geom::Point SPDesktop::d2w(Geom::Point const &p) const
{
    return p * _current_affine.d2w();
}

Geom::Affine SPDesktop::doc2dt() const
{
    return _doc2dt;
}

Geom::Affine SPDesktop::dt2doc() const
{
    return _doc2dt.inverse();
}

Geom::Point SPDesktop::doc2dt(Geom::Point const &p) const
{
    return p * _doc2dt;
}

Geom::Point SPDesktop::dt2doc(Geom::Point const &p) const
{
    return p * dt2doc();
}

void
SPDesktop::show_dialogs()
{

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    if (prefs == NULL) {
        return;
    }

    int active = prefs->getInt("/options/savedialogposition/value", 1);
    if (active == 0) {
        // User has turned off this feature in preferences
        return;
    }

    if (showing_dialogs) {
        return;
    }

    showing_dialogs = TRUE;


    /*
     * Get each dialogs previous state from preferences and reopen on startup if needed, without grabbing focus (canvas retains focus).
     * Map dialog manager's dialog IDs to dialog last visible state preference. FIXME: store this correspondence in dialogs themselves!
     */
    std::map<Glib::ustring, Glib::ustring> mapVerbPreference;
    mapVerbPreference.insert(std::make_pair ("LayersPanel", "/dialogs/layers") );
    mapVerbPreference.insert(std::make_pair ("FillAndStroke", "/dialogs/fillstroke") );
    mapVerbPreference.insert(std::make_pair ("ExtensionEditor", "/dialogs/extensioneditor") );
    mapVerbPreference.insert(std::make_pair ("AlignAndDistribute", "/dialogs/align") );
    mapVerbPreference.insert(std::make_pair ("DocumentMetadata", "/dialogs/documentmetadata") );
    mapVerbPreference.insert(std::make_pair ("DocumentProperties", "/dialogs/documentoptions") );
    mapVerbPreference.insert(std::make_pair ("FilterEffectsDialog", "/dialogs/filtereffects") );
    mapVerbPreference.insert(std::make_pair ("Find", "/dialogs/find") );
    mapVerbPreference.insert(std::make_pair ("Glyphs", "/dialogs/glyphs") );
    mapVerbPreference.insert(std::make_pair ("Messages", "/dialogs/messages") );
    mapVerbPreference.insert(std::make_pair ("Memory", "/dialogs/memory") );
    mapVerbPreference.insert(std::make_pair ("LivePathEffect", "/dialogs/livepatheffect") );
    mapVerbPreference.insert(std::make_pair ("UndoHistory", "/dialogs/undo-history") );
    mapVerbPreference.insert(std::make_pair ("Transformation", "/dialogs/transformation") );
    mapVerbPreference.insert(std::make_pair ("Swatches", "/dialogs/swatches") );
    mapVerbPreference.insert(std::make_pair ("IconPreviewPanel", "/dialogs/iconpreview") );
    mapVerbPreference.insert(std::make_pair ("SvgFontsDialog", "/dialogs/svgfonts") );
    mapVerbPreference.insert(std::make_pair ("InputDevices", "/dialogs/inputdevices") );
    mapVerbPreference.insert(std::make_pair ("InkscapePreferences", "/dialogs/preferences") );
    mapVerbPreference.insert(std::make_pair ("TileDialog", "/dialogs/gridtiler") );
    mapVerbPreference.insert(std::make_pair ("Trace", "/dialogs/trace") );
    mapVerbPreference.insert(std::make_pair ("PixelArt", "/dialogs/pixelart") );
    mapVerbPreference.insert(std::make_pair ("TextFont", "/dialogs/textandfont") );
    mapVerbPreference.insert(std::make_pair ("Export", "/dialogs/export") );
    mapVerbPreference.insert(std::make_pair ("XmlTree", "/dialogs/xml") );
    mapVerbPreference.insert(std::make_pair ("CloneTiler", "/dialogs/clonetiler") );
    mapVerbPreference.insert(std::make_pair ("ObjectProperties", "/dialogs/object") );
    mapVerbPreference.insert(std::make_pair ("SpellCheck", "/dialogs/spellcheck") );
    mapVerbPreference.insert(std::make_pair ("Symbols", "/dialogs/symbols") );
    mapVerbPreference.insert(std::make_pair ("ObjectsPanel", "/dialogs/objects") );
    mapVerbPreference.insert(std::make_pair ("TagsPanel", "/dialogs/tags") );
    mapVerbPreference.insert(std::make_pair ("Prototype", "/dialogs/prototype") );


    for (std::map<Glib::ustring, Glib::ustring>::const_iterator iter = mapVerbPreference.begin(); iter != mapVerbPreference.end(); ++iter) {
        Glib::ustring pref = iter->second;
        int visible = prefs->getInt(pref + "/visible", 0);
        if (visible) {

            // Try to ensure that the panel is created attached to the correct desktop (bug 1720096).
            // There must be a better way of handling this problem!
            INKSCAPE.activate_desktop(this);

            _dlg_mgr->showDialog(iter->first.c_str(), false); // without grabbing focus, we need focus to remain on the canvas
        }
    }
}
/*
 * Pop event context from desktop's context stack. Never used.
 */
// void
// SPDesktop::pop_event_context (unsigned int key)
// {
//    ToolBase *ec = NULL;
//
//    if (event_context && event_context->key == key) {
//        g_return_if_fail (event_context);
//        g_return_if_fail (event_context->next);
//        ec = event_context;
//        sp_event_context_deactivate (ec);
//        event_context = ec->next;
//        sp_event_context_activate (event_context);
//        _event_context_changed_signal.emit (this, ec);
//    }
//
//    ToolBase *ref = event_context;
//    while (ref && ref->next && ref->next->key != key)
//        ref = ref->next;
//
//    if (ref && ref->next) {
//        ec = ref->next;
//        ref->next = ec->next;
//    }
//
//    if (ec) {
//        sp_event_context_finish (ec);
//        g_object_unref (G_OBJECT (ec));
//    }
// }

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

