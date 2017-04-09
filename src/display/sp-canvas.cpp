/*
 * Port of GnomeCanvas for Inkscape needs
 *
 * Authors:
 *   Federico Mena <federico@nuclecu.unam.mx>
 *   Raph Levien <raph@gimp.org>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   fred
 *   bbyak
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 2002-2006 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gdkmm/rectangle.h>
#include <cairomm/region.h>

#include "helper/sp-marshal.h"
#include <2geom/rect.h>
#include <2geom/affine.h>
#include "display/sp-canvas.h"
#include "display/sp-canvas-group.h"
#include "preferences.h"
#include "inkscape.h"
#include "sodipodi-ctrlrect.h"
#include "cms-system.h"
#include "display/rendermode.h"
#include "display/cairo-utils.h"
#include "debug/gdk-event-latency-tracker.h"
#include "desktop.h"

using Inkscape::Debug::GdkEventLatencyTracker;

// gtk_check_version returns non-NULL on failure
static bool const HAS_BROKEN_MOTION_HINTS =
  true || gtk_check_version(2, 12, 0) != NULL;

// Define this to visualize the regions to be redrawn
//#define DEBUG_REDRAW 1;

// Tiles are a way to minimize the number of redraws, eliminating too small redraws.
// The canvas stores a 2D array of ints, each representing a TILE_SIZExTILE_SIZE pixels tile.
// If any part of it is dirtied, the entire tile is dirtied (its int is nonzero) and repainted.
#define TILE_SIZE 16

/**
 * The SPCanvasGroup vtable.
 */
struct SPCanvasGroupClass {
    SPCanvasItemClass parent_class;
};

/**
 * A group of items.
 */
struct SPCanvasGroup {
    /**
     * Adds an item to a canvas group.
     */
    void add(SPCanvasItem *item);

    /**
     * Removes an item from a canvas group.
     */
    void remove(SPCanvasItem *item);

    /**
     * Class initialization function for SPCanvasGroupClass.
     */
    static void classInit(SPCanvasGroupClass *klass);

    /**
     * Callback. Empty.
     */
    static void init(SPCanvasGroup *group);

    /**
     * Callback that destroys all items in group and calls group's virtual
     * destroy() function.
     */
    static void destroy(SPCanvasItem *object);

    /**
     * Update handler for canvas groups.
     */
    static void update(SPCanvasItem *item, Geom::Affine const &affine, unsigned int flags);

    /**
     * Point handler for canvas groups.
     */
    static double point(SPCanvasItem *item, Geom::Point p, SPCanvasItem **actual_item);

    /**
     * Renders all visible canvas group items in buf rectangle.
     */
    static void render(SPCanvasItem *item, SPCanvasBuf *buf);

    static void viewboxChanged(SPCanvasItem *item, Geom::IntRect const &new_area);


    // Data members: ----------------------------------------------------------

    SPCanvasItem item;

    std::list<SPCanvasItem *> items;

};

/**
 * The SPCanvas vtable.
 */
struct SPCanvasClass {
    GtkWidgetClass parent_class;
};

namespace {

gint const UPDATE_PRIORITY = G_PRIORITY_HIGH_IDLE;

GdkWindow *getWindow(SPCanvas *canvas)
{
    return gtk_widget_get_window(reinterpret_cast<GtkWidget *>(canvas));
}


// SPCanvasItem

enum {
    ITEM_EVENT,
    ITEM_LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_VISIBLE
};


void trackLatency(GdkEvent const *event);

enum {
  DESTROY,
  LAST_SIGNAL
};

/**
 * Callback that removes item from all referers and destroys it.
 */
void sp_canvas_item_dispose(GObject           *object);
void sp_canvas_item_finalize(GObject          *object);
void sp_canvas_item_real_destroy(SPCanvasItem *object);

static guint       object_signals[LAST_SIGNAL] = { 0 };

/**
 * Sets up the newly created SPCanvasItem.
 *
 * We make it static for encapsulation reasons since it was nowhere used.
 */
void sp_canvas_item_construct(SPCanvasItem *item, SPCanvasGroup *parent, gchar const *first_arg_name, va_list args);

/**
 * Helper that returns true iff item is descendant of parent.
 */
bool is_descendant(SPCanvasItem const *item, SPCanvasItem const *parent);

guint item_signals[ITEM_LAST_SIGNAL] = { 0 };

} // namespace

G_DEFINE_TYPE(SPCanvasItem, sp_canvas_item, G_TYPE_INITIALLY_UNOWNED);

static void
sp_canvas_item_class_init(SPCanvasItemClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *) klass;

    item_signals[ITEM_EVENT] = g_signal_new ("event",
                                             G_TYPE_FROM_CLASS (klass),
                                             G_SIGNAL_RUN_LAST,
                                             ((glong)((guint8*)&(klass->event) - (guint8*)klass)),
                                             NULL, NULL,
                                             sp_marshal_BOOLEAN__POINTER,
                                             G_TYPE_BOOLEAN, 1,
                                             GDK_TYPE_EVENT);

    gobject_class->dispose  = sp_canvas_item_dispose;
    gobject_class->finalize = sp_canvas_item_finalize;
    klass->destroy          = sp_canvas_item_real_destroy;
  
    object_signals[DESTROY] =
      g_signal_new ("destroy",
                    G_TYPE_FROM_CLASS (gobject_class),
                    (GSignalFlags)(G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS),
		    G_STRUCT_OFFSET (SPCanvasItemClass, destroy),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);
}

static void
sp_canvas_item_init(SPCanvasItem *item)
{
    item->xform = Geom::Affine(Geom::identity());
    item->ctrlResize = 0;
    item->ctrlType = Inkscape::CTRL_TYPE_UNKNOWN;
    item->ctrlFlags = Inkscape::CTRL_FLAG_NORMAL;

    // TODO items should not be visible on creation - this causes kludges with items
    // that should be initially invisible; examples of such items: node handles, the CtrlRect
    // used for rubberbanding, path outline, etc.
    item->visible = TRUE;
    item->in_destruction = false;
    item->pickable = true;
}

SPCanvasItem *sp_canvas_item_new(SPCanvasGroup *parent, GType type, gchar const *first_arg_name, ...)
{
    va_list args;

    g_return_val_if_fail(parent != NULL, NULL);
    g_return_val_if_fail(SP_IS_CANVAS_GROUP(parent), NULL);
    g_return_val_if_fail(g_type_is_a(type, SP_TYPE_CANVAS_ITEM), NULL);

    SPCanvasItem *item = SP_CANVAS_ITEM(g_object_new(type, NULL));

    va_start(args, first_arg_name);
    sp_canvas_item_construct(item, parent, first_arg_name, args);
    va_end(args);

    return item;
}

namespace {

void sp_canvas_item_construct(SPCanvasItem *item, SPCanvasGroup *parent, gchar const *first_arg_name, va_list args)
{
    g_return_if_fail(SP_IS_CANVAS_GROUP(parent));
    g_return_if_fail(SP_IS_CANVAS_ITEM(item));

    item->parent = SP_CANVAS_ITEM(parent);
    item->canvas = item->parent->canvas;

    g_object_set_valist(G_OBJECT(item), first_arg_name, args);

    SP_CANVAS_GROUP(item->parent)->add(item);

    sp_canvas_item_request_update(item);
}

} // namespace

/**
 * Helper function that requests redraw only if item's visible flag is set.
 */
static void redraw_if_visible(SPCanvasItem *item)
{
    if (item->visible) {
        int x0 = (int)(item->x1);
        int x1 = (int)(item->x2);
        int y0 = (int)(item->y1);
        int y1 = (int)(item->y2);

        if (x0 !=0 || x1 !=0 || y0 !=0 || y1 !=0) {
            item->canvas->requestRedraw((int)(item->x1 - 1), (int)(item->y1 -1), (int)(item->x2 + 1), (int)(item->y2 + 1));
        }
    }
}

void sp_canvas_item_destroy(SPCanvasItem *item)
{
  g_return_if_fail(SP_IS_CANVAS_ITEM(item));
  
  if (!item->in_destruction)
    g_object_run_dispose(G_OBJECT(item));
}

