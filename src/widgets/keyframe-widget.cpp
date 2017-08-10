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
#include "ui/tool/event-utils.h"

//#include "sp-path.h"

#include "display/curve.h"
#include <2geom/pathvector.h>
#include <2geom/curves.h>
#include <2geom/transforms.h>
#include <helper/geom.h>
#include "helper/geom-curves.h"

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
using Inkscape::UI::MultiPathManipulator;
using Inkscape::UI::NodeList;
using Inkscape::DocumentUndo;

//static void gotFocus(GtkWidget*, void * data);
//static Gtk::Menu * pMenu = 0;

std::vector<SPObject *> LAYERS_TO_HIDE;

static void gotFocus(GtkWidget* w, GdkEventKey *event, gpointer callback_data)
{
	
}

bool KeyframeWidget::on_my_focus_in_event(GdkEventFocus* event)
{
	//pMenu = 0;
	selectLayer();
	is_focused = true;

	return false;
}


static gint playLoop(gpointer data)
{
	KeyframeWidget* kw = reinterpret_cast<KeyframeWidget*>(data);

	//SPDesktop * desktop = tb->desktop;
	static int i = kw->parent->animation_start;
	SPDesktop * desktop = SP_ACTIVE_DESKTOP;
	if(!desktop)
		return false;

	if(!desktop->is_playing)
		return false;

	SPObject * nextl = desktop->getDocument()->getObjectById(
			std::string(Glib::ustring::format("animationlayer", 1, "keyframe", i)));

	SPObject * thisl = desktop->getDocument()->getObjectById(
			std::string(Glib::ustring::format("animationlayer", 1, "keyframe", i-1)));  //desktop->currentLayer();

	if(kw->parent->animation_start == i)
	{
		thisl = desktop->getDocument()->getObjectById(
						std::string(Glib::ustring::format("animationlayer", 1, "keyframe", kw->parent->animation_stop)));

		if(kw->parent->widgets[kw->parent->animation_stop-1])
			kw->parent->widgets[kw->parent->animation_stop-1]->is_focused = false;

		kw->parent->widgets[i-1]->is_focused = true;
	}

	if(thisl)
		SP_ITEM(thisl)->setHidden(true);

	if(nextl)
		SP_ITEM(nextl)->setHidden(false);

	if(kw->parent->animation_stop == i)
	{
		//nextl = desktop->getDocument()->getObjectById(
		//		std::string(Glib::ustring::format("animationlayer", 1, "keyframe", kw->parent->animation_start)));

		//if(nextl)
		//	SP_ITEM(nextl)->setHidden(false);

		//kw->parent->widgets[i-2]->is_focused = false;
		//kw->parent->widgets[i-1]->is_focused = false;
		//kw->parent->widgets[i+1]->is_focused = false;

		kw->parent->widgets[i-2]->is_focused = false;
		kw->parent->widgets[i-1]->is_focused = true;
		i = kw->parent->animation_start;
		kw->parent->queue_draw();
		return true;

	}


	if(kw->parent->widgets[i-2] && i >= 2)
		kw->parent->widgets[i-2]->is_focused = false;

	if(kw->parent->widgets[i-1])
		kw->parent->widgets[i-1]->is_focused = true;

	kw->parent->queue_draw();

	i++;

	if(!nextl)
		return false;

	return true;
}


bool KeyframeWidget::on_my_key_press_event(GdkEventKey * event)
{
	SPDesktop * desktop = SP_ACTIVE_DESKTOP;


	if(event->keyval == GDK_KEY_space)
	{
		guint delay = (guint)(1000/desktop->fps);

		if(!desktop->is_playing)
		{
			gint func_ref = g_timeout_add(delay, (GSourceFunc)playLoop, this);
			desktop->is_playing = true;
		}
		else
		{
			desktop->is_playing = false;
			defocusAllKeyframes();
		}
	}

	if(event->state & (GDK_SHIFT_MASK))
	{
		parent->shift_held = true;
		
		if(event->keyval == GDK_KEY_Right)
		{

			if(next)
			{
				next->grab_focus();
				parent->several_selected = true;
			}
			//queue_draw();
		}

		if(event->keyval == GDK_KEY_Left)
		{

			if(prev)
			{
				prev->grab_focus();
				parent->several_selected = true;
			}
			//queue_draw();
		}
	}

	if(event->state & GDK_CONTROL_MASK)
		parent->ctrl_held = true;
	//if(Inkscape::UI::state_held_only_control(event->state))
	//	parent->ctrl_held = true;

	if(!(event->state & GDK_CONTROL_MASK) && !(event->state & GDK_SHIFT_MASK) && event->keyval != GDK_KEY_Delete)
	{
		parent->shift_held = false;
		//parent->ctrl_held = false;
		defocusAllKeyframes();
	}

	//queue_draw();
	parent->queue_draw();
	return false;
}

