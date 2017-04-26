#include <gtkmm/table.h>
#include <gtkmm/button.h>
#include <gtkmm/window.h>
#include "keyframe-widget.h"
#include <gdk/gdk.h>
#include "sp-namedview.h"

class KeyframeBar : public Gtk::Table
{
public:
    KeyframeBar(int _id);
    virtual ~KeyframeBar();
	int id;
	void addLayers();
	KeyframeWidget *getCurrentKeyframe();
	
private:
	void rebuildUi();
	
protected:
    virtual bool on_expose_event(GdkEventExpose* event);
	bool on_my_focus_in_event(GdkEventFocus* event);
	bool on_my_button_press_event(GdkEventButton* event);
	//bool on_my_button_press_event();
	Gtk::Button btn;
	Gtk::Button btn2;
	//void on_button_();
	bool on_mouse_(GdkEventMotion* event);
};