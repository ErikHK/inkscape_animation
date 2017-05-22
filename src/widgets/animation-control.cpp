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
#include <gdk/gdkkeysyms.h>
#include "ui/widget/layertypeicon.h"
#include "preferences.h"
#include <gtkmm/icontheme.h>
#include <gtkmm/stock.h>

#include "style.h"
#include "desktop-style.h"
#include "widgets/icon.h"

#include <glibmm/i18n.h>
#include <glibmm/main.h>

#include "inkscape.h"
#include "document-undo.h"

#include "ui/widget/imagetoggler.h"
#include "ui/icon-names.h"

namespace Inkscape {
namespace UI {
namespace Dialogs {
	
//using namespace Inkscape::UI::Tools;
using Inkscape::DocumentUndo;

class AnimationControl::ModelColumns : public Gtk::TreeModel::ColumnRecord
{
	public:
		ModelColumns()
		{ add(m_col_visible); add(m_col_locked); add(m_col_name); add(m_col_object);}

		Gtk::TreeModelColumn<bool> m_col_visible;
		Gtk::TreeModelColumn<bool> m_col_locked;
		Gtk::TreeModelColumn<Glib::ustring> m_col_name;
		Gtk::TreeModelColumn<KeyframeBar*> m_col_object;
};

bool AnimationControl::handleKeyEvent(GdkEventKey *event)
{
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	if(!desktop)
		return false;
	
	
	//switch (Inkscape::UI::Tools::get_group0_keyval(event)) {
		switch (event->keyval) {
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
        case GDK_KEY_F2:
        {
            Gtk::TreeModel::iterator iter = _tree.get_selection()->get_selected();
            if (iter && !_text_renderer->property_editable()) {
                //Rename item
                Gtk::TreeModel::Path *path = new Gtk::TreeModel::Path(iter);
                _text_renderer->property_editable() = true;
                _tree.set_cursor(*path, *_name_column, true);
                grab_focus();
                return true;
            }
        }
        break;

        default:
            return false;
    }
	
	
	
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
		bool newValue = !row[_model->m_col_visible];
		row[_model->m_col_visible] = newValue;
		//item->setHidden(!newValue);
		obj->is_visible = newValue;
		//item->updateRepr();
		//DocumentUndo::done( _desktop->doc() , SP_VERB_DIALOG_LAYERS,
		//					newValue? _("Unhide layer") : _("Hide layer"));
    }
}

void AnimationControl::toggleLocked( Glib::ustring const& str )
{
	
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	if(!desktop)
		return;
	
	Gtk::TreeModel::Children::iterator iter = _tree.get_model()->get_iter(str);
    Gtk::TreeModel::Row row = *iter;

    KeyframeBar* obj = row[_model->m_col_object];
    SPItem* item = ( obj && obj->layer && SP_IS_ITEM(obj->layer) ) ? SP_ITEM(obj->layer) : 0;
    if ( obj ) {
		bool newValue = !row[_model->m_col_locked];
		row[_model->m_col_locked] = newValue;
		if(item)
			item->setLocked(newValue);
		//item->setHidden(!newValue);
		//obj->is_visible = newValue;
		//item->updateRepr();
		//DocumentUndo::done( _desktop->doc() , SP_VERB_DIALOG_LAYERS,
		//					newValue? _("Unhide layer") : _("Hide layer"));
    }
}

/*
bool AnimationControl::_handleKeyEvent(GdkEventKey *event)
{

    //bool empty = _desktop->selection->isEmpty();

    switch (Inkscape::UI::Tools::get_group0_keyval(event)) {
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
        case GDK_KEY_F2:
        {
            Gtk::TreeModel::iterator iter = _tree.get_selection()->get_selected();
            if (iter && !_text_renderer->property_editable()) {
                //Rename item
                Gtk::TreeModel::Path *path = new Gtk::TreeModel::Path(iter);
                _text_renderer->property_editable() = true;
                _tree.set_cursor(*path, *_name_column, true);
                grab_focus();
                return true;
            }
        }
        break;
		
        case GDK_KEY_Home:
            //Move item(s) to top of containing group/layer
            _fireAction( empty ? SP_VERB_LAYER_TO_TOP : SP_VERB_SELECTION_TO_FRONT );
            break;
        case GDK_KEY_End:
            //Move item(s) to bottom of containing group/layer
            _fireAction( empty ? SP_VERB_LAYER_TO_BOTTOM : SP_VERB_SELECTION_TO_BACK );
            break;
        case GDK_KEY_Page_Up:
        {
            //Move item(s) up in containing group/layer
            //int ch = event->state & GDK_SHIFT_MASK ? SP_VERB_LAYER_MOVE_TO_NEXT : SP_VERB_SELECTION_RAISE;
            //_fireAction( empty ? SP_VERB_LAYER_RAISE : ch );
			SPObject * prev_lay = Inkscape::previous_layer(_desktop->currentRoot(), _desktop->currentLayer());
			_desktop->layer_manager->setCurrentLayer(prev_lay);
            break;
        }
        case GDK_KEY_Page_Down:
        {
            //Move item(s) down in containing group/layer
            //int ch = event->state & GDK_SHIFT_MASK ? SP_VERB_LAYER_MOVE_TO_PREV : SP_VERB_SELECTION_LOWER;
            //_fireAction( empty ? SP_VERB_LAYER_LOWER : ch );
			SPObject * next_lay = Inkscape::next_layer(_desktop->currentRoot(), _desktop->currentLayer());
			_desktop->layer_manager->setCurrentLayer(next_lay);
            break;
        }

        //TODO: Handle Ctrl-A, etc.
        default:
            return false;
    }
    return true;
}
*/

void AnimationControl::_handleEditingCancelled()
{
    _text_renderer->property_editable() = false;
}

void AnimationControl::_handleEdited(const Glib::ustring& path, const Glib::ustring& new_text)
{
    Gtk::TreeModel::iterator iter = _tree.get_model()->get_iter(path);
    Gtk::TreeModel::Row row = *iter;

    _renameObject(row, new_text);
    _text_renderer->property_editable() = false;
}

void AnimationControl::_renameObject(Gtk::TreeModel::Row row, const Glib::ustring& name)
{
	SPDesktop * _desktop = SP_ACTIVE_DESKTOP;
	
    if ( row && _desktop) {
         //Glib::ustring str = row[_model->m_col_name];
		 row[_model->m_col_name] = name;
		 /*
        if ( item ) {
            gchar const* oldLabel = item->label();
            if ( !name.empty() && (!oldLabel || name != oldLabel) ) {
                item->setLabel(name.c_str());
                DocumentUndo::done( _desktop->doc() , SP_VERB_NONE,
                                                    "Rename object");
            }
        }
		*/
    }
}

AnimationControl::AnimationControl() : 
_panes(), _keyframe_table(), _scroller(), _tree_scroller(),
_new_layer_button("New Layer"), num_layers(0)
{
	_new_layer_button.signal_clicked().connect(sigc::mem_fun(*this, &AnimationControl::addLayer));
	//signal_key_press_event().connect( sigc::mem_fun(*this, &AnimationControl::handleKeyEvent), false );
	
	//Create the tree model and store
	Gtk::Label * lbl = new Gtk::Label("ID");
	Gtk::Label * lbl2 = new Gtk::Label("Animation Layer");
	//Gtk::Label * lbl3 = new Gtk::Label("Visibility");
	
	/*
	_add = Gtk::manage( new Gtk::Button() );
	_rem = Gtk::manage( new Gtk::Button() );
	_bottom = Gtk::manage( new Gtk::Button() );
	_top = Gtk::manage( new Gtk::Button() );
	_down = Gtk::manage( new Gtk::Button() );
	_up = Gtk::manage( new Gtk::Button() );
	*/
	
	
    //Add object/layer
    Gtk::Button* _add = Gtk::manage( new Gtk::Button() );
    _styleButton(*_add, INKSCAPE_ICON("list-add"), _("Add animation layer"));
    _add->set_relief(Gtk::RELIEF_NONE);
    _add->signal_clicked().connect( sigc::mem_fun(*this, &AnimationControl::addLayer) );
    //_buttonsSecondary.pack_start(*btn, Gtk::PACK_SHRINK);
	//attach(*btn, 0, 1, 1, 2, Gtk::SHRINK, Gtk::SHRINK);
	_buttons.pack_start(*_add, Gtk::PACK_SHRINK);

    //Remove object
    _rem = Gtk::manage( new Gtk::Button() );
    _styleButton(*_rem, INKSCAPE_ICON("list-remove"), _("Remove object"));
    _rem->set_relief(Gtk::RELIEF_NONE);
	_rem->signal_clicked().connect( sigc::mem_fun(*this, &AnimationControl::removeLayer) );
	_buttons.pack_start(*_rem, Gtk::PACK_SHRINK);

    //Move to bottom
    _bottom = Gtk::manage( new Gtk::Button() );
    _styleButton(*_bottom, INKSCAPE_ICON("go-bottom"), _("Move To Bottom"));
    _bottom->set_relief(Gtk::RELIEF_NONE);
    //btn->signal_clicked().connect( sigc::bind( sigc::mem_fun(*this, &ObjectsPanel::_takeAction), (int)BUTTON_BOTTOM) );
	_buttons.pack_start(*_bottom, Gtk::PACK_SHRINK);
    
    //Move down
    _down = Gtk::manage( new Gtk::Button() );
    _styleButton(*_down, INKSCAPE_ICON("go-down"), _("Move Down"));
    _down->set_relief(Gtk::RELIEF_NONE);
    //btn->signal_clicked().connect( sigc::bind( sigc::mem_fun(*this, &ObjectsPanel::_takeAction), (int)BUTTON_DOWN) );
    //_watchingNonBottom.push_back( btn );
    //_buttonsPrimary.pack_end(*btn, Gtk::PACK_SHRINK);
	//attach(*btn, 0, 1, 1, 2, Gtk::SHRINK, Gtk::SHRINK);
	_buttons.pack_start(*_down, Gtk::PACK_SHRINK);
    
    //Move up
    _up = Gtk::manage( new Gtk::Button() );
    _styleButton(*_up, INKSCAPE_ICON("go-up"), _("Move Up"));
    _up->set_relief(Gtk::RELIEF_NONE);
    //btn->signal_clicked().connect( sigc::bind( sigc::mem_fun(*this, &ObjectsPanel::_takeAction), (int)BUTTON_UP) );
	_buttons.pack_start(*_up, Gtk::PACK_SHRINK);
    
    //Move to top
    _top = Gtk::manage( new Gtk::Button() );
    _styleButton(*_top, INKSCAPE_ICON("go-top"), _("Move To Top"));
    _top->set_relief(Gtk::RELIEF_NONE);
    //btn->signal_clicked().connect( sigc::bind( sigc::mem_fun(*this, &ObjectsPanel::_takeAction), (int)BUTTON_TOP) );
    //_watchingNonTop.push_back( btn );
    //__buttonsPrimary.pack_end(*btn, Gtk::PACK_SHRINK);
	//attach(*btn, 0, 1, 1, 2, Gtk::SHRINK, Gtk::SHRINK);
	_buttons.pack_start(*_top, Gtk::PACK_SHRINK);
	
	
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
	
	_tree.signal_key_press_event().connect( sigc::mem_fun(*this, &AnimationControl::handleKeyEvent), false );

	
    //_tree.set_reorderable(true);
    //_tree.enable_model_drag_dest (Gdk::ACTION_MOVE);
	//_tree.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
    Gtk::TreeViewColumn* col;
	
	_text_renderer = Gtk::manage(new Gtk::CellRendererText());
	
	//Set up the label editing signals
    _text_renderer->signal_edited().connect( sigc::mem_fun(*this, &AnimationControl::_handleEdited));
    _text_renderer->signal_editing_canceled().connect( sigc::mem_fun(*this, &AnimationControl::_handleEditingCancelled));
	
	Inkscape::UI::Widget::ImageToggler *eyeRenderer = Gtk::manage( new Inkscape::UI::Widget::ImageToggler(
        INKSCAPE_ICON("object-visible"), INKSCAPE_ICON("object-hidden")) );
		
    int visibleColNum = _tree.append_column("vis", *eyeRenderer) - 1;
	
	eyeRenderer->signal_toggled().connect( sigc::mem_fun(*this, &AnimationControl::toggleVisible) ) ;
	
	//eyeRenderer->signal_toggled().connect( sigc::bind( sigc::mem_fun(*this, &LayersPanel::_toggled), (int)1) );
    eyeRenderer->property_activatable() = true;
    col = _tree.get_column(visibleColNum);
    if ( col ) {
        col->add_attribute( eyeRenderer->property_active(), _model->m_col_visible );
        lbl->show();
        col->set_widget( *lbl );
    }
	
	Inkscape::UI::Widget::ImageToggler * renderer = Gtk::manage( new Inkscape::UI::Widget::ImageToggler(
        INKSCAPE_ICON("object-locked"), INKSCAPE_ICON("object-unlocked")) );
    
	int lockedColNum = _tree.append_column("lock", *renderer) - 1;
	
	renderer->signal_toggled().connect(sigc::mem_fun(*this, &AnimationControl::toggleLocked));
	
	renderer->property_activatable() = true;
    col = _tree.get_column(lockedColNum);
    if ( col ) {
        col->add_attribute( renderer->property_active(), _model->m_col_locked );
        //_lockHeader.set_tooltip_text(_("Toggle lock of Layer, Group, or Object."));
        //_lockHeader.show();
        //col->set_widget( _lockHeader );
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
	
	//check if layers already exist (a saved svg has been opened)
	if(desktop)
	{
		int i = 1;
		SPObject * child = desktop->namedview->document->getObjectById(std::string(Glib::ustring::format("animationlayer", i)));
		while(child)
		{
			if(num_layers < i)
				num_layers++;
			i++;
			child = desktop->namedview->document->getObjectById(std::string(Glib::ustring::format("animationlayer", i)));
		}
	}
	
	for(int i = 0; i < num_layers; i++)
	{
		if(kb_vec.size() <= i)
		{
			SPObject * child = desktop->namedview->document->getObjectById(std::string(Glib::ustring::format("animationlayer", i+1)));
			
			Gtk::TreeModel::iterator iter = _store->append();
			Gtk::TreeModel::Row row = *iter;
			row[_model->m_col_visible] = true;
			row[_model->m_col_locked] = false;
			row[_model->m_col_name] = Glib::ustring::format("animationlayer", i+1);
			
			KeyframeBar* kb = new KeyframeBar(i+1, child);
			row[_model->m_col_object] = kb;
			_keyframe_table.attach(*kb, 0, 1, i+1, i+2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
			kb_vec.push_back(kb);
		}
	}
	
	//TODO: iterate over keyframebars and set pointers to siblings
	if(kb_vec.size() > 0)
	{
		kb_vec[0]->prev = NULL;
		kb_vec[0]->next = NULL;
	}
	if(kb_vec.size() > 1)
	{
		kb_vec[0]->next = kb_vec[1];
		kb_vec[1]->prev = kb_vec[0];
	}
	if(kb_vec.size() > 2)
	{
		kb_vec[1]->next = kb_vec[2];
		kb_vec[2]->prev = kb_vec[1];
	}
	
	if(kb_vec.size() > 3)
	{
		kb_vec[2]->next = kb_vec[3];
		kb_vec[3]->prev = kb_vec[2];
	}
	
	if(kb_vec.size() > 4)
	{
		kb_vec[3]->next = kb_vec[4];
		kb_vec[4]->prev = kb_vec[3];
	}
	
	if(kb_vec.size() > 5)
	{
		//for(int i=0; i < kb_vec.size()-1; i++)
		//{
		//	kb_vec[i]->next = kb_vec[i+1];
		//	kb_vec[i+1]->prev = kb_vec[i];
		//}
		kb_vec[4]->next = kb_vec[5];
		kb_vec[5]->prev = kb_vec[4];
	}
	
	_scroller.add(_keyframe_table);
	_scroller.set_policy(Gtk::POLICY_ALWAYS, Gtk::POLICY_NEVER);
	
	_tree_scroller.add(_tree);
	_tree_scroller.set_policy(Gtk::POLICY_ALWAYS, Gtk::POLICY_NEVER);

	_panes.add1(_tree_scroller);
	_panes.add2(_scroller);
	//add(_panes);
	attach(_panes, 0, 1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	
	//attach(_tree_scroller, 0, 1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	//attach(_scroller, 1, 2, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	
	
	//attach(_new_layer_button, 0, 1, 1, 2, Gtk::SHRINK, Gtk::SHRINK);
	
	attach(_buttons, 0, 1, 1, 2, Gtk::SHRINK, Gtk::SHRINK);
	
	show_all_children();
}

void AnimationControl::_styleButton(Gtk::Button& btn, char const* iconName, char const* tooltip)
{
    GtkWidget *child = sp_icon_new( Inkscape::ICON_SIZE_SMALL_TOOLBAR, iconName );
    gtk_widget_show( child );
    btn.add( *Gtk::manage(Glib::wrap(child)) );
    btn.set_relief(Gtk::RELIEF_NONE);
    btn.set_tooltip_text (tooltip);
}

void AnimationControl::removeLayer()
{
	//Glib::RefPtr<Gtk::TreeSelection> selection = _tree.get_selection();
	
	//_tree.remove(_tree.get_selection());
	
	num_layers--;
	rebuildUi();
}

void AnimationControl::addLayer()
{
	Glib::RefPtr<Gtk::TreeSelection> selection = _tree.get_selection();
	
	//Gtk::TreeModel::iterator iter = selection->get_selected();
	//iter++;
    //Gtk::TreeModel::Row row = *iter;
	num_layers++;
	
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	SPObject * prevLayer = NULL;
	SPObject * lay = NULL;
	//try to add a layer
	if(desktop)
	{
		if(num_layers > 1)
			prevLayer = desktop->namedview->document->getObjectById(std::string(Glib::ustring::format("animationlayer", num_layers-1)));
		//SPObject * lay = Inkscape::create_animation_layer(desktop->currentRoot(), desktop->currentLayer(), Inkscape::LPOS_ABOVE);
		
		if(prevLayer)
			lay = Inkscape::create_animation_layer(desktop->currentRoot(), prevLayer, Inkscape::LPOS_BELOW);
		else
			lay = Inkscape::create_animation_layer(desktop->currentRoot(), desktop->currentRoot(), Inkscape::LPOS_BELOW);
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
