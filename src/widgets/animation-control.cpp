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
#include "verbs.h"

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
		//DocumentUndo::done( desktop->doc() , SP_VERB_DIALOG_LAYERS,
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
		//DocumentUndo::done( desktop->doc() , SP_VERB_DIALOG_LAYERS,
		//					newValue? _("Unlock layer") : _("Lock layer"));
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
		 KeyframeBar * kb = row[_model->m_col_object];
		 
		 kb->layer->getRepr()->setAttribute("name", name);
		 
		 
		 //DocumentUndo::done( _desktop->doc() , SP_VERB_NONE,
         //                                           "Rename object");
		 
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


/**
 * Default row selection function taken from the layers dialog
 */
bool AnimationControl::_rowSelectFunction( Glib::RefPtr<Gtk::TreeModel> const & /*model*/, Gtk::TreeModel::Path const & /*path*/, bool currentlySelected )
{
    bool val = true;
    if ( !currentlySelected && _toggleEvent )
	//	if ( !currentlySelected)
    {
        GdkEvent* event = gtk_get_current_event();
        if ( event ) {
            // (keep these checks separate, so we know when to call gdk_event_free()
            if ( event->type == GDK_BUTTON_PRESS ) {
                GdkEventButton const* target = reinterpret_cast<GdkEventButton const*>(_toggleEvent);
                GdkEventButton const* evtb = reinterpret_cast<GdkEventButton const*>(event);

                if ( (evtb->window == target->window)
                     && (evtb->send_event == target->send_event)
                     && (evtb->time == target->time)
                     && (evtb->state == target->state)
                    )
                {
                    // Ooooh! It's a magic one
                    val = false;
                }
            }
            gdk_event_free(event);
        }
    }
    return val;
}


bool AnimationControl::_handleButtonEvent(GdkEventButton* event)
{
    static unsigned doubleclick = 0;
	
	/*
	//single click
	if ( event->type == GDK_BUTTON_RELEASE && !doubleclick) {
        Gtk::TreeModel::Path path;
        Gtk::TreeViewColumn* col = 0;
        int x = static_cast<int>(event->x);
        int y = static_cast<int>(event->y);
        int x2 = 0;
        int y2 = 0;
        if ( _tree.get_path_at_pos( x, y, path, col, x2, y2 ) && col == _name_column) {
            _tree.set_cursor (path, *_name_column, true);
            //grab_focus();
			return true;
        }
    }
	*/
    //Right mouse button was clicked, launch the pop-up menu
    if ( (event->type == GDK_BUTTON_PRESS) && (event->button == 3) ) {
        Gtk::TreeModel::Path path;
        int x = static_cast<int>(event->x);
        int y = static_cast<int>(event->y);
        if ( _tree.get_path_at_pos( x, y, path ) ) {
            //_checkTreeSelection();
            _popupMenu.popup(event->button, event->time);
            if (_tree.get_selection()->is_selected(path)) {
                return true;
            }
        }
    }
	
	
	//Restore the selection function to allow tree selection on mouse button release
    //if ( event->type == GDK_BUTTON_RELEASE) {
    //    _tree.get_selection()->set_select_function(sigc::mem_fun(*this, &AnimationControl::_rowSelectFunction));
    //}
	
	
	//Second mouse button press, set double click status for when the mouse is released
    if ( (event->type == GDK_2BUTTON_PRESS) && (event->button == 1) ) {
        doubleclick = 1;
    }

    //Double click on mouse button release, if we're over the label column, edit
    //the item name
    if ( event->type == GDK_BUTTON_RELEASE && doubleclick) {
        doubleclick = 0;
        Gtk::TreeModel::Path path;
        Gtk::TreeViewColumn* col = 0;
        int x = static_cast<int>(event->x);
        int y = static_cast<int>(event->y);
        int x2 = 0;
        int y2 = 0;
        if ( _tree.get_path_at_pos( x, y, path, col, x2, y2 ) && col == _name_column) {
            // Double click on the Layer name, enable editing
            _text_renderer->property_editable() = true;
            _tree.set_cursor (path, *_name_column, true);
            grab_focus();
        }
    }
	
	
	
	return false;
}


