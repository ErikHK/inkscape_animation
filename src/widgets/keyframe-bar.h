#include <gtkmm/table.h>
#include <gtkmm/button.h>
#include <gtkmm/window.h>
#include "desktop.h"
#include "selection.h"
//#include "keyframe-widget.h"
//#include "animation-control.h"
#include <gdk/gdk.h>
#include "sp-namedview.h"

class KeyframeWidget;
//class AnimationControl;
class KeyframeBar : public Gtk::Table
{
public:
    KeyframeBar(int _id, SPObject * _layer);
    virtual ~KeyframeBar();
	int id;
	void addLayers();
	bool is_visible;
	bool num_keyframes;
	bool clear_tween;
	KeyframeBar *next;
	KeyframeBar *prev;
	std::vector<KeyframeWidget*> widgets;
	//std::vector<SPObject*> layersToHide;
	
	SPObject * layer;	//associated animation layer
	//KeyframeWidget *getCurrentKeyframe();
	
private:
	void rebuildUi();
	
protected:
    virtual bool on_expose_event(GdkEventExpose* event);
	bool on_my_focus_in_event(GdkEventFocus* event);
	bool on_my_button_press_event(GdkEventButton* event);
	void on_selection_changed();
	//bool on_my_button_press_event();
	Gtk::Button btn;
	Gtk::Button btn2;
	//void on_button_();
	bool on_mouse_(GdkEventMotion* event);
};
