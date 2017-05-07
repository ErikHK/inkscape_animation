#include <gtkmm/drawingarea.h>
#include <gtkmm/checkmenuitem.h>
#include <sp-object.h>
#include "inkscape.h"
//#include "keyframe-bar.h"

class KeyframeBar;
//class AnimationControl;
class KeyframeWidget : public Gtk::DrawingArea
{
public:
	//class KeyframeBar;
    KeyframeWidget(int _id, KeyframeBar * _parent, SPObject * _layer, bool _is_empty);
    virtual ~KeyframeWidget();
	int id;
	int parent_id;
	KeyframeBar * parent;
	SPObject * layer;
	bool is_empty;
	void selectLayer();
	//void createTween();
	//void gotFocus();

protected:
    virtual bool on_expose_event(GdkEventExpose* event);
	virtual bool on_my_focus_in_event(GdkEventFocus* event);
	virtual bool on_my_focus_out_event(GdkEventFocus* event);
	bool on_my_button_press_event(GdkEventButton* event);
	void on_selection_changed();
	
private:
	void _fadeToggled(Gtk::CheckMenuItem* toggler);
	Gtk::Menu *pMenu;

};