namespace {
void sp_canvas_item_dispose(GObject *object)
{
    SPCanvasItem *item = SP_CANVAS_ITEM (object);

    /* guard against reinvocations during
     * destruction with the in_destruction flag.
     */
    if (!item->in_destruction)
    {
      item->in_destruction=true;

      // Hack: if this is a ctrlrect, move it to 0,0;
      // this redraws only the stroke of the rect to be deleted,
      // avoiding redraw of the entire area
      if (SP_IS_CTRLRECT(item)) {
          SP_CTRLRECT(object)->setRectangle(Geom::Rect(Geom::Point(0,0),Geom::Point(0,0)));
          SP_CTRLRECT(object)->update(item->xform, 0);
      } else {
          redraw_if_visible (item);
      }
      item->visible = FALSE;
  
      if (item == item->canvas->_current_item) {
          item->canvas->_current_item = NULL;
          item->canvas->_need_repick = TRUE;
      }
  
      if (item == item->canvas->_new_current_item) {
          item->canvas->_new_current_item = NULL;
          item->canvas->_need_repick = TRUE;
      }

      if (item == item->canvas->_grabbed_item) {
          item->canvas->_grabbed_item = NULL;

#if GTK_CHECK_VERSION(3,0,0)
          GdkDeviceManager *dm = gdk_display_get_device_manager(gdk_display_get_default());
          GdkDevice *device = gdk_device_manager_get_client_pointer(dm);
          gdk_device_ungrab(device, GDK_CURRENT_TIME);
#else
          gdk_pointer_ungrab (GDK_CURRENT_TIME);
#endif
      }

      if (item == item->canvas->_focused_item) {
          item->canvas->_focused_item = NULL;
      }
 
      if (item->parent) {
          SP_CANVAS_GROUP(item->parent)->remove(item);
      }
      
      g_signal_emit (object, object_signals[DESTROY], 0);
      item->in_destruction = false;
    }

    G_OBJECT_CLASS(sp_canvas_item_parent_class)->dispose(object);
}

void sp_canvas_item_real_destroy(SPCanvasItem *object)
{
  g_signal_handlers_destroy(object);
}
	
void sp_canvas_item_finalize(GObject *gobject)
{
  SPCanvasItem *object = SP_CANVAS_ITEM(gobject);

  if (g_object_is_floating (object))
    {
      g_warning ("A floating object was finalized. This means that someone\n"
		 "called g_object_unref() on an object that had only a floating\n"
		 "reference; the initial floating reference is not owned by anyone\n"
		 "and must be removed with g_object_ref_sink().");
    }
  
  G_OBJECT_CLASS (sp_canvas_item_parent_class)->finalize (gobject);
}
} // namespace

/**
 * Helper function to update item and its children.
 *
 * NB! affine is parent2canvas.
 */
static void sp_canvas_item_invoke_update(SPCanvasItem *item, Geom::Affine const &affine, unsigned int flags)
{
    // Apply the child item's transform
    Geom::Affine child_affine = item->xform * affine;

    // apply object flags to child flags
    int child_flags = flags & ~SP_CANVAS_UPDATE_REQUESTED;

    if (item->need_update) {
        child_flags |= SP_CANVAS_UPDATE_REQUESTED;
    }

    if (item->need_affine) {
        child_flags |= SP_CANVAS_UPDATE_AFFINE;
    }

    if (child_flags & (SP_CANVAS_UPDATE_REQUESTED | SP_CANVAS_UPDATE_AFFINE)) {
        if (SP_CANVAS_ITEM_GET_CLASS (item)->update) {
            SP_CANVAS_ITEM_GET_CLASS (item)->update(item, child_affine, child_flags);
        }
    }

    item->need_update = FALSE;
    item->need_affine = FALSE;
}

/**
 * Helper function to invoke the point method of the item.
 *
 * The argument x, y should be in the parent's item-relative coordinate
 * system.  This routine applies the inverse of the item's transform,
 * maintaining the affine invariant.
 */
static double sp_canvas_item_invoke_point(SPCanvasItem *item, Geom::Point p, SPCanvasItem **actual_item)
{
    if (SP_CANVAS_ITEM_GET_CLASS(item)->point) {
        return SP_CANVAS_ITEM_GET_CLASS (item)->point (item, p, actual_item);
    }

    return Geom::infinity();
}

/**
 * Makes the item's affine transformation matrix be equal to the specified
 * matrix.
 *
 * @item: A canvas item.
 * @affine: An affine transformation matrix.
 */
void sp_canvas_item_affine_absolute(SPCanvasItem *item, Geom::Affine const &affine)
{
    item->xform = affine;

    if (!item->need_affine) {
        item->need_affine = TRUE;
        if (item->parent != NULL) {
            sp_canvas_item_request_update (item->parent);
        } else {
            item->canvas->requestUpdate();
        }
    }

    item->canvas->_need_repick = TRUE;
}

/**
 * Raises the item in its parent's stack by the specified number of positions.
 *
 * @param item A canvas item.
 * @param positions Number of steps to raise the item.
 *
 * If the number of positions is greater than the distance to the top of the
 * stack, then the item is put at the top.
 */
void sp_canvas_item_raise(SPCanvasItem *item, int positions)
{
    g_return_if_fail (item != NULL);
    g_return_if_fail (SP_IS_CANVAS_ITEM (item));
    g_return_if_fail (positions >= 0);

    if (!item->parent || positions == 0) {
        return;
    }

    SPCanvasGroup *parent = SP_CANVAS_GROUP (item->parent);
    std::list<SPCanvasItem *>::iterator l = std::find(parent->items.begin(),parent->items.end(), item);
    g_assert (l != parent->items.end());

    for (int i=0; i<=positions && l != parent->items.end(); ++i)
        ++l;

    parent->items.remove(item);
    parent->items.insert(l, item);

    redraw_if_visible (item);
    item->canvas->_need_repick = TRUE;
}

void sp_canvas_item_raise_to_top(SPCanvasItem *item) 
{
    g_return_if_fail (item != NULL);
    g_return_if_fail (SP_IS_CANVAS_ITEM (item));
    if (!item->parent)
        return;
    SPCanvasGroup *parent = SP_CANVAS_GROUP (item->parent);
    parent->items.remove(item);
    parent->items.push_back(item);
    redraw_if_visible (item);
    item->canvas->_need_repick = TRUE;
}



/**
 * Lowers the item in its parent's stack by the specified number of positions.
 *
 * @param item A canvas item.
 * @param positions Number of steps to lower the item.
 *
 * If the number of positions is greater than the distance to the bottom of the
 * stack, then the item is put at the bottom.
 */
void sp_canvas_item_lower(SPCanvasItem *item, int positions)
{
    g_return_if_fail (item != NULL);
    g_return_if_fail (SP_IS_CANVAS_ITEM (item));
    g_return_if_fail (positions >= 1);

    SPCanvasGroup *parent = SP_CANVAS_GROUP(item->parent);

    if (!parent || positions == 0 || item == parent->items.front() ) {
        return;
    }

    std::list<SPCanvasItem *>::iterator l = std::find(parent->items.begin(), parent->items.end(), item);
    g_assert (l != parent->items.end());

    for (int i=0; i<positions && l != parent->items.begin(); ++i) 
        --l;
    
    parent->items.remove(item);
    parent->items.insert(l, item);

    redraw_if_visible (item);
    item->canvas->_need_repick = TRUE;
}

void sp_canvas_item_lower_to_bottom(SPCanvasItem *item)
{
    g_return_if_fail (item != NULL);
    g_return_if_fail (SP_IS_CANVAS_ITEM (item));
    if (!item->parent)
        return;
    SPCanvasGroup *parent = SP_CANVAS_GROUP (item->parent);
    parent->items.remove(item);
    parent->items.push_front(item);
    redraw_if_visible (item); 
    item->canvas->_need_repick = TRUE;
}

bool sp_canvas_item_is_visible(SPCanvasItem *item)
{
    return item->visible;
}

/**
 * Sets visible flag on item and requests a redraw.
 */
void sp_canvas_item_show(SPCanvasItem *item)
{
    g_return_if_fail (item != NULL);
    g_return_if_fail (SP_IS_CANVAS_ITEM (item));

    if (item->visible) {
        return;
    }

    item->visible = TRUE;

    int x0 = (int)(item->x1);
    int x1 = (int)(item->x2);
    int y0 = (int)(item->y1);
    int y1 = (int)(item->y2);

    if (x0 !=0 || x1 !=0 || y0 !=0 || y1 !=0) {
        item->canvas->requestRedraw((int)(item->x1), (int)(item->y1), (int)(item->x2 + 1), (int)(item->y2 + 1));
        item->canvas->_need_repick = TRUE;
    }
}

/**
 * Clears visible flag on item and requests a redraw.
 */
void sp_canvas_item_hide(SPCanvasItem *item)
{
    g_return_if_fail (item != NULL);
    g_return_if_fail (SP_IS_CANVAS_ITEM (item));

    if (!item->visible) {
        return;
    }

    item->visible = FALSE;

    int x0 = (int)(item->x1);
    int x1 = (int)(item->x2);
    int y0 = (int)(item->y1);
    int y1 = (int)(item->y2);

    if (x0 !=0 || x1 !=0 || y0 !=0 || y1 !=0) {
        item->canvas->requestRedraw((int)item->x1, (int)item->y1, (int)(item->x2 + 1), (int)(item->y2 + 1));
        item->canvas->_need_repick = TRUE;
    }
}

/**
 * Grab item under cursor.
 *
 * \pre !canvas->grabbed_item && item->flags & SP_CANVAS_ITEM_VISIBLE
 */