bool KeyframeWidget::on_my_key_release_event(GdkEventKey * event)
{

	//parent->several_selected = true;
	//defocusAllKeyframes();

	queue_draw();

	return false;
}

bool KeyframeWidget::on_my_focus_out_event(GdkEventFocus* event)
{

	if(!parent->shift_held && !parent->ctrl_held)
		is_focused = false;

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

	if(prev && prev->layer != NULL)
		LAYERS_TO_HIDE.push_back(prev->layer);
		
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

	if( parent->several_selected &&  !parent->shift_held && !parent->ctrl_held)
	{
		defocusAllKeyframes();
	}

	is_dragging_over = false;
	parent->queue_draw();
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
				
	if(!layer)
		Inkscape::create_animation_keyframe(desktop->currentRoot(), parent->layer, id);

	layer = desktop->namedview->document->getObjectById(
			Glib::ustring::format("animationlayer", parent_id, "keyframe", id));

	if(!layer)
		return;

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


	while(p)
	{
		//break if a p with at least 1 child is found
		if(p->layer && p->layer->getRepr()->childCount() >= 1)
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

	SPDesktop * desktop = SP_ACTIVE_DESKTOP;
		//emit change
		if(desktop)
			desktop->getSelection()->emit();
}

static void animationStop(KeyframeWidget * kww, gpointer data)
{
	KeyframeWidget* kw = reinterpret_cast<KeyframeWidget*>(data);
	
	if(kw->id < kw->parent->animation_start)
		return;
	
	for(int i=0;i < kw->parent->widgets.size();i++)
		kw->parent->widgets[i]->is_animation_stop = false;
	
	kw->is_animation_stop = true;
	kw->parent->animation_stop = kw->id;
	
	kw->parent->queue_draw();
}

static void animationStart(KeyframeWidget * kww, gpointer data)
{
	KeyframeWidget* kw = reinterpret_cast<KeyframeWidget*>(data);
	
	if(kw->id > kw->parent->animation_stop)
		return;
	
	for(int i=0;i < kw->parent->widgets.size();i++)
		kw->parent->widgets[i]->is_animation_start = false;
	

	kw->is_animation_start = true;
	kw->parent->animation_start = kw->id;
	
	kw->parent->queue_draw();
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


static int numKeyframes(KeyframeWidget * kw)
{

	SPDesktop * desktop = SP_ACTIVE_DESKTOP;

	if(!desktop)
		return 1;

	int num_layers = 1;
	int i = kw->id + 1;

	SPObject * layer = kw->layer;// = desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", kw->id)));
	SPObject * startLayer = kw->layer;
	SPObject * endLayer = NULL;
	SPObject * nextLayer = NULL;


	while(layer)
		{
			//layer = Inkscape::next_layer(desktop->currentRoot(), layer);
			layer = desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", i)));

			if(!layer)
				return 1;

			//as soon as a layer has a child, break and set endLayer to this!
			if (layer->getRepr()->childCount() > 0)
				break;

			num_layers++;
			i++;
		}
		endLayer = layer;

	return num_layers;
}


static void updateTween(KeyframeWidget * kww, gpointer user_data)
{
	KeyframeWidget * kw = reinterpret_cast<KeyframeWidget*>(user_data);

	SPDesktop * desktop = SP_ACTIVE_DESKTOP;

		if(!desktop)
			return;

	int num_keyframes = kw->parent->num_keyframes;

	SPObject * layer = kw->layer;// = desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", kw->id)));
	SPObject * startLayer = kw->layer;
	SPObject * endLayer = desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", num_keyframes)));
	SPObject * nextLayer = NULL;

	layer = startLayer;
	
	if(!layer)
		return;
	
	if(!endLayer)
		return;
	
	int i = 0;
	while(layer != endLayer->next)
	{
		nextLayer = desktop->getDocument()->getObjectById(
					std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", kw->id + i)));
					
		//if it's not a tween, return
		if(i > 0 && ( !nextLayer || !nextLayer->getRepr()->attribute("inkscape:tween")))
			return;

		SPPath * path = NULL;
		if(SP_IS_PATH(startLayer->firstChild()))
			path = SP_PATH(startLayer->firstChild());
		else if(startLayer->getRepr()->childCount() > 1 && SP_IS_PATH(startLayer->firstChild()->next))
			path = SP_PATH(startLayer->firstChild()->next);
		else
			return;
		
		bool is_along_path = false;
		
		if(path->getRepr()->attribute("inkscape:tweenpath"))
			is_along_path = true;

		SPCurve * curve = path->_curve;

		Geom::PathVector pathv = curve->get_pathvector();

		Geom::Point p = pathv.pointAt((i)*pathv.timeRange().max()/(num_keyframes + 1));


		nextLayer = desktop->getDocument()->getObjectById(
		std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", kw->id + i)));

		if(!nextLayer)
			break;

		//SPObject * child = layer->firstChild();
		SPObject * child = layer->firstChild();

		if(!child)
			break;

		if(is_along_path)
			child->setAttribute("transform",
					Glib::ustring::format("translate(", p[0], ",", p[1], ")" ));

		layer = nextLayer;
		i++;

	}

}

static void shapeTween(KeyframeWidget * kw, SPObject * startLayer, SPObject * endLayer)
{
	SPDesktop * desktop = SP_ACTIVE_DESKTOP;
	int num_nodes = 0;
	int i = 0;
	
	SPObject * layer = startLayer;
	SPObject * nextLayer = NULL;
	
	int num_layers = kw->parent->num_keyframes+1;
	
	std::vector<Node*> nodes;
	
	std::vector<Geom::Point> inc_node_pos;
	std::vector<Geom::Point> inc_node_front_handle;
	std::vector<Geom::Point> inc_node_back_handle;
	
	std::vector<Geom::Point> start_nodes_position;
	std::vector<Geom::Point> start_nodes_front;
	std::vector<Geom::Point> start_nodes_back;

	std::vector<Geom::Point> end_nodes_position;
	std::vector<Geom::Point> end_nodes_front;
	std::vector<Geom::Point> end_nodes_back;
	
	num_nodes = SP_PATH(layer->firstChild())->_curve->nodes_in_path();
	
	
	//kw->showAll->set_active(true);
	//desktop->show_all_keyframes = true;

	//showAllKeyframes(kw, kw);
	//desktop->toggleHideAllLayers(false);
	
	//tools_switch(desktop, TOOLS_SELECT);
	//desktop->setCurrentLayer(layer);
	//SP_ITEM(endLayer)->setHidden(false);
	//SP_ITEM(endLayer->firstChild())->setHidden(false);
	//endLayer->getRepr()->setAttribute("style", "opacity:1.0");
	//endLayer->firstChild()->getRepr()->setAttribute("style", "opacity:1.0");
	
	//Inkscape::SelectionHelper::selectAllInAll(desktop);
	//tools_switch(desktop, TOOLS_NODES);
	//Inkscape::SelectionHelper::selectAllInAll(desktop);
	
	//get START nodes
	NodeTool *toolz = get_node_tool();

	if(toolz)
	{
		Inkscape::UI::ControlPointSelection *cps = toolz->_selected_nodes;
		cps->clear();
		cps->selectAll();
		//cps->allPoints();
		
		if(!cps)
			return;

		Node *n = dynamic_cast<Node *>(* cps->begin() );
		PathManipulator &pm = n->nodeList().subpathList().pm();
		MultiPathManipulator &mpm = pm.mpm();
		cps->clear();
		mpm.shiftSelection(1);

		for(int i=0; i < num_nodes*2; i++)
		{
			n = dynamic_cast<Node *>(*cps->begin());
			nodes.push_back( dynamic_cast<Node *> (*pm._selection.begin()) );
			mpm.shiftSelection(1);

		}
		cps->clear();
		cps->clear();
	}

	std::size_t const half_size = nodes.size() / 2;
	std::vector<Node*> start_nodes(nodes.begin(), nodes.begin() + half_size);
	std::vector<Node*> end_nodes(nodes.begin() + half_size, nodes.end());

	for (int i = 0; i < start_nodes.size(); i++) {
		start_nodes_position.push_back(start_nodes[i]->position());
		start_nodes_front.push_back(start_nodes[i]->front()->relativePos());
		start_nodes_back.push_back(start_nodes[i]->back()->relativePos());
	}

	for (int i = 0; i < end_nodes.size(); i++) {
		end_nodes_position.push_back(end_nodes[i]->position());
		end_nodes_front.push_back(end_nodes[i]->front()->relativePos());
		end_nodes_back.push_back(end_nodes[i]->back()->relativePos());
	}
	
	for (int i = 0; i < end_nodes_position.size(); i++) {
		inc_node_pos.push_back( (end_nodes_position[i] - start_nodes_position[i])/(num_layers - 1)  );

		Geom::Point end_front = end_nodes_front[i];
		Geom::Point start_front = start_nodes_front[i];

		Geom::Point end_back = end_nodes_back[i];
		Geom::Point start_back = start_nodes_back[i];

		//Inkscape::UI::Handle * h = end_node->front();

		//if(!end_node->isDegenerate() && !start_node->isDegenerate())
		{
			inc_node_front_handle.push_back(
					(end_nodes_front[i] - start_nodes_front[i]) / (num_layers - 1)  );
			inc_node_back_handle.push_back(
					(end_nodes_back[i] - start_nodes_back[i]) / (num_layers - 1)  );
		}
	}
	
	layer = startLayer;

	i = 1;
	int tot=0;
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

		Inkscape::SelectionHelper::selectNone(desktop);
		tools_switch(desktop, TOOLS_SELECT);
		desktop->toggleHideAllLayers(true);
		SP_ITEM(layer)->setHidden(false);
		desktop->setCurrentLayer(layer);
		Inkscape::SelectionHelper::selectAll(desktop); //select everything
		tools_switch(desktop, TOOLS_NODES);

		NodeTool *toolss = get_node_tool();

		if(toolss)
		{
			Inkscape::UI::ControlPointSelection *cps = toolss->_selected_nodes;
			//Inkscape::UI::ControlPointSelection *cps = tool->_all_points;

			//DOES NOT SELECT IN ORDER!
			cps->selectAll();

			Node * node = dynamic_cast<Node *>(*(cps->begin()));

			if(node && inc_node_pos.size() > 0)
			{
				PathManipulator &pm = node->nodeList().subpathList().pm();
				MultiPathManipulator &mpm = pm.mpm();
				//mpm.selectSubpaths();
				cps->clear();
				//mpm.clear();
				//pm.clear();
				mpm.selectAllinOrder();

				int ind=0;
				int amount = 0;


				//now one is selected, loop
				for(int j=0; j < num_nodes; j++)
				{
					node = dynamic_cast<Node *>(*mpm._selection.begin());
					//cps->clear();
					//nodes.push_back( dynamic_cast<Node *> (*pm._selection.begin()) );
					if(node)
					{

						Geom::Point extra_front = (i-1)*inc_node_front_handle[j];
						Geom::Point extra_back = (i-1)*inc_node_back_handle[j];

						node->move(node->position() + (i-1)*inc_node_pos[j]);

						//if(i > 3)
						{
							node->front()->setRelativePos(start_nodes_front[j] + extra_front);
							node->back()->setRelativePos(start_nodes_back[j] + extra_back);
						}
					}

					mpm.shiftSelection(1);
					pm.update();
					pm.updateHandles();
					mpm.updateHandles();
				}
			}
		}
		layer = nextLayer;
		i++;
	}
	
	SP_ITEM(endLayer)->setHidden(true);
	//desktop->toggleHideAllLayers(true);
	//SP_ITEM(startLayer)->setHidden(false);
}

static void guidedTween(KeyframeWidget * kw, SPObject * startLayer, SPObject * endLayer)
{
	SPDesktop * desktop = SP_ACTIVE_DESKTOP;
	SPObject * layer = startLayer;
	SPObject * nextLayer = NULL;
	int i = 0;
	
	int num_layers = kw->parent->num_keyframes;
	
	if(SP_IS_PATH(layer->firstChild()))
		layer->firstChild()->getRepr()->setAttribute("inkscape:tweenpath", "true");
	else if(SP_IS_PATH(layer->firstChild()->next))
		layer->firstChild()->next->getRepr()->setAttribute("inkscape:tweenpath", "true");		

	desktop->toggleHideAllLayers(true);
	SP_ITEM(startLayer)->setHidden(false);
	SP_ITEM(startLayer->parent)->setHidden(false);

	desktop->setCurrentLayer(startLayer);
	
	kw->parent->clear_tween = true;
	
	DocumentUndo::done(desktop->getDocument(), SP_VERB_CONTEXT_NODE, "Create tween");

	tools_switch(desktop, TOOLS_SELECT);
	Inkscape::SelectionHelper::selectAll(desktop); //select everything
	tools_switch(desktop, TOOLS_NODES);

	NodeTool *tool = get_node_tool();
	
	if(!tool)
		return;
	

	Inkscape::UI::ControlPointSelection *cps = tool->_selected_nodes;
	cps->selectAll();
	int testttt = cps->size();
	
	if(!cps)
		return;
	
	Node *n = dynamic_cast<Node *>(*cps->begin());
			if (!n) return;
	
	NodeList::iterator this_iter = NodeList::get_iterator(n);
	
	int sizeee = n->nodeList().size();
	
	layer = startLayer;
	i = 0;
	while(layer != endLayer->next)
	{
		nextLayer = desktop->getDocument()->getObjectById(
					std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", kw->id + i)));


		SPPath * path = NULL;
		if(SP_IS_PATH(startLayer->firstChild()))
			path = SP_PATH(startLayer->firstChild());
		else
			path = SP_PATH(startLayer->firstChild()->next);

		SPCurve * curve = path->_curve;

		Geom::PathVector pathv = curve->get_pathvector();

		Geom::Point p = pathv.pointAt( i*pathv.timeRange().max()/(num_layers + 1));

		nextLayer = desktop->getDocument()->getObjectById(
		std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", kw->id + i)));

		if(!nextLayer)
			break;

		//SPObject * child = layer->firstChild();
		SPObject * child = layer->firstChild();

		if(!child)
			break;

		child->setAttribute("transform",
				Glib::ustring::format("translate(", p[0], ",", p[1], ")" ));

		layer = nextLayer;
		i++;

	}
}

static void linearTween(KeyframeWidget * kw, SPObject * startLayer, SPObject * endLayer, float start_x, float start_y, float inc_x, float inc_y)
{
	SPDesktop * desktop = SP_ACTIVE_DESKTOP;
	SPObject * layer = startLayer;
	
	int j = 1;
	
	while(layer->next)
	{
		//layer = desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", i)));
		layer = layer->next;
		
		if(!layer)
			return;
		
		SPObject * child = layer->firstChild();
		
		if(!child)
			return;
		
		child->getRepr()->setAttribute("transform", 
			Glib::ustring::format("translate(", start_x + j*inc_x, ",", start_y + j*inc_y, ")" ));

		j++;
	}
	
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
	bool is_along_path = false;
	int i = kw->id+1;
	int num_layers = 1;
	int num_nodes = 0;


	while(layer)
	{
		layer = desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", i)));
		
		if(!layer)
			return;
		
		layer->getRepr()->setAttribute("inkscape:tween", "true");
		
		//as soon as a layer has a child, break and set endLayer to this!
		if (layer->getRepr()->childCount() > 0)
			break;

		num_layers++;
		i++;
	}
	endLayer = layer;
	
	kw->parent->num_keyframes = num_layers;
	
	
	if(endLayer)
	{
		//SP_ITEM(startLayer)->setHidden(false);
		//Inkscape::SelectionHelper::selectNone(desktop);

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
			
			Geom::Point p = SP_SHAPE(child)->desktopGeometricBounds()->midpoint();
			end_x = p[0];
			end_y = p[1];
			//end_x = SP_ITEM(child)->getCenter()[0];
			//end_y = SP_ITEM(child)->getCenter()[1];

			//convert
			end_x = Quantity::convert(end_x, "px", "mm");
			//end_y = Quantity::convert(end_y, "px", "mm");
			end_y = desktop->getDocument()->getHeight().value("mm") - Quantity::convert(end_y, "px", "mm");
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
			Geom::Point p = SP_SHAPE(child)->desktopGeometricBounds()->midpoint();
			//Geom::Point p = SP_SHAPE(child)->desktopVisualBounds()->midpoint();

			start_x = p[0];
			start_y = p[1];

			start_x = Quantity::convert(start_x, "px", "mm");
			start_y = desktop->getDocument()->getHeight().value("mm") - Quantity::convert(start_y, "px", "mm");
		}
		*/

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
	
	inc_x = (end_x - start_x)/(num_layers);
	inc_y = (end_y - start_y)/(num_layers);
	
	inc_r = (end_rgb[0]-start_rgb[0])/num_layers;
	inc_g = (end_rgb[1]-start_rgb[1])/num_layers;
	inc_b = (end_rgb[2]-start_rgb[2])/num_layers;
	
	inc_opacity = (end_opacity - start_opacity)/(num_layers);
	
	//now we have start and end, loop again, and copy children etcetc
	layer = startLayer;
	
	i = 1;
	while(layer != endLayer)
	{
		nextLayer = desktop->getDocument()->getObjectById(
		std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", kw->id + i)));
		
		//if(!kw->onion->get_active())
			layer->getRepr()->setAttribute("style", "opacity:1.0;");

		if(!nextLayer)
			break;
		
		//SPObject * child = layer->firstChild();
		SPObject * child = layer->firstChild();
		
		if(!child)
			break;
		
		//tween color TODO: check if it already has color...
        //paint_res->setColor(d[0], d[1], d[2]);
		if(!SP_ITEM(child)->style->fill.isNone())
		{
			SP_ITEM(child)->style->fill.clear();
			//SP_ITEM(child)->style->fill.setColor(i*16777000/num_layers, 0, 0);
			SP_ITEM(child)->style->fill.setColor(start_rgb[0] + (i-1)*inc_r,
												start_rgb[1] + (i-1)*inc_g,
												start_rgb[2] + (i-1)*inc_b);
			SP_ITEM(child)->style->fill.colorSet = TRUE;
			SP_ITEM(child)->style->fill.set = TRUE;			
		}
		
		//if(!SP_ITEM(child)->style->stroke.isNone()) etcetc
		
		//tween opacity
		SP_ITEM(child)->style->opacity.value = start_opacity + (i-1)*inc_opacity;

		Inkscape::XML::Node * childn = child->getRepr();
		Inkscape::XML::Node * childn_copy = childn->duplicate(desktop->getDocument()->getReprDoc());
		
		/*
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
		*/
		//copy layer childn to nextLayer
		if(childn && childn_copy && nextLayer != endLayer)
		{
			nextLayer->getRepr()->appendChild(childn_copy);
		}
		layer = nextLayer;
		i++;
	}
	
	//check if we have one object each in startlayer and endlayer and that those objects are paths
	if(startLayer->getRepr()->childCount() == 1 && SP_IS_PATH(startLayer->firstChild()) &&
	endLayer->getRepr()->childCount() == 1 && SP_IS_PATH(endLayer->firstChild()) )
		shapeTween(kw, startLayer, endLayer);
		
	else if(startLayer->getRepr()->childCount() == 2 && endLayer->getRepr()->childCount() == 1 
	&& (SP_IS_PATH(startLayer->firstChild()) || SP_IS_PATH(startLayer->firstChild()->next)))
		guidedTween(kw, startLayer, endLayer);
		
	//is group, ellipse, rect etc etc
	else if(startLayer->getRepr()->childCount() == 1 && startLayer->getRepr()->childCount() == 1)
		linearTween(kw, startLayer, endLayer, start_x, start_y, inc_x, inc_y);
	
	//check if a color/opacity tween is in order
	//if()
	
	//emit selection signal
	desktop->getSelection()->emit();


	kw->showAll->set_active(false);
	desktop->show_all_keyframes = false;
	showAllKeyframes(kw, kw);
}

void KeyframeWidget::defocusAllKeyframes()
{
	//if(parent->several_selected)
	//{
		for(int i=0; i < parent->widgets.size(); i++)
		{
			KeyframeWidget * kw = parent->widgets[i];

			kw->is_focused = false;

		}
		parent->several_selected = false;
	//}
	parent->queue_draw();
	//queue_draw();
}

bool KeyframeWidget::on_my_button_release_event(GdkEventButton* event)
{
	grab_focus();
	//gtk_widget_grab_focus(GTK_WIDGET(this));
	//gtk_widget_set_state( GTK_WIDGET(this), GTK_STATE_ACTIVE );
	
	//select layer that corresponds to this keyframe
	//selectLayer();
	
	/*
	
	if(event->state &  (GDK_SHIFT_MASK))
	{
		parent->shift_held = true;

		bool activated = false;

		for(int i=0; i < parent->widgets.size(); i++)
		{
			KeyframeWidget * kw = parent->widgets[i];

			if(activated && (id - 1) == i)
			{
				activated = false;
				break;
			}

			else if(activated)
			{
				kw->is_focused = true;
				parent->several_selected = true;
			}

			if(kw->is_focused)
				activated = true;

			//kw->is_focused = true;
		}
	}
	else
	{
		parent->shift_held = false;
	}
	*/
	
	if(event->state & (GDK_CONTROL_MASK))
	{
		parent->ctrl_held = true;
		parent->several_selected = true;
	}
	else
	{
		parent->ctrl_held = false;
	}
	
	if(!(event->state & GDK_SHIFT_MASK) && !(event->state & GDK_CONTROL_MASK))
	{
		defocusAllKeyframes();
	}

	parent->queue_draw();
	queue_draw();

	return false;
}

bool KeyframeWidget::on_my_button_press_event(GdkEventButton* event)
{

	if (event->type == GDK_BUTTON_PRESS  &&  event->button == 3)
	{
		onion->set_active(SP_ACTIVE_DESKTOP->fade_previous_layers);
		showAll->set_active(SP_ACTIVE_DESKTOP->show_all_keyframes);
		
		pMenu->show_all();
		pMenu->popup(event->button, event->time);
	}
	return false;
}

void KeyframeWidget::on_my_drag_leave_event(const Glib::RefPtr<Gdk::DragContext>& context, guint time)
{
	is_dragging_over = false;
	parent->queue_draw();
}

bool KeyframeWidget::on_my_drag_motion_event(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time)
{

	int xx = x;
	int yy = y;

	for(int i=0;i<parent->widgets.size();i++)
		parent->widgets[i]->is_dragging_over = false;

	is_dragging_over = true;
	parent->queue_draw();

	return false;
}


void KeyframeWidget::on_my_drag_begin_event(const Glib::RefPtr<Gdk::DragContext>& context)
{
	//drag_source_set_icon(NULL, NULL, NULL);
	//return false;
}


void KeyframeWidget::on_my_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int, int,
        const Gtk::SelectionData& selection_data, guint, guint time)
{
	
	SPDesktop * desktop = SP_ACTIVE_DESKTOP;
	if(!desktop)
		return;
	
	const int length = selection_data.get_length();
	if((length >= 0) && (selection_data.get_format() == 8))
	{
		//int idd = atoi(selection_data.get_data_as_string().c_str());
		
		//const char * str = selection_data.get_data_as_string().c_str();
		
		//const char * pch;
		//pch = strtok(str, ",");
		
		//int parent_idd = atoi(pch);
		//pch = strtok(str, ",");
		//int idd = atoi(pch);
		
		std::vector<Glib::ustring> parts = Glib::Regex::split_simple(",", selection_data.get_data_as_string());
		
		int parent_idd = atoi(parts[0].c_str());
		int idd = atoi(parts[1].c_str());
		
		//KeyframeWidget * kw_src = parent->widgets[idd-1];
		SPObject * kw_src_layer = SP_ACTIVE_DESKTOP->getDocument()->getObjectById(
										Glib::ustring::format("animationlayer", parent_idd, "keyframe", idd));

		if(!kw_src_layer)
			return;
		
		if(idd == id && parent_idd == parent_id)
		{
			is_dragging_over = false;
			parent->queue_draw();
			return;
		}

		parent->widgets[id-1]->is_focused = true;

		parent->widgets[id-1]->is_empty = parent->widgets[idd-1]->is_empty;
		
		parent->queue_draw();
		
		//copy contents of idd to this
		while(kw_src_layer->getRepr()->childCount() > 0)
		{
			Inkscape::XML::Node * childn_copy = NULL;
			Inkscape::XML::Node * childn = kw_src_layer->getRepr()->firstChild();
			if(childn)
				childn_copy = childn->duplicate(SP_ACTIVE_DESKTOP->getDocument()->getReprDoc());
			
			//Inkscape::XML::Node * n = kw_src->layer->getRepr()->firstChild();
			if(!layer)
			{
				Inkscape::create_animation_keyframe(SP_ACTIVE_DESKTOP->currentRoot(), parent->layer, id);
				layer = SP_ACTIVE_DESKTOP->getDocument()->getObjectById(
										Glib::ustring::format("animationlayer", parent_id, "keyframe", id));
			}
			
			desktop->setCurrentLayer(layer);

			layer->getRepr()->appendChild(childn_copy);
			kw_src_layer->getRepr()->removeChild(childn);
		}
		
		//kw_src.is_empty = true;
	}

	is_dragging_over = false;
	
	
	//emit change
	desktop->getSelection()->emit();

	context->drag_finish(false, false, time);
}

/* what to send */
void KeyframeWidget::on_my_drag_data_get(const Glib::RefPtr< Gdk::DragContext >&  	context,
		Gtk::SelectionData&  	selection_data,
		guint  	info,
		guint  	time)
{
	//std::string hehe = std::to_string(id);
	
	Glib::ustring hehe = Glib::ustring::format(parent_id, ",", id);
	
	selection_data.set(selection_data.get_target(), 8 /* 8 bits format */,
          (const guchar*) hehe.c_str(),
          sizeof(hehe.c_str()));
	
}

bool KeyframeWidget::on_my_drag_drop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time)
{
	int xxx = x;
	int yyy = y;

	//KeyframeWidget * kw = dynamic_cast<KeyframeWidget*>(context);
	//int iiidddd = id;
	//auto test = context->get_type();
	auto test2 = get_data("id");
	auto test3 = get_data("i");
	int iddddd = id;
	//int id2 = test2->id;
	//auto test2 = context->get_data("id");

	return true;
}

KeyframeWidget::KeyframeWidget(int _id, KeyframeBar * _parent, SPObject * _layer, bool _is_empty)
{
	height = 0;
	parent = _parent;
	layer = _layer;
	id = _id;
	is_focused = false;
	is_animation_stop = false;
	is_animation_start = false;
	animation_start = 1;
	animation_stop = 10;
	
	is_dragging_over = false;

	if(id == 1)
		is_animation_start = true;
	
	if(id==10)
		is_animation_stop = true;
	
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
	onion->set_active(false);
	
	g_signal_connect( onion->gobj(),
						  "activate",
						  G_CALLBACK(onionSkinning),
						  this);
						  
						  
	Gtk::MenuItem *addAnimationStop = new Gtk::MenuItem("Set animation stop");
	
	g_signal_connect( addAnimationStop->gobj(),
						  "activate",
						  G_CALLBACK(animationStop),
						  this);
						  
	Gtk::MenuItem *addAnimationStart = new Gtk::MenuItem("Set animation start");
	
	g_signal_connect( addAnimationStart->gobj(),
						  "activate",
						  G_CALLBACK(animationStart),
						  this);
						  
	settingsItem = Gtk::manage(new Gtk::MenuItem("Settings..."));
	
	g_signal_connect( settingsItem->gobj(),
						  "activate",
						  G_CALLBACK(settings),
						  this);

	pMenu->add(*pItem3);
	pMenu->add(*tweenItem);
	//pMenu->add(*tweenItem2);
	pMenu->add(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	pMenu->add(*showAll);
	pMenu->add(*onion);
	pMenu->add(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	pMenu->add(*addAnimationStart);
	pMenu->add(*addAnimationStop);
	pMenu->add(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	pMenu->add(*settingsItem);

	parent_id = parent->id;
	
	
	this->set_size_request(12, 22);
	
	set_can_focus(true);
	
	is_empty = _is_empty;
	
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	if(desktop)
	{
		Inkscape::Selection * selection = desktop->getSelection();
		sigc::connection _sel_changed_connection;
		sigc::connection _sel_changed_connection2;
		//_sel_changed_connection = selection->connectChanged(
		//	sigc::bind(
		//		sigc::ptr_fun(&KeyframeWidget::on_selection_changed),
		//		desktop));
		
		_sel_changed_connection = selection->connectChangedFirst(
		sigc::hide(sigc::mem_fun(*this, &KeyframeWidget::on_selection_changed)));


		_sel_changed_connection2 = desktop->connectToolSubselectionChanged(
				sigc::hide(sigc::mem_fun(*this, &KeyframeWidget::on_update_tween)));

		//desktop->connectToolSubselectionChanged()

	}
	
	add_events(Gdk::ALL_EVENTS_MASK);
	
	set_can_focus(true);
	//set_receives_default();
	set_sensitive();
	signal_key_press_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_key_press_event));
	signal_button_release_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_button_release_event));
	signal_button_press_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_button_press_event));
	signal_key_release_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_key_release_event));
	signal_focus_in_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_focus_in_event));
	signal_focus_out_event().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_focus_out_event));

	std::vector<std::string> listing;
	listing.push_back("text/html");
	listing.push_back("STRING");
	listing.push_back("number");
	listing.push_back("image/jpeg");
	int entryCount = listing.size();

	GtkTargetEntry* entries = new GtkTargetEntry[entryCount];
	GtkTargetEntry* curr = entries;
	for ( std::vector<std::string>::iterator it = listing.begin(); it != listing.end(); ++it ) {
		curr->target = g_strdup(it->c_str());
		curr->flags = 0;

		//mimeStrings.push_back(*it);

		//curr->info = mimeToInt[curr->target];
		curr++;
	}
	//drag_source_set(entries, GDK_BUTTON1_MASK, GdkDragAction(GDK_ACTION_MOVE | GDK_ACTION_COPY));
	gtk_drag_source_set( GTK_WIDGET(this->gobj() ), GDK_BUTTON1_MASK, entries, entryCount, GdkDragAction(GDK_ACTION_MOVE | GDK_ACTION_COPY));

	set_data(Glib::Quark("i"), (gpointer) this);

	gtk_drag_dest_set( GTK_WIDGET(this->gobj() ), GTK_DEST_DEFAULT_ALL, entries, entryCount, GdkDragAction(GDK_ACTION_MOVE | GDK_ACTION_COPY));

	signal_drag_motion().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_drag_motion_event));
	signal_drag_leave().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_drag_leave_event));
	signal_drag_begin().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_drag_begin_event));
	signal_drag_data_get().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_drag_data_get));
	signal_drag_data_received().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_drag_data_received));

	//g_signal_connect( G_OBJECT(this->gobj()),
	 //                         "drag-data-get",
	   //                       G_CALLBACK(KeyframeWidget::on_my_drag_data_get),
	     //                     this);


	signal_drag_drop().connect(sigc::mem_fun(*this, &KeyframeWidget::on_my_drag_drop));

}

