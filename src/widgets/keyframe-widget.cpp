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

#include "ui/tools-switch.h"

#include "style.h"

#include "keyframe-bar.h"
#include "selection-chemistry.h"
#include "document-undo.h"
#include "verbs.h"

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
using Inkscape::DocumentUndo;

//static void gotFocus(GtkWidget*, void * data);
//static Gtk::Menu * pMenu = 0;

std::vector<SPObject *> LAYERS_TO_HIDE;

static void gotFocus(GtkWidget* w, GdkEventKey *event, gpointer callback_data)
{
	
}

bool KeyframeWidget::on_my_focus_in_event(GdkEventFocus*)
{
	//pMenu = 0;
	selectLayer();
	return false;
}

bool KeyframeWidget::on_my_focus_out_event(GdkEventFocus* event)
{
	
	//pMenu = 0;
	//KeyframeWidget* kw = dynamic_cast<KeyframeWidget*>(event->window);
	
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	
	if(!desktop)
		return false;
	
	//onion->set_active(desktop->fade_previous_layers);
	
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
		
		if(id > 1 && nex->widgets[id-2]->layer)
			LAYERS_TO_HIDE.push_back(nex->widgets[id-2]->layer);
		
		nex = nex->next;
	}

	KeyframeBar * pre = parent->prev;
	
	while(parent && pre && pre->widgets[id-1]->layer)
	{
		LAYERS_TO_HIDE.push_back(pre->widgets[id-1]->layer);
		
		if(id > 1 && pre->widgets[id-2]->layer)
			LAYERS_TO_HIDE.push_back(pre->widgets[id-2]->layer);
		
		pre = pre->prev;
	}
	
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

				
	if(LAYERS_TO_HIDE.size() > 0)
	{
		for(int i = 0; i < LAYERS_TO_HIDE.size(); i++)
		{
			LAYERS_TO_HIDE[i]->getRepr()->setAttribute("style", "opacity:1.0");
			if(!desktop->show_all_keyframes)
				SP_ITEM(LAYERS_TO_HIDE[i])->setHidden(true);
			
		}
		LAYERS_TO_HIDE.clear();
	}
	
	if(desktop->show_all_keyframes)
		return;

	//show current and parent
	if(parent->is_visible)
	{
		SP_ITEM(layer)->setHidden(false);
		if(desktop->fade_previous_layers && layer)
			layer->getRepr()->setAttribute("style", "opacity:1.0");

		if(layer->parent)
			SP_ITEM(layer->parent)->setHidden(false);
		
		if(desktop->fade_previous_layers && prev && prev->layer)
		{
			SP_ITEM(prev->layer)->setHidden(false);
			prev->layer->getRepr()->setAttribute("style", "opacity:0.5");
			
			if(prev->prev && prev->prev->layer)
			{
				prev->prev->layer->getRepr()->setAttribute("style", "opacity:1.0");
				SP_ITEM(prev->prev->layer)->setHidden(true);
			}
		}
	}
	//////////also check if other layers exist, then show them as well, if they are set to be shown!//////////
	KeyframeBar * next_kb = parent->next;
	
	while(next_kb)
	{
		if(next_kb->is_visible && next_kb->layer)
		{
			SP_ITEM(next_kb->layer)->setHidden(false);
			if(next_kb->widgets[id-1]->layer)
				SP_ITEM(next_kb->widgets[id-1]->layer)->setHidden(false);
			
			//also onion skinning
			if(id > 1)
			{
				if(next_kb->widgets[id-2]->layer && desktop->fade_previous_layers)
				{
					next_kb->widgets[id-2]->layer->getRepr()->setAttribute("style", "opacity:.5");
					SP_ITEM(next_kb->widgets[id-2]->layer)->setHidden(false);
				}
			}
		}
		
		next_kb = next_kb->next;
	}
	
	next_kb = parent->prev;
	while(next_kb)
	{
		if(next_kb->is_visible && next_kb->layer)
		{
			SP_ITEM(next_kb->layer)->setHidden(false);
			if(next_kb->widgets[id-1]->layer)
				SP_ITEM(next_kb->widgets[id-1]->layer)->setHidden(false);
			
			
			//also onion skinning
			if(id > 1)
			{
				if(next_kb->widgets[id-2]->layer && desktop->fade_previous_layers)
				{
					next_kb->widgets[id-2]->layer->getRepr()->setAttribute("style", "opacity:.5");
					SP_ITEM(next_kb->widgets[id-2]->layer)->setHidden(false);
				}
			}
			
		}	
		next_kb = next_kb->prev;
	}
}


