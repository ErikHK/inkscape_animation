#include <gtkmm/table.h>
#include "keyframe-widget.h"


class KeyframeBar : public Gtk::Table
{
public:
    KeyframeBar(int _id);
    virtual ~KeyframeBar();
	int id;
	//KeyframeWidget * widgets[100];
	

protected:
    virtual bool on_expose_event(GdkEventExpose* event);
	bool on_my_focus_in_event(GdkEventFocus* event);
	bool on_my_button_press_event(GdkEventButton* event);
	
};