KeyframeWidget::~KeyframeWidget()
{
}

void KeyframeWidget::on_update_tween()
{
	//is_empty = false;
	updateTween(this, this);
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

		if(id % 5 == 0)
			cr->set_source_rgba(.8, .8, .8, 1);
		else if(id % 2 == 0)
			cr->set_source_rgba(1, 1, 1, 1);
		else
			cr->set_source_rgba(.9, .9, .9, 1);
		

		cr->paint();

		if(has_focus())
			is_focused = true;

		if(is_focused)
			cr->set_source_rgba(.8, 0, 0, .75);
		
		cr->paint();
		
		if(!is_empty)
		{
			cr->set_source_rgba(0, 0, 0, 1);
			cr->arc(width/2, height/2, width/4, 0, 6.283);
			cr->fill();
		}
		
		//add line to the bottom
		cr->set_source_rgba(.6,.6,.6,1);
		cr->set_line_width(1.0);
		cr->move_to(0, height-.5);
		cr->line_to(width, height-.5);
		cr->stroke();

		if(is_animation_stop)
		{
			cr->set_source_rgba(.4, 0, 0, .75);
			cr->rectangle(width*3/4, 0, width/4, height);
		}
		if(is_animation_start)
		{
			cr->set_source_rgba(.4, 0, 0, .75);
			cr->rectangle(0, 0, width/4, height);
		}
		
		if(is_dragging_over)
		{
			cr->set_source_rgba(0, 0, 0, .75);
			cr->rectangle(0, 0, width, height);
		}

		cr->fill();
	

	}
	
	if(height > 10)
		set_size_request(12, height);
	
	return true;
}
