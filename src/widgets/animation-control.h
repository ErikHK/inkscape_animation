#include <gtkmm/table.h>
#include <gtkmm/paned.h>
#include <gdk/gdk.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treemodelcolumn.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>
#include "sp-object.h"
#include "keyframe-bar.h"

namespace Inkscape {
namespace UI {
namespace Dialogs {

//class KeyframeBar;
//class KeyframeWidget;
class AnimationControl : public Gtk::Table
{
public:
    AnimationControl();
    virtual ~AnimationControl();
	void addLayer();
	void rebuildUi();
	int num_layers;
	void toggleVisible( Glib::ustring const& str );
	void toggleLocked( Glib::ustring const& str );
	//std::vector<SPObject*> layersToHide;
	std::vector<KeyframeBar*> kb_vec;
	
protected:
    virtual bool on_expose_event(GtkWidget * widget, GdkEventExpose* event);
	bool on_my_focus_in_event(GdkEventFocus* event);
	bool on_my_button_press_event(GdkEventButton* event);
	bool handleKeyEvent(GdkEventKey *event);
	bool on_mouse_(GdkEventMotion* event);
	
private:
	Gtk::Paned _panes;
	class ModelColumns;
	Glib::RefPtr<Gtk::TreeStore> _store;
	ModelColumns* _model;
    Gtk::TreeView _tree;
	Gtk::Table _keyframe_table;
	Gtk::ScrolledWindow _scroller;
	Gtk::ScrolledWindow _tree_scroller;
	Gtk::Button _new_layer_button;
	
};
}
}
}