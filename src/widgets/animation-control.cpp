#include <ctime>
#include <cmath>
#include <cairomm/context.h>
#include <glibmm/main.h>
#include "desktop.h"
#include "document.h"
#include "animation-control.h"
#include "keyframe-bar.h"
#include <gdkmm/general.h>

static void gotFocus(GtkWidget* , GdkEventKey *event, gpointer callback_data)
{
	
}


AnimationControl::AnimationControl() : _panes()
{
	//std::vector<Gtk::Widget*> wlist;
	KeyframeBar* kb = new KeyframeBar(1);
	KeyframeBar* kb2 = new KeyframeBar(2);

	show_all_children();
	

	_panes.add1(*kb);
	_panes.add2(*kb2);
	add(_panes);
	show_all_children();
}

AnimationControl::~AnimationControl()
{
}

bool AnimationControl::on_expose_event(GtkWidget * widget, GdkEventExpose* event)
{
	return true;
}

bool AnimationControl::on_my_button_press_event(GdkEventButton*)
{
	return false;
}

bool AnimationControl::on_mouse_(GdkEventMotion* event)
{
	return true;
}

bool AnimationControl::on_my_focus_in_event(GdkEventFocus*)
{
	return false;
}



