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

//void gotFocus(GtkWidget* , GdkEventKey *event, gpointer callback_data);

KeyframeWidget::KeyframeWidget(int _id)
{
	/*
	g_signal_connect( G_OBJECT(this->gobj()),
                          "focus-in-event",
                          G_CALLBACK(gotFocus),
                          this);
		*/				  
	
	id = _id;
	this->set_size_request(40, 40);
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
  return true;
}

bool KeyframeWidget::on_timeout()
{
	
}
/*
static void gotFocus(GtkWidget* , GdkEventKey *event, gpointer callback_data)
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
*/