#ifndef SEEN_TWEEN_H
#define SEEN_TWEEN_H

/*
 * Tween implementation
 *
 * Author:
 *   Erik HK <erikhk@gmail.com>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

//#include "sp-path.h"
//#include "sp-conn-end-pair.h"

class KeyframeWidget;
class KeyframeBar;

class Tween {
public:
	Tween(KeyframeWidget * start);
	virtual ~Tween();
	const char * tweenId;
	const char * tweenPathId;
	const char * tweenStartId;
	
	SPPath * tweenPath;
	KeyframeWidget * start;
	KeyframeWidget * end;
	
	SPObject * endLayer;
	SPObject * startLayer;

	std::vector<SPObject *> objects;

	int numFrames;
	
	SPCurve * createGuide(float start_x, float start_y, float end_x, float end_y);
	void linearTween(SPObject * startLayer, SPObject * endLayer, float start_x, float start_y, float end_x, float end_y, float inc_x, float inc_y);
	void copyObjectToKeyframes(SPObject * start_layer, SPObject * end_layer);
	void showAllKeyframes();
	void update();
	void addToTween(SPObject * obj);
	float easeInOut(float t, float a);
	float easeIn(float t, float a);
	float easeOut(float t, float a);
	void setScale(SPObject * child, double width, double height);
	void setPosition(SPObject * child, Geom::Point p);
	Geom::Point moveGroupToCenter(SPGroup * g);
	Geom::Point getPoint(Geom::PathVector pathv, float t);
	float getEaseValue();
	
};

#endif // SEEN_TWEEN_H

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
