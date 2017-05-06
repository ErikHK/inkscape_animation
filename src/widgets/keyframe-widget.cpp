#include <ctime>
#include <cmath>
#include <cairomm/context.h>
#include <glibmm/main.h>
#include "keyframe-widget.h"
#include "desktop.h"
#include "document.h"
#include "inkscape.h" // for SP_ACTIVE_DESKTOP
#include "layer-fns.h" //LPOS_ABOVE
#include "layer-manager.h"
#include "layer-model.h"
//#include "ui/previewable.h"
#include "sp-namedview.h"

#include "keyframe-bar.h"

#include <gdkmm/general.h>
#include <gtkmm/menu.h>
#include "ui/tool/path-manipulator.h"
#include "ui/tool/multi-path-manipulator.h"
#include "ui/tools/node-tool.h"
#include "ui/tool/control-point-selection.h"

using Inkscape::UI::Tools::NodeTool;
using Inkscape::Util::Quantity;
using Inkscape::UI::Node;
using Inkscape::UI::PathManipulator;
using Inkscape::UI::NodeList;

//static void gotFocus(GtkWidget*, void * data);
static Gtk::Menu * pMenu = 0;

std::vector<SPObject *> LAYERS_TO_HIDE;

static void gotFocus(GtkWidget* w, GdkEventKey *event, gpointer callback_data)
{
	
}

bool KeyframeWidget::on_my_focus_in_event(GdkEventFocus*)
{
	pMenu = 0;
	selectLayer();
	return false;
}

bool KeyframeWidget::on_my_focus_out_event(GdkEventFocus* event)
{
	pMenu = 0;
	//KeyframeWidget* kw = dynamic_cast<KeyframeWidget*>(event->window);
	
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	
	if(!desktop)
		return false;
	
	//if(layer)
		//SP_ITEM(layer)->setHidden(true);
		//layer->getRepr()->setAttribute("style", "display:none");
	
	if(layer != NULL)
		LAYERS_TO_HIDE.push_back(layer);
		
	if(parent == NULL)
		return false;
	
	//if(parent->next == NULL)
	//	return false;
	KeyframeBar * nex = parent->next;
	
	while(parent && nex && nex->widgets[id-1]->layer)
	{
		LAYERS_TO_HIDE.push_back(nex->widgets[id-1]->layer);
		nex = nex->next;
	}

	KeyframeBar * pre = parent->prev;
	
	while(parent && pre && pre->widgets[id-1]->layer)
	{
		LAYERS_TO_HIDE.push_back(pre->widgets[id-1]->layer);
		pre = pre->next;
	}
	
	
	//if(parent && parent->next->widgets[id-1]->layer)
	//	parent->animationControl->layersToHide.push_back(parent->next->widgets[id-1]->layer);
	/*
	if(!parent)
		return false;
		
	if(!parent->next)
		return false;
	
	if(parent->next->widgets[id-1]->layer)
	{
		desktop->setCurrentLayer(parent->next->widgets[id-1]->layer);
		//SP_ITEM(parent->next->widgets[id-1]->layer)->setHidden(true);
		Inkscape::XML::Node * n = parent->next->widgets[id-1]->layer->getRepr();
		//SP_ITEM(parent->next->widgets[id-1]->layer)->getRepr()->setAttribute("style", "display:none");
		if(n)
			n->setAttribute("style", "display:none");
		//parent->next->layer->getRepr()->setAttribute("style", "display:none");
	}
	*/
	
	//selectLayer();
	//if(layer)
	//	SP_ITEM(layer)->setHidden(true);
	return false;
}

