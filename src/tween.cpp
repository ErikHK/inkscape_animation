/*
 * Tween implementation
 *
 * Author:
 *   Erik HK <erikhk@gmail.com>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glibmm/i18n.h>

#include <vector>

#include "xml/repr.h"

#include "display/curve.h"
#include <2geom/pathvector.h>
#include <2geom/curves.h>
#include "helper/geom-curves.h"

#include "svg/svg.h"
#include "attributes.h"

#include "sp-path.h"
#include "sp-guide.h"
#include "sp-rect.h"
#include "sp-ellipse.h"
#include "sp-star.h"
#include "sp-item-group.h"
#include "sp-item-transform.h"

#include "tween.h"
#include "widgets/keyframe-widget.h"
#include "widgets/keyframe-bar.h"

#include "document.h"
#include "desktop.h"

#include "desktop-style.h"
#include "ui/tools/tool-base.h"
#include "inkscape.h"
#include "style.h"
#include "message-stack.h"
#include "selection.h"

#include "layer-manager.h"
#include "layer-model.h"
#include "layer-fns.h"

#include "document-undo.h"
#include "verbs.h"


//#define noPATH_VERBOSE

using Inkscape::DocumentUndo;

void Tween::createGuide(float start_x, float start_y, float end_x, float end_y)
{
	SPCurve * c = new SPCurve();
	
	c->moveto(Geom::Point(start_x, start_y));
	c->lineto(Geom::Point(end_x, end_y));
	//c->curveto(Geom::Point(start_x, start_y), Geom::Point(start_x, end_y), Geom::Point(end_x, end_y));
	
	//c->moveto(Geom::Point(0,0));
	//c->curveto(Geom::Point(0, 0), Geom::Point(.1*100, 0), Geom::Point(100, 100));
	
	
	
	
	if(!SP_ACTIVE_DOCUMENT)
		return;

    //Inkscape::XML::Document *xml_doc = SP_ACTIVE_DOCUMENT->getReprDoc();
	Inkscape::XML::Document *xml_doc = SP_ACTIVE_DESKTOP->doc()->getReprDoc();


    Inkscape::XML::Node *repr;
    if ( c && !c->is_empty() && xml_doc) {

		repr = xml_doc->createElement("svg:path");
		gchar *str = sp_svg_write_path( c->get_pathvector() );
		////////g_assert( str != NULL );

		repr->setAttribute("inkscape:tweenpath", "true");
		//repr->setAttribute("inkscape:tweenid", start->layer->getRepr()->attribute("id"));
		////////g_free(str);
		
		//repr->setAttribute("d", "M 77.863093,50.559522 111.125,73.994046");
		repr->setAttribute("d", str);
		repr->setAttribute("style", "stroke:#303030;fill:none;fill-rule:evenodd;stroke-width:1;stroke-linecap:butt;stroke-linejoin:miter;stroke-miterlimit:4;stroke-dasharray:1,2;stroke-dashoffset:0;stroke-opacity:1");
		
		
		
		//store in ...
		//SPItem *item = SP_ITEM(SP_ACTIVE_DESKTOP->currentLayer()->parent->appendChildRepr(repr));
		SP_ACTIVE_DESKTOP->currentLayer()->parent->appendChild(repr);
		
		SPObject * tweenp = SP_ACTIVE_DESKTOP->getDocument()->getObjectById(repr->attribute("id"));
		
		if(SP_IS_PATH(tweenp))
			tweenPath = SP_PATH(tweenp);
			
		if(start->layer)
		{
			//SPItem *item = SP_ITEM(start->layer->appendChildRepr(repr));
			//start->layer->appendChild(repr);
			
			
			//repr->setAttribute("d", str);
			
			//start->layer->appendChildRepr(repr);
			
			//SP_PATH(item)->tweenId = start->layer->getRepr()->attribute("id");
			
			//if(SP_IS_PATH(item))
			//	tweenPath = SP_PATH(item);
		}
		
		//SPItem *item = SP_ITEM(SP_ACTIVE_DESKTOP->currentLayer()->parent->appendChildRepr(repr));
		//SP_ACTIVE_DESKTOP->currentLayer()->parent->appendChild(repr);
		//Inkscape::GC::release(repr);

    }

	//return NULL;
}


void Tween::linearTween(SPObject * startLayer, SPObject * endLayer, float start_x, float start_y, float end_x, float end_y, float inc_x, float inc_y)
{
	SPDesktop * desktop = SP_ACTIVE_DESKTOP;
	SPObject * layer = startLayer;

	int j = 0;

	createGuide(start_x, start_y, end_x, end_y);

	while(layer->next)
	{
		//layer = desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", kw->parent_id, "keyframe", i)));

		if(!layer)
			return;

		SPObject * child = layer->firstChild();

		if(!child)
			return;

		if(tweenPath)
		{
			layer->getRepr()->setAttribute("inkscape:tweenpathid", tweenPath->getId());
			tweenPath->tweenId = startLayer->getId();
		}

		layer->getRepr()->setAttribute("inkscape:tweenstartid", startLayer->getId());
		
		tweenId = startLayer->getId();

		Geom::Point p(start_x + j*inc_x, start_y + j*inc_y);
		
		setPosition(child, p);
		
		layer = layer->next;
		j++;
	}
}

void Tween::copyObjectToKeyframes(SPObject * start_layer, SPObject * end_layer)
{
	
	int id = atoi(start_layer->getRepr()->attribute("inkscape:keyframe"));
	int parent_id = atoi(start_layer->parent->getRepr()->attribute("num"));
	
	
	
	int i = 0;
	Inkscape::XML::Node * obj_to_copy = start_layer->getRepr()->nthChild(i);
	SPObject * layer = start_layer;
	if(obj_to_copy)
	{
		Inkscape::XML::Node * obj_to_copy_copy = NULL;

		obj_to_copy_copy = obj_to_copy->duplicate(SP_ACTIVE_DESKTOP->getDocument()->getReprDoc());
			
		//Inkscape::XML::Node * n = kw_src->layer->getRepr()->firstChild();
		
		//desktop->setCurrentLayer(layer);

		//start_layer->getRepr()->appendChild(childn_copy);
		
		
		while(layer && layer != end_layer)
		{
			
			id++;
			
			//only copy if the layer has no children
			if(layer->getRepr()->childCount() == 0)
			{
				layer->getRepr()->appendChild(obj_to_copy->duplicate(SP_ACTIVE_DESKTOP->getDocument()->getReprDoc()));
				layer->getRepr()->setAttribute("inkscape:tween", "true");
			}
			//layer = layer->next;
			
			layer = SP_ACTIVE_DESKTOP->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", parent_id, "keyframe", id)));
			
			if(!layer)
			{
				Inkscape::create_animation_keyframe(SP_ACTIVE_DESKTOP->currentRoot(), start_layer->parent, id);
				layer = SP_ACTIVE_DESKTOP->getDocument()->getObjectById(
										Glib::ustring::format("animationlayer", parent_id, "keyframe", id));
			}
			
			//if(!layer)
			//	break;
			
		}
		
		
		//this works:
		/*
		if(layer)
		{
			layer->getRepr()->appendChild(obj_to_copy_copy);
		}
		*/
		

	}
	
	SP_ACTIVE_DESKTOP->getSelection()->emit();
	
}