static void insertKeyframe(KeyframeWidget * kww, gpointer user_data)
{
	KeyframeWidget* kw = reinterpret_cast<KeyframeWidget*>(user_data);
	
	KeyframeWidget * p = kw->prev;
	
	if(!p)
		return;
	
	if(!p->layer)
		return;

	while(p)
	{
		//break if a p with at least 1 child is found
		if(p->layer->getRepr()->childCount() >= 1)
			break;
		
		p = p->prev;
	}
	
	if(!p)
		return;
	
	if(p && p->layer && p->layer->getRepr()->childCount() > 0)
	{
		Inkscape::XML::Node * childn_copy = NULL;
		Inkscape::XML::Node * childn = p->layer->getRepr()->firstChild();
		if(childn)
			childn_copy = childn->duplicate(SP_ACTIVE_DESKTOP->getDocument()->getReprDoc());
		if(childn_copy && kw->layer->getRepr()->childCount() == 0)
			kw->layer->getRepr()->appendChild(childn_copy);
	}
}

static void onionSkinning(KeyframeWidget * kww, gpointer user_data)
{
	KeyframeWidget* kw = reinterpret_cast<KeyframeWidget*>(user_data);
	
	if(SP_ACTIVE_DESKTOP && kw->onion)
		SP_ACTIVE_DESKTOP->fade_previous_layers = kw->onion->get_active();
}

static void settings(KeyframeWidget * kww, gpointer user_data)
{
	KeyframeWidget* kw = reinterpret_cast<KeyframeWidget*>(user_data);
	
}

static void showAllKeyframes(KeyframeWidget * kww, gpointer user_data)
{
	KeyframeWidget* kw = reinterpret_cast<KeyframeWidget*>(user_data);
	
	SPDesktop * desktop = SP_ACTIVE_DESKTOP;
	
	if(!desktop)
		return;
	
	//pMenu = 0;
	if(SP_ACTIVE_DESKTOP && kw->showAll)
		SP_ACTIVE_DESKTOP->show_all_keyframes = kw->showAll->get_active();
	
	SP_ACTIVE_DESKTOP->toggleHideAllLayers(!kw->showAll->get_active());
	
	//show layer1
	SPObject * layer = desktop->getDocument()->getObjectById("layer1");
	
	if(layer)
		SP_ITEM(layer)->setHidden(false);
	
	kw->layer->getRepr()->setAttribute("style", "opacity:1.0");
	if(kw->prev && kw->prev->layer)
		kw->prev->layer->getRepr()->setAttribute("style", "opacity:1.0");
}


static NodeTool *get_node_tool()
{
    NodeTool *tool = 0;
    if (SP_ACTIVE_DESKTOP ) {
        Inkscape::UI::Tools::ToolBase *ec = SP_ACTIVE_DESKTOP->event_context;
        if (INK_IS_NODE_TOOL(ec)) {
            tool = static_cast<NodeTool*>(ec);
        }
    }
    return tool;
}