void KeyframeWidget::selectLayer()
{
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	
	if(!desktop)
		return;

	if(!layer)
		layer = desktop->namedview->document->getObjectById(
	Glib::ustring::format("animationlayer", parent_id, "keyframe", id));

	desktop->setCurrentLayer(layer);

	//hide all layers
	//TAKES A LOT OF TIME
	//desktop->toggleHideAllLayers(true);

	//show current and parent
	SP_ITEM(layer)->setHidden(false);
	if(layer->parent)
		SP_ITEM(layer->parent)->setHidden(false);
	
	//////////also check if other layers exist, then show them as well, if they are set to be shown!//////////
	KeyframeBar * next_kb = parent->next;
	
	while(next_kb)
	{
		if(next_kb->is_visible)
		{
			SP_ITEM(next_kb->layer)->setHidden(false);
			if(next_kb->widgets[id-1]->layer)
				SP_ITEM(next_kb->widgets[id-1]->layer)->setHidden(false);
		}
		//else
		//	LAYERS_TO_HIDE.push_back(next_kb->layer);
		
		next_kb = next_kb->next;
	}
	
	next_kb = parent->prev;
	while(next_kb)
	{
		if(next_kb->is_visible)
		{
			SP_ITEM(next_kb->layer)->setHidden(false);
			if(next_kb->widgets[id-1]->layer)
				SP_ITEM(next_kb->widgets[id-1]->layer)->setHidden(false);
		}
		
		next_kb = next_kb->prev;
	}
	
	if(LAYERS_TO_HIDE.size() > 0)
	{
		for(int i = 0; i < LAYERS_TO_HIDE.size(); i++)
			//LAYERS_TO_HIDE[i]->getRepr()->setAttribute("style", "display:none");
			SP_ITEM(LAYERS_TO_HIDE[i])->setHidden(true);
		LAYERS_TO_HIDE.clear();
	}
}