void Tween::showAllKeyframes()
{
	KeyframeWidget* kw = start;
	
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
	
	if(kw->layer)
		kw->layer->getRepr()->setAttribute("style", "opacity:1.0");
	if(kw->prev && kw->prev->layer)
		kw->prev->layer->getRepr()->setAttribute("style", "opacity:1.0");
}


float Tween::easeInOut(float t, float a)
{
	a = 1/a;
	float ret = pow(t,a)/(pow(t,a) + pow(1-t, a));
	if (ret >= 1)
		return .99;
	return ret;
}

float Tween::easeIn(float t, float a)
{
	/*
	float ret = pow(t, 1-a);
	if(ret >= 1)
		return .99;
	return ret;
	*/
	
	SPCurve * c = new SPCurve();
	c->moveto(Geom::Point(0, 0));
	c->curveto(Geom::Point(0, 0), Geom::Point(a, 0), Geom::Point(1, 1));
	
	
	Geom::PathVector pathv = c->get_pathvector();
	auto maxt = pathv.timeRange().max();
	//auto length = c->length();
	
	//std::cout << length << std::endl;
	
	//Geom::Point testp(a, 0);
	float tt = 0;
	while( t - abs(pathv.valueAt(tt, Geom::Dim2(0))) > .005 )
	{
		tt += .001;
	}
	//now the time is right!
	
	
	float valueat = pathv.valueAt(tt, Geom::Dim2(1));
	delete c;
	return valueat;
	
	
	
}

