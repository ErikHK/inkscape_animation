/*
 * Selection and transformation context
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Abhishek Sharma
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2010      authors
 * Copyright (C) 2006      Johan Engelen <johan@shouraizou.nl>
 * Copyright (C) 1999-2005 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <cstring>
#include <string>
#include <gdk/gdkkeysyms.h>
#include "macros.h"
#include "rubberband.h"
#include "document.h"
#include "document-undo.h"
#include "selection.h"
#include "sp-cursor.h"
#include "style.h"
#include "pixmaps/cursor-select-m.xpm"
#include "pixmaps/cursor-select-d.xpm"
#include "pixmaps/handles.xpm"
#include <glibmm/i18n.h>

#include "ui/tools/select-tool.h"
#include "selection-chemistry.h"
#ifdef WITH_DBUS
#include "extension/dbus/document-interface.h"
#endif
#include "desktop.h"

#include "sp-root.h"
#include "preferences.h"
#include "ui/tools-switch.h"
#include "message-stack.h"
#include "selection-describer.h"
#include "seltrans.h"
#include "box3d.h"
#include "display/sp-canvas.h"
#include "display/sp-canvas-item.h"
#include "display/drawing-item.h"

using Inkscape::DocumentUndo;

GdkPixbuf *handles[13];

namespace Inkscape {
namespace UI {
namespace Tools {

static GdkCursor *CursorSelectMouseover = NULL;
static GdkCursor *CursorSelectDragging = NULL;

static gint rb_escaped = 0; // if non-zero, rubberband was canceled by esc, so the next button release should not deselect
static gint drag_escaped = 0; // if non-zero, drag was canceled by esc

const std::string& SelectTool::getPrefsPath() {
    return SelectTool::prefsPath;
}

const std::string SelectTool::prefsPath = "/tools/select";


//Creates rotated variations for handles
static void
sp_load_handles(int start, int count, char const **xpm) {
    handles[start] = gdk_pixbuf_new_from_xpm_data((gchar const **)xpm);
    for(int i = start + 1; i < start + count; i++) {
        // We use either the original at *start or previous loop item to rotate
        handles[i] = gdk_pixbuf_rotate_simple(handles[i-1], GDK_PIXBUF_ROTATE_CLOCKWISE);
    }
}

SelectTool::SelectTool()
    // Don't load a default cursor
    : ToolBase(NULL, 0, 0)
    , dragging(false)
    , moved(false)
    , button_press_shift(false)
    , button_press_ctrl(false)
    , button_press_alt(false)
    , cycling_wrap(true)
    , item(NULL)
    , grabbed(NULL)
    , _seltrans(NULL)
    , _describer(NULL)
{
    // cursors in select context
    CursorSelectMouseover = sp_cursor_new_from_xpm(cursor_select_m_xpm , 1, 1);
    CursorSelectDragging = sp_cursor_new_from_xpm(cursor_select_d_xpm , 1, 1);
    
    // selection handles
    sp_load_handles(0, 2, handle_scale_xpm);
    sp_load_handles(2, 2, handle_stretch_xpm);
    sp_load_handles(4, 4, handle_rotate_xpm);
    sp_load_handles(8, 4, handle_skew_xpm);
    sp_load_handles(12, 1, handle_center_xpm);
}

//static gint xp = 0, yp = 0; // where drag started
//static gint tolerance = 0;
//static bool within_tolerance = false;
static bool is_cycling = false;


SelectTool::~SelectTool() {
    this->enableGrDrag(false);

    if (this->grabbed) {
        sp_canvas_item_ungrab(this->grabbed, GDK_CURRENT_TIME);
        this->grabbed = NULL;
    }

    delete this->_seltrans;
    this->_seltrans = NULL;

    delete this->_describer;
    this->_describer = NULL;

    if (CursorSelectDragging) {
#if GTK_CHECK_VERSION(3,0,0)
        g_object_unref(CursorSelectDragging);
#else
        gdk_cursor_unref (CursorSelectDragging);
#endif
        CursorSelectDragging = NULL;
    }
    
    if (CursorSelectMouseover) {
#if GTK_CHECK_VERSION(3,0,0)
        g_object_unref(CursorSelectMouseover);
#else
        gdk_cursor_unref (CursorSelectMouseover);
#endif
        CursorSelectMouseover = NULL;
    }
}

void SelectTool::setup() {
    ToolBase::setup();

    this->_describer = new Inkscape::SelectionDescriber(
                desktop->selection, 
                desktop->messageStack(),
                _("Click selection to toggle scale/rotation handles"),
                _("No objects selected. Click, Shift+click, Alt+scroll mouse on top of objects, or drag around objects to select.")
    );

    this->_seltrans = new Inkscape::SelTrans(desktop);

    sp_event_context_read(this, "show");
    sp_event_context_read(this, "transform");

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    if (prefs->getBool("/tools/select/gradientdrag")) {
        this->enableGrDrag();
    }
}

void SelectTool::set(const Inkscape::Preferences::Entry& val) {
    Glib::ustring path = val.getEntryName();

    if (path == "show") {
        if (val.getString() == "outline") {
            this->_seltrans->setShow(Inkscape::SelTrans::SHOW_OUTLINE);
        } else {
            this->_seltrans->setShow(Inkscape::SelTrans::SHOW_CONTENT);
        }
    }
}

bool SelectTool::sp_select_context_abort() {
    Inkscape::SelTrans *seltrans = this->_seltrans;

    if (this->dragging) {
        if (this->moved) { // cancel dragging an object
            seltrans->ungrab();
            this->moved = FALSE;
            this->dragging = FALSE;
            sp_event_context_discard_delayed_snap_event(this);
            drag_escaped = 1;

            if (this->item) {
                // only undo if the item is still valid
                if (this->item->document) {
                    DocumentUndo::undo(desktop->getDocument());
                }

                sp_object_unref( this->item, NULL);
            } else if (this->button_press_ctrl) {
                // NOTE:  This is a workaround to a bug.
                // When the ctrl key is held, sc->item is not defined
                // so in this case (only), we skip the object doc check
                DocumentUndo::undo(desktop->getDocument());
            }
            this->item = NULL;

            SP_EVENT_CONTEXT(this)->desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Move canceled."));
            return true;
        }
    } else {
        if (Inkscape::Rubberband::get(desktop)->is_started()) {
            Inkscape::Rubberband::get(desktop)->stop();
            rb_escaped = 1;
            SP_EVENT_CONTEXT(this)->defaultMessageContext()->clear();
            SP_EVENT_CONTEXT(this)->desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Selection canceled."));
            return true;
        }
    }
    return false;
}

static bool
key_is_a_modifier (guint key) {
    return (key == GDK_KEY_Alt_L ||
                key == GDK_KEY_Alt_R ||
                key == GDK_KEY_Control_L ||
                key == GDK_KEY_Control_R ||
                key == GDK_KEY_Shift_L ||
                key == GDK_KEY_Shift_R ||
                key == GDK_KEY_Meta_L ||  // Meta is when you press Shift+Alt (at least on my machine)
            key == GDK_KEY_Meta_R);
}

static void
sp_select_context_up_one_layer(SPDesktop *desktop)
{
    /* Click in empty place, go up one level -- but don't leave a layer to root.
     *
     * (Rationale: we don't usually allow users to go to the root, since that
     * detracts from the layer metaphor: objects at the root level can in front
     * of or behind layers.  Whereas it's fine to go to the root if editing
     * a document that has no layers (e.g. a non-Inkscape document).)
     *
     * Once we support editing SVG "islands" (e.g. <svg> embedded in an xhtml
     * document), we might consider further restricting the below to disallow
     * leaving a layer to go to a non-layer.
     */
    SPObject *const current_layer = desktop->currentLayer();
    if (current_layer) {
        SPObject *const parent = current_layer->parent;
        SPGroup *current_group = dynamic_cast<SPGroup *>(current_layer);
        if ( parent
             && ( parent->parent
                  || !( current_group
                        && ( SPGroup::LAYER == current_group->layerMode() ) ) ) )
        {
            desktop->setCurrentLayer(parent);
            if (current_group && (SPGroup::LAYER != current_group->layerMode())) {
                desktop->getSelection()->set(current_layer);
            }
        }
    }
}

