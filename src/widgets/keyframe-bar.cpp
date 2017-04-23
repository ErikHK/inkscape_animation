#include <ctime>
#include <cmath>
#include <cairomm/context.h>
#include <glibmm/main.h>
#include "desktop.h"
#include "document.h"
#include "inkscape.h" // for SP_ACTIVE_DESKTOP
#include "layer-fns.h" //LPOS_ABOVE
#include "layer-manager.h"
#include "layer-model.h"
//#include "ui/previewable.h"
//#include "sp-namedview.h"
#include "keyframe-bar.h"

#include <gdkmm/general.h>

//static void gotFocus(GtkWidget*, void * data);

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


KeyframeBar::KeyframeBar(int _id)
{
	id = _id;
	
	
	KeyframeWidget* kw = new KeyframeWidget(1);
	
	attach(*kw, 0,1,0,1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	
	/*
	g_signal_connect( G_OBJECT(tbl->gobj()),
					  "clicked",
					  G_CALLBACK(gotClicked),
					  NULL);
	*/
	
	
	for(int i=1;i < 200;i++)
	{
		kw = new KeyframeWidget(i+1);
		
		/*
		g_signal_connect( G_OBJECT(kw->gobj()),
					  "focus-in-event",
					  G_CALLBACK(gotFocus),
					  this);
			*/
			
		
		attach(*kw, i, i+1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		
		kw->signal_focus_in_event().connect(sigc::mem_fun(*this, &KeyframeBar::on_my_focus_in_event));
		kw->signal_button_press_event().connect(sigc::mem_fun(*this, &KeyframeBar::on_my_button_press_event));
	}
	
	show_all_children();
	set_can_focus(true);
}

KeyframeBar::~KeyframeBar()
{
}

bool KeyframeBar::on_expose_event(GdkEventExpose* event)
{
	
	g_signal_connect( G_OBJECT(this->gobj()),
					  "focus-in-event",
					  G_CALLBACK(gotFocus),
					  NULL);
	
}

bool KeyframeBar::on_my_button_press_event(GdkEventButton*)
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



bool KeyframeBar::on_my_focus_in_event(GdkEventFocus*)
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



