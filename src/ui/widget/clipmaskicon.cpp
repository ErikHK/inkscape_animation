/*
 * Authors:
 *   Theodore Janeczko
 *
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ui/widget/clipmaskicon.h"

#include <gtkmm/icontheme.h>

#include "widgets/icon.h"
#include "widgets/toolbox.h"
#include "ui/icon-names.h"
#include "layertypeicon.h"

namespace Inkscape {
namespace UI {
namespace Widget {

ClipMaskIcon::ClipMaskIcon() :
    Glib::ObjectBase(typeid(ClipMaskIcon)),
    Gtk::CellRendererPixbuf(),
    _pixClipName(INKSCAPE_ICON("object-clipped")),
    _pixMaskName(INKSCAPE_ICON("object-masked")),
    _pixBothName(INKSCAPE_ICON("object-clip-mask")),
    _property_active(*this, "active", 0),
    _property_pixbuf_clip(*this, "pixbuf_on", Glib::RefPtr<Gdk::Pixbuf>(0)),
    _property_pixbuf_mask(*this, "pixbuf_off", Glib::RefPtr<Gdk::Pixbuf>(0)),
    _property_pixbuf_both(*this, "pixbuf_on", Glib::RefPtr<Gdk::Pixbuf>(0))
{
    
    property_mode() = Gtk::CELL_RENDERER_MODE_ACTIVATABLE;
    phys = sp_icon_get_phys_size((int)Inkscape::ICON_SIZE_DECORATION);
    Glib::RefPtr<Gtk::IconTheme> icon_theme = Gtk::IconTheme::get_default();

    if (!icon_theme->has_icon(_pixClipName)) {
        Inkscape::queueIconPrerender( INKSCAPE_ICON(_pixClipName.data()), Inkscape::ICON_SIZE_DECORATION );
    }
    if (!icon_theme->has_icon(_pixMaskName)) {
        Inkscape::queueIconPrerender( INKSCAPE_ICON(_pixMaskName.data()), Inkscape::ICON_SIZE_DECORATION );
    }
    if (!icon_theme->has_icon(_pixBothName)) {
        Inkscape::queueIconPrerender( INKSCAPE_ICON(_pixBothName.data()), Inkscape::ICON_SIZE_DECORATION );
    }

    if (icon_theme->has_icon(_pixClipName)) {
        _property_pixbuf_clip = icon_theme->load_icon(_pixClipName, phys, (Gtk::IconLookupFlags)0);
    }
    if (icon_theme->has_icon(_pixMaskName)) {
        _property_pixbuf_mask = icon_theme->load_icon(_pixMaskName, phys, (Gtk::IconLookupFlags)0);
    }
    if (icon_theme->has_icon(_pixBothName)) {
        _property_pixbuf_both = icon_theme->load_icon(_pixBothName, phys, (Gtk::IconLookupFlags)0);
    }

    property_pixbuf() = Glib::RefPtr<Gdk::Pixbuf>(0);
}


#if WITH_GTKMM_3_0
void ClipMaskIcon::get_preferred_height_vfunc(Gtk::Widget& widget,
                                              int& min_h,
                                              int& nat_h) const
{
    Gtk::CellRendererPixbuf::get_preferred_height_vfunc(widget, min_h, nat_h);

    if (min_h) {
        min_h += (min_h) >> 1;
    }
    
    if (nat_h) {
        nat_h += (nat_h) >> 1;
    }
}

void ClipMaskIcon::get_preferred_width_vfunc(Gtk::Widget& widget,
                                             int& min_w,
                                             int& nat_w) const
{
    Gtk::CellRendererPixbuf::get_preferred_width_vfunc(widget, min_w, nat_w);

    if (min_w) {
        min_w += (min_w) >> 1;
    }
    
    if (nat_w) {
        nat_w += (nat_w) >> 1;
    }
}
#else
void ClipMaskIcon::get_size_vfunc(Gtk::Widget& widget,
                                  const Gdk::Rectangle* cell_area,
                                  int* x_offset,
                                  int* y_offset,
                                  int* width,
                                  int* height ) const
{
    Gtk::CellRendererPixbuf::get_size_vfunc( widget, cell_area, x_offset, y_offset, width, height );

    if ( width ) {
        *width = phys;//+= (*width) >> 1;
    }
    if ( height ) {
        *height =phys;//+= (*height) >> 1;
    }
}
#endif

#if WITH_GTKMM_3_0
void ClipMaskIcon::render_vfunc( const Cairo::RefPtr<Cairo::Context>& cr,
                                 Gtk::Widget& widget,
                                 const Gdk::Rectangle& background_area,
                                 const Gdk::Rectangle& cell_area,
                                 Gtk::CellRendererState flags )
#else
void ClipMaskIcon::render_vfunc( const Glib::RefPtr<Gdk::Drawable>& window,
                                 Gtk::Widget& widget,
                                 const Gdk::Rectangle& background_area,
                                 const Gdk::Rectangle& cell_area,
                                 const Gdk::Rectangle& expose_area,
                                 Gtk::CellRendererState flags )
#endif
{
    switch (_property_active.get_value())
    {
        case 1:
            property_pixbuf() = _property_pixbuf_clip;
            break;
        case 2:
            property_pixbuf() = _property_pixbuf_mask;
            break;
        case 3:
            property_pixbuf() = _property_pixbuf_both;
            break;
        default:
            property_pixbuf() = Glib::RefPtr<Gdk::Pixbuf>(0);
            break;
    }
#if WITH_GTKMM_3_0
    Gtk::CellRendererPixbuf::render_vfunc( cr, widget, background_area, cell_area, flags );
#else
    Gtk::CellRendererPixbuf::render_vfunc( window, widget, background_area, cell_area, expose_area, flags );
#endif
}

bool ClipMaskIcon::activate_vfunc(GdkEvent* /*event*/,
                                  Gtk::Widget& /*widget*/,
                                  const Glib::ustring& /*path*/,
                                  const Gdk::Rectangle& /*background_area*/,
                                  const Gdk::Rectangle& /*cell_area*/,
                                  Gtk::CellRendererState /*flags*/)
{
    return false;
}


} // namespace Widget
} // namespace UI
} // namespace Inkscape

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