bool SelectTool::item_handler(SPItem* item, GdkEvent* event) {
    gint ret = FALSE;

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    tolerance = prefs->getIntLimited("/options/dragtolerance/value", 0, 0, 100);

    // make sure we still have valid objects to move around
    if (this->item && this->item->document == NULL) {
        this->sp_select_context_abort();
    }

    switch (event->type) {
        case GDK_BUTTON_PRESS:
            if (event->button.button == 1 && !this->space_panning) {
                /* Left mousebutton */

                // save drag origin
                xp = (gint) event->button.x;
                yp = (gint) event->button.y;
                within_tolerance = true;

                // remember what modifiers were on before button press
                this->button_press_shift = (event->button.state & GDK_SHIFT_MASK) ? true : false;
                this->button_press_ctrl = (event->button.state & GDK_CONTROL_MASK) ? true : false;
                this->button_press_alt = (event->button.state & GDK_MOD1_MASK) ? true : false;

                if (event->button.state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)) {
                    // if shift or ctrl was pressed, do not move objects;
                    // pass the event to root handler which will perform rubberband, shift-click, ctrl-click, ctrl-drag
                } else {
                    GdkWindow* window = gtk_widget_get_window (GTK_WIDGET (desktop->getCanvas()));
                   
                    this->dragging = TRUE;
                    this->moved = FALSE;

                    gdk_window_set_cursor(window, CursorSelectDragging);

                    desktop->canvas->forceFullRedrawAfterInterruptions(5);

                    // remember the clicked item in this->item:
                    if (this->item) {
                        sp_object_unref(this->item, NULL);
                        this->item = NULL;
                    }

                    this->item = sp_event_context_find_item (desktop,
                                              Geom::Point(event->button.x, event->button.y), event->button.state & GDK_MOD1_MASK, FALSE);
                    sp_object_ref(this->item, NULL);

                    rb_escaped = drag_escaped = 0;

                    if (this->grabbed) {
                        sp_canvas_item_ungrab(this->grabbed, event->button.time);
                        this->grabbed = NULL;
                    }

                    sp_canvas_item_grab(SP_CANVAS_ITEM(desktop->drawing),
                                        GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK |
                                        GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK,
                                        NULL, event->button.time);

                    this->grabbed = SP_CANVAS_ITEM(desktop->drawing);

                    desktop->canvas->forceFullRedrawAfterInterruptions(5);

                    ret = TRUE;
                }
            } else if (event->button.button == 3) {
                // right click; do not eat it so that right-click menu can appear, but cancel dragging & rubberband
                this->sp_select_context_abort();
            }
            break;

        case GDK_ENTER_NOTIFY: {
            if (!desktop->isWaitingCursor() && !this->dragging) {
                GdkWindow* window = gtk_widget_get_window (GTK_WIDGET (desktop->getCanvas()));

                gdk_window_set_cursor(window, CursorSelectMouseover);
            }
            break;
        }
        case GDK_LEAVE_NOTIFY:
            if (!desktop->isWaitingCursor() && !this->dragging) {
                GdkWindow* window = gtk_widget_get_window (GTK_WIDGET (desktop->getCanvas()));

                gdk_window_set_cursor(window, this->cursor);
            }
            break;

        case GDK_KEY_PRESS:
            if (get_latin_keyval (&event->key) == GDK_KEY_space) {
                if (this->dragging && this->grabbed) {
                    /* stamping mode: show content mode moving */
                    _seltrans->stamp();
                    ret = TRUE;
                }
            } else if (get_latin_keyval (&event->key) == GDK_KEY_Tab) {
                if (this->dragging && this->grabbed) {
                    _seltrans->getNextClosestPoint(false);
                    ret = TRUE;
                }
            } else if (get_latin_keyval (&event->key) == GDK_KEY_ISO_Left_Tab) {
                if (this->dragging && this->grabbed) {
                    _seltrans->getNextClosestPoint(true);
                    ret = TRUE;
                }
            }
            break;

        default:
            break;
    }

    if (!ret) {
        ret = ToolBase::item_handler(item, event);
    }

    return ret;
}

