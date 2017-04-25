#include <gtkmm/table.h>
#include <gtkmm/paned.h>
#include <gdk/gdk.h>

class AnimationControl : public Gtk::Table
{
public:
    AnimationControl();
    virtual ~AnimationControl();
	
protected:
    virtual bool on_expose_event(GtkWidget * widget, GdkEventExpose* event);
	bool on_my_focus_in_event(GdkEventFocus* event);
	bool on_my_button_press_event(GdkEventButton* event);
	bool on_mouse_(GdkEventMotion* event);
	
private:
	Gtk::Paned _panes;
	
};