int sp_canvas_item_grab(SPCanvasItem *item, guint event_mask, GdkCursor *cursor, guint32 etime)
{
    g_return_val_if_fail (item != NULL, -1);
    g_return_val_if_fail (SP_IS_CANVAS_ITEM (item), -1);
    g_return_val_if_fail (gtk_widget_get_mapped (GTK_WIDGET (item->canvas)), -1);

    if (item->canvas->_grabbed_item) {
        return -1;
    }

    // This test disallows grabbing events by an invisible item, which may be useful
    // sometimes. An example is the hidden control point used for the selector component,
    // where it is used for object selection and rubberbanding. There seems to be nothing
    // preventing this except this test, so I removed it.
    // -- Krzysztof Kosiński, 2009.08.12
    //if (!(item->flags & SP_CANVAS_ITEM_VISIBLE))
    //    return -1;

    if (HAS_BROKEN_MOTION_HINTS) {
        event_mask &= ~GDK_POINTER_MOTION_HINT_MASK;
    }

    // fixme: Top hack (Lauris)
    // fixme: If we add key masks to event mask, Gdk will abort (Lauris)
    // fixme: But Canvas actualle does get key events, so all we need is routing these here
#if GTK_CHECK_VERSION(3,0,0)
    GdkDeviceManager *dm = gdk_display_get_device_manager(gdk_display_get_default());
    GdkDevice *device = gdk_device_manager_get_client_pointer(dm);
    gdk_device_grab(device, 
                    getWindow(item->canvas),
                    GDK_OWNERSHIP_NONE,
                    FALSE,
                    (GdkEventMask)(event_mask & (~(GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK))),
                    cursor,
                    etime);
#else
    gdk_pointer_grab( getWindow(item->canvas), FALSE,
                      (GdkEventMask)(event_mask & (~(GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK))),
                      NULL, cursor, etime);
#endif

    item->canvas->_grabbed_item = item;
    item->canvas->_grabbed_event_mask = event_mask;
    item->canvas->_current_item = item; // So that events go to the grabbed item

    return 0;
}

/**
 * Ungrabs the item, which must have been grabbed in the canvas, and ungrabs the
 * mouse.
 *
 * @param item A canvas item that holds a grab.
 * @param etime The timestamp for ungrabbing the mouse.
 */
void sp_canvas_item_ungrab(SPCanvasItem *item, guint32 etime)
{
    g_return_if_fail (item != NULL);
    g_return_if_fail (SP_IS_CANVAS_ITEM (item));

    if (item->canvas->_grabbed_item != item) {
        return;
    }

    item->canvas->_grabbed_item = NULL;

#if GTK_CHECK_VERSION(3,0,0)
    GdkDeviceManager *dm = gdk_display_get_device_manager(gdk_display_get_default());
    GdkDevice *device = gdk_device_manager_get_client_pointer(dm);
    gdk_device_ungrab(device, etime);
#else
    gdk_pointer_ungrab (etime);
#endif
}

/**
 * Returns the product of all transformation matrices from the root item down
 * to the item.
 */
Geom::Affine sp_canvas_item_i2w_affine(SPCanvasItem const *item)
{
    g_assert (SP_IS_CANVAS_ITEM (item)); // should we get this?

    Geom::Affine affine = Geom::identity();

    while (item) {
        affine *= item->xform;
        item = item->parent;
    }
    return affine;
}

namespace {

bool is_descendant(SPCanvasItem const *item, SPCanvasItem const *parent)
{
    while (item) {
        if (item == parent) {
            return true;
        }
        item = item->parent;
    }

    return false;
}

} // namespace

/**
 * Requests that the canvas queue an update for the specified item.
 *
 * To be used only by item implementations.
 */
void sp_canvas_item_request_update(SPCanvasItem *item)
{
    if (item->need_update) {
        return;
    }

    item->need_update = TRUE;

    if (item->parent != NULL) {
        // Recurse up the tree
        sp_canvas_item_request_update (item->parent);
    } else {
        // Have reached the top of the tree, make sure the update call gets scheduled.
        item->canvas->requestUpdate();
    }
}

/**
 * Returns position of item in group.
 */
gint sp_canvas_item_order (SPCanvasItem * item)
{
    SPCanvasGroup * p = SP_CANVAS_GROUP(item->parent);
    size_t index = 0;
    for (std::list<SPCanvasItem*>::const_iterator it = p->items.begin(); it != p->items.end(); ++it, ++index) {
        if ((*it) == item) {
            return index;
        }
    }

    return -1;
}

// SPCanvasGroup
G_DEFINE_TYPE(SPCanvasGroup, sp_canvas_group, SP_TYPE_CANVAS_ITEM);

static void sp_canvas_group_class_init(SPCanvasGroupClass *klass)
{
    SPCanvasItemClass *item_class = reinterpret_cast<SPCanvasItemClass *>(klass);

    item_class->destroy = SPCanvasGroup::destroy;
    item_class->update = SPCanvasGroup::update;
    item_class->render = SPCanvasGroup::render;
    item_class->point = SPCanvasGroup::point;
    item_class->viewbox_changed = SPCanvasGroup::viewboxChanged;
}

static void sp_canvas_group_init(SPCanvasGroup * group)
{
    new (&group->items) std::list<SPCanvasItem *>;
}

void SPCanvasGroup::destroy(SPCanvasItem *object)
{
    g_return_if_fail(object != NULL);
    g_return_if_fail(SP_IS_CANVAS_GROUP(object));

    SPCanvasGroup *group = SP_CANVAS_GROUP(object);

    for (std::list<SPCanvasItem *>::iterator it = group->items.begin(); it != group->items.end(); ++it) {
        sp_canvas_item_destroy(*it);
    }

    group->items.clear();
    group->items.~list(); // invoke manually

    if (SP_CANVAS_ITEM_CLASS(sp_canvas_group_parent_class)->destroy) {
        (* SP_CANVAS_ITEM_CLASS(sp_canvas_group_parent_class)->destroy)(object);
    }
}

void SPCanvasGroup::update(SPCanvasItem *item, Geom::Affine const &affine, unsigned int flags)
{
    SPCanvasGroup const *group = SP_CANVAS_GROUP(item);
    Geom::OptRect bounds;

    for (std::list<SPCanvasItem *>::const_iterator it = group->items.begin(); it != group->items.end(); ++it) {
        SPCanvasItem *i = *it;

        sp_canvas_item_invoke_update (i, affine, flags);

        if ( (i->x2 > i->x1) && (i->y2 > i->y1) ) {
            bounds.expandTo(Geom::Point(i->x1, i->y1));
            bounds.expandTo(Geom::Point(i->x2, i->y2));
        }
    }

    if (bounds) {
        item->x1 = bounds->min()[Geom::X];
        item->y1 = bounds->min()[Geom::Y];
        item->x2 = bounds->max()[Geom::X];
        item->y2 = bounds->max()[Geom::Y];
    } else {
        // FIXME ?
        item->x1 = item->x2 = item->y1 = item->y2 = 0;
    }
}

double SPCanvasGroup::point(SPCanvasItem *item, Geom::Point p, SPCanvasItem **actual_item)
{
    SPCanvasGroup const *group = SP_CANVAS_GROUP(item);
    double const x = p[Geom::X];
    double const y = p[Geom::Y];
    int x1 = (int)(x - item->canvas->_close_enough);
    int y1 = (int)(y - item->canvas->_close_enough);
    int x2 = (int)(x + item->canvas->_close_enough);
    int y2 = (int)(y + item->canvas->_close_enough);

    double best = 0.0;
    *actual_item = NULL;

    double dist = 0.0;
    for (std::list<SPCanvasItem *>::const_iterator it = group->items.begin(); it != group->items.end(); ++it) {
        SPCanvasItem *child = *it;

        if ((child->x1 <= x2) && (child->y1 <= y2) && (child->x2 >= x1) && (child->y2 >= y1)) {
            SPCanvasItem *point_item = NULL; // cater for incomplete item implementations

            int pickable;
            if (child->visible && child->pickable && SP_CANVAS_ITEM_GET_CLASS(child)->point) {
                dist = sp_canvas_item_invoke_point(child, p, &point_item);
                pickable = TRUE;
            } else {
                pickable = FALSE;
            }

            // TODO: This metric should be improved, because in case of (partly) overlapping items we will now
            // always select the last one that has been added to the group. We could instead select the one
            // of which the center is the closest, for example. One can then move to the center
            // of the item to be focused, and have that one selected. Of course this will only work if the
            // centers are not coincident, but at least it's better than what we have now.
            // See the extensive comment in Inkscape::SelTrans::_updateHandles()
            if (pickable && point_item && ((int) (dist + 0.5) <= item->canvas->_close_enough)) {
                best = dist;
                *actual_item = point_item;
            }
        }
    }

    return best;
}

void SPCanvasGroup::render(SPCanvasItem *item, SPCanvasBuf *buf)
{
    SPCanvasGroup const *group = SP_CANVAS_GROUP(item);

    for (std::list<SPCanvasItem *>::const_iterator it = group->items.begin(); it != group->items.end(); ++it) {
        SPCanvasItem *child = *it;
        if (child->visible) {
            if ((child->x1 < buf->rect.right()) &&
                (child->y1 < buf->rect.bottom()) &&
                (child->x2 > buf->rect.left()) &&
                (child->y2 > buf->rect.top())) {
                if (SP_CANVAS_ITEM_GET_CLASS(child)->render) {
                    SP_CANVAS_ITEM_GET_CLASS(child)->render(child, buf);
                }
            }
        }
    }
}