AnimationControl::AnimationControl() : 
_panes(), _keyframe_table(), _scroller(), _tree_scroller(),
_new_layer_button("New Layer"), num_layers(0), _toggleEvent(0)
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
	
	//_changed_signal.emit();
	
	
	Gtk::MenuItem *rename = new Gtk::MenuItem("Rename");
	_popupMenu.append(*rename);
	Gtk::MenuItem *duplicate = new Gtk::MenuItem("Duplicate");
	_popupMenu.append(*duplicate);
	Gtk::MenuItem *neww = new Gtk::MenuItem("New");
	_popupMenu.append(*neww);
	
	_popupMenu.append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	
	Gtk::MenuItem *solo = new Gtk::MenuItem("Solo");
	_popupMenu.append(*solo);
	Gtk::MenuItem *showall = new Gtk::MenuItem("Show All");
	_popupMenu.append(*showall);
	Gtk::MenuItem *hideall = new Gtk::MenuItem("Hide All");
	_popupMenu.append(*hideall);
	
	_popupMenu.show_all_children();
	
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
    _styleButton(*_rem, INKSCAPE_ICON("list-remove"), _("Remove animation layer"));
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
	_down->signal_clicked().connect( sigc::mem_fun(*this, &AnimationControl::moveLayerDown) );
    //_watchingNonBottom.push_back( btn );
    //_buttonsPrimary.pack_end(*btn, Gtk::PACK_SHRINK);
	//attach(*btn, 0, 1, 1, 2, Gtk::SHRINK, Gtk::SHRINK);
	_buttons.pack_start(*_down, Gtk::PACK_SHRINK);
    
    //Move up
    _up = Gtk::manage( new Gtk::Button() );
    _styleButton(*_up, INKSCAPE_ICON("go-up"), _("Move Up"));
    _up->set_relief(Gtk::RELIEF_NONE);
	_up->signal_clicked().connect( sigc::mem_fun(*this, &AnimationControl::moveLayerUp) );
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
	
	_tree.signal_button_press_event().connect( sigc::mem_fun(*this, &AnimationControl::_handleButtonEvent), false );
    //_tree.signal_button_release_event().connect( sigc::mem_fun(*this, &AnimationControl::_handleButtonEvent), false );
	_tree.signal_key_press_event().connect( sigc::mem_fun(*this, &AnimationControl::handleKeyEvent), false );
	//_tree.get_selection()->set_select_function( sigc::mem_fun(*this, &AnimationControl::_rowSelectFunction) );



	_scroller.set_shadow_type(Gtk::SHADOW_NONE);
	_scroller.add(_keyframe_table);
	_scroller.set_policy(Gtk::POLICY_ALWAYS, Gtk::POLICY_NEVER);

	_tree_scroller.set_size_request(300);

	_tree_scroller.set_shadow_type(Gtk::SHADOW_NONE);
	_tree_scroller.add(_tree); //this fails!
	_tree_scroller.set_policy(Gtk::POLICY_ALWAYS, Gtk::POLICY_NEVER);

	_panes.add1(_tree_scroller);
	_panes.add2(_scroller);
	//add(_panes);
	attach(_panes, 0, 1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);

	//attach(_tree_scroller, 0, 1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	//attach(_scroller, 1, 2, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);

	//attach(_new_layer_button, 0, 1, 1, 2, Gtk::SHRINK, Gtk::SHRINK);

	attach(_buttons, 0, 1, 1, 2, Gtk::SHRINK, Gtk::SHRINK);

    //_tree.set_reorderable(true);
    //_tree.enable_model_drag_dest (Gdk::ACTION_MOVE);
	//_tree.get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
    Gtk::TreeViewColumn* col;
	
	_text_renderer = Gtk::manage(new Gtk::CellRendererText());
	
	_text_renderer->set_fixed_size(-1,20);

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
	/*
	if(desktop)
	{
		int i = 1;
		SPObject * child = desktop->namedview->document->getObjectById(std::string(Glib::ustring::format("animationlayer", i)));
		while(child)
		{
			if(num_layers < i)
				num_layers++;
			i++;
			//child = desktop->namedview->document->getObjectById(std::string(Glib::ustring::format("animationlayer", i)));
			child = Inkscape::next_layer(desktop->currentRoot(), child);
		}
	}
	*/
	/*
	for(int i = 0; i < kb_vec.size(); i++)
	{
		if(kb_vec.size() <= i)
		{
			SPObject * child = desktop->namedview->document->getObjectById(std::string(Glib::ustring::format("animationlayer", i+1)));
			
			if(child)
			{
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
	}
	*/
	

	//TODO: iterate over keyframebars and set pointers to siblings


	int s = kb_vec.size();

	if(s>0)
	{
		kb_vec[0]->prev = NULL;
		kb_vec[0]->next = NULL;
	}

	for(int i=0; i < s-1; i++)
	{
		kb_vec[i]->next = kb_vec[i+1];
		kb_vec[i+1]->prev = kb_vec[i];
	}

	if(s>1)
	{
		kb_vec[s-1]->prev = kb_vec[s-2];
		kb_vec[s-1]->next = NULL;
	}


	/*
	if(kb_vec.size() > 0)
	{
		kb_vec[0]->prev = NULL;
		kb_vec[0]->next = NULL;
	}
	if(kb_vec.size() > 1)
	{
		kb_vec[0]->next = kb_vec[1];
		kb_vec[1]->prev = kb_vec[0];
		kb_vec[1]->next = NULL;
	}
	if(kb_vec.size() > 2)
	{
		kb_vec[1]->next = kb_vec[2];
		kb_vec[2]->prev = kb_vec[1];
		kb_vec[2]->next = NULL;
	}
	
	if(kb_vec.size() > 3)
	{
		kb_vec[2]->next = kb_vec[3];
		kb_vec[3]->prev = kb_vec[2];
		kb_vec[3]->next = NULL;
	}
	
	if(kb_vec.size() > 4)
	{
		kb_vec[3]->next = kb_vec[4];
		kb_vec[4]->prev = kb_vec[3];
		kb_vec[4]->next = NULL;
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
		kb_vec[5]->next = NULL;
	}
	*/
	
	/*
	_scroller.set_shadow_type(Gtk::SHADOW_NONE);
	_scroller.add(_keyframe_table);
	_scroller.set_policy(Gtk::POLICY_ALWAYS, Gtk::POLICY_NEVER);
	
	_tree_scroller.set_size_request(300);
	
	_tree_scroller.set_shadow_type(Gtk::SHADOW_NONE);
	//_tree_scroller.add(_tree); //this fails!
	_tree_scroller.set_policy(Gtk::POLICY_ALWAYS, Gtk::POLICY_NEVER);

	//_panes.add1(_tree_scroller);
	//_panes.add2(_scroller);
	//add(_panes);
	attach(_panes, 0, 1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	
	//attach(_tree_scroller, 0, 1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	//attach(_scroller, 1, 2, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	
	//attach(_new_layer_button, 0, 1, 1, 2, Gtk::SHRINK, Gtk::SHRINK);
	
	attach(_buttons, 0, 1, 1, 2, Gtk::SHRINK, Gtk::SHRINK);
	*/
	
	_tree.set_size_request(-1, num_layers*22);


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
	
	SPDesktop * desktop = SP_ACTIVE_DESKTOP;
	if(!desktop)
		return;
	
	Gtk::TreeModel::iterator iterr = _tree.get_selection()->get_selected();
	if(!iterr)
		return;
	
    Gtk::TreeModel::Path *path = new Gtk::TreeModel::Path(iterr);
	if(!path)
		return;
	
	//Glib::RefPtr<Gtk::TreeSelection> selection = _tree.get_selection();
	
	//_tree.remove(_tree.get_selection());
	
	Gtk::TreeModel::iterator iter = _store->get_iter(*path);
	
	if(!iter)
		return;
	
	//Gtk::TreeModel::iterator iter = _tree.get_model()->get_iter(path);
    Gtk::TreeModel::Row row = *iter;
	
	KeyframeBar* kb = row[_model->m_col_object];
	if(!kb)
		return;
	
	int id = kb->id;

	//remove layer too
	SPObject * lay = kb->layer;
	
	if(lay)
	{
		//Inkscape::XML::Node * n = lay->getRepr();
		//if(n)
		//	n->parent()->removeChild(n);
		SP_ITEM(lay)->setHidden(true);
		
		_store->erase(iter);
		//layers.erase(layers.begin() + id);
		//look for correct kb in kb_vec
		int ind=0;
		for(int i=0;i < kb_vec.size(); i++)
		{
			if(kb_vec[i]->id == id)
				ind = i;
		}
		kb_vec.erase(kb_vec.begin() + ind);
		_keyframe_table.remove(*kb);
		
		num_layers--;
		rebuildUi();
	}
}

