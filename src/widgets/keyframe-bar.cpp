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
#include "inkscape.h"
#include "ui/icon-names.h"
#include "widgets/icon.h"
#include <gtkmm/icontheme.h>
#include "ui/widget/layertypeicon.h"
#include "ui/widget/insertordericon.h"
#include "ui/widget/clipmaskicon.h"
#include "ui/widget/imagetoggler.h"

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
: btn("hehe"), btn2("hehe2")
{
	std::vector<Gtk::Widget*> wlist;
	
	id = _id;
	KeyframeWidget* kw = new KeyframeWidget(1);
	
	//attach(*kw, 0,1,0,1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	//attach(btn, 0,1,0,1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	//attach(btn2, 1,2,0,1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	
	//wlist.push_back(&btn);
	//wlist.push_back(&btn2);
	
	
	//btn.signal_clicked().connect( sigc::mem_fun(*this, &KeyframeBar::on_button_));
	
	/*
	g_signal_connect( G_OBJECT(tbl->gobj()),
					  "clicked",
					  G_CALLBACK(gotClicked),
					  NULL);
	*/
	
	for(int i=1;i < 100;i++)
	{
		kw = new KeyframeWidget(i);

		/*
		g_signal_connect( G_OBJECT(kw->gobj()),
					  "focus-in-event",
					  G_CALLBACK(gotFocus),
					  this);
			*/

		attach(*kw, i, i+1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		
		wlist.push_back(kw);
		
		//kw->add_events(Gdk::POINTER_MOTION_MASK|Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK |Gdk::PROXIMITY_IN_MASK|Gdk::PROXIMITY_OUT_MASK|Gdk::SCROLL_MASK|Gdk::FOCUS_CHANGE_MASK|Gdk::BUTTON_PRESS_MASK);
		
		add_events(Gdk::ALL_EVENTS_MASK);
		kw->add_events(Gdk::ALL_EVENTS_MASK);
		
		//kw->signal_focus_in_event().connect(sigc::mem_fun(*this, &KeyframeBar::on_my_focus_in_event));
		kw->signal_button_press_event().connect(sigc::mem_fun(*this, &KeyframeBar::on_my_button_press_event));
		//kw->signal_motion_notify_event().connect(sigc::mem_fun(*this, &KeyframeBar::on_mouse_));
		kw->set_can_focus(true);
	}
	
	set_focus_chain(wlist);
	show_all_children();
	set_focus_chain(wlist);
}

KeyframeBar::~KeyframeBar()
{
}

bool KeyframeBar::on_expose_event(GtkWidget * widget, GdkEventExpose* event)
{
	gtk_widget_grab_focus(widget);
	
	/*
	g_signal_connect( G_OBJECT(this->gobj()),
					  "focus-in-event",
					  G_CALLBACK(gotFocus),
					  NULL);
	*/
}

bool KeyframeBar::on_my_button_press_event(GdkEventButton*)
//bool KeyframeBar::on_my_button_press_event()
//void KeyframeBar::on_button_()
{
	//gtk_widget_grab_focus(this->gobj());
	grab_focus();
	//addLayers();
	return false;
}

bool KeyframeBar::on_mouse_(GdkEventMotion* event)
{
	addLayers();
	return true;
	
}

void KeyframeBar::addLayers()
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
	addLayers();
	return false;
}



