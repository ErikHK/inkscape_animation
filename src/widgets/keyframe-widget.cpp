#include <ctime>
#include <cmath>
#include <cairomm/context.h>
#include <glibmm/main.h>
#include "keyframe-widget.h"

KeyframeWidget::KeyframeWidget()
{
  //Glib::signal_timeout().connect( sigc::mem_fun(*this, &KeyframeWidget::on_timeout), 1000 );
}

KeyframeWidget::~KeyframeWidget()
{
}

bool KeyframeWidget::on_expose_event(GdkEventExpose* event)
{
	  Glib::RefPtr<Gdk::Window> window = get_window();
  if(window)
  {
	Gtk::Allocation allocation = get_allocation();
  
  
  
  }
  
  return true;
}

bool KeyframeWidget::on_timeout()
{
	
}