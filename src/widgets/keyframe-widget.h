#include <gtkmm/drawingarea.h>

class KeyframeWidget : public Gtk::DrawingArea
{
public:
    KeyframeWidget();
    virtual ~KeyframeWidget();

protected:
    virtual bool on_expose_event(GdkEventExpose* event);
    bool on_timeout();

};
