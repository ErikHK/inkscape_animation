#include <ctime>
#include <cmath>
#include <cairomm/context.h>
#include <glibmm/main.h>
#include "desktop.h"
#include "document.h"
#include "inkscape.h" // for SP_ACTIVE_DESKTOP
#include "layer-fns.h" //LPOS_ABOVE
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
_new_layer_button("New Layer"), num_layers(0)
{
	
	_new_layer_button.signal_clicked().connect(sigc::mem_fun(*this, &AnimationControl::addLayer));
	
	
	//Create the tree model and store
	Gtk::Label * lbl = new Gtk::Label("ID");
	Gtk::Label * lbl2 = new Gtk::Label("Animation Layer");
	Gtk::CellRendererText *_text_renderer;
	Gtk::TreeView m_TreeView;
	
    ModelColumns *zoop = new ModelColumns();
    _model = zoop;

    _store = Gtk::TreeStore::create( *zoop );
		
	Glib::RefPtr<Gtk::ListStore> m_refTreeModel;
	
	m_refTreeModel = Gtk::ListStore::create(*_model);
	m_TreeView.set_model(m_refTreeModel);

	
	
	
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
	_tree.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
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
	
	
	rebuildUi();
}



AnimationControl::~AnimationControl()
{
}


void AnimationControl::rebuildUi()
{
	//clear tree thingy
	_store->clear();

	//add a label that says keyframes, otherwise it won't line up...
	Gtk::Label * lbl3 = new Gtk::Label("Keyframes");
	_keyframe_table.attach(*lbl3, 0, 1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	
	for(int i = 0; i < num_layers; i++)
	{
		Gtk::TreeModel::iterator iter = _store->append();
		Gtk::TreeModel::Row row = *iter;
		row[_model->m_col_id] = i+1;
		row[_model->m_col_name] = Glib::ustring::format("animationlayer", i+1);
		
		KeyframeBar* kb = new KeyframeBar(i+1);
		_keyframe_table.attach(*kb, 0, 1, i+1, i+2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	}
	
	_scroller.add(_keyframe_table);
	_scroller.set_policy(Gtk::POLICY_ALWAYS, Gtk::POLICY_NEVER);
	
	_tree_scroller.add(_tree);
	_tree_scroller.set_policy(Gtk::POLICY_ALWAYS, Gtk::POLICY_NEVER);

	_panes.add1(_tree_scroller);
	_panes.add2(_scroller);
	//add(_panes);
	attach(_panes, 0, 1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	attach(_new_layer_button, 0, 1, 1, 2, Gtk::SHRINK, Gtk::SHRINK);
	
	show_all_children();
}

void AnimationControl::addLayer()
{
	
	Glib::RefPtr<Gtk::TreeSelection> selection = _tree.get_selection();
	
	//Gtk::TreeModel::iterator iter = selection->get_selected();
	//iter++;
    //Gtk::TreeModel::Row row = *iter;
	num_layers++;
	
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	
	//try to add a layer
	if(desktop)
	{
		SPObject * lay = Inkscape::create_animation_layer(desktop->currentRoot(), desktop->currentLayer(), Inkscape::LPOS_ABOVE);
		if(!lay)
			return;
		
		desktop->setCurrentLayer(lay);
		//SPObject * nextLayer = desktop->namedview->document->getObjectById(ids);
		
		for(int i=0;i < 25;i++)
			Inkscape::create_animation_keyframe(desktop->currentRoot(), desktop->currentLayer(), i+1);
	}
	
	rebuildUi();
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