void SelectTool::sp_select_context_cycle_through_items(Inkscape::Selection *selection, GdkEventScroll *scroll_event, bool shift_pressed) {
    if ( this->cycling_items.empty() )
        return;

    Inkscape::DrawingItem *arenaitem;

    if(cycling_cur_item) {
        arenaitem = cycling_cur_item->get_arenaitem(desktop->dkey);
        arenaitem->setOpacity(0.3);
    }

    // Find next item and activate it


    std::vector<SPItem *>::iterator next = cycling_items.end();

    if (scroll_event->direction == GDK_SCROLL_UP) {
        if (! cycling_cur_item) {
            next = cycling_items.begin();
        } else {
            next = std::find( cycling_items.begin(), cycling_items.end(), cycling_cur_item );
            g_assert (next != cycling_items.end());
            next++;
            if (next == cycling_items.end())
                if ( cycling_wrap ) 
                    next = cycling_items.begin();
                else 
                    next--;
        }
    } else { 
        if (! cycling_cur_item) {
            next = cycling_items.end();
            next--;
        } else {
            next = std::find( cycling_items.begin(), cycling_items.end(), cycling_cur_item );
            g_assert (next != cycling_items.end());
            if (next == cycling_items.begin()){
                if ( cycling_wrap ) { 
                    next = cycling_items.end();
                    next--;
                }
            } else {
                next--;
            }
        }
    }

    this->cycling_cur_item = *next;
    g_assert(next != cycling_items.end());
    g_assert(cycling_cur_item != NULL);

    arenaitem = cycling_cur_item->get_arenaitem(desktop->dkey);
    arenaitem->setOpacity(1.0);

    if (shift_pressed) {
        selection->add(cycling_cur_item);
    } else {
        selection->set(cycling_cur_item);
    }
}

void SelectTool::sp_select_context_reset_opacities() {
    for (std::vector<SPItem *>::const_iterator l = this->cycling_items_cmp.begin(); l != this->cycling_items_cmp.end(); ++l ) {
        SPItem *item = *l;
        if (item) {
            Inkscape::DrawingItem *arenaitem = item->get_arenaitem(desktop->dkey);
            arenaitem->setOpacity(SP_SCALE24_TO_FLOAT(item->style->opacity.value));
        } else {
            g_assert_not_reached();
        }
    }

    this->cycling_items_cmp.clear();
    this->cycling_cur_item = NULL;
}