void SPCanvasGroup::viewboxChanged(SPCanvasItem *item, Geom::IntRect const &new_area)
{
    SPCanvasGroup *group = SP_CANVAS_GROUP(item);
    
    for (std::list<SPCanvasItem *>::const_iterator it = group->items.begin(); it != group->items.end(); ++it) {
        SPCanvasItem *child = *it;
        if (child->visible) {
            if (SP_CANVAS_ITEM_GET_CLASS(child)->viewbox_changed) {
                SP_CANVAS_ITEM_GET_CLASS(child)->viewbox_changed(child, new_area);
            }
        }
    }
}

void SPCanvasGroup::add(SPCanvasItem *item)
{
    g_object_ref(item);
    g_object_ref_sink(item);

    items.push_back(item);

    sp_canvas_item_request_update(item);
}

void SPCanvasGroup::remove(SPCanvasItem *item)
{
 
    g_return_if_fail(item != NULL);
    items.remove(item);

    // Unparent the child
    item->parent = NULL;
    g_object_unref(item);

}

G_DEFINE_TYPE(SPCanvas, sp_canvas, GTK_TYPE_WIDGET);

void sp_canvas_class_init(SPCanvasClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->dispose = SPCanvas::dispose;

    widget_class->realize              = SPCanvas::handle_realize;
    widget_class->unrealize            = SPCanvas::handle_unrealize;

#if GTK_CHECK_VERSION(3,0,0)
    widget_class->get_preferred_width  = SPCanvas::handle_get_preferred_width;
    widget_class->get_preferred_height = SPCanvas::handle_get_preferred_height;
    widget_class->draw                 = SPCanvas::handle_draw;
#else
    widget_class->size_request         = SPCanvas::handle_size_request;
    widget_class->expose_event         = SPCanvas::handle_expose;
#endif

    widget_class->size_allocate        = SPCanvas::handle_size_allocate;
    widget_class->button_press_event   = SPCanvas::handle_button;
    widget_class->button_release_event = SPCanvas::handle_button;
    widget_class->motion_notify_event  = SPCanvas::handle_motion;
    widget_class->scroll_event         = SPCanvas::handle_scroll;
    widget_class->key_press_event      = SPCanvas::handle_key_event;
    widget_class->key_release_event    = SPCanvas::handle_key_event;
    widget_class->enter_notify_event   = SPCanvas::handle_crossing;
    widget_class->leave_notify_event   = SPCanvas::handle_crossing;
    widget_class->focus_in_event       = SPCanvas::handle_focus_in;
    widget_class->focus_out_event      = SPCanvas::handle_focus_out;
}

static void sp_canvas_init(SPCanvas *canvas)
{
    gtk_widget_set_has_window (GTK_WIDGET (canvas), TRUE);
    gtk_widget_set_double_buffered (GTK_WIDGET (canvas), FALSE);
    gtk_widget_set_can_focus (GTK_WIDGET (canvas), TRUE);

    canvas->_pick_event.type = GDK_LEAVE_NOTIFY;
    canvas->_pick_event.crossing.x = 0;
    canvas->_pick_event.crossing.y = 0;

    // Create the root item as a special case
    canvas->_root = SP_CANVAS_ITEM(g_object_new(SP_TYPE_CANVAS_GROUP, NULL));
    canvas->_root->canvas = canvas;

    g_object_ref (canvas->_root);
    g_object_ref_sink (canvas->_root);

    canvas->_need_repick = TRUE;

    // See comment at in sp-canvas.h.
    canvas->_gen_all_enter_events = false;
    
    canvas->_drawing_disabled = false;

    canvas->_tiles=NULL;
    canvas->_tLeft=canvas->_tTop=canvas->_tRight=canvas->_tBottom=0;
    canvas->_tile_h=canvas->_tile_w=0;

    canvas->_forced_redraw_count = 0;
    canvas->_forced_redraw_limit = -1;

#if defined(HAVE_LIBLCMS1) || defined(HAVE_LIBLCMS2)
    canvas->_enable_cms_display_adj = false;
    new (&canvas->_cms_key) Glib::ustring("");
#endif // defined(HAVE_LIBLCMS1) || defined(HAVE_LIBLCMS2)

    canvas->_is_scrolling = false;
}

void SPCanvas::shutdownTransients()
{
    // We turn off the need_redraw flag, since if the canvas is mapped again
    // it will request a redraw anyways.  We do not turn off the need_update
    // flag, though, because updates are not queued when the canvas remaps
    // itself.
    //
    _need_redraw = FALSE;
    if (_tiles) g_free(_tiles);
    _tiles = NULL;
    _tLeft = _tTop = _tRight = _tBottom = 0;
    _tile_h = _tile_w = 0;

    if (_grabbed_item) {
        _grabbed_item = NULL;
#if GTK_CHECK_VERSION(3,0,0)
        GdkDeviceManager *dm = gdk_display_get_device_manager(gdk_display_get_default());
        GdkDevice *device = gdk_device_manager_get_client_pointer(dm);
        gdk_device_ungrab(device, GDK_CURRENT_TIME);
#else
        gdk_pointer_ungrab(GDK_CURRENT_TIME);
#endif
    }
    removeIdle();
}

void SPCanvas::dispose(GObject *object)
{
    SPCanvas *canvas = SP_CANVAS(object);

    if (canvas->_root) {
        g_object_unref (canvas->_root);
        canvas->_root = NULL;
    }

    canvas->shutdownTransients();
#if defined(HAVE_LIBLCMS1) || defined(HAVE_LIBLCMS2)
    canvas->_cms_key.~ustring();
#endif
    if (G_OBJECT_CLASS(sp_canvas_parent_class)->dispose) {
        (* G_OBJECT_CLASS(sp_canvas_parent_class)->dispose)(object);
    }
}

namespace {

void trackLatency(GdkEvent const *event)
{
    GdkEventLatencyTracker &tracker = GdkEventLatencyTracker::default_tracker();
    boost::optional<double> latency = tracker.process(event);
    if (latency && *latency > 2.0) {
        //g_warning("Event latency reached %f sec (%1.4f)", *latency, tracker.getSkew());
    }
}

} // namespace

GtkWidget *SPCanvas::createAA()
{
    SPCanvas *canvas = SP_CANVAS(g_object_new(SP_TYPE_CANVAS, NULL));
    return GTK_WIDGET(canvas);
}

void SPCanvas::handle_realize(GtkWidget *widget)
{
    GdkWindowAttr attributes;
    GtkAllocation allocation;
    attributes.window_type = GDK_WINDOW_CHILD;
    gtk_widget_get_allocation (widget, &allocation);
    attributes.x = allocation.x;
    attributes.y = allocation.y;
    attributes.width = allocation.width;
    attributes.height = allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gdk_visual_get_system();

#if !GTK_CHECK_VERSION(3,0,0)
    attributes.colormap = gdk_colormap_get_system();
#endif

    attributes.event_mask = (gtk_widget_get_events (widget) |
                             GDK_EXPOSURE_MASK |
                             GDK_BUTTON_PRESS_MASK |
                             GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK |
                             ( HAS_BROKEN_MOTION_HINTS ?
                               0 : GDK_POINTER_MOTION_HINT_MASK ) |
                             GDK_PROXIMITY_IN_MASK |
                             GDK_PROXIMITY_OUT_MASK |
                             GDK_KEY_PRESS_MASK |
                             GDK_KEY_RELEASE_MASK |
                             GDK_ENTER_NOTIFY_MASK |
                             GDK_LEAVE_NOTIFY_MASK |
                             GDK_SCROLL_MASK |
                             GDK_FOCUS_CHANGE_MASK);

#if GTK_CHECK_VERSION(3,0,0)
    gint attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
#else
    gint attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
#endif

    GdkWindow *window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
    gtk_widget_set_window (widget, window);
    gdk_window_set_user_data (window, widget);

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    if (prefs->getBool("/options/useextinput/value", true)) {
        gtk_widget_set_events(widget, attributes.event_mask);
#if !GTK_CHECK_VERSION(3,0,0)
        gtk_widget_set_extension_events(widget, GDK_EXTENSION_EVENTS_ALL);
        // TODO: Extension event stuff has been deprecated in GTK+ 3
#endif
    }

#if !GTK_CHECK_VERSION(3,0,0)
    // This does nothing in GTK+ 3
    GtkStyle *style = gtk_widget_get_style (widget);
    gtk_widget_set_style (widget, gtk_style_attach (style, window));
#endif

    gtk_widget_set_realized (widget, TRUE);
}

void SPCanvas::handle_unrealize(GtkWidget *widget)
{
    SPCanvas *canvas = SP_CANVAS (widget);

    canvas->_current_item = NULL;
    canvas->_grabbed_item = NULL;
    canvas->_focused_item = NULL;

    canvas->shutdownTransients();

    if (GTK_WIDGET_CLASS(sp_canvas_parent_class)->unrealize)
        (* GTK_WIDGET_CLASS(sp_canvas_parent_class)->unrealize)(widget);
}


#if GTK_CHECK_VERSION(3,0,0)
void SPCanvas::handle_get_preferred_width(GtkWidget *widget, gint *minimum_width, gint *natural_width)
{
    static_cast<void>(SP_CANVAS (widget));
    *minimum_width = 256;
    *natural_width = 256;
}

