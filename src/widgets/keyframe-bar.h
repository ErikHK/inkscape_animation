#include <gtkmm/table.h>
#include <gtkmm/button.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treemodelcolumn.h>
#include <gtkmm/liststore.h>
#include "keyframe-widget.h"
#include <gdk/gdk.h>

class KeyframeBar : public Gtk::Table
{
public:
    KeyframeBar(int _id);
    virtual ~KeyframeBar();
	int id;
	void addLayers();
	
protected:
    virtual bool on_expose_event(GtkWidget * widget, GdkEventExpose* event);
	bool on_my_focus_in_event(GdkEventFocus* event);
	bool on_my_button_press_event(GdkEventButton* event);
	//bool on_my_button_press_event();
	Gtk::Button btn;
	Gtk::Button btn2;
	//void on_button_();
	bool on_mouse_(GdkEventMotion* event);
	
private:
	class ModelColumns;
	Glib::RefPtr<Gtk::TreeStore> _store;
	ModelColumns* _model;
    Gtk::TreeView _tree;
	
};