void AnimationControl::addLayer()
{
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
		
		//lay->getRepr()->setAttribute("num", num_layers);
		
		desktop->setCurrentLayer(lay);
		//SPObject * nextLayer = desktop->namedview->document->getObjectById(ids);
		
		//for(int i=0;i < 10;i++)
		//create only first frame layer
		Inkscape::create_animation_keyframe(desktop->currentRoot(), lay, 1);
	}
	
	//if(kb_vec.size() < num_layers)
	//{
		SPObject * child = lay;
		
		if(child)
		{
			Gtk::TreeModel::iterator iter = _store->append();
			Gtk::TreeModel::Row row = *iter;
			row[_model->m_col_visible] = true;
			row[_model->m_col_locked] = false;
			
			//row[_model->m_col_name] = Glib::ustring::format("animationlayer", num_layers);
			if(child->getRepr()->attribute("name"))
				row[_model->m_col_name] = Glib::ustring::format(child->getRepr()->attribute("name"));
			else
				row[_model->m_col_name] = Glib::ustring::format(child->getRepr()->attribute("id"));
			
			Glib::ustring str = Glib::ustring::format(child->getRepr()->attribute("num"));
			
			int place = atoi(str.c_str());
			
			//KeyframeBar* kb = new KeyframeBar(atoi(str.substr(str.length()-1, 1).c_str()), child);
			KeyframeBar* kb = new KeyframeBar(place, child);  //hmm här det kraschar?
			//KeyframeBar* kb = new KeyframeBar(num_layers, child);
			row[_model->m_col_object] = kb;
			_keyframe_table.attach(*kb, 0, 1, place-1, place, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
			kb_vec.push_back(kb);
		}
	//}
	
	rebuildUi();
}

