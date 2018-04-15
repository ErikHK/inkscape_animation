#include "inkview-options-group.h"

#include <glibmm/i18n.h>

InkviewOptionsGroup::InkviewOptionsGroup()
    : Glib::OptionGroup(N_("Inkscape Options"),
                        N_("Default program options"))
{
    // Entry for the "fullscreen" option
    Glib::OptionEntry entry_fullscreen;
    entry_fullscreen.set_short_name('f');
    entry_fullscreen.set_long_name("fullscreen");
    entry_fullscreen.set_description(N_("Launch in fullscreen mode"));
    add_entry(entry_fullscreen, fullscreen);

    // Entry for the "recursive" option
    Glib::OptionEntry entry_recursive;
    entry_recursive.set_short_name('r');
    entry_recursive.set_long_name("recursive");
    entry_recursive.set_description(N_("Search folders recursively"));
    add_entry(entry_recursive, recursive);

    // Entry for the "timer" option
    Glib::OptionEntry entry_timer;
    entry_timer.set_short_name('t');
    entry_timer.set_long_name("timer");
    entry_timer.set_arg_description(N_("NUM"));
    entry_timer.set_description(N_("Change image every NUM seconds"));
    add_entry(entry_timer, timer);

    // Entry for the "scale" option
    Glib::OptionEntry entry_scale;
    entry_scale.set_short_name('s');
    entry_scale.set_long_name("scale");
    entry_scale.set_arg_description(N_("NUM"));
    entry_scale.set_description(N_("Scale image by factor NUM"));
    add_entry(entry_scale, scale);

    // Entry for the remaining non-option arguments
    Glib::OptionEntry entry_args;
    entry_args.set_long_name(G_OPTION_REMAINING);
    entry_args.set_arg_description(N_("FILES/FOLDERS…"));

    add_entry(entry_args, filenames);
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
