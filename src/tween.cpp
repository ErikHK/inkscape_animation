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
	
	if(!SP_ACTIVE_DOCUMENT)
		return;

    //Inkscape::XML::Document *xml_doc = SP_ACTIVE_DOCUMENT->getReprDoc();
	Inkscape::XML::Document *xml_doc = SP_ACTIVE_DESKTOP->doc()->getReprDoc();


    Inkscape::XML::Node *repr;
    if ( c && !c->is_empty() && xml_doc) {

		repr = xml_doc->createElement("svg:path");
		gchar *str = sp_svg_write_path( c->get_pathvector() );
		////////g_assert( str != NULL );

		//repr->setAttribute("inkscape:tweenpath", "true");
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
		Inkscape::GC::release(repr);

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
			layer->getRepr()->setAttribute("inkscape:tweenpathid", tweenPath->getId());

		layer->getRepr()->setAttribute("inkscape:tweenstartid", startLayer->getId());
		
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
	float ret = pow(t,a)/(pow(t,a) + pow(1-t, a));
	if (ret >= 1)
		return .99;
	return ret;
}

float Tween::easeIn(float t, float a)
{
	float ret = pow(t, a);
	if(ret >= 1)
		return .99;
	return ret;
}

float Tween::easeOut(float t, float a)
{
	float ret = pow(t, 1/a);
	if(ret >= 1)
		return .99;
	return ret;
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
}

void Tween::update()
{
	SPDesktop * desktop = SP_ACTIVE_DESKTOP;
	SPPath * path = NULL;

	if(!desktop)
		return;

	SPObject * selected = desktop->getSelection()->single();

	const char * layerid = NULL;

	//check if it's a tween path
	if(SP_IS_TWEENPATH(selected))
	{
		path = SP_PATH(selected);
		//layerid = path->tweenId;
		layerid = tweenId;
	}
	else
	{
		SPObject * obj = desktop->currentLayer();

		if(obj && obj->getRepr())
			layerid = obj->getRepr()->attribute("inkscape:tweenstartid");
	}

	//if there is no tween associated with this path or layer, return
	if(!layerid)
		return;

	SPObject * layer = desktop->getDocument()->getObjectById(layerid);

	if(!layer)
		return;

	if(!path)
		return;

	int num_frames = 10;
	if(layer->getRepr()->attribute("inkscape:tweenlayers"))
		num_frames = atoi(layer->getRepr()->attribute("inkscape:tweenlayers"));

	SPObject * startLayer = layer;
	SPObject * endLayer = NULL;
	SPObject * nextLayer = NULL;
	
	SPCurve * curve = path->_curve;
	Geom::PathVector pathv = curve->get_pathvector();

	SPObject * child = NULL;

	float rotation = 0;

	Geom::Point p2(0,0);
	Geom::Point origp(0,0);
	Geom::Point rotp(0,0);
	Geom::OptRect test2(0,0);

	child = startLayer->firstChild();
	if(layer->getRepr()->attribute("inkscape:rotation"))
		rotation = atoi(layer->getRepr()->attribute("inkscape:rotation"));
	
	//if(!rotation)
	//	rotation = SP_ITEM(child)->transform.

	if(child)
	{
		Geom::Point p = pathv.initialPoint();
		
		
		//SP_ITEM(child)->transform.setTranslation(p);
		
		setPosition(child, p);

		//p2 = SP_ITEM(child)->getCenter() - Geom::Point(0, desktop->getDocument()->getHeight().value("px"));

		//origp = SP_ITEM(child)->transform.translation();
		//rotp = SP_ITEM(child)->transform.rotationCenter();

		//test2 = SP_ITEM(child)->documentVisualBounds();

		///////////////////////////////////////////////////////////child->setAttribute("transform",
		//						Glib::ustring::format("translate(", p[0], ",", p[1], ")" ));
	}

	layer = layer->next;

	int i = 1;
	while(layer)
	{
		//if(!layer->getRepr()->attribute("inkscape:tween") && !layer->getRepr()->attribute("inkscape:tweenstart"))
		//	return;
		
		if(!layer->getRepr()->attribute("inkscape:tween"))
			return;

		if(layer->getRepr()->attribute("inkscape:tweenend"))
		{
			endLayer = layer;
			child = endLayer->firstChild();
			if(child)
			{
				Geom::Point p = pathv.finalPoint();
				//SP_ITEM(child)->transform.setTranslation(p);
				
				setPosition(child, p);

				Geom::Affine test = Geom::Rotate::around(p, rotation*M_PI/180);

				//SP_ITEM(child)->transform *= test;

				//child->updateRepr();
				//////////////////////////////child->setAttribute("transform",
						//Glib::ustring::format("translate(", p[0], ",", p[1], ")" ));
			}

			//layer->updateRepr();
			break;
		}

		//Geom::Point p = pathv.pointAt(easeIn((i)*pathv.timeRange().max()/(num_frames + 1), 1.2));

		Geom::Point p(0,0);
		const char * easein = startLayer->getRepr()->attribute("inkscape:easein");
		const char * easeout = startLayer->getRepr()->attribute("inkscape:easeout");

		if(easein && easein != "0" && easeout && easeout != "0")
		{
			float power = atoi(easein)+1;
			p = pathv.pointAt(easeInOut((i)*pathv.timeRange().max()/(num_frames), power));
		}
		else if(easein && easein != "0")
		{
			float power = atoi(easein)+1;
			p = pathv.pointAt(easeIn((i)*pathv.timeRange().max()/(num_frames), power));
		}
		else if(easeout && easeout != "0")
		{
			float power = atoi(easein)+1;
			p = pathv.pointAt(easeOut((i)*pathv.timeRange().max()/(num_frames), power));
		}else
			p = pathv.pointAt(i*pathv.timeRange().max()/(num_frames));

		child = layer->firstChild();

		if(child)
		{
			Geom::Affine aff = Geom::Translate(p);
			//SP_ITEM(child)->transform.setTranslation(p); //does not update immediately for some objects!!
			
			
			setPosition(child, p);
			
			origp = SP_ITEM(child)->transform.translation();
			test2 = SP_ITEM(child)->visualBounds();


			Geom::Point rot2p = Geom::Point(SP_ITEM(child)->transform_center_x, SP_ITEM(child)->transform_center_y);

			//Geom::Affine test = Geom::Rotate::around(p + p2, i*rotation*M_PI/180/(num_frames));
			Geom::Affine test = Geom::Rotate::around(p + Geom::Point(test2->width()/2, test2->height()/2), i*rotation*M_PI/180/(num_frames));
			Geom::Affine res = aff*test;
			//SP_ITEM(child)->transform = res;
			std::cout<<test2->width()<<std::endl;


			//child->updateRepr();
		}

		nextLayer = layer->next;
		layer = nextLayer;
		i++;
	}

}

