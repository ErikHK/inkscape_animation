#include <gtkmm/drawingarea.h>


class KeyframeWidget : public Gtk::DrawingArea
{
public:
    KeyframeWidget(int _id);
    virtual ~KeyframeWidget();
	int id;
	bool is_empty;

protected:
    virtual bool on_expose_event(GdkEventExpose* event);
    bool on_timeout();

};