#include <ctime>
#include <cmath>
#include <cairomm/context.h>
#include <glibmm/main.h>
#include "desktop.h"
#include "document.h"
#include "inkscape.h" // for SP_ACTIVE_DESKTOP
#include "layer-fns.h" //LPOS_ABOVE
#include "animation-control.h"
#include <gdkmm/general.h>
#include <gtkmm/treeselection.h>
#include "layer-manager.h"

#include "ui/widget/imagetoggler.h"
#include "ui/icon-names.h"

namespace Inkscape {
namespace UI {
namespace Dialogs {

class AnimationControl::ModelColumns : public Gtk::TreeModel::ColumnRecord
{
	public:
		ModelColumns()
		{ add(m_col_id); add(m_col_name); add(m_col_object);}

		Gtk::TreeModelColumn<bool> m_col_id;
		Gtk::TreeModelColumn<Glib::ustring> m_col_name;
		Gtk::TreeModelColumn<KeyframeBar*> m_col_object;
};

bool AnimationControl::handleKeyEvent(GdkEventKey *event)
{
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	if(!desktop)
		return false;
	
	//if(Inkscape::UI::Tools::get_group0_keyval(event) == GDK_KEY_Page_Up)
		if(event->keyval == GDK_KEY_Page_Up)
	{
		SPObject * next_lay = Inkscape::next_layer(desktop->currentRoot(), desktop->currentLayer());
		if(next_lay)
			desktop->layer_manager->setCurrentLayer(next_lay);
	}
	//else if(Inkscape::UI::Tools::get_group0_keyval(event) == GDK_KEY_Page_Down)
	else if(event->keyval == GDK_KEY_Page_Down)
	{
		SPObject * next_lay = Inkscape::previous_layer(desktop->currentRoot(), desktop->currentLayer());
		if(next_lay)
			desktop->layer_manager->setCurrentLayer(next_lay);
	}
	
	return false;
}

void AnimationControl::toggleVisible( Glib::ustring const& str )
{
	
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	if(!desktop)
		return;
	
	Gtk::TreeModel::Children::iterator iter = _tree.get_model()->get_iter(str);
    Gtk::TreeModel::Row row = *iter;

    KeyframeBar* obj = row[_model->m_col_object];
    //SPItem* item = ( obj && SP_IS_ITEM(obj) ) ? SP_ITEM(obj) : 0;
    if ( obj ) {
		bool newValue = !row[_model->m_col_id];
		row[_model->m_col_id] = newValue;
		//item->setHidden(!newValue);
		obj->is_visible = newValue;
		//item->updateRepr();
		//DocumentUndo::done( _desktop->doc() , SP_VERB_DIALOG_LAYERS,
		//					newValue? _("Unhide layer") : _("Hide layer"));
    }
}

AnimationControl::AnimationControl() : 
_panes(), _keyframe_table(), _scroller(), _tree_scroller(),
_new_layer_button("New Layer"), num_layers(0)
{
	
	_new_layer_button.signal_clicked().connect(sigc::mem_fun(*this, &AnimationControl::addLayer));
	signal_key_press_event().connect( sigc::mem_fun(*this, &AnimationControl::handleKeyEvent), false );
	
	//Create the tree model and store
	Gtk::Label * lbl = new Gtk::Label("ID");
	Gtk::Label * lbl2 = new Gtk::Label("Animation Layer");
	//Gtk::Label * lbl3 = new Gtk::Label("Visibility");
	Gtk::CellRendererText *_text_renderer;
	Gtk::TreeView m_TreeView;
	
    ModelColumns *zoop = new ModelColumns();
    _model = zoop;

    _store = Gtk::TreeStore::create( *zoop );
		
	//Glib::RefPtr<Gtk::ListStore> m_refTreeModel;
	
	//m_refTreeModel = Gtk::ListStore::create(*_model);
	//m_TreeView.set_model(m_refTreeModel);
	
    //Set up the tree
    _tree.set_model( _store );
    _tree.set_headers_visible(false);
    //_tree.set_reorderable(true);
    //_tree.enable_model_drag_dest (Gdk::ACTION_MOVE);
	//_tree.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
    Gtk::TreeViewColumn* col;
	Gtk::TreeView::Column *_name_column;
	_text_renderer = Gtk::manage(new Gtk::CellRendererText());
	
	//Animation layer ID
	/*
	_text_renderer = Gtk::manage(new Gtk::CellRendererText());
    int idColNum = _tree.append_column("highlight", *_text_renderer) - 1;
    col = _tree.get_column(idColNum);
    if ( col ) {
        col->add_attribute( _text_renderer->property_text(), _model->m_col_id );
        //lbl.set_tooltip_text(_("Highlight color of outline in Node tool. Click to set. If alpha is zero, use inherited color."));
        lbl->show();
        col->set_widget( *lbl );
    }
	*/
	
	Inkscape::UI::Widget::ImageToggler *eyeRenderer = Gtk::manage( new Inkscape::UI::Widget::ImageToggler(
        INKSCAPE_ICON("object-visible"), INKSCAPE_ICON("object-hidden")) );
		
    int visibleColNum = _tree.append_column("vis", *eyeRenderer) - 1;
	eyeRenderer->signal_toggled().connect( sigc::mem_fun(*this, &AnimationControl::toggleVisible) ) ;
	//eyeRenderer->signal_toggled().connect( sigc::bind( sigc::mem_fun(*this, &LayersPanel::_toggled), (int)1) );
    eyeRenderer->property_activatable() = true;
    col = _tree.get_column(visibleColNum);
    if ( col ) {
        col->add_attribute( eyeRenderer->property_active(), _model->m_col_id );
        lbl->show();
        col->set_widget( *lbl );
    }
	
    //Label
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
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	//clear tree thingy
	//_store->clear();

	//add a label that says keyframes, otherwise it won't line up...
	//Gtk::Label * lbl3 = new Gtk::Label("Keyframes");
	//_keyframe_table.attach(*lbl3, 0, 1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	//i = 0, kb_vec.size() = 1
	for(int i = 0; i < num_layers; i++)
	{
		if(kb_vec.size() <= i)
		{
			SPObject * child = desktop->namedview->document->getObjectById(std::string(Glib::ustring::format("animationlayer", i+1)));
			
			Gtk::TreeModel::iterator iter = _store->append();
			Gtk::TreeModel::Row row = *iter;
			row[_model->m_col_id] = true;
			row[_model->m_col_name] = Glib::ustring::format("animationlayer", i+1);
			
			KeyframeBar* kb = new KeyframeBar(i+1, child);
			row[_model->m_col_object] = kb;
			_keyframe_table.attach(*kb, 0, 1, i+1, i+2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
			kb_vec.push_back(kb);
		}
	}
	
	//iterate over keyframebars and set pointers to siblings
	
	if(kb_vec.size() > 0)
	{
		kb_vec[0]->prev = NULL;
		kb_vec[0]->next = NULL;
	}
	if(kb_vec.size() > 1)
	{
		//for(int i=0; i < kb_vec.size()-1; i++)
		//{
		//	kb_vec[i]->next = kb_vec[i+1];
		//	kb_vec[i+1]->prev = kb_vec[i];
		//}
		kb_vec[0]->next = kb_vec[1];
		kb_vec[1]->prev = kb_vec[0];
	}
	if(kb_vec.size() > 2)
	{
		//for(int i=0; i < kb_vec.size()-1; i++)
		//{
		//	kb_vec[i]->next = kb_vec[i+1];
		//	kb_vec[i+1]->prev = kb_vec[i];
		//}
		kb_vec[1]->next = kb_vec[2];
		kb_vec[2]->prev = kb_vec[1];
	}
	
	if(kb_vec.size() > 3)
	{
		//for(int i=0; i < kb_vec.size()-1; i++)
		//{
		//	kb_vec[i]->next = kb_vec[i+1];
		//	kb_vec[i+1]->prev = kb_vec[i];
		//}
		kb_vec[2]->next = kb_vec[3];
		kb_vec[3]->prev = kb_vec[2];
	}
	
	if(kb_vec.size() > 4)
	{
		//for(int i=0; i < kb_vec.size()-1; i++)
		//{
		//	kb_vec[i]->next = kb_vec[i+1];
		//	kb_vec[i+1]->prev = kb_vec[i];
		//}
		kb_vec[3]->next = kb_vec[4];
		kb_vec[4]->prev = kb_vec[3];
	}
	
	
	//kb_vec[kb_vec.size()-1]->next = NULL;
	
	
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
		//SPObject * lay = Inkscape::create_animation_layer(desktop->currentRoot(), desktop->currentLayer(), Inkscape::LPOS_ABOVE);
		SPObject * lay = Inkscape::create_animation_layer(desktop->currentRoot(), desktop->currentRoot(), Inkscape::LPOS_ABOVE);
		if(!lay)
			return;
		
		desktop->setCurrentLayer(lay);
		//SPObject * nextLayer = desktop->namedview->document->getObjectById(ids);
		
		for(int i=0;i < 50;i++)
			Inkscape::create_animation_keyframe(desktop->currentRoot(), desktop->currentLayer(), i+1);
	}
	
	rebuildUi();
}

bool AnimationControl::on_expose_event(GtkWidget * widget, GdkEventExpose* event)
{
	//rebuildUi();
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


}
}
}