Tween::Tween(KeyframeWidget * start) {
	
	SPObject * layer = start->layer;
	SPObject * startLayer = start->layer;
	SPObject * endLayer = NULL;
	SPObject * nextLayer = NULL;
	float start_x=0, start_y=0, end_x=0, end_y=0, inc_x=0, inc_y=0, start_opacity=1, end_opacity=1, inc_opacity=0, inc_r=0, inc_g=0, inc_b=0;
	float scale_x=1, scale_y=1, inc_scale_x=0, inc_scale_y=0;
	float start_scale_x = 1, end_scale_x = 1, start_scale_y=1, end_scale_y=1;
	gfloat start_rgb[3];
	gfloat end_rgb[3];
	std::string xs = "x";
	std::string ys = "y";
	bool is_group = false;
	bool is_path = false;
	bool is_along_path = false;
	int i = start->id+1;
	int num_layers = 1;
	int num_nodes = 0;
	
	//sigc::connection _sel_changed_connection;
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	
	if(desktop)
	{
		//Inkscape::Selection * selection = desktop->getSelection();
		
		//_sel_changed_connection = selection->connectChangedFirst(
		//		sigc::hide(sigc::mem_fun(*this, &Tween::update)));
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
		
		layer->getRepr()->setAttribute("inkscape:tween", "true");
		
		//as soon as a layer has a child, break and set endLayer to this!
		if (layer->getRepr()->childCount() > 0)
			break;

		num_layers++;
		i++;
	}
	
	endLayer = layer;
	//end = endLayer;
	
	//set tweenend to end layer
	endLayer->getRepr()->setAttribute("inkscape:tweenend", "true");
	endLayer->getRepr()->setAttribute("inkscape:tween", "true");
	startLayer->getRepr()->setAttribute("inkscape:tweenlayers", Glib::ustring::format(num_layers));
	
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
			is_path = true;
		}
		
		//else if a group, take special care later!
		if(!strcmp(childn->name(), "svg:g"))
		{
			is_group = true;
			end_x = SP_ITEM(child)->transform.translation()[0];
			end_y = SP_ITEM(child)->transform.translation()[1];

			end_scale_x = SP_ITEM(child)->transform[0];
			end_scale_y = SP_ITEM(child)->transform[3];
		}
	}
	
	
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
		
		if(is_group)
		{
			start_x = SP_ITEM(child)->transform.translation()[0];
			start_y = SP_ITEM(child)->transform.translation()[1];
			start_scale_x = SP_ITEM(child)->transform[0];
			start_scale_y = SP_ITEM(child)->transform[3];
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
	
	//spdc_flush_white in freehand-base.cpp!
	//FreehandBase *dc = new FreehandBase();

	inc_scale_x = (end_scale_x - start_scale_x)/num_layers;
	inc_scale_y = (end_scale_y - start_scale_y)/num_layers;

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
		
		//SPObject * child = layer->firstChild();
		SPObject * child = layer->firstChild();
		
		if(!child)
			break;
		
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
		
		//if(!SP_ITEM(child)->style->stroke.isNone()) etcetc
		
		//tween opacity
		SP_ITEM(child)->style->opacity.value = start_opacity + (i-1)*inc_opacity;

		//scale
		//SP_ITEM(child)->scaleCenter(Geom::Scale(i*inc_scale_x, i*inc_scale_y));
		//SP_ITEM(child)->scaleCenter(Geom::Scale(10, 10));
		
		if(SP_IS_RECT(child))
		{
			SPRect * rect = SP_RECT(child);
		}

		setScale(child, start_scale_x + (i-1)*inc_scale_x, start_scale_y + (i-1)*inc_scale_y);
		//SP_ITEM(child)->transform[0] = start_scale_x + (i-1)*inc_scale_x;
		//SP_ITEM(child)->transform[3] = start_scale_y + (i-1)*inc_scale_y;

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

	
	//is group, ellipse, rect etc etc
	if(startLayer->getRepr()->childCount() == 1)
		linearTween(startLayer, endLayer, start_x, start_y, end_x, end_y, inc_x, inc_y);
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