void SPCanvas::handle_get_preferred_height(GtkWidget *widget, gint *minimum_height, gint *natural_height)
{
    static_cast<void>(SP_CANVAS (widget));
    *minimum_height = 256;
    *natural_height = 256;
}
#else
void SPCanvas::handle_size_request(GtkWidget *widget, GtkRequisition *req)
{
    static_cast<void>(SP_CANVAS (widget));

    req->width = 256;
    req->height = 256;
}
#endif


void SPCanvas::handle_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    SPCanvas *canvas = SP_CANVAS (widget);
    GtkAllocation widg_allocation;
   
    gtk_widget_get_allocation (widget, &widg_allocation);

//    Geom::IntRect old_area = Geom::IntRect::from_xywh(canvas->x0, canvas->y0,
//        widg_allocation.width, widg_allocation.height);

    Geom::IntRect new_area = Geom::IntRect::from_xywh(canvas->_x0, canvas->_y0,
        allocation->width, allocation->height);

    // Schedule redraw of new region
    canvas->resizeTiles(canvas->_x0, canvas->_y0, canvas->_x0 + allocation->width, canvas->_y0 + allocation->height);
    if (SP_CANVAS_ITEM_GET_CLASS (canvas->_root)->viewbox_changed)
        SP_CANVAS_ITEM_GET_CLASS (canvas->_root)->viewbox_changed (canvas->_root, new_area);

    if (allocation->width > widg_allocation.width) {
        canvas->requestRedraw(canvas->_x0 + widg_allocation.width,
                              0,
                              canvas->_x0 + allocation->width,
                              canvas->_y0 + allocation->height);
    }
    if (allocation->height > widg_allocation.height) {
        canvas->requestRedraw(0,
                              canvas->_y0 + widg_allocation.height,
                              canvas->_x0 + allocation->width,
                              canvas->_y0 + allocation->height);
    }

    gtk_widget_set_allocation (widget, allocation);

    if (gtk_widget_get_realized (widget)) {
        gdk_window_move_resize (gtk_widget_get_window (widget),
                                allocation->x, allocation->y,
                                allocation->width, allocation->height);
    }
}

int SPCanvas::emitEvent(GdkEvent *event)
{
    guint mask;

    if (_grabbed_item) {
        switch (event->type) {
        case GDK_ENTER_NOTIFY:
            mask = GDK_ENTER_NOTIFY_MASK;
            break;
        case GDK_LEAVE_NOTIFY:
            mask = GDK_LEAVE_NOTIFY_MASK;
            break;
        case GDK_MOTION_NOTIFY:
            mask = GDK_POINTER_MOTION_MASK;
            break;
        case GDK_BUTTON_PRESS:
        case GDK_2BUTTON_PRESS:
        case GDK_3BUTTON_PRESS:
            mask = GDK_BUTTON_PRESS_MASK;
            break;
        case GDK_BUTTON_RELEASE:
            mask = GDK_BUTTON_RELEASE_MASK;
            break;
        case GDK_KEY_PRESS:
            mask = GDK_KEY_PRESS_MASK;
            break;
        case GDK_KEY_RELEASE:
            mask = GDK_KEY_RELEASE_MASK;
            break;
        case GDK_SCROLL:
            mask = GDK_SCROLL_MASK;
#if GTK_CHECK_VERSION(3,0,0)
            mask |= GDK_SMOOTH_SCROLL_MASK;
#endif
            break;
        default:
            mask = 0;
            break;
        }

        if (!(mask & _grabbed_event_mask)) return FALSE;
    }

    // Convert to world coordinates -- we have two cases because of different
    // offsets of the fields in the event structures.

    GdkEvent *ev = gdk_event_copy(event);

    switch (ev->type) {
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
        ev->crossing.x += _x0;
        ev->crossing.y += _y0;
        break;
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
        ev->motion.x += _x0;
        ev->motion.y += _y0;
        break;
    default:
        break;
    }
    // Block Undo and Redo while we drag /anything/
    if(event->type == GDK_BUTTON_PRESS && event->button.button == 1)
        _is_dragging = true;
    else if(event->type == GDK_BUTTON_RELEASE)
        _is_dragging = false;

    // Choose where we send the event

    // canvas->current_item becomes NULL in some cases under Win32
    // (e.g. if the pointer leaves the window).  So this is a hack that
    // Lauris applied to SP to get around the problem.
    //
    SPCanvasItem* item = NULL;
    if (_grabbed_item && !is_descendant(_current_item, _grabbed_item)) {
        item = _grabbed_item;
    } else {
        item = _current_item;
    }

    if (_focused_item &&
        ((event->type == GDK_KEY_PRESS) ||
         (event->type == GDK_KEY_RELEASE) ||
         (event->type == GDK_FOCUS_CHANGE))) {
        item = _focused_item;
    }

    // The event is propagated up the hierarchy (for if someone connected to
    // a group instead of a leaf event), and emission is stopped if a
    // handler returns TRUE, just like for GtkWidget events.

    gint finished = FALSE;

    while (item && !finished) {
        g_object_ref (item);
        g_signal_emit (G_OBJECT (item), item_signals[ITEM_EVENT], 0, ev, &finished);
        SPCanvasItem *parent = item->parent;
        g_object_unref (item);
        item = parent;
    }

    gdk_event_free(ev);

    return finished;
}

int SPCanvas::pickCurrentItem(GdkEvent *event)
{
    int button_down = 0;

    if (!_root) // canvas may have already be destroyed by closing desktop during interrupted display!
        return FALSE;

    int retval = FALSE;

    if (_gen_all_enter_events == false) {
        // If a button is down, we'll perform enter and leave events on the
        // current item, but not enter on any other item.  This is more or
        // less like X pointer grabbing for canvas items.
        //
        button_down = _state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK |
                GDK_BUTTON3_MASK | GDK_BUTTON4_MASK | GDK_BUTTON5_MASK);

        if (!button_down) _left_grabbed_item = FALSE;
    }

    // Save the event in the canvas.  This is used to synthesize enter and
    // leave events in case the current item changes.  It is also used to
    // re-pick the current item if the current one gets deleted.  Also,
    // synthesize an enter event.

    if (event != &_pick_event) {
        if ((event->type == GDK_MOTION_NOTIFY) || (event->type == GDK_BUTTON_RELEASE)) {
            // these fields have the same offsets in both types of events

            _pick_event.crossing.type       = GDK_ENTER_NOTIFY;
            _pick_event.crossing.window     = event->motion.window;
            _pick_event.crossing.send_event = event->motion.send_event;
            _pick_event.crossing.subwindow  = NULL;
            _pick_event.crossing.x          = event->motion.x;
            _pick_event.crossing.y          = event->motion.y;
            _pick_event.crossing.mode       = GDK_CROSSING_NORMAL;
            _pick_event.crossing.detail     = GDK_NOTIFY_NONLINEAR;
            _pick_event.crossing.focus      = FALSE;
            _pick_event.crossing.state      = event->motion.state;

            // these fields don't have the same offsets in both types of events

            if (event->type == GDK_MOTION_NOTIFY) {
                _pick_event.crossing.x_root = event->motion.x_root;
                _pick_event.crossing.y_root = event->motion.y_root;
            } else {
                _pick_event.crossing.x_root = event->button.x_root;
                _pick_event.crossing.y_root = event->button.y_root;
            }
        } else {
            _pick_event = *event;
        }
    }

    // Don't do anything else if this is a recursive call
    if (_in_repick) {
        return retval;
    }

    // LeaveNotify means that there is no current item, so we don't look for one
    if (_pick_event.type != GDK_LEAVE_NOTIFY) {
        // these fields don't have the same offsets in both types of events
        double x, y;

        if (_pick_event.type == GDK_ENTER_NOTIFY) {
            x = _pick_event.crossing.x;
            y = _pick_event.crossing.y;
        } else {
            x = _pick_event.motion.x;
            y = _pick_event.motion.y;
        }

        // world coords
        x += _x0;
        y += _y0;

        // find the closest item
        if (_root->visible) {
            sp_canvas_item_invoke_point (_root, Geom::Point(x, y), &_new_current_item);
        } else {
            _new_current_item = NULL;
        }
    } else {
        _new_current_item = NULL;
    }

    if ((_new_current_item == _current_item) && !_left_grabbed_item) {
        return retval; // current item did not change
    }

    // Synthesize events for old and new current items

    if ((_new_current_item != _current_item) &&
        _current_item != NULL && !_left_grabbed_item)
    {
        GdkEvent new_event;

        new_event = _pick_event;
        new_event.type = GDK_LEAVE_NOTIFY;

        new_event.crossing.detail = GDK_NOTIFY_ANCESTOR;
        new_event.crossing.subwindow = NULL;
        _in_repick = TRUE;
        retval = emitEvent(&new_event);
        _in_repick = FALSE;
    }

    if (_gen_all_enter_events == false) {
        // new_current_item may have been set to NULL during the call to
        // emitEvent() above
        if ((_new_current_item != _current_item) && button_down) {
            _left_grabbed_item = TRUE;
            return retval;
        }
    }

    // Handle the rest of cases
    _left_grabbed_item = FALSE;
    _current_item = _new_current_item;

    if (_current_item != NULL) {
        GdkEvent new_event;

        new_event = _pick_event;
        new_event.type = GDK_ENTER_NOTIFY;
        new_event.crossing.detail = GDK_NOTIFY_ANCESTOR;
        new_event.crossing.subwindow = NULL;
        retval = emitEvent(&new_event);
    }

    return retval;
}

