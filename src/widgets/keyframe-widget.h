#include <gtkmm/drawingarea.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/targetlist.h>
#include <sp-object.h>
#include "inkscape.h"

#include <gtkmm/selectiondata.h>
//#include "keyframe-bar.h"
//#include "animation-control.h"

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
	KeyframeWidget * next;
	KeyframeWidget * prev;
	SPObject * layer;
	bool is_empty;
	bool is_focused;
	void selectLayer();
	void defocusAllKeyframes();
	Gtk::CheckMenuItem *onion;
	Gtk::CheckMenuItem *showAll;
	Gtk::MenuItem *settingsItem;
	//void createTween();
	//void gotFocus();
	//void showAllKeyframes(KeyframeWidget * kww, gpointer user_data);
	//void onionSkinning(KeyframeWidget * kww, gpointer user_data);
	//void insertKeyframe(KeyframeWidget * kww, gpointer user_data);

protected:
    virtual bool on_expose_event(GdkEventExpose* event);
	virtual bool on_my_focus_in_event(GdkEventFocus* event);
	virtual bool on_my_focus_out_event(GdkEventFocus* event);
	bool on_my_key_press_event(GdkEventKey * event);
	bool on_my_key_release_event(GdkEventKey* event);
	bool on_my_button_release_event(GdkEventButton* event);
	bool on_my_button_press_event(GdkEventButton* event);
	bool on_my_drag_motion_event(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time);
	void on_my_drag_begin_event(const Glib::RefPtr<Gdk::DragContext>& context);
	void on_my_drag_data_get(const Glib::RefPtr< Gdk::DragContext >&  	context,
			Gtk::SelectionData&  	selection_data,
			guint  	info,
			guint  	time);
	void on_my_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int, int,
        const Gtk::SelectionData& selection_data, guint, guint time);

	bool on_my_drag_drop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time);
	void on_selection_changed();
	void on_update_tween();
	
private:
	Gtk::Menu *pMenu;



};