float Tween::easeOut(float t, float a)
{
	/*
	float ret = pow(t, 1/a);
	if(ret >= 1)
		return .99;
	return ret;
	*/
	
	SPCurve * c = new SPCurve();
	c->moveto(Geom::Point(0, 0));
	c->curveto(Geom::Point(0, 0), Geom::Point(0, a), Geom::Point(1, 1));
	
	
	Geom::PathVector pathv = c->get_pathvector();
	auto maxt = pathv.timeRange().max();
	//auto length = c->length();
	
	//std::cout << length << std::endl;
	
	//Geom::Point testp(a, 0);
	float tt = 0;
	while( t - abs(pathv.valueAt(tt, Geom::Dim2(0))) > .005 )
	{
		tt += .001;
	}
	//now the time is right!
	
	
	float valueat = pathv.valueAt(tt, Geom::Dim2(1));
	delete c;
	return valueat;
}

void Tween::setScale(SPObject * child, double width, double height)
{
	if(SP_IS_RECT(child))
	{
		SPRect * rect = SP_RECT(child);
		rect->setPosition(rect->x.value, rect->y.value, width, height);
	}

	if(SP_IS_GENERICELLIPSE(child))
	{
		SPGenericEllipse * ellipse = SP_GENERICELLIPSE(child);
		ellipse->position_set(ellipse->cx.value, ellipse->cy.value, width, height);
	}

	if(SP_IS_STAR(child))
	{
		SPStar * star = SP_STAR(child);
	}

	if(SP_IS_GROUP(child) && !SP_IS_LAYER(child))
	{
		SPGroup * group = SP_GROUP(child);
		group->transform.setExpansionX(width);
		group->transform.setExpansionY(height);
	}
}

void Tween::setPosition(SPObject * child, Geom::Point p)
{
	if(SP_IS_RECT(child))
	{
		SPRect * rect = SP_RECT(child);
		rect->setPosition(p[0], p[1], rect->width.value, rect->height.value);
	}

	if(SP_IS_GENERICELLIPSE(child))
	{
		SPGenericEllipse * ellipse = SP_GENERICELLIPSE(child);
		ellipse->position_set(p[0], p[1], ellipse->rx.value, ellipse->ry.value);
	}

	if(SP_IS_STAR(child))
	{
		SPStar * star = SP_STAR(child);
		star->center = p;
		star->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
	}

	if(SP_IS_GROUP(child) && !SP_IS_LAYER(child))
	{
		SPGroup * group = SP_GROUP(child);
		group->transform.setTranslation(p);
		//group->commitTransform();
		//group->transform.setTranslation(Geom::Point(0,0));
		group->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
	}
}