gint SPCanvas::handle_button(GtkWidget *widget, GdkEventButton *event)
{
    SPCanvas *canvas = SP_CANVAS (widget);

    int retval = FALSE;

    // dispatch normally regardless of the event's window if an item
    // has a pointer grab in effect
    if (!canvas->_grabbed_item &&
        event->window != getWindow(canvas))
        return retval;

    int mask;
    switch (event->button) {
    case 1:
        mask = GDK_BUTTON1_MASK;
        break;
    case 2:
        mask = GDK_BUTTON2_MASK;
        break;
    case 3:
        mask = GDK_BUTTON3_MASK;
        break;
    case 4:
        mask = GDK_BUTTON4_MASK;
        break;
    case 5:
        mask = GDK_BUTTON5_MASK;
        break;
    default:
        mask = 0;
    }

    switch (event->type) {
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
        // Pick the current item as if the button were not pressed, and
        // then process the event.
        //
        canvas->_state = event->state;
        canvas->pickCurrentItem(reinterpret_cast<GdkEvent *>(event));
        canvas->_state ^= mask;
        retval = canvas->emitEvent((GdkEvent *) event);
        break;

    case GDK_BUTTON_RELEASE:
        // Process the event as if the button were pressed, then repick
        // after the button has been released
        //
        canvas->_state = event->state;
        retval = canvas->emitEvent((GdkEvent *) event);
        event->state ^= mask;
        canvas->_state = event->state;
        canvas->pickCurrentItem(reinterpret_cast<GdkEvent *>(event));
        event->state ^= mask;

        break;

    default:
        g_assert_not_reached ();
    }

    return retval;
}

gint SPCanvas::handle_scroll(GtkWidget *widget, GdkEventScroll *event)
{
    return SP_CANVAS(widget)->emitEvent(reinterpret_cast<GdkEvent *>(event));
}

static inline void request_motions(GdkWindow *w, GdkEventMotion *event) {
#if GTK_CHECK_VERSION(3,0,0)
    gdk_window_get_device_position(w,
                                   gdk_event_get_device((GdkEvent *)(event)),
                                   NULL, NULL, NULL);
#else
    gdk_window_get_pointer(w, NULL, NULL, NULL);
#endif
    gdk_event_request_motions(event);
}

int SPCanvas::handle_motion(GtkWidget *widget, GdkEventMotion *event)
{
    int status;
    SPCanvas *canvas = SP_CANVAS (widget);

    trackLatency((GdkEvent *)event);

    if (event->window != getWindow(canvas)) {
        return FALSE;
    }

    if (canvas->_root == NULL) // canvas being deleted
        return FALSE;

    canvas->_state = event->state;
    canvas->pickCurrentItem(reinterpret_cast<GdkEvent *>(event));
    status = canvas->emitEvent(reinterpret_cast<GdkEvent *>(event));
    if (event->is_hint) {
        request_motions(gtk_widget_get_window (widget), event);
    }

    return status;
}

void SPCanvas::paintSingleBuffer(Geom::IntRect const &paint_rect, Geom::IntRect const &canvas_rect, int /*sw*/)
{
    GtkWidget *widget = GTK_WIDGET (this);

    // Mark the region clean
    markRect(paint_rect, 0);

    SPCanvasBuf buf;
    buf.buf = NULL;
    buf.buf_rowstride = 0;
    buf.rect = paint_rect;
    buf.visible_rect = canvas_rect;
    buf.is_empty = true;
    //buf.ct = gdk_cairo_create(widget->window);

    // create temporary surface
    cairo_surface_t *imgs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, paint_rect.width(), paint_rect.height());
    buf.ct = cairo_create(imgs);
    //cairo_translate(buf.ct, -x0, -y0);

    // fix coordinates, clip all drawing to the tile and clear the background
    //cairo_translate(buf.ct, paint_rect.left() - canvas->x0, paint_rect.top() - canvas->y0);
    //cairo_rectangle(buf.ct, 0, 0, paint_rect.width(), paint_rect.height());
    //cairo_set_line_width(buf.ct, 3);
    //cairo_set_source_rgba(buf.ct, 1.0, 0.0, 0.0, 0.1);
    //cairo_stroke_preserve(buf.ct);
    //cairo_clip(buf.ct);

#if GTK_CHECK_VERSION(3,0,0)
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    GdkRGBA color;
    gtk_style_context_get_background_color(context,
                                           gtk_widget_get_state_flags(widget),
                                           &color);
    gdk_cairo_set_source_rgba(buf.ct, &color);
#else
    GtkStyle *style = gtk_widget_get_style (widget);
    gdk_cairo_set_source_color(buf.ct, &style->bg[GTK_STATE_NORMAL]);
#endif

    cairo_set_operator(buf.ct, CAIRO_OPERATOR_SOURCE);
    //cairo_rectangle(buf.ct, 0, 0, paint_rect.width(), paint_rec.height());
    cairo_paint(buf.ct);
    cairo_set_operator(buf.ct, CAIRO_OPERATOR_OVER);

    if (_root->visible) {
        SP_CANVAS_ITEM_GET_CLASS(_root)->render(_root, &buf);
    }

    // output to X
    cairo_destroy(buf.ct);

#if defined(HAVE_LIBLCMS1) || defined(HAVE_LIBLCMS2)
    if (_enable_cms_display_adj) {
        cmsHTRANSFORM transf = 0;
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        bool fromDisplay = prefs->getBool( "/options/displayprofile/from_display");
        if ( fromDisplay ) {
            transf = Inkscape::CMSSystem::getDisplayPer(_cms_key);
        } else {
            transf = Inkscape::CMSSystem::getDisplayTransform();
        }
        
        if (transf) {
            cairo_surface_flush(imgs);
            unsigned char *px = cairo_image_surface_get_data(imgs);
            int stride = cairo_image_surface_get_stride(imgs);
            for (int i=0; i<paint_rect.height(); ++i) {
                unsigned char *row = px + i*stride;
                Inkscape::CMSSystem::doTransform(transf, row, row, paint_rect.width());
            }
            cairo_surface_mark_dirty(imgs);
        }
    }
#endif // defined(HAVE_LIBLCMS1) || defined(HAVE_LIBLCMS2)

    cairo_t *xct = gdk_cairo_create(gtk_widget_get_window (widget));
    cairo_translate(xct, paint_rect.left() - _x0, paint_rect.top() - _y0);
    cairo_rectangle(xct, 0, 0, paint_rect.width(), paint_rect.height());
    cairo_clip(xct);
    cairo_set_source_surface(xct, imgs, 0, 0);
    cairo_set_operator(xct, CAIRO_OPERATOR_SOURCE);
    cairo_paint(xct);
    cairo_destroy(xct);
    cairo_surface_destroy(imgs);
}

struct PaintRectSetup {
    Geom::IntRect big_rect;
    GTimeVal start_time;
    int max_pixels;
    Geom::Point mouse_loc;
};

int SPCanvas::paintRectInternal(PaintRectSetup const *setup, Geom::IntRect const &this_rect)
{
    GTimeVal now;
    g_get_current_time (&now);

    glong elapsed = (now.tv_sec - setup->start_time.tv_sec) * 1000000
        + (now.tv_usec - setup->start_time.tv_usec);

    // Allow only very fast buffers to be run together;
    // as soon as the total redraw time exceeds 1ms, cancel;
    // this returns control to the idle loop and allows Inkscape to process user input
    // (potentially interrupting the redraw); as soon as Inkscape has some more idle time,
    // it will get back and finish painting what remains to paint.
    if (elapsed > 1000) {

        // Interrupting redraw isn't always good.
        // For example, when you drag one node of a big path, only the buffer containing
        // the mouse cursor will be redrawn again and again, and the rest of the path
        // will remain stale because Inkscape never has enough idle time to redraw all
        // of the screen. To work around this, such operations set a forced_redraw_limit > 0.
        // If this limit is set, and if we have aborted redraw more times than is allowed,
        // interrupting is blocked and we're forced to redraw full screen once
        // (after which we can again interrupt forced_redraw_limit times).
        if (_forced_redraw_limit < 0 ||
            _forced_redraw_count < _forced_redraw_limit) {

            if (_forced_redraw_limit != -1) {
                _forced_redraw_count++;
            }

            return false;
        }
    }

    // Find the optimal buffer dimensions
    int bw = this_rect.width();
    int bh = this_rect.height();
    if ((bw < 1) || (bh < 1))
        return 0;

    if (bw * bh < setup->max_pixels) {
        // We are small enough
        /*
        GdkRectangle r;
        r.x = this_rect.x0 - setup->canvas->x0;
        r.y = this_rect.y0 - setup->canvas->y0;
        r.width = this_rect.x1 - this_rect.x0;
        r.height = this_rect.y1 - this_rect.y0;

        GdkWindow *window = gtk_widget_get_window(GTK_WIDGET(setup->canvas));
        gdk_window_begin_paint_rect(window, &r);
        */

        paintSingleBuffer(this_rect, setup->big_rect, bw);
        //gdk_window_end_paint(window);
        return 1;
    }

    Geom::IntRect lo, hi;

/*
This test determines the redraw strategy:

bw < bh (strips mode) splits across the smaller dimension of the rect and therefore (on
horizontally-stretched windows) results in redrawing in horizontal strips (from cursor point, in
both directions if the cursor is in the middle). This is traditional for Inkscape since old days,
and seems to be faster for drawings with many smaller objects at zoom-out.

bw > bh (chunks mode) splits across the larger dimension of the rect and therefore paints in
almost-square chunks, again from the cursor point. It's sometimes faster for drawings with few slow
(e.g. blurred) objects crossing the entire screen. It also appears to be somewhat psychologically
faster.

The default for now is the strips mode.
*/
    if (bw < bh || bh < 2 * TILE_SIZE) {
        int mid = this_rect[Geom::X].middle();
        // Make sure that mid lies on a tile boundary
        mid = (mid / TILE_SIZE) * TILE_SIZE;

        lo = Geom::IntRect(this_rect.left(), this_rect.top(), mid, this_rect.bottom());
        hi = Geom::IntRect(mid, this_rect.top(), this_rect.right(), this_rect.bottom());

        if (setup->mouse_loc[Geom::X] < mid) {
            // Always paint towards the mouse first
            return paintRectInternal(setup, lo)
                && paintRectInternal(setup, hi);
        } else {
            return paintRectInternal(setup, hi)
                && paintRectInternal(setup, lo);
        }
    } else {
        int mid = this_rect[Geom::Y].middle();
        // Make sure that mid lies on a tile boundary
        mid = (mid / TILE_SIZE) * TILE_SIZE;

        lo = Geom::IntRect(this_rect.left(), this_rect.top(), this_rect.right(), mid);
        hi = Geom::IntRect(this_rect.left(), mid, this_rect.right(), this_rect.bottom());

        if (setup->mouse_loc[Geom::Y] < mid) {
            // Always paint towards the mouse first
            return paintRectInternal(setup, lo)
                && paintRectInternal(setup, hi);
        } else {
            return paintRectInternal(setup, hi)
                && paintRectInternal(setup, lo);
        }
    }
}


