#include <gtkmm/drawingarea.h>

class KeyframeBar;
class KeyframeWidget : public Gtk::DrawingArea
{
public:
	//class KeyframeBar;
    KeyframeWidget(int _id, KeyframeBar * _parent, bool _is_empty);
    virtual ~KeyframeWidget();
	int id;
	int parent_id;
	KeyframeBar * parent;
	bool is_empty;
	void selectLayer();
	//void createTween();
	//void gotFocus();

protected:
    virtual bool on_expose_event(GdkEventExpose* event);
	virtual bool on_my_focus_in_event(GdkEventFocus* event);
	bool on_my_button_press_event(GdkEventButton* event);

};