static void createTween(KeyframeWidget * kww, gpointer user_data)
{
	KeyframeWidget* kw = reinterpret_cast<KeyframeWidget*>(user_data);
	
	pMenu = 0;
	SPDesktop * desktop = SP_ACTIVE_DESKTOP;
	
	if(!desktop)
		return;
	
	SPObject * layer = kw->layer;// = desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", kw->id)));
	SPObject * startLayer = kw->layer;
	SPObject * endLayer;
	SPObject * nextLayer;
	float start_x=0, start_y=0, end_x=0, end_y=0, inc_x=0, inc_y=0;
	std::string xs = "x";
	std::string ys = "y";
	bool is_group = false;
	int i = kw->id+1;
	int num_layers = 1;
	while(layer)
	{
		//layer = Inkscape::next_layer(desktop->currentRoot(), layer);
		layer = desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", i)));
		
		//as soon as a layer has a child, break and set endLayer to this!
		if (layer->getRepr()->childCount() > 0)
			break;
		
		num_layers++;
		i++;
	}
	endLayer = layer;
	
	if(endLayer)
	{
		Inkscape::XML::Node * child = endLayer->getRepr()->firstChild();
		
		if(!child)
			return;
		
		//if a circle or an ellipse, use cx and cy
		if(!strcmp(child->name(), "svg:circle") || !strcmp(child->name(), "svg:ellipse"))
		{
			xs = "cx";
			ys = "cy";
		}
		
		//else if a group, take special care later!
		if(!strcmp(child->name(), "svg:g"))
		{
			is_group = true;
		}
		
		if(!is_group)
		{
			end_x = std::stof(child->attribute(xs.c_str()));
			end_y = std::stof(child->attribute(ys.c_str()));
		}
	}
	
	if(startLayer)
	{
		Inkscape::XML::Node * child = startLayer->getRepr()->firstChild();
		if(!child)
			return;

		if(!is_group)
		{
			start_x = std::stof(child->attribute(xs.c_str()));
			start_y = std::stof(child->attribute(ys.c_str()));
		}
	}
	
	inc_x = (end_x - start_x)/num_layers;
	inc_y = (end_y - start_y)/num_layers;
	
	//now we have start and end, loop again, and copy children etcetc
	layer = startLayer;
	i = kw->id+1;
	while(layer != endLayer)
	{
		//nextLayer = Inkscape::next_layer(desktop->currentRoot(), layer);
		nextLayer = desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", i)));
		
		if(nextLayer && layer)
		{
			Inkscape::XML::Node * child = layer->getRepr()->firstChild();
			Inkscape::XML::Node * child_copy = child->duplicate(desktop->getDocument()->getReprDoc());
			
			if(!is_group)
			{
				child_copy->setAttribute(xs.c_str(), Glib::ustring::format(start_x + i*inc_x));
				child_copy->setAttribute(ys.c_str(), Glib::ustring::format(start_y + i*inc_y));
			}
			
			//Inkscape::XML::Node *child = desktop->getDocument()->getReprDoc()->createElement("svg:path");
			//child->setAttribute("style", style);
			//copy layer child to nextLayer
			if(child)
			{
				nextLayer->getRepr()->appendChild(child_copy);
				//Inkscape::GC::release(child);
			}
		}
		layer = nextLayer;
		i++;
	}
		
		
	NodeTool *tool = 0;
    Inkscape::UI::Tools::ToolBase *ec = desktop->event_context;
	if (INK_IS_NODE_TOOL(ec)) {
		tool = static_cast<NodeTool*>(ec);
	}
	
	if(!tool)
		return;
	
	Inkscape::UI::ControlPointSelection *cps = tool->_selected_nodes;
	
	if(!cps)
		return;
	
	Node *n = dynamic_cast<Node *>(*cps->begin());
			if (!n) return;
	
	NodeList::iterator this_iter = NodeList::get_iterator(n);
	
	int sizeee = n->nodeList().size();
	
	//if(n->nodeList().begin() == n->nodeList().end())
	//	return;
	
	PathManipulator &pm = n->nodeList().subpathList().pm();
	
	
	//float inc = 1.0/num_layers;
	float inc = 1 - 1/num_layers;
	float mult = inc;
	
	

	if(!n->nodeList().closed())
	{
	
		for(int iii = 0; iii < sizeee-1; iii++)
		{
			inc = 1 - 1/num_layers;
			//mult = inc;
			mult = 1;
			
			for(int k=0; k < num_layers/(sizeee - 1); k++)
			{
				mult -= (sizeee-1)*1.0/num_layers;
				pm.insertNode(this_iter, mult/(mult + (sizeee-1)*1.0/num_layers), false);
			}
			
			pm._selection.clear();
			
			//go to next point and add more there etc
			for(int iiii=0; iiii < num_layers/(sizeee - 1) + 1; iiii++)
				this_iter++;
			
			if(!this_iter)
				break;
			
		}
	
	}
	//else the path is closed!
	else
	{
		for(int iii = 0; iii < sizeee; iii++)
		{
			inc = 1 - 1/num_layers;
			//mult = inc;
			mult = 1;
			
			for(int k=0; k < num_layers/(sizeee ); k++)
			{
				mult -= (sizeee)*1.0/num_layers;
				pm.insertNode(this_iter, mult/(mult + (sizeee)*1.0/num_layers), false);
			}
			
			pm._selection.clear();
			
			//go to next point and add more there etc
			for(int iiii=0; iiii < num_layers/(sizeee ) + 1; iiii++)
				this_iter++;
			
			if(!this_iter)
				break;
			
		}
	}
	
	int ii = 1;
	SPObject * lay;

	for(NodeList::iterator j = n->nodeList().begin(); j != n->nodeList().end(); ++j) {
		Node *node = dynamic_cast<Node*>(&*j);
		if (node) {
			//std::string id = node->nodeList().subpathList().pm().item()->getId(); 
			double x = Quantity::convert(node->position()[0], "px", "mm");
			double y = desktop->getDocument()->getHeight().value("mm") - Quantity::convert(node->position()[1], "px", "mm");
				
			//ii = std::distance(tool->_multipath->_selection.begin(), i);
			ii = std::distance(n->nodeList().begin(), j);
			//ii = (int)i - (int)tool->_multipath->_selection.begin();
			
			//lay = desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("layer", ii)));
			lay = desktop->namedview->document->getObjectById(
					Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", ii));

			if(lay)
			{
				if(lay->getRepr()->childCount() == 0)
					return;
				
				Inkscape::XML::Node * child = lay->getRepr()->firstChild();
				if(!child)
					return;
				
				if(!is_group)
				{
					child->setAttribute(xs, Glib::ustring::format(x));
					child->setAttribute(ys, Glib::ustring::format(y));
				}
				
				else //is group!
				{
					child->setAttribute("transform", Glib::ustring::format("translate(", x, ",", y, ")" ));
				}
			}
		}		
	}
}

bool KeyframeWidget::on_my_button_press_event(GdkEventButton* event)
{
	grab_focus();
	//gtk_widget_grab_focus(GTK_WIDGET(this));
	//gtk_widget_set_state( GTK_WIDGET(this), GTK_STATE_ACTIVE );
	
	//select layer that corresponds to this keyframe
	//selectLayer();
	
	if (event->type == GDK_BUTTON_PRESS  &&  event->button == 3 && !pMenu)
	{
		std::cout << "RIGHT-CLICK";
		pMenu = new Gtk::Menu();
		Gtk::MenuItem *pItem = new Gtk::MenuItem("Create tween");
		
		g_signal_connect( pItem->gobj(),
                              "activate",
                              G_CALLBACK(createTween),
                              this);
		
		Gtk::MenuItem *pItem2 = new Gtk::MenuItem("Something other");
		
		g_signal_connect( pItem2->gobj(),
                              "activate",
                              G_CALLBACK(createTween),
                              this);
		
		pMenu->add(*pItem);
		pMenu->add(*pItem2);
		pMenu->show_all();
		pMenu->popup(event->button, event->time);
	}
	
}

KeyframeWidget::KeyframeWidget(int _id, KeyframeBar * _parent, SPObject * _layer, bool _is_empty)
{
	parent = _parent;
	layer = _layer;
	id = _id;
	
	parent_id = parent->id;
	
	this->set_size_request(15, 21);
	
	set_can_focus(true);
	
	is_empty = _is_empty;
	
	
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	if(desktop)
	{
		Inkscape::Selection * selection = desktop->getSelection();
		sigc::connection _sel_changed_connection;
		//_sel_changed_connection = selection->connectChanged(
		//	sigc::bind(
		//		sigc::ptr_fun(&KeyframeWidget::on_selection_changed),
		//		desktop));
		
		_sel_changed_connection = selection->connectChanged(
		sigc::hide(sigc::mem_fun(*this, &KeyframeWidget::on_selection_changed)));
	}
	
}

KeyframeWidget::~KeyframeWidget()
{
}

void KeyframeWidget::on_selection_changed()
{
	if(layer)
	{
		if(layer->getRepr()->childCount() > 0)
			is_empty = false;
		else
			is_empty = true;
	}
	
	queue_draw();
}

bool KeyframeWidget::on_expose_event(GdkEventExpose* event)
{
	Glib::RefPtr<Gdk::Window> window = get_window();
	if(window)
	{
		Gtk::Allocation allocation = get_allocation();
		const int width = allocation.get_width();
		const int height = allocation.get_height();
		
		Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();
		cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

		if(id % 2 == 0)
			cr->set_source_rgba(1, 1, 1, 1);
		else
			cr->set_source_rgba(.8, .8, .8, 1);
		
		if(has_focus())
			cr->set_source_rgba(.8, 0, 0, 1);
		
		cr->paint();
		
		if(!is_empty)
		{
			cr->set_source_rgba(0, 0, 0, 1);
			cr->arc(width/2, height/2, width/4, 0, 6.283);
		}
		
		cr->fill();
	
		//add line to the bottom
		cr->set_source_rgba(.3,.3,.3,1);
		cr->set_line_width(1.0);
		cr->move_to(0, height-.5);
		cr->line_to(width, height-.5);
		cr->stroke();
  }

	add_events(Gdk::ALL_EVENTS_MASK);
	
	signal_button_press_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_button_press_event));
	signal_focus_in_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_focus_in_event));
	signal_focus_out_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_focus_out_event));
	set_can_focus(true);
	//set_receives_default();
    set_sensitive();
	
	return true;
}