bool SPCanvas::paintRect(int xx0, int yy0, int xx1, int yy1)
{
    GtkAllocation allocation;
    g_return_val_if_fail (!_need_update, false);

    gtk_widget_get_allocation(GTK_WIDGET(this), &allocation);

    Geom::IntRect canvas_rect = Geom::IntRect::from_xywh(_x0, _y0,
        allocation.width, allocation.height);
    Geom::IntRect paint_rect(xx0, yy0, xx1, yy1);

    Geom::OptIntRect area = paint_rect & canvas_rect;
    if (!area || area->hasZeroArea()) return 0;

    paint_rect = *area;

    PaintRectSetup setup;
    setup.big_rect = paint_rect;

    // Save the mouse location
    gint x, y;

#if GTK_CHECK_VERSION(3,0,0)
    GdkDeviceManager *dm = gdk_display_get_device_manager(gdk_display_get_default());
    GdkDevice *device = gdk_device_manager_get_client_pointer(dm);

    gdk_window_get_device_position(gtk_widget_get_window(GTK_WIDGET(this)),
                                   device,
                                   &x, &y, NULL);
#else
    gdk_window_get_pointer(gtk_widget_get_window(GTK_WIDGET(this)), &x, &y, NULL);
#endif

    setup.mouse_loc = sp_canvas_window_to_world(this, Geom::Point(x,y));

    if (_rendermode != Inkscape::RENDERMODE_OUTLINE) {
        // use 256K as a compromise to not slow down gradients
        // 256K is the cached buffer and we need 4 channels
        setup.max_pixels = 65536; // 256K/4
    } else {
        // paths only, so 1M works faster
        // 1M is the cached buffer and we need 4 channels
        setup.max_pixels = 262144;
    }

    // Start the clock
    g_get_current_time(&(setup.start_time));

    // Go
    return paintRectInternal(&setup, paint_rect);
}

void SPCanvas::forceFullRedrawAfterInterruptions(unsigned int count)
{
    _forced_redraw_limit = count;
    _forced_redraw_count = 0;
}

void SPCanvas::endForcedFullRedraws()
{
    _forced_redraw_limit = -1;
}

#if GTK_CHECK_VERSION(3,0,0)
gboolean SPCanvas::handle_draw(GtkWidget *widget, cairo_t *cr) {
    SPCanvas *canvas = SP_CANVAS(widget);

    cairo_rectangle_list_t *rects = cairo_copy_clip_rectangle_list(cr);

    for (int i = 0; i < rects->num_rectangles; i++) {
        cairo_rectangle_t rectangle = rects->rectangles[i];

        Geom::IntRect r = Geom::IntRect::from_xywh(rectangle.x + canvas->_x0, rectangle.y + canvas->_y0,
                                                   rectangle.width, rectangle.height);

        canvas->requestRedraw(r.left(), r.top(), r.right(), r.bottom());
    }

    cairo_rectangle_list_destroy(rects);

	return FALSE;
}
#else
gboolean SPCanvas::handle_expose(GtkWidget *widget, GdkEventExpose *event)
{
    SPCanvas *canvas = SP_CANVAS(widget);

    if (!gtk_widget_is_drawable (widget) ||
        (event->window != getWindow(canvas))) {
        return FALSE;
    }

    int n_rects = 0;
    GdkRectangle *rects = NULL;
    gdk_region_get_rectangles(event->region, &rects, &n_rects);

    if(rects == NULL)
        return FALSE;
    
    for (int i = 0; i < n_rects; i++) {
        GdkRectangle rectangle = rects[i];

        Geom::IntRect r = Geom::IntRect::from_xywh(rectangle.x + canvas->_x0, rectangle.y + canvas->_y0,
                                                   rectangle.width, rectangle.height);
            
        canvas->requestRedraw(r.left(), r.top(), r.right(), r.bottom());
    }

    return FALSE;
}
#endif


gint SPCanvas::handle_key_event(GtkWidget *widget, GdkEventKey *event)
{
    return SP_CANVAS(widget)->emitEvent(reinterpret_cast<GdkEvent *>(event));
}

gint SPCanvas::handle_crossing(GtkWidget *widget, GdkEventCrossing *event)
{
    SPCanvas *canvas = SP_CANVAS (widget);

    if (event->window != getWindow(canvas)) {
        return FALSE;
    }

    canvas->_state = event->state;
    return canvas->pickCurrentItem(reinterpret_cast<GdkEvent *>(event));
}

gint SPCanvas::handle_focus_in(GtkWidget *widget, GdkEventFocus *event)
{
    gtk_widget_grab_focus (widget);

    SPCanvas *canvas = SP_CANVAS (widget);

    if (canvas->_focused_item) {
        return canvas->emitEvent(reinterpret_cast<GdkEvent *>(event));
    } else {
        return FALSE;
    }
}

gint SPCanvas::handle_focus_out(GtkWidget *widget, GdkEventFocus *event)
{
    SPCanvas *canvas = SP_CANVAS(widget);

    if (canvas->_focused_item) {
        return canvas->emitEvent(reinterpret_cast<GdkEvent *>(event));
    } else {
        return FALSE;
    }
}

int SPCanvas::paint()
{
    if (_need_update) {
        sp_canvas_item_invoke_update(_root, Geom::identity(), 0);
        _need_update = FALSE;
    }

    if (!_need_redraw) {
        return TRUE;
    }

    Cairo::RefPtr<Cairo::Region> to_paint = Cairo::Region::create();

    for (int j = _tTop; j < _tBottom; ++j) {
        for (int i = _tLeft; i < _tRight; ++i) {
            int tile_index = (i - _tLeft) + (j - _tTop) * _tile_h;

            if (_tiles[tile_index]) { // if this tile is dirtied (nonzero)
                Cairo::RectangleInt rect = {i*TILE_SIZE, j*TILE_SIZE,
                                   TILE_SIZE, TILE_SIZE};
                to_paint->do_union(rect);
            }
        }
    }

    int n_rect = to_paint->get_num_rectangles();

    if (n_rect > 0) {
        for (int i=0; i < n_rect; i++) {
            Cairo::RectangleInt rect = to_paint->get_rectangle(i);
            int x0 = rect.x;
            int y0 = rect.y;
            int x1 = x0 + rect.width;
            int y1 = y0 + rect.height;
            if (!paintRect(x0, y0, x1, y1)) {
                // Aborted
                return FALSE;
            };
        }
    }

    _need_redraw = FALSE;

    // we've had a full unaborted redraw, reset the full redraw counter
    if (_forced_redraw_limit != -1) {
        _forced_redraw_count = 0;
    }

    return TRUE;
}

int SPCanvas::doUpdate()
{
    if (!_root) { // canvas may have already be destroyed by closing desktop during interrupted display!
        return TRUE;
    }
    if (_drawing_disabled) {
        return TRUE;
    }

    // Cause the update if necessary
    if (_need_update) {
        sp_canvas_item_invoke_update(_root, Geom::identity(), 0);
        _need_update = FALSE;
    }

    // Paint if able to
    if (gtk_widget_is_drawable(GTK_WIDGET(this))) {
        return paint();
    }

    // Pick new current item
    while (_need_repick) {
        _need_repick = FALSE;
        pickCurrentItem(&_pick_event);
    }

    return TRUE;
}

