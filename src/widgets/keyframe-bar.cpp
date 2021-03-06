#include <ctime>
#include <cmath>
#include <cairomm/context.h>
#include <glibmm/main.h>
#include "document.h"
#include "inkscape.h" // for SP_ACTIVE_DESKTOP
#include "layer-fns.h" //LPOS_ABOVE
#include "layer-manager.h"
#include "layer-model.h"
//#include "ui/previewable.h"
//#include "object/sp-namedview.h"
#include "keyframe-bar.h"
#include "keyframe-widget.h"
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

/*
KeyframeWidget * KeyframeBar::getCurrentKeyframe()
{
	Gtk::Widget * w = SP_ACTIVE_DESKTOP->getToplevel()->get_focus();
	return dynamic_cast<KeyframeWidget *>(w);
}
*/

void KeyframeBar::on_selection_changed()
{
	//rebuildUi();
}

void KeyframeBar::update()
{
	if(widgets[0])
		widgets[0]->selectLayer();
}

static void my_getsize(KeyframeBar * kbb, gpointer user_data)
{
	KeyframeBar* kb = reinterpret_cast<KeyframeBar*>(user_data);
	
	kb->height = kb->get_allocation().get_height();
	kb->widgets[0]->height = kb->height;
	kb->widgets[0]->queue_draw();
}

KeyframeBar::KeyframeBar(int _id, SPObject * _layer)
: num_keyframes(1)
{
	height = 0;
	next = NULL;
	prev = NULL;
	
	//animation_start = 1;
	//animation_stop = 10;
	
	shift_held = false;
	ctrl_held = false;
	clear_tween = false;
	unselect = false;
	id = _id;
	is_visible = true;
	several_selected = false;
	layer = _layer;
	rebuildUi();
	
	//g_signal_connect(GTK_WIDGET(this), "size-allocate", G_CALLBACK(my_getsize), this);

	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	if(desktop)
	{
		Inkscape::Selection * selection = desktop->getSelection();
		sigc::connection _sel_changed_connection;
		
		//_sel_changed_connection = selection->connectChanged(
		//	sigc::bind(
		//		sigc::ptr_fun(&KeyframeBar::on_selection_changed),
		//		desktop));
		//_sel_changed_connection = selection->connectChanged(
		//sigc::bind(sigc::mem_fun(*this, &KeyframeBar::on_selection_changed), false)
		//);
		
		_sel_changed_connection = selection->connectChanged(
		sigc::hide(sigc::mem_fun(*this, &KeyframeBar::on_selection_changed)));
	}
	
	add_events(Gdk::ALL_EVENTS_MASK);
	this->signal_key_press_event().connect(sigc::mem_fun(*this, &KeyframeBar::on_my_key_press_event));
}

KeyframeBar::~KeyframeBar()
{
}

bool KeyframeBar::on_expose_event(GdkEventExpose* event)
{
	//rebuildUi();
	//grab_focus();
}

bool KeyframeBar::on_my_button_press_event(GdkEventButton* event)
{
	//rebuildUi();
	if(event->state & GDK_CONTROL_MASK)
	{
		ctrl_held = true;
	}

	grab_focus();
	
	if(event->state &  (GDK_SHIFT_MASK))
	{
		shift_held = true;

		bool activated = false;

		for(int i=0; i < widgets.size(); i++)
		{
			KeyframeWidget * kw = widgets[i];

			if(activated && (id - 1) == i)
			{
				activated = false;
				//break;
				return true;
			}

			else if(activated)
			{
				kw->is_focused = true;
				several_selected = true;
			}

			if(kw->is_focused)
				activated = true;

			//kw->is_focused = true;
		}
	}
	else
	{
		shift_held = false;
	}
	/*
	if(event->state & GDK_SHIFT_MASK)
	{
		shift_held = true;

		bool activated = false;

		for(int i=0; i < widgets.size(); i++)
		{
			KeyframeWidget * kw = widgets[i];

			if(activated && kw->is_focused)
			{
				activated = false;
			}
			else if(activated)
				kw->is_focused = true;

			if(kw->is_focused)
				activated = true;

			//kw->is_focused = true;
		}
	}
	else
		shift_held = false;
*/
	//if(event->keyval == GDK_KEY_Delete)
	//	deleteAllActiveKeyframes();
	queue_draw();
	return true;
}