float Tween::getEaseValue()
{
	char const * easeval = NULL;
	float easev = 0;
		
		if(startLayer)
		{
			easeval = startLayer->getRepr()->attribute("inkscape:ease");
			
			if(easeval)
				easev = atof(easeval);
			
		}
		
	return easev;
}

Geom::Point Tween::getPoint(Geom::PathVector pathv, float t)
{
	float easev = getEaseValue();
	Geom::Point p;
	
	if(easev < 0)
		p = pathv.pointAt(easeOut(t, -easev)); 
	else if(easev > 0)
		p = pathv.pointAt(easeIn(t, easev));
	else
		p = pathv.pointAt(t);
	
	return p;
}

void Tween::update()
{
	
	//std::cout << "Tween::update()" << std::endl;
	SPDesktop * desktop = SP_ACTIVE_DESKTOP;
	SPPath * path = NULL;

	if(!desktop)
		return;
	
	//if(desktop->getSelection()->isEmpty())
	//	return;

	SPObject * selected = desktop->getSelection()->single();
	
	//if(!selected)
	//	return;
	
	//if(!SP_IS_PATH(selected))
	//	return;

	const char * layerid = NULL;

	
	//check if it's a tween path
	if(selected && SP_IS_TWEENPATH(selected))
	{
		path = SP_PATH(selected);
		layerid = path->tweenId;
		//layerid = tweenId;
		//std::cout << layerid;
	}
	
	else
	{
		path = tweenPath;
		
		//SPObject * obj = desktop->currentLayer();

		//if(obj && obj->getRepr())
		//	layerid = obj->getRepr()->attribute("inkscape:tweenstartid");
	}

	//std::cout << "tweenId: " << std::endl;
	
	//if there is no tween associated with this path or layer, return
	if(!layerid)
		return;
	
	if(tweenId != path->tweenId)
		return;

	
	//SPObject * layer = desktop->getDocument()->getObjectById(layerid);
	//SPObject * layer = NULL;

	
	//if(!layer)
	//	return;

	//if(!path)
	//	return;

	
	//std::cout << "tweenid: " << tweenId << std::endl;
	
	
	//std::cout << "layer: " << std::endl;
	//int num_frames = numFrames;
	if(objects.empty())
		return;
	
	numFrames = objects.size();
	//numFrames = 10;
	//numFrames = 0;
	int testarrr = 0;

	SPObject * nextLayer = NULL;
	
	SPCurve * curve = path->_curve;
	
	if(!curve)
		return;
	
	
	Geom::PathVector pathv = curve->get_pathvector();
	
	SPObject * child = NULL;

	float rotation = 0;
	
	if(startLayer && startLayer->getRepr()->attribute("inkscape:rotation"))
		rotation = atoi(startLayer->getRepr()->attribute("inkscape:rotation"));

	Geom::Point p(0,0);

	//if there are no objects, return
	if(objects.empty())
		return;

	//child = startLayer->firstChild();
	
	//child = objects[0];

	//if(child)
	//{
	//	Geom::Point p = pathv.initialPoint();
	//	setPosition(child, p);
	//}
	 
	
	for(int i=0; i < numFrames-1; i++){
		auto test = pathv.timeRange().max();
		
		auto tot = i*test/(numFrames-1);
		
		if(curve->is_closed())
			tot = i*test/(numFrames);
		
		p = getPoint(pathv, tot);
		
		Geom::OptRect test2(0,0);
		//test2 = SP_ITEM(objects[i])->documentVisualBounds();
		test2 = SP_ITEM(objects[i])->visualBounds();
		Geom::Affine aff = Geom::Translate(p);
		
		//SP_ITEM(child)->transform.setTranslation(p);

		
		//p = pathv.pointAt(.1);
		Geom::Affine testtt = Geom::Rotate::around(p, i*rotation*M_PI/180/numFrames);
		//Geom::Affine testtt = Geom::Rotate::around(p + Geom::Point(10, 10), i*rotation*M_PI/180/numFrames);
		//Geom::Affine testtt = Geom::Rotate::around(p + Geom::Point(test2->width()/2, test2->height()/2), i*rotation*M_PI/180/(numFrames));
		//Geom::Affine testtt = Geom::Rotate::around(p + Geom::Point(test2->width()/2, 0), i*rotation*M_PI/180/(numFrames));
		//Geom::Affine testtt = Geom::Rotate::around(p + test2->midpoint(), i*rotation*M_PI/180/(numFrames));
		//Geom::Affine testtt = Geom::Rotate::around(p + Geom::Point(SP_RECT(objects[i])->width.value/2, SP_RECT(objects[i])->height.value/2), i*rotation*M_PI/180/(numFrames));
		//Geom::Affine testtt = Geom::Rotate::around(p + Geom::Point(SP_RECT(objects[i])->width.computed/2, SP_RECT(objects[i])->height.computed/2), i*rotation*M_PI/180/(numFrames));
		
		//Geom::Affine const testtt = Geom::Rotate::around(p + Geom::Point(test2->width()/2, test2->height()/2), i*rotation*M_PI/180/(numFrames));
		
		//Geom::Rotate const testtt = Geom::Rotate(i*rotation*M_PI/180/(numFrames));
		
		
		SP_ITEM(objects[i])->transform = testtt;
		//sp_item_rotate_rel(SP_ITEM(objects[i]), testtt);
		
		if(SP_IS_GROUP(objects[i]))
			setPosition(objects[i], p);
		else
			setPosition(objects[i], p - Geom::Point(test2->width()/2, test2->height()/2));
		
		std::cout << test2->width();
	}

	//set child to last
	child = objects.back();

	if(child)
	{
		
		Geom::Point p = pathv.finalPoint();
		
		
		
		if(curve->is_closed())
		{
			auto t = (numFrames-2)*pathv.timeRange().max()/(numFrames-1);
			p = getPoint(pathv, t);
		}
		
		Geom::OptRect test2 = SP_ITEM(child)->visualBounds();
		
		Geom::Affine testtt = Geom::Rotate::around(p, rotation*M_PI/180);
		SP_ITEM(child)->transform = testtt;
		
		if(SP_IS_GROUP(child))
			setPosition(child, p);
		else
			setPosition(child, p - Geom::Point(test2->width()/2, test2->height()/2));
	}
}

