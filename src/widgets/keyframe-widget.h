#include <gtkmm/drawingarea.h>


class KeyframeWidget : public Gtk::DrawingArea
{
public:
    KeyframeWidget(int _id);
    virtual ~KeyframeWidget();
	int id;
	bool is_empty;
	//void gotFocus();
	
	

protected:
    virtual bool on_expose_event(GdkEventExpose* event);
	bool on_my_focus_in_event(GdkEventFocus* event);
	bool on_my_button_press_event(GdkEventButton* event);

};