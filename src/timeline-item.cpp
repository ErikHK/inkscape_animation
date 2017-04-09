/**
 * @file
 * Inkscape color swatch UI item.
 */
/* Authors:
 *   Jon A. Cruz
 *   Abhishek Sharma
 *
 * Copyright (C) 2010 Jon A. Cruz
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "timeline-item.h"
#include <cairomm/context.h>



//namespace Inkscape {
//namespace UI {
//namespace Dialogs {

TimelineItem::TimelineItem()
{	
	
}

TimelineItem::~TimelineItem()
{
}

bool TimelineItem::on_expose_event(GdkEventExpose* event)
{
	  Glib::RefPtr<Gdk::Window> window = get_window();
  if(window)
  {
	
	
  Gtk::Allocation allocation = get_allocation();
  const int width = allocation.get_width();
  const int height = allocation.get_height();

  double x0=0.1, y0=0.5, // start point
         x1=0.4, y1=0.9,  // control point #1
         x2=0.6, y2=0.1,  // control point #2
         x3=0.9, y3=0.5;  // end point

  // scale to unit square (0 to 1 width and height)
  //cr->scale(width, height);
      Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();

  cr->set_line_width(10.0);
  // draw curve
  cr->move_to(x0, y0);
  cr->curve_to(x1, y1, x2, y2, x3, y3);
  cr->stroke();
  // show control points
  cr->set_source_rgba(1, 0.2, 0.2, 0.6);
  cr->move_to(x0, y0);
  cr->line_to (x1, y1);
  cr->move_to(x2, y2);
  cr->line_to (x3, y3);
  cr->stroke();

  }
  return true;
}

//} // namespace Dialogs
//} // namespace UI
//} // namespace Inkscape

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