void Tween::addToTween(SPObject * obj)
{
	if(obj)
		objects.push_back(obj);
	
	//update??
	//update();
}

Geom::Point Tween::moveGroupToCenter(SPGroup * g)
{
	Geom::OptRect rect = g->desktopVisualBounds();
	//Geom::OptRect bbbox = g->bbox;
	Geom::Point pp = -rect->midpoint() + Geom::Point(0, SP_ACTIVE_DOCUMENT->getHeight().value("px"));
	
	g->translateChildItems(Geom::Translate(-rect->midpoint() + Geom::Point(0, SP_ACTIVE_DOCUMENT->getHeight().value("px")) ));
	
	//g->transform.setTranslation( pp -  Geom::Point(0, SP_ACTIVE_DOCUMENT->getHeight().value("px")));
	
	//g->transform.setTranslation(Geom::Point(0,0));
	
	return pp;
	//g->translateChildItems(Geom::Translate(Geom::Point(-1000, -1000)));
	
	//g->translateChildItems(Geom::Translate(g->bbox(Geom::identity(), SPItem::GEOMETRIC_BBOX)->min()));
	
}

/*
void Tween::setProperties(SPItem * item)
{
	
}
*/

Tween::Tween(KeyframeWidget * start) {
	
	SPObject * layer = start->layer;

	startLayer = start->layer;
	endLayer = NULL;

	SPObject * nextLayer = NULL;
	float start_x=0, start_y=0, end_x=0, end_y=0, inc_x=0, inc_y=0, start_opacity=1, end_opacity=1, inc_opacity=0, inc_r=0, inc_g=0, inc_b=0;
	float scale_x=1, scale_y=1, inc_scale_x=0, inc_scale_y=0;
	
	float stroke_scale=1, inc_stroke_scale=0;
	float start_scale_x = 1, end_scale_x = 1, start_scale_y=1, end_scale_y=1;
	
	float start_stroke_scale = 1, end_stroke_scale = 1;
	
	float start_stroke_dasharray_scale[4] = {1,1,1,1};
	float end_stroke_dasharray_scale[4] = {1,1,1,1};
	float inc_stroke_dasharray_scale[4] = {0,0,0,0};
	
	gfloat start_rgb[3];
	gfloat end_rgb[3];
	std::string xs = "x";
	std::string ys = "y";
	bool is_path = false;
	bool is_along_path = false;
	int i = start->id+1;
	int num_layers = 1;
	int num_nodes = 0;
	
	Geom::Point offss(0,0);
	Geom::Point offss2(0,0);
	
	sigc::connection _sel_changed_connection;
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	
	tweenId = NULL;
	numFrames = 0;

	if(desktop)
	{
		Inkscape::Selection * selection = desktop->getSelection();
		
		_sel_changed_connection = selection->connectChangedFirst(
				sigc::hide(sigc::mem_fun(*this, &Tween::update)));

		//sigc::connection connect_tween_expansion = desktop->connectTweenExpansion(
		//		sigc::mem_fun(*this, &Tween::addToTween));
	}
	
	if(!desktop)
		return;

	if(startLayer)
	{
		startLayer->getRepr()->setAttribute("inkscape:tweenstart", "true");
		startLayer->getRepr()->setAttribute("inkscape:tween", "true");
	}
	
	while(layer)
	{
		layer = desktop->getDocument()->getObjectById(std::string(Glib::ustring::format("animationlayer", start->parent_id, "keyframe", i)));
		
		if(!layer)
		{
			Inkscape::create_animation_keyframe(desktop->currentRoot(), start->parent->layer, i);
			layer = SP_ACTIVE_DESKTOP->getDocument()->getObjectById(
										Glib::ustring::format("animationlayer", start->parent_id, "keyframe", i));
		}
		
		if(layer && layer->getRepr())
			layer->getRepr()->setAttribute("inkscape:tween", "true");
		
		//as soon as a layer has a child, break and set endLayer to this!
		if (layer && layer->getRepr() && layer->getRepr()->childCount() > 0)
			break;

		num_layers++;
		i++;
	}
	
	
	if(!layer)
		return;

	endLayer = layer;
	//end = endLayer;
	
	//set tweenend to end layer
	endLayer->getRepr()->setAttribute("inkscape:tweenend", "true");
	endLayer->getRepr()->setAttribute("inkscape:tween", "true");
	startLayer->getRepr()->setAttribute("inkscape:tweenlayers", Glib::ustring::format(num_layers));
	numFrames = num_layers;
	
	
	if(startLayer)
	{
		SPObject * child = startLayer->firstChild();
		Inkscape::XML::Node * childn = startLayer->getRepr()->firstChild();
		if(!childn)
			return;
		
		if(!child)
			return;

		sp_color_get_rgb_floatv (&SP_ITEM(child)->style->fill.value.color, start_rgb);

		//get opacity
		start_opacity = SP_ITEM(child)->style->opacity.value;
		
		start_stroke_scale = SP_ITEM(child)->style->stroke_width.value;
		
		for(int j=0; j < SP_ITEM(child)->style->stroke_dasharray.values.size(); j++)
			start_stroke_dasharray_scale[j] = SP_ITEM(child)->style->stroke_dasharray.values[j];
		
		if(SP_IS_GROUP(child) && !SP_IS_LAYER(child))
		{
			offss = moveGroupToCenter(SP_GROUP(child));
			
			offss2 = SP_GROUP(child)->transform.translation();
			
			SP_GROUP(child)->commitTransform();
			
			Geom::Point px = SP_GROUP(child)->geometricBounds()->midpoint();
			
			//start_x = SP_GROUP(child)->transform.translation()[0];
			//start_y = SP_GROUP(child)->transform.translation()[1];
			
			start_x = px[Geom::X];
			start_y = px[Geom::Y];
			
			
			
			SP_GROUP(child)->transform.setTranslation(Geom::Point(0,0));
			SP_GROUP(child)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
			
			start_scale_x = SP_GROUP(child)->transform.expansionX();
			start_scale_y = SP_GROUP(child)->transform.expansionY();
			
		}
		
		//if ellipse
		if(SP_IS_GENERICELLIPSE(child))
		{
			SPGenericEllipse * ellipse = SP_GENERICELLIPSE(child);
			
			start_scale_x = ellipse->rx.value;
			start_scale_y = ellipse->ry.value;
			start_x = ellipse->cx.value;
			start_y = ellipse->cy.value;
		}

		//if rect
		if(SP_IS_RECT(child))
		{
			SPRect * rect = SP_RECT(child);
			
			start_scale_x = rect->width.value;
			start_scale_y = rect->height.value;
			start_x = rect->x.value;
			start_y = rect->y.value;
		}
	}
	
	
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
		
		end_stroke_scale = SP_ITEM(child)->style->stroke_width.value;
		
		for(int j=0; j < SP_ITEM(child)->style->stroke_dasharray.values.size(); j++)
			end_stroke_dasharray_scale[j] = SP_ITEM(child)->style->stroke_dasharray.values[j];

		//if ellipse
		if(SP_IS_GENERICELLIPSE(child))
		{
			SPGenericEllipse * ellipse = SP_GENERICELLIPSE(child);
			
			end_scale_x = ellipse->rx.value;
			end_scale_y = ellipse->ry.value;
			end_x = ellipse->cx.value;
			end_y = ellipse->cy.value;
		}

		//if rect
		if(SP_IS_RECT(child))
		{
			SPRect * rect = SP_RECT(child);
			
			end_scale_x = rect->width.value;
			end_scale_y = rect->height.value;
			end_x = rect->x.value;
			end_y = rect->y.value;
		}
		
		if(SP_IS_PATH(child))
		{
			SPPath * path = SP_PATH(child);
			is_path = true;
		}
		
		//else if a group, take special care!
		if(SP_IS_GROUP(child) && !SP_IS_LAYER(child))
		{
			
			//SP_GROUP(child)->commitTransform();
			
			//Geom::Point px = SP_GROUP(child)->geometricBounds()->midpoint();
			
			//start_x = SP_GROUP(child)->transform.translation()[0];
			//start_y = SP_GROUP(child)->transform.translation()[1];
			
			//end_x = px[Geom::X];
			//end_y = px[Geom::Y];
			
			//SP_GROUP(child)->transform.setTranslation(Geom::Point(0,0));
			
			end_x = SP_GROUP(child)->transform.translation()[0] - offss2[Geom::X];
			end_y = SP_GROUP(child)->transform.translation()[1] - offss2[Geom::Y];
			
			Geom::Point newp = Geom::Point(offss2[Geom::X], - offss2[Geom::Y]);
			
			SP_GROUP(child)->translateChildItems(Geom::Translate(offss + 3.78*newp) );
			
			
			
			//moveGroupToCenter(SP_GROUP(child));
			SP_GROUP(child)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
			
			end_scale_x = SP_GROUP(child)->transform.expansionX();
			end_scale_y = SP_GROUP(child)->transform.expansionY();
			
			
		}
	}
	
	
	
	inc_scale_x = (end_scale_x - start_scale_x)/num_layers;
	inc_scale_y = (end_scale_y - start_scale_y)/num_layers;
	
	inc_stroke_scale = (end_stroke_scale - start_stroke_scale)/num_layers;
	
	for(int j=0; j < 4; j++)
		inc_stroke_dasharray_scale[j] = (end_stroke_dasharray_scale[j] - start_stroke_dasharray_scale[j])/num_layers;

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
		//layer->getRepr()->setAttribute("inkscape:tweenlayers", Glib::ustring::format(num_layers));
		
		nextLayer = desktop->getDocument()->getObjectById(
		std::string(Glib::ustring::format("animationlayer", start->parent_id, "keyframe", start->id + i)));
		
		//if(!start->onion->get_active())
			layer->getRepr()->setAttribute("style", "opacity:1.0;");

		if(!nextLayer)
			break;
		
		SPObject * child = layer->firstChild();
		
		if(!child)
			break;
		
		objects.push_back(child);

		//TODO: check if it's a group, in that case loop through etc...
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
		
		//tween opacity
		SP_ITEM(child)->style->opacity.value = start_opacity + (i-1)*inc_opacity;

		if(SP_IS_RECT(child))
		{
			SPRect * rect = SP_RECT(child);
		}

		
		for(int j=0; j < SP_ITEM(child)->style->stroke_dasharray.values.size(); j++)
		{
			SP_ITEM(child)->style->stroke_dasharray.values[j] = start_stroke_dasharray_scale[j] + (i-1)*inc_stroke_dasharray_scale[j];
			SP_ITEM(child)->style->stroke_dasharray.set = true;
		}
		
		setScale(child, start_scale_x + (i-1)*inc_scale_x, start_scale_y + (i-1)*inc_scale_y);
		
		SP_ITEM(child)->style->stroke_width.value = start_stroke_scale + (i-1)*inc_stroke_scale;
		SP_ITEM(child)->style->stroke_width.computed = start_stroke_scale + (i-1)*inc_stroke_scale;
		SP_ITEM(child)->style->stroke_width.set = true;
		

		Inkscape::XML::Node * childn = child->getRepr();
		Inkscape::XML::Node * childn_copy = childn->duplicate(desktop->getDocument()->getReprDoc());

		//copy layer childn to nextLayer
		if(childn && childn_copy && nextLayer != endLayer)
		{
			nextLayer->getRepr()->appendChild(childn_copy);
		}
		layer = nextLayer;
		i++;
	}
	
	//copy to the end as well...
	
	
	
	
	objects.push_back(endLayer->firstChild());
	
	Geom::OptRect rectt = SP_ITEM(endLayer->firstChild())->visualBounds();

	//is group, ellipse, rect etc etc
	if(startLayer->getRepr()->childCount() == 1)
		//linearTween(startLayer, endLayer, 
		//	start_x + rectt->width()/2 - offss[Geom::X]/3.78,
		//	start_y + rectt->height()/2 + offss[Geom::Y]/3.78,
		//	end_x + rectt->width()/2 - offss[Geom::X]/3.78,
		//	end_y + rectt->height()/2 + offss[Geom::Y]/3.78,
		//	inc_x, inc_y);
		linearTween(startLayer, endLayer, 
			start_x - offss[Geom::X]/3.78,
			start_y + offss[Geom::Y]/3.78,
			end_x - offss[Geom::X]/3.78,
			end_y + offss[Geom::Y]/3.78,
			inc_x, inc_y);
		
		
		//linearTween(startLayer, endLayer, start_x, start_y, end_x, end_y, inc_x, inc_y);
	
	/////////////////////update();
	/*
	//check if a color/opacity tween is in order
	//if()
	
	//emit selection signal
	desktop->getSelection()->emit();

	DocumentUndo::done(desktop->getDocument(), SP_VERB_NONE, "Create tween");

	start->showAll->set_active(false);
	desktop->show_all_keyframes = false;
	showAllKeyframes();
	
	*/
}

Tween::~Tween() {
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