bool SelectTool::root_handler(GdkEvent* event) {
    SPItem *item = NULL;
    SPItem *item_at_point = NULL, *group_at_point = NULL, *item_in_group = NULL;
    gint ret = FALSE;

    Inkscape::Selection *selection = desktop->getSelection();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    // make sure we still have valid objects to move around
    if (this->item && this->item->document == NULL) {
        this->sp_select_context_abort();
    }

    switch (event->type) {
        case GDK_2BUTTON_PRESS:
            if (event->button.button == 1) {
                if (!selection->isEmpty()) {
                    SPItem *clicked_item = selection->itemList()[0];

                    if (dynamic_cast<SPGroup *>(clicked_item) && !dynamic_cast<SPBox3D *>(clicked_item)) { // enter group if it's not a 3D box
                        desktop->setCurrentLayer(clicked_item);
                        desktop->getSelection()->clear();
                        this->dragging = false;
                        sp_event_context_discard_delayed_snap_event(this);

                        desktop->canvas->endForcedFullRedraws();
                    } else { // switch tool
                        Geom::Point const button_pt(event->button.x, event->button.y);
                        Geom::Point const p(desktop->w2d(button_pt));
                        tools_switch_by_item (desktop, clicked_item, p);
                    }
                } else {
                    sp_select_context_up_one_layer(desktop);
                }

                ret = TRUE;
            }
            break;

        case GDK_BUTTON_PRESS:
            if (event->button.button == 1 && !this->space_panning) {
                // save drag origin
                xp = (gint) event->button.x;
                yp = (gint) event->button.y;
                within_tolerance = true;

                Geom::Point const button_pt(event->button.x, event->button.y);
                Geom::Point const p(desktop->w2d(button_pt));

                if (event->button.state & GDK_MOD1_MASK) {
                    Inkscape::Rubberband::get(desktop)->setMode(RUBBERBAND_MODE_TOUCHPATH);
                }

                Inkscape::Rubberband::get(desktop)->start(desktop, p);

                if (this->grabbed) {
                    sp_canvas_item_ungrab(this->grabbed, event->button.time);
                    this->grabbed = NULL;
                }

                sp_canvas_item_grab(SP_CANVAS_ITEM(desktop->acetate),
                                    GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK,
                                    NULL, event->button.time);

                this->grabbed = SP_CANVAS_ITEM(desktop->acetate);

                // remember what modifiers were on before button press
                this->button_press_shift = (event->button.state & GDK_SHIFT_MASK) ? true : false;
                this->button_press_ctrl = (event->button.state & GDK_CONTROL_MASK) ? true : false;
                this->button_press_alt = (event->button.state & GDK_MOD1_MASK) ? true : false;

                this->moved = FALSE;

                rb_escaped = drag_escaped = 0;

                ret = TRUE;
            } else if (event->button.button == 3) {
                // right click; do not eat it so that right-click menu can appear, but cancel dragging & rubberband
                this->sp_select_context_abort();
            }
            break;

        case GDK_MOTION_NOTIFY:
        {
            tolerance = prefs->getIntLimited("/options/dragtolerance/value", 0, 0, 100);

            if ((event->motion.state & GDK_BUTTON1_MASK) && !this->space_panning) {
                Geom::Point const motion_pt(event->motion.x, event->motion.y);
                Geom::Point const p(desktop->w2d(motion_pt));

                if ( within_tolerance
                     && ( abs( (gint) event->motion.x - xp ) < tolerance )
                     && ( abs( (gint) event->motion.y - yp ) < tolerance ) ) {
                    break; // do not drag if we're within tolerance from origin
                }
                // Once the user has moved farther than tolerance from the original location
                // (indicating they intend to move the object, not click), then always process the
                // motion notify coordinates as given (no snapping back to origin)
                within_tolerance = false;

                if (this->button_press_ctrl || (this->button_press_alt && !this->button_press_shift && !selection->isEmpty())) {
                    // if it's not click and ctrl or alt was pressed (the latter with some selection
                    // but not with shift) we want to drag rather than rubberband
                    this->dragging = TRUE;

                    GdkWindow* window = gtk_widget_get_window (GTK_WIDGET (desktop->getCanvas()));

                    gdk_window_set_cursor(window, CursorSelectDragging);

                    desktop->canvas->forceFullRedrawAfterInterruptions(5);
                }

                if (this->dragging) {
                    /* User has dragged fast, so we get events on root (lauris)*/
                    // not only that; we will end up here when ctrl-dragging as well
                    // and also when we started within tolerance, but trespassed tolerance outside of item
                    if (Inkscape::Rubberband::get(desktop)->is_started()) {
                        Inkscape::Rubberband::get(desktop)->stop();
                    }
                    this->defaultMessageContext()->clear();

                    item_at_point = desktop->getItemAtPoint(Geom::Point(event->button.x, event->button.y), FALSE);

                    if (!item_at_point) { // if no item at this point, try at the click point (bug 1012200)
                        item_at_point = desktop->getItemAtPoint(Geom::Point(xp, yp), FALSE);
                    }

                    if (item_at_point || this->moved || this->button_press_alt) {
                        // drag only if starting from an item, or if something is already grabbed, or if alt-dragging
                        if (!this->moved) {
                            item_in_group = desktop->getItemAtPoint(Geom::Point(event->button.x, event->button.y), TRUE);
                            group_at_point = desktop->getGroupAtPoint(Geom::Point(event->button.x, event->button.y));

                            {
                                SPGroup *selGroup = dynamic_cast<SPGroup *>(selection->single());
                                if (selGroup && (selGroup->layerMode() == SPGroup::LAYER)) {
                                    group_at_point = selGroup;
                                }
                            }

                            // group-at-point is meant to be topmost item if it's a group,
                            // not topmost group of all items at point
                            if (group_at_point != item_in_group &&
                                !(group_at_point && item_at_point &&
                                  group_at_point->isAncestorOf(item_at_point))) {
                                group_at_point = NULL;
                            }

                            // if neither a group nor an item (possibly in a group) at point are selected, set selection to the item at point
                            if ((!item_in_group || !selection->includes(item_in_group)) &&
                                (!group_at_point || !selection->includes(group_at_point))
                                && !this->button_press_alt) {
                                // select what is under cursor
                                if (!_seltrans->isEmpty()) {
                                    _seltrans->resetState();
                                }

                                // when simply ctrl-dragging, we don't want to go into groups
                                if (item_at_point && !selection->includes(item_at_point)) {
                                    selection->set(item_at_point);
                                }
                            } // otherwise, do not change selection so that dragging selected-within-group items, as well as alt-dragging, is possible

                            _seltrans->grab(p, -1, -1, FALSE, TRUE);
                            this->moved = TRUE;
                        }

                        if (!_seltrans->isEmpty()) {
                            _seltrans->moveTo(p, event->button.state);
                        }

                        desktop->scroll_to_point(p);
                        gobble_motion_events(GDK_BUTTON1_MASK);
                        ret = TRUE;
                    } else {
                        this->dragging = FALSE;
                        sp_event_context_discard_delayed_snap_event(this);
                        desktop->canvas->endForcedFullRedraws();
                    }
                } else {
                    if (Inkscape::Rubberband::get(desktop)->is_started()) {
                        Inkscape::Rubberband::get(desktop)->move(p);

                        if (Inkscape::Rubberband::get(desktop)->getMode() == RUBBERBAND_MODE_TOUCHPATH) {
                            this->defaultMessageContext()->set(Inkscape::NORMAL_MESSAGE, _("<b>Draw over</b> objects to select them; release <b>Alt</b> to switch to rubberband selection"));
                        } else {
                            this->defaultMessageContext()->set(Inkscape::NORMAL_MESSAGE, _("<b>Drag around</b> objects to select them; press <b>Alt</b> to switch to touch selection"));
                        }

                        gobble_motion_events(GDK_BUTTON1_MASK);
                    }
                }
            }
            break;
        }
        case GDK_BUTTON_RELEASE:
            xp = yp = 0;

            if ((event->button.button == 1) && (this->grabbed) && !this->space_panning) {
                if (this->dragging) {
                    GdkWindow* window;

                    if (this->moved) {
                        // item has been moved
                        _seltrans->ungrab();
                        this->moved = FALSE;
#ifdef WITH_DBUS
                        dbus_send_ping(desktop, this->item);
#endif
                    } else if (this->item && !drag_escaped) {
                        // item has not been moved -> simply a click, do selecting
                        if (!selection->isEmpty()) {
                            if (event->button.state & GDK_SHIFT_MASK) {
                                // with shift, toggle selection
                                _seltrans->resetState();
                                selection->toggle(this->item);
                            } else {
                                SPObject* single = selection->single();
                                SPGroup *singleGroup = dynamic_cast<SPGroup *>(single);
                                // without shift, increase state (i.e. toggle scale/rotation handles)
                                if (selection->includes(this->item)) {
                                    _seltrans->increaseState();
                                } else if (singleGroup && (singleGroup->layerMode() == SPGroup::LAYER) && single->isAncestorOf(this->item)) {
                                    _seltrans->increaseState();
                                } else {
                                    _seltrans->resetState();
                                    selection->set(this->item);
                                }
                            }
                        } else { // simple or shift click, no previous selection
                            _seltrans->resetState();
                            selection->set(this->item);
                        }
                    }

                    this->dragging = FALSE;
                    window = gtk_widget_get_window (GTK_WIDGET (desktop->getCanvas()));

                    gdk_window_set_cursor(window, CursorSelectMouseover);
                    sp_event_context_discard_delayed_snap_event(this);
                    desktop->canvas->endForcedFullRedraws();

                    if (this->item) {
                        sp_object_unref( this->item, NULL);
                    }

                    this->item = NULL;
                } else {
                    Inkscape::Rubberband *r = Inkscape::Rubberband::get(desktop);

                    if (r->is_started() && !within_tolerance) {
                        // this was a rubberband drag
                        std::vector<SPItem*> items;

                        if (r->getMode() == RUBBERBAND_MODE_RECT) {
                            Geom::OptRect const b = r->getRectangle();
                            items = desktop->getDocument()->getItemsInBox(desktop->dkey, *b);
                        } else if (r->getMode() == RUBBERBAND_MODE_TOUCHPATH) {
                            items = desktop->getDocument()->getItemsAtPoints(desktop->dkey, r->getPoints());
                        }

                        _seltrans->resetState();
                        r->stop();
                        this->defaultMessageContext()->clear();

                        if (event->button.state & GDK_SHIFT_MASK) {
                            // with shift, add to selection
                            selection->addList (items);
                        } else {
                            // without shift, simply select anew
                            selection->setList (items);
                        }

                    } else { // it was just a click, or a too small rubberband
                        r->stop();

                        if (this->button_press_shift && !rb_escaped && !drag_escaped) {
                            // this was a shift+click or alt+shift+click, select what was clicked upon
                            this->button_press_shift = false;

                            if (this->button_press_ctrl) {
                                // go into groups, honoring Alt
                                item = sp_event_context_find_item (desktop,
                                                   Geom::Point(event->button.x, event->button.y), event->button.state & GDK_MOD1_MASK, TRUE);
                                this->button_press_ctrl = FALSE;
                            } else {
                                // don't go into groups, honoring Alt
                                item = sp_event_context_find_item (desktop,
                                                   Geom::Point(event->button.x, event->button.y), event->button.state & GDK_MOD1_MASK, FALSE);
                            }

                            if (item) {
                                selection->toggle(item);
                                item = NULL;
                            }

                        } else if ((this->button_press_ctrl || this->button_press_alt) && !rb_escaped && !drag_escaped) { // ctrl+click, alt+click
                            item = sp_event_context_find_item (desktop,
                                         Geom::Point(event->button.x, event->button.y), this->button_press_alt, this->button_press_ctrl);

                            this->button_press_ctrl = FALSE;
                            this->button_press_alt = FALSE;

                            if (item) {
                                if (selection->includes(item)) {
                                    _seltrans->increaseState();
                                } else {
                                    _seltrans->resetState();
                                    selection->set(item);
                                }

                                item = NULL;
                            }
                        } else { // click without shift, simply deselect, unless with Alt or something was cancelled
                            if (!selection->isEmpty()) {
                                if (!(rb_escaped) && !(drag_escaped) && !(event->button.state & GDK_MOD1_MASK)) {
                                    selection->clear();
                                }

                                rb_escaped = 0;
                            }
                        }
                    }

                    ret = TRUE;
                }

                if (this->grabbed) {
                    sp_canvas_item_ungrab(this->grabbed, event->button.time);
                    this->grabbed = NULL;
                }

                desktop->updateNow();
            }

            if (event->button.button == 1) {
                Inkscape::Rubberband::get(desktop)->stop(); // might have been started in another tool!
            }

            this->button_press_shift = false;
            this->button_press_ctrl = false;
            this->button_press_alt = false;
            break;

        case GDK_SCROLL: {

            GdkEventScroll *scroll_event = (GdkEventScroll*) event;

            if ( ! (scroll_event->state & GDK_MOD1_MASK)) // do nothing specific if alt was not pressed
                break;

            bool shift_pressed = scroll_event->state & GDK_SHIFT_MASK;
            is_cycling = true;

            /* Rebuild list of items underneath the mouse pointer */
            Geom::Point p = desktop->d2w(desktop->point()); 
            SPItem *item = desktop->getItemAtPoint(p, true, NULL);
            this->cycling_items.clear();

            SPItem *tmp = NULL;
            while(item != NULL) {
                this->cycling_items.push_back(item);
                item = desktop->getItemAtPoint(p, true, item);
                if (selection->includes(item)) tmp = item;
            }

            /* Compare current item list with item list during previous scroll ... */
            bool item_lists_differ = this->cycling_items != this->cycling_items_cmp;

            if(item_lists_differ) {
                this->sp_select_context_reset_opacities();
                for (std::vector<SPItem *>::const_iterator l = this->cycling_items_cmp.begin(); l != this->cycling_items_cmp.end(); ++l) 
                    selection->remove(*l); // deselects the previous content of the cycling loop
                this->cycling_items_cmp = (this->cycling_items);

                // set opacities in new stack
                for(std::vector<SPItem *>::const_iterator l = this->cycling_items.begin(); l != this->cycling_items.end(); ++l) {
                    SPItem *item =*l;
                    if (item) {
                        Inkscape::DrawingItem *arenaitem = item->get_arenaitem(desktop->dkey);
                        arenaitem->setOpacity(0.3);
                    }
                }
            }
            if(!cycling_cur_item) cycling_cur_item = tmp;

            this->cycling_wrap = prefs->getBool("/options/selection/cycleWrap", true);

            // Cycle through the items underneath the mouse pointer, one-by-one
            this->sp_select_context_cycle_through_items(selection, scroll_event, shift_pressed);

            ret = TRUE;

            GtkWindow *w =GTK_WINDOW(gtk_widget_get_toplevel( GTK_WIDGET(desktop->canvas) ));
            if (w) {
                gtk_window_present(w);
                gtk_widget_grab_focus (GTK_WIDGET(desktop->canvas)); 
            }
            break;
        }

        case GDK_KEY_PRESS: // keybindings for select context
            {
            {
            guint keyval = get_latin_keyval(&event->key);
            
                bool alt = ( MOD__ALT(event)
                                    || (keyval == GDK_KEY_Alt_L)
                                    || (keyval == GDK_KEY_Alt_R)
                                    || (keyval == GDK_KEY_Meta_L)
                                    || (keyval == GDK_KEY_Meta_R));

            if (!key_is_a_modifier (keyval)) {
                    this->defaultMessageContext()->clear();
            } else if (this->grabbed || _seltrans->isGrabbed()) {
                if (Inkscape::Rubberband::get(desktop)->is_started()) {
                    // if Alt then change cursor to moving cursor:
                    if (alt) {
                        Inkscape::Rubberband::get(desktop)->setMode(RUBBERBAND_MODE_TOUCHPATH);
                    }
                } else {
                // do not change the statusbar text when mousekey is down to move or transform the object,
                // because the statusbar text is already updated somewhere else.
                   break;
                }
            } else {
                    sp_event_show_modifier_tip (this->defaultMessageContext(), event,
                                                _("<b>Ctrl</b>: click to select in groups; drag to move hor/vert"),
                                                _("<b>Shift</b>: click to toggle select; drag for rubberband selection"),
                                                _("<b>Alt</b>: click to select under; scroll mouse-wheel to cycle-select; drag to move selected or select by touch"));
                    
                    // if Alt and nonempty selection, show moving cursor ("move selected"):
                    if (alt && !selection->isEmpty() && !desktop->isWaitingCursor()) {
                        GdkWindow* window = gtk_widget_get_window (GTK_WIDGET (desktop->getCanvas()));

                        gdk_window_set_cursor(window, CursorSelectDragging);
                    }
                    //*/
                    break;
            }
            }

            gdouble const nudge = prefs->getDoubleLimited("/options/nudgedistance/value", 2, 0, 1000, "px"); // in px
            gdouble const offset = prefs->getDoubleLimited("/options/defaultscale/value", 2, 0, 1000, "px");
            int const snaps = prefs->getInt("/options/rotationsnapsperpi/value", 12);

            switch (get_latin_keyval (&event->key)) {
                case GDK_KEY_Left: // move selection left
                case GDK_KEY_KP_Left:
                    if (!MOD__CTRL(event)) { // not ctrl
                        gint mul = 1 + gobble_key_events( get_latin_keyval(&event->key), 0); // with any mask
                        
                        if (MOD__ALT(event)) { // alt
                            if (MOD__SHIFT(event)) {
                                sp_selection_move_screen(desktop->getSelection(), mul*-10, 0); // shift
                            } else {
                                sp_selection_move_screen(desktop->getSelection(), mul*-1, 0); // no shift
                            }
                        } else { // no alt
                            if (MOD__SHIFT(event)) {
                                sp_selection_move(desktop->getSelection(), mul*-10*nudge, 0); // shift
                            } else {
                                sp_selection_move(desktop->getSelection(), mul*-nudge, 0); // no shift
                            }
                        }
                        
                        ret = TRUE;
                    }
                    break;
                    
                case GDK_KEY_Up: // move selection up
                case GDK_KEY_KP_Up:
                    if (!MOD__CTRL(event)) { // not ctrl
                        gint mul = 1 + gobble_key_events(get_latin_keyval(&event->key), 0); // with any mask
                        
                        if (MOD__ALT(event)) { // alt
                            if (MOD__SHIFT(event)) {
                                sp_selection_move_screen(desktop->getSelection(), 0, mul*10); // shift
                            } else {
                                sp_selection_move_screen(desktop->getSelection(), 0, mul*1); // no shift
                            }
                        } else { // no alt
                            if (MOD__SHIFT(event)) {
                                sp_selection_move(desktop->getSelection(), 0, mul*10*nudge); // shift
                            } else {
                                sp_selection_move(desktop->getSelection(), 0, mul*nudge); // no shift
                            }
                        }
                        
                        ret = TRUE;
                    }
                    break;
                    
                case GDK_KEY_Right: // move selection right
                case GDK_KEY_KP_Right:
                    if (!MOD__CTRL(event)) { // not ctrl
                        gint mul = 1 + gobble_key_events(get_latin_keyval(&event->key), 0); // with any mask
                        
                        if (MOD__ALT(event)) { // alt
                            if (MOD__SHIFT(event)) {
                                sp_selection_move_screen(desktop->getSelection(), mul*10, 0); // shift
                            } else {
                                sp_selection_move_screen(desktop->getSelection(), mul*1, 0); // no shift
                            }
                        } else { // no alt
                            if (MOD__SHIFT(event)) {
                                sp_selection_move(desktop->getSelection(), mul*10*nudge, 0); // shift
                            } else {
                                sp_selection_move(desktop->getSelection(), mul*nudge, 0); // no shift
                            }
                        }
                        
                        ret = TRUE;
                    }
                    break;
                    
                case GDK_KEY_Down: // move selection down
                case GDK_KEY_KP_Down:
                    if (!MOD__CTRL(event)) { // not ctrl
                        gint mul = 1 + gobble_key_events(get_latin_keyval(&event->key), 0); // with any mask
                        
                        if (MOD__ALT(event)) { // alt
                            if (MOD__SHIFT(event)) {
                                sp_selection_move_screen(desktop->getSelection(), 0, mul*-10); // shift
                            } else {
                                sp_selection_move_screen(desktop->getSelection(), 0, mul*-1); // no shift
                            }
                        } else { // no alt
                            if (MOD__SHIFT(event)) {
                                sp_selection_move(desktop->getSelection(), 0, mul*-10*nudge); // shift
                            } else {
                                sp_selection_move(desktop->getSelection(), 0, mul*-nudge); // no shift
                            }
                        }
                        
                        ret = TRUE;
                    }
                    break;
                    
                case GDK_KEY_Escape:
                    if (!this->sp_select_context_abort()) {
                        selection->clear();
                    }
                    
                    ret = TRUE;
                    break;

                case GDK_KEY_a:
                case GDK_KEY_A:
                    if (MOD__CTRL_ONLY(event)) {
                        sp_edit_select_all(desktop);
                        ret = TRUE;
                    }
                    break;
                    
                case GDK_KEY_space:
                    /* stamping mode: show outline mode moving */
                    /* FIXME: Is next condition ok? (lauris) */
                    if (this->dragging && this->grabbed) {
                        _seltrans->stamp();
                        ret = TRUE;
                    }
                    break;
                    
                case GDK_KEY_x:
                case GDK_KEY_X:
                    if (MOD__ALT_ONLY(event)) {
                        desktop->setToolboxFocusTo ("altx");
                        ret = TRUE;
                    }
                    break;
                    
                case GDK_KEY_bracketleft:
                    if (MOD__ALT(event)) {
                        gint mul = 1 + gobble_key_events(get_latin_keyval(&event->key), 0); // with any mask
                        sp_selection_rotate_screen(selection, mul*1);
                    } else if (MOD__CTRL(event)) {
                        sp_selection_rotate(selection, 90);
                    } else if (snaps) {
                        sp_selection_rotate(selection, 180.0/snaps);
                    }
                    
                    ret = TRUE;
                    break;
                    
                case GDK_KEY_bracketright:
                    if (MOD__ALT(event)) {
                        gint mul = 1 + gobble_key_events(get_latin_keyval(&event->key), 0); // with any mask
                        sp_selection_rotate_screen(selection, -1*mul);
                    } else if (MOD__CTRL(event)) {
                        sp_selection_rotate(selection, -90);
                    } else if (snaps) {
                        sp_selection_rotate(selection, -180.0/snaps);
                    }
                    
                    ret = TRUE;
                    break;
                    
                case GDK_KEY_less:
                case GDK_KEY_comma:
                    if (MOD__ALT(event)) {
                        gint mul = 1 + gobble_key_events(get_latin_keyval(&event->key), 0); // with any mask
                        sp_selection_scale_screen(selection, -2*mul);
                    } else if (MOD__CTRL(event)) {
                        sp_selection_scale_times(selection, 0.5);
                    } else {
                        gint mul = 1 + gobble_key_events(get_latin_keyval(&event->key), 0); // with any mask
                        sp_selection_scale(selection, -offset*mul);
                    }
                    
                    ret = TRUE;
                    break;
                    
                case GDK_KEY_greater:
                case GDK_KEY_period:
                    if (MOD__ALT(event)) {
                        gint mul = 1 + gobble_key_events(get_latin_keyval(&event->key), 0); // with any mask
                        sp_selection_scale_screen(selection, 2*mul);
                    } else if (MOD__CTRL(event)) {
                        sp_selection_scale_times(selection, 2);
                    } else {
                        gint mul = 1 + gobble_key_events(get_latin_keyval(&event->key), 0); // with any mask
                        sp_selection_scale(selection, offset*mul);
                    }
                    
                    ret = TRUE;
                    break;
                    
                case GDK_KEY_Return:
                    if (MOD__CTRL_ONLY(event)) {
                        if (selection->singleItem()) {
                            SPItem *clicked_item = selection->singleItem();
                            SPGroup *clickedGroup = dynamic_cast<SPGroup *>(clicked_item);
                            if ( (clickedGroup && (clickedGroup->layerMode() != SPGroup::LAYER)) || dynamic_cast<SPBox3D *>(clicked_item)) { // enter group or a 3D box
                                desktop->setCurrentLayer(clicked_item);
                                desktop->getSelection()->clear();
                            } else {
                                this->desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Selected object is not a group. Cannot enter."));
                            }
                        }
                        
                        ret = TRUE;
                    }
                    break;
                    
                case GDK_KEY_BackSpace:
                    if (MOD__CTRL_ONLY(event)) {
                        sp_select_context_up_one_layer(desktop);
                        ret = TRUE;
                    }
                    break;
                    
                case GDK_KEY_s:
                case GDK_KEY_S:
                    if (MOD__SHIFT_ONLY(event)) {
                        if (!selection->isEmpty()) {
                            _seltrans->increaseState();
                        }
                        
                        ret = TRUE;
                    }
                    break;
                    
                case GDK_KEY_g:
                case GDK_KEY_G:
                    if (MOD__SHIFT_ONLY(event)) {
                        sp_selection_to_guides(desktop);
                        ret = true;
                    }
                    break;
                    
                default:
                    break;
            }
            break;
            }
        case GDK_KEY_RELEASE: {
            guint keyval = get_latin_keyval(&event->key);
            if (key_is_a_modifier (keyval)) {
                this->defaultMessageContext()->clear();
            }
            
            bool alt = ( MOD__ALT(event)
                         || (keyval == GDK_KEY_Alt_L)
                         || (keyval == GDK_KEY_Alt_R)
                         || (keyval == GDK_KEY_Meta_L)
                         || (keyval == GDK_KEY_Meta_R));

            if (Inkscape::Rubberband::get(desktop)->is_started()) {
                // if Alt then change cursor to moving cursor:
                if (alt) {
                    Inkscape::Rubberband::get(desktop)->setMode(RUBBERBAND_MODE_RECT);
                }
            } else {
                if (alt) {
                    // quit cycle-selection and reset opacities
                    if (is_cycling) {
                        this->sp_select_context_reset_opacities();
                        is_cycling = false;
                    }
                }
            }

            // set cursor to default.
            if (!desktop->isWaitingCursor()) {
                // Do we need to reset the cursor here on key release ?
                //GdkWindow* window = gtk_widget_get_window (GTK_WIDGET (desktop->getCanvas()));
                //gdk_window_set_cursor(window, event_context->cursor);
            }
            break;
        }
        default:
            break;
    }

    if (!ret) {
        ret = ToolBase::root_handler(event);
    }

    return ret;
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
