/** @file
 * @brief Inkscape animation timeline (keyframes).
 */
/* Authors:
 *   Erik HK
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <gtkmm/drawingarea.h>



//namespace Inkscape {
//namespace UI {
//namespace Dialogs {


class TimelineItem : public Gtk::DrawingArea
{
public:
	TimelineItem();
	virtual ~TimelineItem();
    
protected:
	//bool (const Cairo::RefPtr<Cairo::Context>& cr);
	virtual bool on_expose_event(GdkEventExpose* event);
};

//} // namespace Dialogs
//} // namespace UI
//} // namespace Inkscape

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
