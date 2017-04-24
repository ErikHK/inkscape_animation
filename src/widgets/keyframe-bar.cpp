#include <ctime>
#include <cmath>
#include <cairomm/context.h>
#include <glibmm/main.h>
#include "desktop.h"
#include "document.h"
#include "inkscape.h" // for SP_ACTIVE_DESKTOP
#include "layer-fns.h" //LPOS_ABOVE
#include "layer-manager.h"
#include "layer-model.h"
//#include "ui/previewable.h"
//#include "sp-namedview.h"
#include "keyframe-bar.h"
#include <gdkmm/general.h>

//static void gotFocus(GtkWidget*, void * data);

class KeyframeBar::ModelColumns : public Gtk::TreeModel::ColumnRecord
  {
  public:

    ModelColumns()
    { add(m_col_id); add(m_col_name);}

    Gtk::TreeModelColumn<unsigned int> m_col_id;
    Gtk::TreeModelColumn<Glib::ustring> m_col_name;
  };


static void gotFocus(GtkWidget* , GdkEventKey *event, gpointer callback_data)
{
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	//SPDocument *doc = SP_ACTIVE_DOCUMENT;
	//LayerManager * lm = desktop->layer_manager;
	
	//Glib::ustring strr = Glib::ustring::format(id);
	//Glib::ustring ids("layer" + strr);
	//ids = lm->getNextLayerName(NULL, desktop->currentLayer()->label());
	
	while(desktop->getDocument()->getReprRoot()->childCount() < 100)
	{
		SPObject * lay = Inkscape::create_layer(desktop->currentRoot(), desktop->currentLayer(), Inkscape::LPOS_ABOVE);
		//lm->setCurrentLayer(lay);
	}
	
}


