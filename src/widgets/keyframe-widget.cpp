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
	gtk_widget_grab_focus (w);
	
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	//SPDocument *doc = SP_ACTIVE_DOCUMENT;
	//LayerManager * lm = desktop->layer_manager;
	
	//Glib::ustring strr = Glib::ustring::format(id);
	//Glib::ustring ids("layer" + strr);
	//ids = lm->getNextLayerName(NULL, desktop->currentLayer()->label());
	
	while(desktop->getDocument()->getReprRoot()->childCount() < 100)
	{
		SPObject * lay = Inkscape::create_layer(desktop->currentRoot(), desktop->currentLayer(), Inkscape::LPOS_ABOVE);
		//lm->setCurrentLayer(lay);
	}
	
}


bool KeyframeWidget::on_my_focus_in_event(GdkEventFocus*)
{
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	//SPDocument *doc = SP_ACTIVE_DOCUMENT;
	//LayerManager * lm = desktop->layer_manager;
	
	//Glib::ustring strr = Glib::ustring::format(id);
	//Glib::ustring ids("layer" + strr);
	//ids = lm->getNextLayerName(NULL, desktop->currentLayer()->label());
	
	while(desktop->getDocument()->getReprRoot()->childCount() < 100)
	{
		SPObject * lay = Inkscape::create_layer(desktop->currentRoot(), desktop->currentLayer(), Inkscape::LPOS_ABOVE);
		//lm->setCurrentLayer(lay);
	}
	
}


bool KeyframeWidget::on_my_button_press_event(GdkEventButton* event)
{
	gtk_widget_grab_focus(GTK_WIDGET(this));
	gtk_widget_set_state( GTK_WIDGET(this), GTK_STATE_ACTIVE );
	
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	//SPDocument *doc = SP_ACTIVE_DOCUMENT;
	//LayerManager * lm = desktop->layer_manager;
	
	//Glib::ustring strr = Glib::ustring::format(id);
	//Glib::ustring ids("layer" + strr);
	//ids = lm->getNextLayerName(NULL, desktop->currentLayer()->label());
	
	while(desktop->getDocument()->getReprRoot()->childCount() < 100)
	{
		SPObject * lay = Inkscape::create_layer(desktop->currentRoot(), desktop->currentLayer(), Inkscape::LPOS_ABOVE);
		//lm->setCurrentLayer(lay);
	}
	
	
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



KeyframeWidget::KeyframeWidget(int _id)
{	
	
	id = _id;
	this->set_size_request(25, 30);
	
	set_can_focus(true);
	grab_focus();
	
	//set_focus_on_click ();
	is_empty = false;
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
	
		//GtkAllocation allocation;// = get_allocation();
		//gtk_widget_get_allocation (GTK_WIDGET(this), &allocation);
		//int width = allocation.width;
		//int height = allocation.height;
		
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
		//cr->paint();
		//cr->destroy();
  }

	

  
	//add_events(Gdk::POINTER_MOTION_MASK|Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK |Gdk::PROXIMITY_IN_MASK|Gdk::PROXIMITY_OUT_MASK|Gdk::SCROLL_MASK|Gdk::FOCUS_CHANGE_MASK|Gdk::BUTTON_PRESS_MASK);
	
	add_events(Gdk::ALL_EVENTS_MASK);
	
	/*
	g_signal_connect( G_OBJECT(this->gobj()),
                          "focus-in-event",
                          G_CALLBACK(gotFocus),
                          this);
	*/
	//this->signal_realize().connect(sigc::ptr_fun(&gotFocus));
	//Gtk::DrawingArea::signal_clicked().connect(sigc::mem_fun(*this, &KeyframeWidget::gotFocus));
	//signal_realize().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_focus_in_event));
	//signal_focus_in_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_focus_in_event));
	
	
	
	//Glib::wrap(gobj())->signal_focus_in_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_focus_in_event));
	signal_button_press_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_button_press_event));
	set_can_focus(true);
	gtk_widget_set_can_focus(GTK_WIDGET(this), TRUE);
	set_can_default(true);
    //set_relief(Gtk::RELIEF_NORMAL);
	//set_update_policy(Gtk::UPDATE_ALWAYS);
	
	gtk_widget_set_receives_default(GTK_WIDGET(this), TRUE );

    gtk_widget_set_sensitive( GTK_WIDGET(this), TRUE );
	
	return true;
}