gint SPCanvas::idle_handler(gpointer data)
{
    SPCanvas *canvas = SP_CANVAS (data);
    int const ret = canvas->doUpdate();
    if (ret) {
        // Reset idle id
        canvas->_idle_id = 0;
    }
    return !ret;
}

void SPCanvas::addIdle()
{
    if (_idle_id == 0) {
        _idle_id = gdk_threads_add_idle_full(UPDATE_PRIORITY, idle_handler, this, NULL);
    }
}
void SPCanvas::removeIdle()
{
    if (_idle_id) {
        g_source_remove(_idle_id);
        _idle_id = 0;
    }
}

SPCanvasGroup *SPCanvas::getRoot()
{
    return SP_CANVAS_GROUP(_root);
}

void SPCanvas::scrollTo(double cx, double cy, unsigned int clear, bool is_scrolling)
{
    GtkAllocation allocation;

    int ix = (int) round(cx); // ix and iy are the new canvas coordinates (integer screen pixels)
    int iy = (int) round(cy); // cx might be negative, so (int)(cx + 0.5) will not do!
    int dx = ix - _x0; // dx and dy specify the displacement (scroll) of the
    int dy = iy - _y0; // canvas w.r.t its previous position

    Geom::IntRect old_area = getViewboxIntegers();
    Geom::IntRect new_area = old_area + Geom::IntPoint(dx, dy);
    
    _dx0 = cx; // here the 'd' stands for double, not delta!
    _dy0 = cy;
    _x0 = ix;
    _y0 = iy;

    gtk_widget_get_allocation(&_widget, &allocation);

    resizeTiles(_x0, _y0, _x0 + allocation.width, _y0 + allocation.height);
    if (SP_CANVAS_ITEM_GET_CLASS(_root)->viewbox_changed) {
        SP_CANVAS_ITEM_GET_CLASS(_root)->viewbox_changed(_root, new_area);
    }

    if (!clear) {
        // scrolling without zoom; redraw only the newly exposed areas
        if ((dx != 0) || (dy != 0)) {
            this->_is_scrolling = is_scrolling;
            if (gtk_widget_get_realized(GTK_WIDGET(this))) {
                gdk_window_scroll(getWindow(this), -dx, -dy);
            }
        }
    } else {
        // scrolling as part of zoom; do nothing here - the next do_update will perform full redraw
    }
}

void SPCanvas::updateNow()
{
    if (_need_update || _need_redraw) {
        doUpdate();
    }
}

void SPCanvas::requestUpdate()
{
    _need_update = TRUE;
    addIdle();
}

void SPCanvas::requestRedraw(int x0, int y0, int x1, int y1)
{
    GtkAllocation allocation;

    if (!gtk_widget_is_drawable( GTK_WIDGET(this) )) {
        return;
    }
    if ((x0 >= x1) || (y0 >= y1)) {
        return;
    }

    Geom::IntRect bbox(x0, y0, x1, y1);
    gtk_widget_get_allocation(GTK_WIDGET(this), &allocation);

    Geom::IntRect canvas_rect = Geom::IntRect::from_xywh(this->_x0, this->_y0,
                                                         allocation.width, allocation.height);
    
    Geom::OptIntRect clip = bbox & canvas_rect;
    if (clip) {
        dirtyRect(*clip);
        addIdle();
    }
}

/**
 * Sets world coordinates from win and canvas.
 */
void sp_canvas_window_to_world(SPCanvas const *canvas, double winx, double winy, double *worldx, double *worldy)
{
    g_return_if_fail (canvas != NULL);
    g_return_if_fail (SP_IS_CANVAS (canvas));

    if (worldx) *worldx = canvas->_x0 + winx;
    if (worldy) *worldy = canvas->_y0 + winy;
}

/**
 * Sets win coordinates from world and canvas.
 */
void sp_canvas_world_to_window(SPCanvas const *canvas, double worldx, double worldy, double *winx, double *winy)
{
    g_return_if_fail (canvas != NULL);
    g_return_if_fail (SP_IS_CANVAS (canvas));

    if (winx) *winx = worldx - canvas->_x0;
    if (winy) *winy = worldy - canvas->_y0;
}

/**
 * Converts point from win to world coordinates.
 */
Geom::Point sp_canvas_window_to_world(SPCanvas const *canvas, Geom::Point const win)
{
    g_assert (canvas != NULL);
    g_assert (SP_IS_CANVAS (canvas));

    return Geom::Point(canvas->_x0 + win[0], canvas->_y0 + win[1]);
}

/**
 * Converts point from world to win coordinates.
 */
Geom::Point sp_canvas_world_to_window(SPCanvas const *canvas, Geom::Point const world)
{
    g_assert (canvas != NULL);
    g_assert (SP_IS_CANVAS (canvas));

    return Geom::Point(world[0] - canvas->_x0, world[1] - canvas->_y0);
}

/**
 * Returns true if point given in world coordinates is inside window.
 */
bool sp_canvas_world_pt_inside_window(SPCanvas const *canvas, Geom::Point const &world)
{
    GtkAllocation allocation;
    
    g_assert( canvas != NULL );
    g_assert(SP_IS_CANVAS(canvas));

    GtkWidget *w = GTK_WIDGET(canvas);
    gtk_widget_get_allocation (w, &allocation);

    return ( ( canvas->_x0 <= world[Geom::X] )  &&
             ( canvas->_y0 <= world[Geom::Y] )  &&
             ( world[Geom::X] < canvas->_x0 + allocation.width )  &&
             ( world[Geom::Y] < canvas->_y0 + allocation.height ) );
}

/**
 * Return canvas window coordinates as Geom::Rect.
 */
Geom::Rect SPCanvas::getViewbox() const
{
    GtkAllocation allocation;

    gtk_widget_get_allocation (GTK_WIDGET (this), &allocation);
    return Geom::Rect(Geom::Point(_dx0, _dy0),
                      Geom::Point(_dx0 + allocation.width, _dy0 + allocation.height));
}

/**
 * Return canvas window coordinates as integer rectangle.
 */
Geom::IntRect SPCanvas::getViewboxIntegers() const
{
    GtkAllocation allocation;

    gtk_widget_get_allocation (GTK_WIDGET(this), &allocation);
    Geom::IntRect ret;
    ret.setMin(Geom::IntPoint(_x0, _y0));
    ret.setMax(Geom::IntPoint(_x0 + allocation.width, _y0 + allocation.height));
    return ret;
}

inline int sp_canvas_tile_floor(int x)
{
    return (x & (~(TILE_SIZE - 1))) / TILE_SIZE;
}

inline int sp_canvas_tile_ceil(int x)
{
    return ((x + (TILE_SIZE - 1)) & (~(TILE_SIZE - 1))) / TILE_SIZE;
}

void SPCanvas::resizeTiles(int nl, int nt, int nr, int nb)
{
    if ( nl >= nr || nt >= nb ) {
        if (_tiles) g_free(_tiles);
        _tLeft = _tTop = _tRight = _tBottom = 0;
        _tile_h = _tile_w = 0;
        _tiles = NULL;
        return;
    }
    int tl = sp_canvas_tile_floor(nl);
    int tt = sp_canvas_tile_floor(nt);
    int tr = sp_canvas_tile_ceil(nr);
    int tb = sp_canvas_tile_ceil(nb);

    int nh = tr-tl, nv = tb-tt;
    uint8_t *ntiles = (uint8_t*) g_malloc(nh * nv * sizeof(uint8_t));
    for (int i = tl; i < tr; i++) {
        for (int j = tt; j < tb; j++) {
            int ind = (i-tl) + (j-tt)*nh;
            if ( i >= _tLeft && i < _tRight && j >= _tTop && j < _tBottom ) {
                ntiles[ind] = _tiles[(i - _tLeft) + (j - _tTop) * _tile_h]; // copy from the old tile
            } else {
                ntiles[ind] = 0; // newly exposed areas get 0
            }
        }
    }
    if (_tiles) g_free(_tiles);
    _tiles = ntiles;
    _tLeft = tl;
    _tTop = tt;
    _tRight = tr;
    _tBottom = tb;
    _tile_h = nh;
    _tile_w = nv;
}

void SPCanvas::dirtyRect(Geom::IntRect const &area) {
    _need_redraw = TRUE;
    markRect(area, 1);
}

void SPCanvas::markRect(Geom::IntRect const &area, uint8_t val)
{
    int tl = sp_canvas_tile_floor(area.left());
    int tt = sp_canvas_tile_floor(area.top());
    int tr = sp_canvas_tile_ceil(area.right());
    int tb = sp_canvas_tile_ceil(area.bottom());
    if ( tl >= _tRight || tr <= _tLeft || tt >= _tBottom || tb <= _tTop ) return;
    if ( tl < _tLeft ) tl = _tLeft;
    if ( tr > _tRight ) tr = _tRight;
    if ( tt < _tTop ) tt = _tTop;
    if ( tb > _tBottom ) tb = _tBottom;

    for (int i=tl; i<tr; i++) {
        for (int j=tt; j<tb; j++) {
            _tiles[(i - _tLeft) + (j - _tTop) * _tile_h] = val;
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