bool KeyframeBar::on_mouse_(GdkEventMotion* event)
{
	//addLayers();
	//rebuildUi();
	return false;
}

//also remove properties like tween things
void KeyframeBar::deleteAllActiveKeyframes()
{
	for(int i=0; i < widgets.size(); i++)
	{
		KeyframeWidget * kw = widgets[i];
		
		while(kw->layer &&  kw->layer->getRepr()->childCount() > 0 && kw->is_focused)
		{
			Inkscape::XML::Node * child = kw->layer->getRepr()->firstChild();
			Inkscape::XML::Node * layer = kw->layer->getRepr();

			if(layer->attribute("inkscape:tween"))
			{
				const char * test = layer->attribute("inkscape:tweenpathid");
				SPObject * tweenpath = NULL;
				if(test)
					tweenpath = SP_ACTIVE_DESKTOP->getDocument()->getObjectById(test);

				if(tweenpath)
					tweenpath->deleteObject();


				layer->setAttribute("inkscape:tween", NULL);
			}
			if(layer->attribute("inkscape:tweenstart"))
				layer->setAttribute("inkscape:tweenstart", NULL);
			if(layer->attribute("inkscape:tweenlayers"))
				layer->setAttribute("inkscape:tweenlayers", NULL);



			kw->layer->getRepr()->removeChild(child);
		}
	}

	SPDesktop * desktop = SP_ACTIVE_DESKTOP;
	//emit change
	if(desktop)
		desktop->getSelection()->emit();
}

bool KeyframeBar::on_my_key_press_event(GdkEventKey * event)
{

	if(event->state & GDK_SHIFT_MASK)
	{
		shift_held = true;
	}

	/*
	if(event->state & GDK_SHIFT_MASK)
	{
		shift_held = true;
		
		bool activated = false;
		
		for(int i=0; i < widgets.size(); i++)
		{
			KeyframeWidget * kw = widgets[i];

			if(activated && kw->is_focused)
			{
				activated = false;
			}
			else if(activated)
				kw->is_focused = true;
			
			if(kw->is_focused)
				activated = true;

			kw->is_focused = true;	
		}
	}
	else
		shift_held = false;
	*/
	if(event->keyval == GDK_KEY_Delete)
		deleteAllActiveKeyframes();

	return false;
}

void KeyframeBar::rebuildUi()
{
	//remove all objects
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
	
	//int num_keyframes = animation_layer->getRepr()->childCount();
	int num_keyframes = 100;
	
	//std::vector<Gtk::Widget*> wlist;
	
	KeyframeWidget* kw;
	
	for(int i=1;i <= num_keyframes;i++)
	{
		
		SPObject * assoc_layer = NULL;

		if(i==1)
			desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", i)));

		//keyframe has objects
		if(animation_layer->getRepr()->nthChild(i-1) && animation_layer->getRepr()->nthChild(i-1)->childCount() > 0)
			kw = new KeyframeWidget(i, this, assoc_layer, false);
		else
			kw = new KeyframeWidget(i, this, assoc_layer, true);
		
		attach(*kw, i, i+1, 0, 1, Gtk::SHRINK, Gtk::SHRINK);
		
		//wlist.push_back(kw);
		widgets.push_back(kw);
		
		
		kw->add_events(Gdk::ALL_EVENTS_MASK);
		
		//kw->signal_focus_in_event().connect(sigc::mem_fun(*this, &KeyframeBar::on_my_focus_in_event));
		kw->signal_button_press_event().connect(sigc::mem_fun(*this, &KeyframeBar::on_my_button_press_event));
		kw->signal_motion_notify_event().connect(sigc::mem_fun(*this, &KeyframeBar::on_mouse_));
		
		kw->set_can_focus(true);
	}

	
	for(int i=0; i < widgets.size(); i++)
	{
		if(i>0)
			widgets[i]->prev = widgets[i-1];
		else
			widgets[i]->prev = NULL;
		
		if(i < widgets.size() - 1)
			widgets[i]->next = widgets[i+1];
		else
			widgets[i]->next = NULL;
	}
	

	set_focus_chain(widgets);
	show_all_children();
	set_focus_chain(widgets);
	
	
}

bool KeyframeBar::on_my_focus_in_event(GdkEventFocus*)
{
	//addLayers();
	return false;
}



