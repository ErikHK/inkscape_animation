#include <ctime>
#include <cmath>
#include <cairomm/context.h>
#include <glibmm/main.h>
#include "desktop.h"
#include "document.h"
#include "animation-control.h"
#include "keyframe-bar.h"
#include <gdkmm/general.h>
#include <gtkmm/treeselection.h>

static void gotFocus(GtkWidget* , GdkEventKey *event, gpointer callback_data)
{
	
}


class AnimationControl::ModelColumns : public Gtk::TreeModel::ColumnRecord
{
	public:
		ModelColumns()
		{ add(m_col_id); add(m_col_name);}

		Gtk::TreeModelColumn<int> m_col_id;
		Gtk::TreeModelColumn<Glib::ustring> m_col_name;
};

AnimationControl::AnimationControl() : 
_panes(), _keyframe_table(), _scroller(), _tree_scroller(),
_new_layer_button("New Layer")
{
	//std::vector<Gtk::Widget*> wlist;
	KeyframeBar* kb = new KeyframeBar(1);
	KeyframeBar* kb2 = new KeyframeBar(2);
	KeyframeBar* kb3 = new KeyframeBar(3);
	KeyframeBar* kb4 = new KeyframeBar(4);

	
	//Create the tree model and store
	Gtk::Label * lbl = new Gtk::Label("ID");
	Gtk::Label * lbl2 = new Gtk::Label("Animation Layer");
	Gtk::CellRendererText *_text_renderer;
	Gtk::TreeView m_TreeView;
	
    ModelColumns *zoop = new ModelColumns();
    _model = zoop;

    _store = Gtk::TreeStore::create( *zoop );
	
	Gtk::TreeModel::iterator iter = _store->prepend();
    Gtk::TreeModel::Row row = *iter;
	row[_model->m_col_id] = 1;
	row[_model->m_col_name] = "Billy Bob";
		
	Glib::RefPtr<Gtk::ListStore> m_refTreeModel;
	
	m_refTreeModel = Gtk::ListStore::create(*_model);
	m_TreeView.set_model(m_refTreeModel);

	iter = _store->prepend();
	row = *iter;
	//Fill the TreeView's model
	row[_model->m_col_id] = 2;
	row[_model->m_col_name] = "Billy Bob2";
	
	iter = _store->prepend();
	row = *iter;
	//Fill the TreeView's model
	row[_model->m_col_id] = 3;
	row[_model->m_col_name] = "Billy Bob3";
	
	iter = _store->prepend();
	row = *iter;
	//Fill the TreeView's model
	row[_model->m_col_id] = 4;
	row[_model->m_col_name] = "Billy Bob4";
	
	//_store->append(row.children());
	
	//row = *(m_refTreeModel->append());
	//row[_model->m_col_id] = 2;
	//row[_model->m_col_name] = "Joey Jojo";
	//row[_model->m_col_name] = *btn2;
	
	//_store->append(row.children());
	
    //Set up the tree
    _tree.set_model( _store );
    _tree.set_headers_visible(true);
    _tree.set_reorderable(true);
    _tree.enable_model_drag_dest (Gdk::ACTION_MOVE);
	
    Gtk::TreeViewColumn* col;
	Gtk::TreeView::Column *_name_column;
	
	
	//Animation layer ID
	_text_renderer = Gtk::manage(new Gtk::CellRendererText());
    int idColNum = _tree.append_column("highlight", *_text_renderer) - 1;
    col = _tree.get_column(idColNum);
    if ( col ) {
        col->add_attribute( _text_renderer->property_text(), _model->m_col_id );
        //lbl.set_tooltip_text(_("Highlight color of outline in Node tool. Click to set. If alpha is zero, use inherited color."));
        lbl->show();
        col->set_widget( *lbl );
    }
	
	/*
	Inkscape::UI::Widget::ImageToggler *eyeRenderer = Gtk::manage( new Inkscape::UI::Widget::ImageToggler(
        INKSCAPE_ICON("object-visible"), INKSCAPE_ICON("object-hidden")) );
    int visibleColNum = _tree.append_column("vis", *eyeRenderer) - 1;
    eyeRenderer->property_activatable() = true;
    col = _tree.get_column(visibleColNum);
    if ( col ) {
        col->add_attribute( eyeRenderer->property_active(), _model->m_col_id );
        // In order to get tooltips on header, we must create our own label.
        //_visibleHeader.set_tooltip_text(_("Toggle visibility of Layer, Group, or Object."));
        lbl->show();
        col->set_widget( *lbl );
    }
	*/
	
    //Label
	//_text_renderer = Gtk::manage(new Gtk::Table());
    int nameColNum = _tree.append_column("Name", *_text_renderer) - 1;
    _name_column = _tree.get_column(nameColNum);
    if( _name_column ) {
        _name_column->add_attribute(_text_renderer->property_text(), _model->m_col_name);
        //lbl2.set_tooltip_text(_("Layer/Group/Object label (inkscape:label). Double-click to set. Default value is object 'id'."));
        lbl2->show();
        _name_column->set_widget( *lbl2 );
    }
	
	//_new_layer_button.set_hexpand(false);

	Gtk::Label * lbl3 = new Gtk::Label("Keyframes");
	_keyframe_table.attach(*lbl3, 0, 1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	_keyframe_table.attach(*kb, 0, 1, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	_keyframe_table.attach(*kb2, 0, 1, 2, 3, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	_keyframe_table.attach(*kb3, 0, 1, 3, 4, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	_keyframe_table.attach(*kb4, 0, 1, 4, 5, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	
	_scroller.add(_keyframe_table);
	_scroller.set_policy(Gtk::POLICY_ALWAYS, Gtk::POLICY_NEVER);
	
	_tree_scroller.add(_tree);
	_tree_scroller.set_policy(Gtk::POLICY_ALWAYS, Gtk::POLICY_NEVER);

	_panes.add1(_tree_scroller);
	_panes.add2(_scroller);
	//add(_panes);
	attach(_panes, 0, 1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	attach(_new_layer_button, 0, 1, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	
	show_all_children();
}

AnimationControl::~AnimationControl()
{
}

bool AnimationControl::on_expose_event(GtkWidget * widget, GdkEventExpose* event)
{
	return true;
}

bool AnimationControl::on_my_button_press_event(GdkEventButton*)
{
	return false;
}

bool AnimationControl::on_mouse_(GdkEventMotion* event)
{
	return true;
}

bool AnimationControl::on_my_focus_in_event(GdkEventFocus*)
{
	return false;
}



