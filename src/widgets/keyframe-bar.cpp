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

	
}


KeyframeWidget * KeyframeBar::getCurrentKeyframe()
{
	Gtk::Widget * w = SP_ACTIVE_DESKTOP->getToplevel()->get_focus();
	return dynamic_cast<KeyframeWidget *>(w);
}


KeyframeBar::KeyframeBar(int _id)
: btn("hehe"), btn2("hehe2")
{
	id = _id;
	rebuildUi();
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
	rebuildUi();
	//gtk_widget_grab_focus(this->gobj());
	grab_focus();
	//addLayers();
	return false;
}

bool KeyframeBar::on_mouse_(GdkEventMotion* event)
{
	//addLayers();
	rebuildUi();
	return true;
	
}

void KeyframeBar::rebuildUi()
{
	
	//remove objects
	std::vector<Gtk::Widget *> children = get_children();
    for (unsigned int i = 0; i < children.size(); ++i) {
        Gtk::Widget *child = children[i];
        //child->destroy();
		remove(*child);
    }
	
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	if(!desktop)
		return;
	
	SPObject * animation_layer = desktop->namedview->document->getObjectById(Glib::ustring::format("animationlayer", id));
	if(!animation_layer)
		return;
	
	int num_keyframes = animation_layer->getRepr()->childCount();
	
	std::vector<Gtk::Widget*> wlist;
	
	KeyframeWidget* kw;
	
	for(int i=1;i <= num_keyframes;i++)
	{
		
		//keyframe has objects
		if(animation_layer->getRepr()->nthChild(i-1) && animation_layer->getRepr()->nthChild(i-1)->childCount() > 0)
			kw = new KeyframeWidget(i, false);
		else
			kw = new KeyframeWidget(i, true);

		//attach(*kw, i, i+1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		attach(*kw, i, i+1, 0, 1, Gtk::SHRINK, Gtk::SHRINK);
		
		wlist.push_back(kw);
		
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

void KeyframeBar::addLayers()
{
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;

	while(desktop->getDocument()->getReprRoot()->childCount() < 100)
	{
		SPObject * lay = Inkscape::create_layer(desktop->currentRoot(), desktop->currentLayer(), Inkscape::LPOS_ABOVE);
		//lm->setCurrentLayer(lay);
	}
}

bool KeyframeBar::on_my_focus_in_event(GdkEventFocus*)
{
	//addLayers();
	return false;
}