static void createTween(KeyframeWidget * kww, gpointer user_data)
{
	KeyframeWidget* kw = reinterpret_cast<KeyframeWidget*>(user_data);
	
	//pMenu = 0;
	SPDesktop * desktop = SP_ACTIVE_DESKTOP;
	
	if(!desktop)
		return;
	
	SPObject * layer = kw->layer;// = desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", kw->id)));
	SPObject * startLayer = kw->layer;
	SPObject * endLayer = NULL;
	SPObject * nextLayer = NULL;
	float start_x=0, start_y=0, end_x=0, end_y=0, inc_x=0, inc_y=0, start_opacity=1, end_opacity=1, inc_opacity=0, inc_r=0, inc_g=0, inc_b=0;
	gfloat start_rgb[3];
	gfloat end_rgb[3];
	std::string xs = "x";
	std::string ys = "y";
	bool is_group = false;
	bool is_path = false;
	int i = kw->id+1;
	int num_layers = 1;
	
	std::vector<Node *> start_nodes;
	std::vector<Node *> end_nodes;
	
	std::vector<Geom::Point> inc_node_pos;
	std::vector<Geom::Point> inc_node_front_handle;
	std::vector<Geom::Point> inc_node_back_handle;
	
	//desktop->toggleHideAllLayers(true);
	//desktop->toggleLockAllLayers(true);
	
	//SP_ITEM(startLayer)->setLocked(true);
	
	while(layer)
	{
		//layer = Inkscape::next_layer(desktop->currentRoot(), layer);
		layer = desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", i)));
		
		if(!layer)
			return;
		
		//as soon as a layer has a child, break and set endLayer to this!
		if (layer->getRepr()->childCount() > 0)
			break;

		num_layers++;
		i++;
	}
	endLayer = layer;
	
	if(endLayer)
	{
		Inkscape::XML::Node * childn = endLayer->getRepr()->firstChild();
		SPObject * child = endLayer->firstChild();
		
		if(!childn)
			return;
		
		if(!child)
			return;
		
		//get end color
		sp_color_get_rgb_floatv (&SP_ITEM(child)->style->fill.value.color, end_rgb);
		
		//get opacity
		end_opacity = SP_ITEM(child)->style->opacity.value;
		
		
		Inkscape::SelectionHelper::selectNone(desktop);
		desktop->setCurrentLayer(endLayer);
		//SP_ITEM(endLayer)->setHidden(false);
		//SP_ITEM(endLayer)->setLocked(false);
		
		//tools_switch(desktop, TOOLS_SELECT);
		
		Inkscape::SelectionHelper::selectAll(desktop);
		tools_switch(desktop, TOOLS_NODES);
		
		//get nodes
		NodeTool *tool = get_node_tool();
			
		if(tool)
		{
			Inkscape::UI::ControlPointSelection *cps = tool->_selected_nodes;
			cps->selectAll();
			Node *n = NULL;
			
			for (Inkscape::UI::ControlPointSelection::iterator ii = cps->begin(); ii != cps->end(); ++ii) {
				n = dynamic_cast<Node *>(*ii);
				end_nodes.push_back(n);
			}
			cps->clear();
		}
		
		
		//Inkscape::SelectionHelper::selectNone(desktop);
		
		
		//if a circle or an ellipse, use cx and cy
		if(!strcmp(childn->name(), "svg:circle") || !strcmp(childn->name(), "svg:ellipse"))
		{
			xs = "cx";
			ys = "cy";
		}
		
		if(!strcmp(childn->name(), "svg:path"))
		{
			is_path = true;
		}
		
		//else if a group, take special care later!
		if(!strcmp(childn->name(), "svg:g"))
		{
			is_group = true;
		}
		
		if(!is_group && !is_path)
		{
			end_x = std::stof(childn->attribute(xs.c_str()));
			end_y = std::stof(childn->attribute(ys.c_str()));
		}
		
		/*
		if(is_path)
		{
			//end_x = SP_ITEM(child)->transform.translation()[0];
			//end_y = SP_ITEM(child)->transform.translation()[1];
			
			
			NodeTool *tool = get_node_tool();
			
			if(tool)
			{
				Inkscape::UI::ControlPointSelection *cps = tool->_selected_nodes;
				cps->selectAll();
				Node *n = dynamic_cast<Node *>(*cps->begin());
				
				if(n)
				{
					PathManipulator &pm = n->nodeList().subpathList().pm();
					Geom::Point pos = n->position();
					end_x = pos[0];
					end_y = pos[1];
				}
				else
				{
					end_x = SP_ITEM(child)->getCenter()[0];
					end_y = SP_ITEM(child)->getCenter()[1];
				}
			}
			
			//convert
			//end_x = Quantity::convert(end_x, "px", "mm");
			//end_y = desktop->getDocument()->getHeight().value("mm") - Quantity::convert(end_y, "px", "mm");
		}
		*/
	}
	
	//SP_ITEM(startLayer)->setLocked(false);
	
	if(startLayer)
	{
		//SP_ITEM(startLayer)->setHidden(false);
		
		SPObject * child = startLayer->firstChild();
		Inkscape::XML::Node * childn = startLayer->getRepr()->firstChild();
		if(!childn)
			return;
		
		if(!child)
			return;
		
		sp_color_get_rgb_floatv (&SP_ITEM(child)->style->fill.value.color, start_rgb);
		
		//get opacity
		start_opacity = SP_ITEM(child)->style->opacity.value;
		
		
		Inkscape::SelectionHelper::selectNone(desktop);
		desktop->setCurrentLayer(startLayer);
		//SP_ITEM(startLayer)->setHidden(false);
		//SP_ITEM(startLayer)->setLocked(false);
		//tools_switch(desktop, TOOLS_SELECT);
		
		Inkscape::SelectionHelper::selectAll(desktop);
		tools_switch(desktop, TOOLS_NODES);
		
		//get nodes
		NodeTool *tool = get_node_tool();
		
		if(tool)
		{
			Inkscape::UI::ControlPointSelection *cps = tool->_selected_nodes;
			cps->selectAll();
			Node *n = NULL;
			
			for (Inkscape::UI::ControlPointSelection::iterator ii = cps->begin(); ii != cps->end(); ++ii) {
				n = dynamic_cast<Node *>(*ii);
				start_nodes.push_back(n);
			}
			cps->clear();
		}
		//Inkscape::SelectionHelper::selectNone(desktop);
		
		
		if(!is_group && !is_path)
		{
			start_x = std::stof(childn->attribute(xs.c_str()));
			start_y = std::stof(childn->attribute(ys.c_str()));
		}
		
		if(is_group)
		{
			start_x = SP_ITEM(child)->transform.translation()[0];
			start_y = SP_ITEM(child)->transform.translation()[1];
		}
		/*
		if(is_path)
		{
			
			NodeTool *tool = get_node_tool();
			
			if(tool)
			{
				Inkscape::UI::ControlPointSelection *cps = tool->_selected_nodes;
				cps->selectAll();
				Node *n = dynamic_cast<Node *>(*cps->begin());
				
				if(n)
				{
					PathManipulator &pm = n->nodeList().subpathList().pm();
					Geom::Point pos = n->position();
					start_x = pos[0];
					start_y = pos[1];
				}
				else
				{
					start_x = SP_ITEM(child)->getCenter()[0];
					start_y = SP_ITEM(child)->getCenter()[1];
				}
			}
			
			//convert
			//start_x = Quantity::convert(start_x, "px", "mm");
			//start_y = desktop->getDocument()->getHeight().value("mm") - Quantity::convert(start_y, "px", "mm");
		}
		*/
	}
	
	for (int i = 0; i < end_nodes.size(); i++) {
		inc_node_pos.push_back( (end_nodes[i]->position() - start_nodes[i]->position())/num_layers  );
		//inc_node_front_handle.push_back( (end_nodes[i]->front()->position() - start_nodes[i]->front()->position())/num_layers);
		//inc_node_back_handle.push_back( (end_nodes[i]->back()->position() - start_nodes[i]->back()->position())/num_layers);
		
		//inc_node_front_handle.push_back((end_nodes[i]->front()->relativePos())/num_layers);
		//inc_node_back_handle.push_back((end_nodes[i]->back()->relativePos())/num_layers);
		
		inc_node_front_handle.push_back( 
		(end_nodes[i]->front()->relativePos() - start_nodes[i]->front()->relativePos()) / num_layers  );
		
		inc_node_back_handle.push_back( 
		(end_nodes[i]->back()->relativePos() - start_nodes[i]->back()->relativePos()) / num_layers  );
		
	}
	
	inc_x = (end_x - start_x)/(num_layers);
	inc_y = (end_y - start_y)/(num_layers);
	
	inc_r = (end_rgb[0]-start_rgb[0])/num_layers;
	inc_g = (end_rgb[1]-start_rgb[1])/num_layers;
	inc_b = (end_rgb[2]-start_rgb[2])/num_layers;
	
	inc_opacity = (end_opacity - start_opacity)/(num_layers);
	
	//NodeList::iterator node_iter = NodeList::get_iterator(n);
	
	//now we have start and end, loop again, and copy children etcetc
	layer = startLayer;
	
	i = 1;
	while(layer != endLayer)
	{
		nextLayer = desktop->getDocument()->getObjectById(
		std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", kw->id + i)));
		
		if(!nextLayer)
			break;
		
		//SPObject * child = layer->firstChild();
		SPObject * child = layer->firstChild();
		
		if(!child)
			break;
		
		//tween color TODO: check if it already has color...
        //paint_res->setColor(d[0], d[1], d[2]);
		SP_ITEM(child)->style->fill.clear();
		//SP_ITEM(child)->style->fill.setColor(i*16777000/num_layers, 0, 0);
		SP_ITEM(child)->style->fill.setColor(start_rgb[0] + (i-1)*inc_r,
											start_rgb[1] + (i-1)*inc_g,
											start_rgb[2] + (i-1)*inc_b);
		SP_ITEM(child)->style->fill.colorSet = TRUE;
		SP_ITEM(child)->style->fill.set = TRUE;
		
		//tween opacity
		SP_ITEM(child)->style->opacity.value = start_opacity + (i-1)*inc_opacity;
		
		//if(nextLayer == endLayer)
		//	break;
		
		if(is_path)
		{
			//SP_PATH(child)->transform.setTranslation(Geom::Point(start_x + i*inc_x, start_y + i*inc_y));
			
			Inkscape::SelectionHelper::selectNone(desktop);
			desktop->setCurrentLayer(layer);
			//SP_ITEM(layer)->setHidden(false);
			//SP_ITEM(layer)->setLocked(false);
			//tools_switch(desktop, TOOLS_SELECT);
			
			Inkscape::SelectionHelper::selectAll(desktop);
			tools_switch(desktop, TOOLS_NODES);
			// TODO remove the tools_switch atrocity.
			//if (!tools_isactive(desktop, TOOLS_NODES)) {
			//	tools_switch(desktop, TOOLS_NODES);
			//}
			
			NodeTool *tool = get_node_tool();
			
			if(tool)
			{
				Inkscape::UI::ControlPointSelection *cps = tool->_selected_nodes;
				cps->selectAll();
				
				if(cps && !cps->empty())
				{
					
					Node *n = NULL;
					int j = 0;
					for (Inkscape::UI::ControlPointSelection::iterator ii = cps->begin(); ii != cps->end(); ++ii) {
						n = dynamic_cast<Node *>(*ii);
					
						if(n)
						{
							PathManipulator &pm = n->nodeList().subpathList().pm();
							//Geom::Point pos = n->position();
							//n->move(pos + Geom::Point(200, 0));
							//n->move(n->position() + (i-1)*inc_node_pos[i-1]);
							//n->setRelativePos((i-1)*inc_node_pos[i-1]);
							//n->front()->setRelativePos((i-1)*inc_node_front_handle[j]);
							//n->back()->setRelativePos((i-1)*inc_node_back_handle[j]);
							
							n->front()->setRelativePos( end_nodes[j]->front()->relativePos() + (i-1)*inc_node_front_handle[j]);
							n->back()->setRelativePos( end_nodes[j]->back()->relativePos() + (i-1)*inc_node_back_handle[j]);
							//n->updateHandles();
							pm.update();
							//child->updateRepr(); //this fucks it up, why??
							//pm._selection.clear();
							//cps->clear();
							//Inkscape::SelectionHelper::selectNone(desktop);
						}
						j++;
					}
					
					cps->clear();
				}
			}
		}
		
		//child->updateRepr();
		Inkscape::XML::Node * childn = child->getRepr();
		Inkscape::XML::Node * childn_copy = childn->duplicate(desktop->getDocument()->getReprDoc());
		
		if(!is_group && !is_path)
		{
			childn_copy->setAttribute(xs.c_str(), Glib::ustring::format(start_x + i*inc_x));
			childn_copy->setAttribute(ys.c_str(), Glib::ustring::format(start_y + i*inc_y));
		}
		else if(is_group && !is_path)
		{
			childn_copy->setAttribute("transform", 
			Glib::ustring::format("translate(", start_x + i*inc_x, ",", start_y + i*inc_y, ")" ));
		}
		
		//Inkscape::XML::Node *childn = desktop->getDocument()->getReprDoc()->createElement("svg:path");
		//childn->setAttribute("style", style);
		//copy layer childn to nextLayer
		if(childn && childn_copy)
		{
			nextLayer->getRepr()->appendChild(childn_copy);
		}
		
		//child->updateRepr();
		//layer->updateRepr();
		//nextLayer->updateRepr();
		layer = nextLayer;
		i++;
	}
	
	/*
	layer = startLayer;
	i = 1;
	while(layer != endLayer)
	{
		Inkscape::SelectionHelper::selectNone(desktop);
		nextLayer = desktop->getDocument()->getObjectById(
		std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", kw->id + i)));
		
		if(!nextLayer)
			break;
		
		layer = nextLayer;
		desktop->setCurrentLayer(layer);
		SPObject * child = layer->firstChild();
		
		if(!child)
			break;
		
		if(layer == endLayer)
			break;
		
		if(is_path)
		{
			//tools_switch(desktop, TOOLS_NODES);
			
			Inkscape::SelectionHelper::selectAll(desktop);
			// TODO remove the tools_switch atrocity.
			if (!tools_isactive(desktop, TOOLS_NODES)) {
				tools_switch(desktop, TOOLS_NODES);
			}
			
			NodeTool *tool = get_node_tool();
			
			if(tool)
			{
				Inkscape::UI::ControlPointSelection *cps = tool->_selected_nodes;
				cps->selectAll();
				
				if(!cps->empty())
				{
					
					Node *n = NULL;
					//for (Inkscape::UI::ControlPointSelection::iterator ii = cps->begin(); ii != cps->end(); ++ii) {
					//	n = dynamic_cast<Node *>(*ii);
					//}
					n = dynamic_cast<Node *>(*cps->begin());
					
					if(n)
					{
						PathManipulator &pm = n->nodeList().subpathList().pm();
						Geom::Point pos = n->position();
						n->move(pos + Geom::Point(200, 0));
						pm.update();
					}
				}
				
				cps->clear();
			}
		}
		i++;
	}
	*/
	
	desktop->toggleHideAllLayers(true);
	//desktop->toggleLockAllLayers(false);
	SP_ITEM(startLayer->parent)->setHidden(false);
	//SP_ITEM(startLayer->parent)->setLocked(false);

	//if(is_path)
	//	return;
	
	desktop->setCurrentLayer(startLayer);
	
	kw->parent->clear_tween = true;
	
	DocumentUndo::done(desktop->getDocument(), SP_VERB_CONTEXT_NODE, "Create tween");
	
	
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	return;
	
	
	NodeTool *tool = get_node_tool();
	
	if(!tool)
		return;
	
	Inkscape::UI::ControlPointSelection *cps = tool->_selected_nodes;
	
	if(!cps)
		return;
	
	//have selected nodes, check if we have one object each in startlayer and endlayer, in that case, return
	if(startLayer->getRepr()->childCount() == 1 && endLayer->getRepr()->childCount() == 1)
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
			//inc = 1 - 1/(num_layers);
			//mult = inc;
			mult = 1;
			
			for(int k=0; k < num_layers/(sizeee); k++)
			{
				mult -= (sizeee)*1.0/(num_layers+1);
				pm.insertNode(this_iter, mult/(mult + (sizeee)*1.0/(num_layers+1)), false);
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
					Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", ii+1));

			if(lay)
			{
				if(lay->getRepr()->childCount() == 0)
					return;
				
				Inkscape::XML::Node * childn = lay->getRepr()->firstChild();
				if(!childn)
					return;
				
				if(!is_group && !is_path)
				{
					childn->setAttribute(xs, Glib::ustring::format(x));
					childn->setAttribute(ys, Glib::ustring::format(y));
				}
				
				else if(is_group) //is group!
				{
					childn->setAttribute("transform", Glib::ustring::format("translate(", x, ",", y, ")" ));
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
	
	if (event->type == GDK_BUTTON_PRESS  &&  event->button == 3)
	{
		onion->set_active(SP_ACTIVE_DESKTOP->fade_previous_layers);
		showAll->set_active(SP_ACTIVE_DESKTOP->show_all_keyframes);
		
		pMenu->show_all();
		pMenu->popup(event->button, event->time);
	}
}

KeyframeWidget::KeyframeWidget(int _id, KeyframeBar * _parent, SPObject * _layer, bool _is_empty)
{
	parent = _parent;
	layer = _layer;
	id = _id;
	
	next = NULL;
	prev = NULL;
	
	pMenu = new Gtk::Menu();
	

	Gtk::MenuItem *pItem3 = new Gtk::MenuItem("Insert keyframe");
	
	g_signal_connect( pItem3->gobj(),
						  "activate",
						  G_CALLBACK(insertKeyframe),
						  this);
	
	Gtk::MenuItem *tweenItem = new Gtk::MenuItem("Create tween");
	
	g_signal_connect( tweenItem->gobj(),
						  "activate",
						  G_CALLBACK(createTween),
						  this);
	
	//Gtk::MenuItem *pItem2 = new Gtk::MenuItem("Show all keyframes");
	showAll = Gtk::manage(new Gtk::CheckMenuItem("Show all keyframes"));
	showAll->set_active(false);
	
	g_signal_connect( showAll->gobj(),
						  "activate",
						  G_CALLBACK(showAllKeyframes),
						  this);
						  
	onion = Gtk::manage(new Gtk::CheckMenuItem("Onion skinning"));
	onion->set_active(true);
	
	g_signal_connect( onion->gobj(),
						  "activate",
						  G_CALLBACK(onionSkinning),
						  this);
						  
	settingsItem = Gtk::manage(new Gtk::MenuItem("Settings..."));
	
	g_signal_connect( settingsItem->gobj(),
						  "activate",
						  G_CALLBACK(settings),
						  this);

	pMenu->add(*pItem3);
	pMenu->add(*tweenItem);
	pMenu->add(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	pMenu->add(*showAll);
	pMenu->add(*onion);
	pMenu->add(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	pMenu->add(*settingsItem);

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
	
	
	
	add_events(Gdk::ALL_EVENTS_MASK);
	
	signal_button_press_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_button_press_event));
	signal_focus_in_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_focus_in_event));
	signal_focus_out_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_focus_out_event));
	set_can_focus(true);
	//set_receives_default();
    set_sensitive();
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
	
	if(parent->clear_tween)
	{
		//SP_ACTIVE_DESKTOP->toggleHideAllLayers(true);
		//parent->clear_tween = false;
	}
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
	
	return true;
}