KeyframeBar::KeyframeBar(int _id)
: btn("hehe"), btn2("hehe2")
{
	std::vector<Gtk::Widget*> wlist;
	
	id = _id;
	KeyframeWidget* kw = new KeyframeWidget(1);
	
	//attach(*kw, 0,1,0,1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	//attach(btn, 0,1,0,1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	//attach(btn2, 1,2,0,1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	
	//wlist.push_back(&btn);
	//wlist.push_back(&btn2);
	
	
	//btn.signal_clicked().connect( sigc::mem_fun(*this, &KeyframeBar::on_button_));
	
	/*
	g_signal_connect( G_OBJECT(tbl->gobj()),
					  "clicked",
					  G_CALLBACK(gotClicked),
					  NULL);
	*/
	
	for(int i=2;i < 100;i++)
	{
		kw = new KeyframeWidget(i+1);

		/*
		g_signal_connect( G_OBJECT(kw->gobj()),
					  "focus-in-event",
					  G_CALLBACK(gotFocus),
					  this);
			*/

		//attach(*kw, i, i+1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		
		//wlist.push_back(kw);
		
		//kw->add_events(Gdk::POINTER_MOTION_MASK|Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK |Gdk::PROXIMITY_IN_MASK|Gdk::PROXIMITY_OUT_MASK|Gdk::SCROLL_MASK|Gdk::FOCUS_CHANGE_MASK|Gdk::BUTTON_PRESS_MASK);
		
		add_events(Gdk::ALL_EVENTS_MASK);
		kw->add_events(Gdk::ALL_EVENTS_MASK);
		
		//kw->signal_focus_in_event().connect(sigc::mem_fun(*this, &KeyframeBar::on_my_focus_in_event));
		kw->signal_button_press_event().connect(sigc::mem_fun(*this, &KeyframeBar::on_my_button_press_event));
		//kw->signal_motion_notify_event().connect(sigc::mem_fun(*this, &KeyframeBar::on_mouse_));
		kw->set_can_focus(true);
	}
	
	//set_focus_chain(wlist);
	show_all_children();
	//set_focus_chain(wlist);
	
	
	//Create the tree model and store
	Gtk::Label * lbl = new Gtk::Label("hehehe");
	Gtk::Label * lbl2 = new Gtk::Label("hehehe2");
	Gtk::CellRendererText *_text_renderer;
	Gtk::TreeView m_TreeView;
	
    ModelColumns *zoop = new ModelColumns();
    _model = zoop;

    _store = Gtk::TreeStore::create( *zoop );
	
	Gtk::Button * btn3 = new Gtk::Button("heheheheheheh");
	
	Gtk::TreeModel::iterator iter = _store->prepend();
    Gtk::TreeModel::Row row = *iter;
	row[_model->m_col_id] = 1;
	row[_model->m_col_name] = "Billy Bob";
	kw->show();
		
	Glib::RefPtr<Gtk::ListStore> m_refTreeModel;
	
	m_refTreeModel = Gtk::ListStore::create(*_model);
	m_TreeView.set_model(m_refTreeModel);

	iter = _store->prepend();
	row = *iter;
	//Fill the TreeView's model
	row[_model->m_col_id] = 2;
	row[_model->m_col_name] = "Billy Bob2";
	
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
	
	
	
	
	int visibleColNum;// = _tree.append_column("vis", *eyeRenderer) - 1;
    Gtk::TreeViewColumn* col;
	Gtk::TreeView::Column *_name_column;

    //Label
    _text_renderer = Gtk::manage(new Gtk::CellRendererText());
	//_text_renderer = Gtk::manage(new Gtk::Table());
    int nameColNum = _tree.append_column("Name", *_text_renderer) - 1;
    _name_column = _tree.get_column(nameColNum);
    if( _name_column ) {
        _name_column->add_attribute(_text_renderer->property_text(), _model->m_col_name);
        //lbl2.set_tooltip_text(_("Layer/Group/Object label (inkscape:label). Double-click to set. Default value is object 'id'."));
        lbl2->show();
        _name_column->set_widget( *lbl2 );
    }
	
	//Animation layer ID
    int idColNum = _tree.append_column("highlight", *_text_renderer) - 1;
    col = _tree.get_column(idColNum);
    if ( col ) {
        col->add_attribute( _text_renderer->property_text(), _model->m_col_id );
        //lbl.set_tooltip_text(_("Highlight color of outline in Node tool. Click to set. If alpha is zero, use inherited color."));
        lbl->show();
        col->set_widget( *lbl );
    }

	
	add(_tree);
	show_all_children();
	
}

KeyframeBar::~KeyframeBar()
{
}

bool KeyframeBar::on_expose_event(GtkWidget * widget, GdkEventExpose* event)
{
	gtk_widget_grab_focus(widget);
	
	/*
	g_signal_connect( G_OBJECT(this->gobj()),
					  "focus-in-event",
					  G_CALLBACK(gotFocus),
					  NULL);
	*/
}

bool KeyframeBar::on_my_button_press_event(GdkEventButton*)
//bool KeyframeBar::on_my_button_press_event()
//void KeyframeBar::on_button_()
{
	//gtk_widget_grab_focus(this->gobj());
	grab_focus();
	//addLayers();
	return false;
}

bool KeyframeBar::on_mouse_(GdkEventMotion* event)
{
	addLayers();
	return true;
	
}

void KeyframeBar::addLayers()
{
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	//SPDocument *doc = SP_ACTIVE_DOCUMENT;
	//LayerManager * lm = desktop->layer_manager;
	
	//Glib::ustring strr = Glib::ustring::format(id);
	//Glib::ustring ids("layer" + strr);
	//ids = lm->getNextLayerName(NULL, desktop->currentLayer()->label());
	
	while(desktop->getDocument()->getReprRoot()->childCount() < 100)
	{
		SPObject * lay = Inkscape::create_layer(desktop->currentRoot(), desktop->currentLayer(), Inkscape::LPOS_ABOVE);
		//lm->setCurrentLayer(lay);
	}
}

bool KeyframeBar::on_my_focus_in_event(GdkEventFocus*)
{
	addLayers();
	return false;
}



