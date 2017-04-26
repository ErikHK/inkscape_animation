#include <ctime>
#include <cmath>
#include <cairomm/context.h>
#include <glibmm/main.h>
#include "keyframe-widget.h"
#include "desktop.h"
#include "document.h"
#include "inkscape.h" // for SP_ACTIVE_DESKTOP
#include "layer-fns.h" //LPOS_ABOVE
#include "layer-manager.h"
#include "layer-model.h"
//#include "ui/previewable.h"
//#include "sp-namedview.h"

#include <gdkmm/general.h>
#include <gtkmm/menu.h>


//static void gotFocus(GtkWidget*, void * data);

static void gotFocus(GtkWidget* w, GdkEventKey *event, gpointer callback_data)
{
	
}


bool KeyframeWidget::on_my_focus_in_event(GdkEventFocus*)
{
	
}


bool KeyframeWidget::on_my_button_press_event(GdkEventButton* event)
{
	gtk_widget_grab_focus(GTK_WIDGET(this));
	gtk_widget_set_state( GTK_WIDGET(this), GTK_STATE_ACTIVE );
	
	
	if (event->type == GDK_BUTTON_PRESS  &&  event->button == 3)
	{
		Gtk::Menu *pMenu = new Gtk::Menu();
		Gtk::MenuItem *pItem = new Gtk::MenuItem("Create tween");
		Gtk::MenuItem *pItem2 = new Gtk::MenuItem("Something other");
		pMenu->add(*pItem);
		pMenu->add(*pItem2);
		pMenu->show_all();
		pMenu->popup(event->button, event->time);	
	}
	
	grab_focus();
}



KeyframeWidget::KeyframeWidget(int _id, bool _is_empty)
{	
	
	id = _id;
	this->set_size_request(15, 21);
	
	set_can_focus(true);
	
	is_empty = _is_empty;
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
		const int width = allocation.get_width();
		const int height = allocation.get_height();
		
		Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();
		cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

		if(id % 2 == 0)
			cr->set_source_rgba(1, 1, 1, 1);
		else
			cr->set_source_rgba(.8, .8, .8, 1);
		
		if(has_focus())
			cr->set_source_rgba(.8, 0, 0, 1);
		
		cr->paint();
		
		if(!is_empty)
		{
			cr->set_source_rgba(0, 0, 0, 1);
			cr->arc(width/2, height/2, width/4, 0, 6.283);
		}
		
		cr->fill();
	
		//add line to the bottom
		cr->set_source_rgba(.3,.3,.3,1);
		cr->set_line_width(1.0);
		cr->move_to(0, height-.5);
		cr->line_to(width, height-.5);
		cr->stroke();
  }

	add_events(Gdk::ALL_EVENTS_MASK);
	
	signal_button_press_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_button_press_event));
	set_can_focus(true);
	set_receives_default();
    set_sensitive();
	
	return true;
}