void AnimationControl::moveLayer(int dir)
{
		SPDesktop * desktop = SP_ACTIVE_DESKTOP;
	if(!desktop)
		return;
	
	Gtk::TreeModel::iterator iterr = _tree.get_selection()->get_selected();
	if(!iterr)
		return;
	
    Gtk::TreeModel::Path *path = new Gtk::TreeModel::Path(iterr);
	if(!path)
		return;
	
	Gtk::TreeModel::iterator iter = _store->get_iter(*path);
	
	if(!iter)
		return;
	
    Gtk::TreeModel::Row row = *iter;


	
	KeyframeBar* kb = row[_model->m_col_object];
	
	if(!kb)
		return;
	
	int id = kb->id;

	SPObject * lay = kb->layer;
	
	if(lay)
	{

		//look for correct kb in kb_vec
		int ind=0;
		for(int i=0;i < kb_vec.size(); i++)
		{
			if(kb_vec[i]->id == id)
				ind = i;
		}

		if(ind > 0 && dir==1) //move up
		{
			KeyframeBar * pkb = kb->prev;
			if(pkb)
			{
				std::iter_swap(kb_vec.begin() + ind, kb_vec.begin() + ind - 1);
				_store->iter_swap(iter, iter--);
				
				_keyframe_table.remove(*kb);
				_keyframe_table.remove(*pkb);

				_keyframe_table.attach(*pkb, 0, 1, ind, ind+1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
				_keyframe_table.attach(*kb, 0, 1, ind-1, ind, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
				
				//change order of layers
				//if(pkb->prev)
				//	lay->getRepr()->parent()->changeOrder(lay->getRepr(), pkb->prev->layer->getRepr());
				
				SP_ITEM(lay)->raiseOne();
			}
			
		}
		
		else if(ind < kb_vec.size()-1 && dir==0) //move down
		{
			KeyframeBar * nkb = kb->next;
			if(nkb)
			{
				std::iter_swap(kb_vec.begin() + ind, kb_vec.begin() + ind + 1);
				_store->iter_swap(iter, iter++);
				
				_keyframe_table.remove(*kb);
				_keyframe_table.remove(*nkb);

				_keyframe_table.attach(*nkb, 0, 1, ind, ind+1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
				_keyframe_table.attach(*kb, 0, 1, ind+1, ind+2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
				
				//change order of layers
				//lay->getRepr()->parent()->changeOrder(lay->getRepr(), nkb->layer->getRepr());
				
				SP_ITEM(lay)->lowerOne();
			}
			
		}
		rebuildUi();
	}
}


void AnimationControl::moveLayerUp()
{
	moveLayer(1);
}

void AnimationControl::moveLayerDown()
{
	moveLayer(0);
}


bool AnimationControl::on_expose_event(GtkWidget * widget, GdkEventExpose* event)
{
	//rebuildUi();
	return false;
}

bool AnimationControl::on_my_button_press_event(GdkEventButton*)
{
	return false;
}

bool AnimationControl::on_mouse_(GdkEventMotion* event)
{
	return false;
}

bool AnimationControl::on_my_focus_in_event(GdkEventFocus*)
{
	return false;
}


}
}
}
