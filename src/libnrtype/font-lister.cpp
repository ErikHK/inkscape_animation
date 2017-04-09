#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtkmm/liststore.h>
#include <gtkmm/treemodel.h>

#include <libnrtype/font-instance.h>
#include <libnrtype/one-glyph.h>

#include "font-lister.h"
#include "FontFactory.h"

#include "desktop.h"
#include "desktop-style.h"
#include "document.h"
#include "inkscape.h"
#include "preferences.h"
#include "sp-object.h"
#include "sp-root.h"
#include "xml/repr.h"

#include <glibmm/regex.h>

//#define DEBUG_FONT

// CSS dictates that font family names are case insensitive.
// This should really implement full Unicode case unfolding.
bool familyNamesAreEqual(const Glib::ustring &a, const Glib::ustring &b)
{
    return (a.casefold().compare(b.casefold()) == 0);
}

static const char* sp_font_family_get_name(PangoFontFamily* family)
{
    const char* name = pango_font_family_get_name(family);
    if (strncmp(name, "Sans", 4) == 0 && strlen(name) == 4)
        return "sans-serif";
    if (strncmp(name, "Serif", 5) == 0 && strlen(name) == 5)
        return "serif";
    if (strncmp(name, "Monospace", 9) == 0 && strlen(name) == 9)
        return "monospace";
    return name;
}

namespace Inkscape {

FontLister::FontLister()
{
    font_list_store = Gtk::ListStore::create(FontList);
    font_list_store->freeze_notify();
    
    /* Create default styles for use when font-family is unknown on system. */
    default_styles = g_list_append(NULL, new StyleNames("Normal"));
    default_styles = g_list_append(default_styles, new StyleNames("Italic"));
    default_styles = g_list_append(default_styles, new StyleNames("Bold"));
    default_styles = g_list_append(default_styles, new StyleNames("Bold Italic"));

    // Get sorted font families from Pango
    std::vector<PangoFontFamily *> familyVector;
    font_factory::Default()->GetUIFamilies(familyVector);

    // Traverse through the family names and set up the list store
    for (size_t i = 0; i < familyVector.size(); ++i) {
        const char* displayName = sp_font_family_get_name(familyVector[i]);
        
        if (displayName == 0 || *displayName == '\0') {
            continue;
        }
        
        Glib::ustring familyName = displayName;
        if (!familyName.empty()) {
            Gtk::TreeModel::iterator treeModelIter = font_list_store->append();
            (*treeModelIter)[FontList.family] = familyName;

            // we don't set this now (too slow) but the style will be cached if the user 
            // ever decides to use this font
            (*treeModelIter)[FontList.styles] = NULL;
            // store the pango representation for generating the style
            (*treeModelIter)[FontList.pango_family] = familyVector[i];
            (*treeModelIter)[FontList.onSystem] = true;
        }
    }

    current_family_row = 0;
    current_family = "sans-serif";
    current_style = "Normal";
    current_fontspec = "sans-serif"; // Empty style -> Normal
    current_fontspec_system = "Sans";

    font_list_store->thaw_notify();

    style_list_store = Gtk::ListStore::create(FontStyleList);

    // Initialize style store with defaults
    style_list_store->freeze_notify();
    style_list_store->clear();
    for (GList *l = default_styles; l; l = l->next) {
        Gtk::TreeModel::iterator treeModelIter = style_list_store->append();
        (*treeModelIter)[FontStyleList.cssStyle] = ((StyleNames *)l->data)->CssName;
        (*treeModelIter)[FontStyleList.displayStyle] = ((StyleNames *)l->data)->DisplayName;
    }
    style_list_store->thaw_notify();
}

FontLister::~FontLister()
{
    // Delete default_styles
    for (GList *l = default_styles; l; l = l->next) {
        delete ((StyleNames *)l->data);
    }

    // Delete other styles
    Gtk::TreeModel::iterator iter = font_list_store->get_iter("0");
    while (iter != font_list_store->children().end()) {
        Gtk::TreeModel::Row row = *iter;
        GList *styles = row[FontList.styles];
        for (GList *l = styles; l; l = l->next) {
            delete ((StyleNames *)l->data);
        }
        ++iter;
    }
}

FontLister *FontLister::get_instance()
{
    static Inkscape::FontLister *instance = new Inkscape::FontLister();
    return instance;
}

void FontLister::ensureRowStyles(GtkTreeModel* model, GtkTreeIter const* iterator)
{
    Gtk::TreeIter iter(model, iterator);
    Gtk::TreeModel::Row row = *iter;
    if (!row[FontList.styles]) {
        if (row[FontList.pango_family]) {
            row[FontList.styles] = font_factory::Default()->GetUIStyles(row[FontList.pango_family]);
        } else {
            row[FontList.styles] = default_styles;
        }
    }
}

// Example of how to use "foreach_iter"
// bool
// FontLister::print_document_font( const Gtk::TreeModel::iterator &iter ) {
//   Gtk::TreeModel::Row row = *iter;
//   if( !row[FontList.onSystem] ) {
// 	   std::cout << " Not on system: " << row[FontList.family] << std::endl;
// 	   return false;
//   }
//   return true;
// }
// font_list_store->foreach_iter( sigc::mem_fun(*this, &FontLister::print_document_font ));

/* Used to insert a font that was not in the document and not on the system into the font list. */
void FontLister::insert_font_family(Glib::ustring new_family)
{
    GList *styles = default_styles;

    /* In case this is a fallback list, check if first font-family on system. */
    std::vector<Glib::ustring> tokens = Glib::Regex::split_simple(",", new_family);
    if (!tokens.empty() && !tokens[0].empty()) {
        Gtk::TreeModel::iterator iter2 = font_list_store->get_iter("0");
        while (iter2 != font_list_store->children().end()) {
            Gtk::TreeModel::Row row = *iter2;
            if (row[FontList.onSystem] && familyNamesAreEqual(tokens[0], row[FontList.family])) {
                if (!row[FontList.styles]) {
                    row[FontList.styles] = font_factory::Default()->GetUIStyles(row[FontList.pango_family]);
                }
                styles = row[FontList.styles];
                break;
            }
            ++iter2;
        }
    }

    Gtk::TreeModel::iterator treeModelIter = font_list_store->prepend();
    (*treeModelIter)[FontList.family] = new_family;
    (*treeModelIter)[FontList.styles] = styles;
    (*treeModelIter)[FontList.onSystem] = false;
    (*treeModelIter)[FontList.pango_family] = NULL;
}

void FontLister::update_font_list(SPDocument *document)
{
    SPObject *r = document->getRoot();
    if (!r) {
        return;
    }

    font_list_store->freeze_notify();

    /* Find if current row is in document or system part of list */
    gboolean row_is_system = false;
    if (current_family_row > -1) {
        Gtk::TreePath path;
        path.push_back(current_family_row);
        Gtk::TreeModel::iterator iter = font_list_store->get_iter(path);
        if (iter) {
            row_is_system = (*iter)[FontList.onSystem];
            // std::cout << "  In:  row: " << current_family_row << "  " << (*iter)[FontList.family] << std::endl;
        }
    }

    /* Clear all old document font-family entries */
    Gtk::TreeModel::iterator iter = font_list_store->get_iter("0");
    while (iter != font_list_store->children().end()) {
        Gtk::TreeModel::Row row = *iter;
        if (!row[FontList.onSystem]) {
            // std::cout << " Not on system: " << row[FontList.family] << std::endl;
            iter = font_list_store->erase(iter);
        } else {
            // std::cout << " First on system: " << row[FontList.family] << std::endl;
            break;
        }
    }

    /* Get "font-family"s used in document. */
    std::list<Glib::ustring> fontfamilies;
    update_font_list_recursive(r, &fontfamilies);

    fontfamilies.sort();
    fontfamilies.unique();
    fontfamilies.reverse();


    /* Insert separator */
    if (!fontfamilies.empty()) {
        Gtk::TreeModel::iterator treeModelIter = font_list_store->prepend();
        (*treeModelIter)[FontList.family] = "#";
        (*treeModelIter)[FontList.onSystem] = false;
    }

    /* Insert font-family's in document. */
    std::list<Glib::ustring>::iterator i;
    for (i = fontfamilies.begin(); i != fontfamilies.end(); ++i) {

        GList *styles = default_styles;

        /* See if font-family (or first in fallback list) is on system. If so, get styles. */
        std::vector<Glib::ustring> tokens = Glib::Regex::split_simple(",", *i);
        if (!tokens.empty() && !tokens[0].empty()) {

            Gtk::TreeModel::iterator iter2 = font_list_store->get_iter("0");
            while (iter2 != font_list_store->children().end()) {
                Gtk::TreeModel::Row row = *iter2;
                if (row[FontList.onSystem] && familyNamesAreEqual(tokens[0], row[FontList.family])) {
                    if (!row[FontList.styles]) {
                        row[FontList.styles] = font_factory::Default()->GetUIStyles(row[FontList.pango_family]);
                    }
                    styles = row[FontList.styles];
                    break;
                }
                ++iter2;
            }
        }

        Gtk::TreeModel::iterator treeModelIter = font_list_store->prepend();
        (*treeModelIter)[FontList.family] = reinterpret_cast<const char *>(g_strdup((*i).c_str()));
        (*treeModelIter)[FontList.styles] = styles;
        (*treeModelIter)[FontList.onSystem] = false;    // false if document font
        (*treeModelIter)[FontList.pango_family] = NULL; // CHECK ME (set to pango_family if on system?)

    }

    /* Now we do a song and dance to find the correct row as the row corresponding
     * to the current_family may have changed. We can't simply search for the
     * family name in the list since it can occur twice, once in the document
     * font family part and once in the system font family part. Above we determined
     * which part it is in.
     */
    if (current_family_row > -1) {
        int start = 0;
        if (row_is_system)
            start = fontfamilies.size();
        int length = font_list_store->children().size();
        for (int i = 0; i < length; ++i) {
            int row = i + start;
            if (row >= length)
                row -= length;
            Gtk::TreePath path;
            path.push_back(row);
            Gtk::TreeModel::iterator iter = font_list_store->get_iter(path);
            if (iter) {
                if (familyNamesAreEqual(current_family, (*iter)[FontList.family])) {
                    current_family_row = row;
                    break;
                }
            }
        }
    }
    // std::cout << "  Out: row: " << current_family_row << "  " << current_family << std::endl;

    font_list_store->thaw_notify();
}

void FontLister::update_font_list_recursive(SPObject *r, std::list<Glib::ustring> *l)
{
    const gchar *font_family = r->style->font_family.value;
    if (font_family) {
        l->push_back(Glib::ustring(font_family));
    }

    for (SPObject *child = r->firstChild(); child; child = child->getNext()) {
        update_font_list_recursive(child, l);
    }
}

Glib::ustring FontLister::canonize_fontspec(Glib::ustring fontspec)
{

    // Pass fontspec to and back from Pango to get a the fontspec in
    // canonical form.  -inkscape-font-specification relies on the
    // Pango constructed fontspec not changing form. If it does,
    // this is the place to fix it.
    PangoFontDescription *descr = pango_font_description_from_string(fontspec.c_str());
    gchar *canonized = pango_font_description_to_string(descr);
    Glib::ustring Canonized = canonized;
    g_free(canonized);
    pango_font_description_free(descr);

    // Pango canonized strings remove space after comma between family names. Put it back.
    size_t i = 0;
    while ((i = Canonized.find(",", i)) != std::string::npos) {
        Canonized.replace(i, 1, ", ");
        i += 2;
    }

    return Canonized;
}

Glib::ustring FontLister::system_fontspec(Glib::ustring fontspec)
{

    // Find what Pango thinks is the closest match.
    Glib::ustring out = fontspec;

    PangoFontDescription *descr = pango_font_description_from_string(fontspec.c_str());
    font_instance *res = (font_factory::Default())->Face(descr);
    if (res && res->pFont) {
        PangoFontDescription *nFaceDesc = pango_font_describe(res->pFont);
        out = sp_font_description_get_family(nFaceDesc);
    }
    pango_font_description_free(descr);

    return out;
}

std::pair<Glib::ustring, Glib::ustring> FontLister::ui_from_fontspec(Glib::ustring fontspec)
{
    PangoFontDescription *descr = pango_font_description_from_string(fontspec.c_str());
    const gchar *family = pango_font_description_get_family(descr);
    if (!family)
        family = "sans-serif";
    Glib::ustring Family = family;

    // PANGO BUG...
    //   A font spec of Delicious, 500 Italic should result in a family of 'Delicious'
    //   and a style of 'Medium Italic'. It results instead with: a family of
    //   'Delicious, 500' with a style of 'Medium Italic'. We chop of any weight numbers
    //   at the end of the family:  match ",[1-9]00^".
    Glib::RefPtr<Glib::Regex> weight = Glib::Regex::create(",[1-9]00$");
    Family = weight->replace(Family, 0, "", Glib::REGEX_MATCH_PARTIAL);

    // Pango canonized strings remove space after comma between family names. Put it back.
    size_t i = 0;
    while ((i = Family.find(",", i)) != std::string::npos) {
        Family.replace(i, 1, ", ");
        i += 2;
    }

    pango_font_description_unset_fields(descr, PANGO_FONT_MASK_FAMILY);
    gchar *style = pango_font_description_to_string(descr);
    Glib::ustring Style = style;
    pango_font_description_free(descr);
    g_free(style);

    return std::make_pair(Family, Style);
}

std::pair<Glib::ustring, Glib::ustring> FontLister::selection_update()
{

#ifdef DEBUG_FONT
    std::cout << "\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
    std::cout << "FontLister::selection_update: entrance" << std::endl;
#endif
    // Get fontspec from a selection, preferences, or thin air.
    Glib::ustring fontspec;
    SPStyle query(SP_ACTIVE_DOCUMENT);

    // Directly from stored font specification.
    int result =
        sp_desktop_query_style(SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_FONT_SPECIFICATION);

    //std::cout << "  Attempting selected style" << std::endl;
    if (result != QUERY_STYLE_NOTHING && query.font_specification.set) {
        fontspec = query.font_specification.value;
        //std::cout << "   fontspec from query   :" << fontspec << ":" << std::endl;
    }

    // From style
    if (fontspec.empty()) {
        //std::cout << "  Attempting desktop style" << std::endl;
        int rfamily = sp_desktop_query_style(SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_FONTFAMILY);
        int rstyle = sp_desktop_query_style(SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_FONTSTYLE);

        // Must have text in selection
        if (rfamily != QUERY_STYLE_NOTHING && rstyle != QUERY_STYLE_NOTHING) {
            fontspec = fontspec_from_style(&query);
        }
        //std::cout << "   fontspec from style   :" << fontspec << ":" << std::endl;
    }

    // From preferences
    if (fontspec.empty()) {
        //std::cout << "  Attempting preferences" << std::endl;
        query.readFromPrefs("/tools/text");
        fontspec = fontspec_from_style(&query);
        //std::cout << "   fontspec from prefs   :" << fontspec << ":" << std::endl;
    }

    // From thin air
    if (fontspec.empty()) {
        //std::cout << "  Attempting thin air" << std::endl;
        fontspec = current_family + ", " + current_style;
        //std::cout << "   fontspec from thin air   :" << fontspec << ":" << std::endl;
    }

    // Do we really need? Removes spaces between font-families.
    //current_fontspec = canonize_fontspec( fontspec );
    current_fontspec = fontspec; // Ignore for now

    current_fontspec_system = system_fontspec(current_fontspec);

    std::pair<Glib::ustring, Glib::ustring> ui = ui_from_fontspec(current_fontspec);
    set_font_family(ui.first);
    set_font_style(ui.second);

#ifdef DEBUG_FONT
    std::cout << "   family_row:           :" << current_family_row << ":" << std::endl;
    std::cout << "   canonized:            :" << current_fontspec << ":" << std::endl;
    std::cout << "   system:               :" << current_fontspec_system << ":" << std::endl;
    std::cout << "   family:               :" << current_family << ":" << std::endl;
    std::cout << "   style:                :" << current_style << ":" << std::endl;
    std::cout << "FontLister::selection_update: exit" << std::endl;
    std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n" << std::endl;
#endif
    return std::make_pair(current_family, current_style);
}


// Set fontspec. If check is false, best style match will not be done.
void FontLister::set_fontspec(Glib::ustring new_fontspec, bool /*check*/)
{
    std::pair<Glib::ustring, Glib::ustring> ui = ui_from_fontspec(new_fontspec);
    Glib::ustring new_family = ui.first;
    Glib::ustring new_style = ui.second;

#ifdef DEBUG_FONT
    std::cout << "FontLister::set_fontspec: family: " << new_family
              << "   style:" << new_style << std::endl;
#endif

    set_font_family(new_family, false);
    set_font_style(new_style);
}


// TODO: use to determine font-selector best style
std::pair<Glib::ustring, Glib::ustring> FontLister::new_font_family(Glib::ustring new_family, bool /*check_style*/)
{
#ifdef DEBUG_FONT
    std::cout << "\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
    std::cout << "FontLister::new_font_family: " << new_family << std::endl;
#endif

    // No need to do anything if new family is same as old family.
    if (familyNamesAreEqual(new_family, current_family)) {
#ifdef DEBUG_FONT
        std::cout << "FontLister::new_font_family: exit: no change in family." << std::endl;
        std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n" << std::endl;
#endif
        return std::make_pair(current_family, current_style);
    }

    // We need to do two things:
    // 1. Update style list for new family.
    // 2. Select best valid style match to old style.

    // For finding style list, use list of first family in font-family list.
    GList *styles = NULL;
    Gtk::TreeModel::iterator iter = font_list_store->get_iter("0");
    while (iter != font_list_store->children().end()) {

        Gtk::TreeModel::Row row = *iter;

        if (familyNamesAreEqual(new_family, row[FontList.family])) {
            if (!row[FontList.styles]) {
                row[FontList.styles] = font_factory::Default()->GetUIStyles(row[FontList.pango_family]);
            }
            styles = row[FontList.styles];
            break;
        }
        ++iter;
    }

    // Newly typed in font-family may not yet be in list... use default list.
    // TODO: if font-family is list, check if first family in list is on system
    // and set style accordingly.
    if (styles == NULL) {
        styles = default_styles;
    }

    // Update style list.
    style_list_store->freeze_notify();
    style_list_store->clear();

    for (GList *l = styles; l; l = l->next) {
        Gtk::TreeModel::iterator treeModelIter = style_list_store->append();
        (*treeModelIter)[FontStyleList.cssStyle] = ((StyleNames *)l->data)->CssName;
        (*treeModelIter)[FontStyleList.displayStyle] = ((StyleNames *)l->data)->DisplayName;
    }

    style_list_store->thaw_notify();

    // Find best match to the style from the old font-family to the
    // styles available with the new font.
    // TODO: Maybe check if an exact match exists before using Pango.
    Glib::ustring best_style = get_best_style_match(new_family, current_style);

#ifdef DEBUG_FONT
    std::cout << "FontLister::new_font_family: exit: " << new_family << " " << best_style << std::endl;
    std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n" << std::endl;
#endif
    return std::make_pair(new_family, best_style);
}

std::pair<Glib::ustring, Glib::ustring> FontLister::set_font_family(Glib::ustring new_family, bool check_style)
{

#ifdef DEBUG_FONT
    std::cout << "\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
    std::cout << "FontLister::set_font_family: " << new_family << std::endl;
#endif

    std::pair<Glib::ustring, Glib::ustring> ui = new_font_family(new_family, check_style);
    current_family = ui.first;
    current_style = ui.second;
    current_fontspec = canonize_fontspec(current_family + ", " + current_style);
    current_fontspec_system = system_fontspec(current_fontspec);

#ifdef DEBUG_FONT
    std::cout << "   family_row:           :" << current_family_row << ":" << std::endl;
    std::cout << "   canonized:            :" << current_fontspec << ":" << std::endl;
    std::cout << "   system:               :" << current_fontspec_system << ":" << std::endl;
    std::cout << "   family:               :" << current_family << ":" << std::endl;
    std::cout << "   style:                :" << current_style << ":" << std::endl;
    std::cout << "FontLister::set_font_family: end" << std::endl;
    std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n" << std::endl;
#endif
    return ui;
}


std::pair<Glib::ustring, Glib::ustring> FontLister::set_font_family(int row, bool check_style)
{

#ifdef DEBUG_FONT
    std::cout << "\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
    std::cout << "FontLister::set_font_family( row ): " << row << std::endl;
#endif

    current_family_row = row;
    Gtk::TreePath path;
    path.push_back(row);
    Glib::ustring new_family = current_family;
    Gtk::TreeModel::iterator iter = font_list_store->get_iter(path);
    if (iter) {
        new_family = (*iter)[FontList.family];
    }

    std::pair<Glib::ustring, Glib::ustring> ui = set_font_family(new_family, check_style);

#ifdef DEBUG_FONT
    std::cout << "FontLister::set_font_family( row ): end" << std::endl;
    std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n" << std::endl;
#endif
    return ui;
}


void FontLister::set_font_style(Glib::ustring new_style)
{

// TODO: Validate input using Pango. If Pango doesn't recognize a style it will
// attach the "invalid" style to the font-family.

#ifdef DEBUG_FONT
    std::cout << "\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
    std::cout << "FontLister:set_font_style: " << new_style << std::endl;
#endif

    current_style = new_style;
    current_fontspec = canonize_fontspec(current_family + ", " + current_style);
    current_fontspec_system = system_fontspec(current_fontspec);

#ifdef DEBUG_FONT
    std::cout << "   canonized:            :" << current_fontspec << ":" << std::endl;
    std::cout << "   system:               :" << current_fontspec_system << ":" << std::endl;
    std::cout << "   family:                " << current_family << std::endl;
    std::cout << "   style:                 " << current_style << std::endl;
    std::cout << "FontLister::set_font_style: end" << std::endl;
    std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n" << std::endl;
#endif
}


// We do this ourselves as we can't rely on FontFactory.
void FontLister::fill_css(SPCSSAttr *css, Glib::ustring fontspec)
{

    if (fontspec.empty()) {
        fontspec = current_fontspec;
    }
    std::pair<Glib::ustring, Glib::ustring> ui = ui_from_fontspec(fontspec);

    Glib::ustring family = ui.first;


    // Font spec is single quoted... for the moment
    Glib::ustring fontspec_quoted(fontspec);
    css_quote(fontspec_quoted);
    sp_repr_css_set_property(css, "-inkscape-font-specification", fontspec_quoted.c_str());

    // Font families needs to be properly quoted in CSS (used unquoted in font-lister)
    css_font_family_quote(family);
    sp_repr_css_set_property(css, "font-family", family.c_str());

    PangoFontDescription *desc = pango_font_description_from_string(fontspec.c_str());
    PangoWeight weight = pango_font_description_get_weight(desc);
    switch (weight) {
        case PANGO_WEIGHT_THIN:
            sp_repr_css_set_property(css, "font-weight", "100");
            break;
        case PANGO_WEIGHT_ULTRALIGHT:
            sp_repr_css_set_property(css, "font-weight", "200");
            break;
        case PANGO_WEIGHT_LIGHT:
            sp_repr_css_set_property(css, "font-weight", "300");
            break;
#if PANGO_VERSION_CHECK(1,36,6)
        case PANGO_WEIGHT_SEMILIGHT:
            sp_repr_css_set_property(css, "font-weight", "350");
            break;
#endif
        case PANGO_WEIGHT_BOOK:
            sp_repr_css_set_property(css, "font-weight", "380");
            break;
        case PANGO_WEIGHT_NORMAL:
            sp_repr_css_set_property(css, "font-weight", "normal");
            break;
        case PANGO_WEIGHT_MEDIUM:
            sp_repr_css_set_property(css, "font-weight", "500");
            break;
        case PANGO_WEIGHT_SEMIBOLD:
            sp_repr_css_set_property(css, "font-weight", "600");
            break;
        case PANGO_WEIGHT_BOLD:
            sp_repr_css_set_property(css, "font-weight", "bold");
            break;
        case PANGO_WEIGHT_ULTRABOLD:
            sp_repr_css_set_property(css, "font-weight", "800");
            break;
        case PANGO_WEIGHT_HEAVY:
            sp_repr_css_set_property(css, "font-weight", "900");
            break;
        case PANGO_WEIGHT_ULTRAHEAVY:
            sp_repr_css_set_property(css, "font-weight", "1000");
            break;
    }

    PangoStyle style = pango_font_description_get_style(desc);
    switch (style) {
        case PANGO_STYLE_NORMAL:
            sp_repr_css_set_property(css, "font-style", "normal");
            break;
        case PANGO_STYLE_OBLIQUE:
            sp_repr_css_set_property(css, "font-style", "oblique");
            break;
        case PANGO_STYLE_ITALIC:
            sp_repr_css_set_property(css, "font-style", "italic");
            break;
    }

    PangoStretch stretch = pango_font_description_get_stretch(desc);
    switch (stretch) {
        case PANGO_STRETCH_ULTRA_CONDENSED:
            sp_repr_css_set_property(css, "font-stretch", "ultra-condensed");
            break;
        case PANGO_STRETCH_EXTRA_CONDENSED:
            sp_repr_css_set_property(css, "font-stretch", "extra-condensed");
            break;
        case PANGO_STRETCH_CONDENSED:
            sp_repr_css_set_property(css, "font-stretch", "condensed");
            break;
        case PANGO_STRETCH_SEMI_CONDENSED:
            sp_repr_css_set_property(css, "font-stretch", "semi-condensed");
            break;
        case PANGO_STRETCH_NORMAL:
            sp_repr_css_set_property(css, "font-stretch", "normal");
            break;
        case PANGO_STRETCH_SEMI_EXPANDED:
            sp_repr_css_set_property(css, "font-stretch", "semi-expanded");
            break;
        case PANGO_STRETCH_EXPANDED:
            sp_repr_css_set_property(css, "font-stretch", "expanded");
            break;
        case PANGO_STRETCH_EXTRA_EXPANDED:
            sp_repr_css_set_property(css, "font-stretch", "extra-expanded");
            break;
        case PANGO_STRETCH_ULTRA_EXPANDED:
            sp_repr_css_set_property(css, "font-stretch", "ultra-expanded");
            break;
    }

    PangoVariant variant = pango_font_description_get_variant(desc);
    switch (variant) {
        case PANGO_VARIANT_NORMAL:
            sp_repr_css_set_property(css, "font-variant", "normal");
            break;
        case PANGO_VARIANT_SMALL_CAPS:
            sp_repr_css_set_property(css, "font-variant", "small-caps");
            break;
    }
}

// We do this ourselves as we can't rely on FontFactory.
Glib::ustring FontLister::fontspec_from_style(SPStyle *style)
{

    //std::cout << "FontLister:fontspec_from_style: " << std::endl;

    Glib::ustring fontspec;
    if (style) {

        //  First try to use the font specification if it is set
        if (style->font_specification.set && style->font_specification.value && *style->font_specification.value) {

            fontspec = style->font_specification.value;

        } else {

            fontspec = style->font_family.value;
            fontspec += ",";

            // Use weight names as defined by Pango
            switch (style->font_weight.computed) {

                case SP_CSS_FONT_WEIGHT_100:
                    fontspec += " Thin";
                    break;

                case SP_CSS_FONT_WEIGHT_200:
                    fontspec += " Ultra-Light";
                    break;

                case SP_CSS_FONT_WEIGHT_300:
                    fontspec += " Light";
                    break;

                case SP_CSS_FONT_WEIGHT_400:
                case SP_CSS_FONT_WEIGHT_NORMAL:
                    //fontspec += " normal";
                    break;

                case SP_CSS_FONT_WEIGHT_500:
                    fontspec += " Medium";
                    break;

                case SP_CSS_FONT_WEIGHT_600:
                    fontspec += " Semi-Bold";
                    break;

                case SP_CSS_FONT_WEIGHT_700:
                case SP_CSS_FONT_WEIGHT_BOLD:
                    fontspec += " Bold";
                    break;

                case SP_CSS_FONT_WEIGHT_800:
                    fontspec += " Ultra-Bold";
                    break;

                case SP_CSS_FONT_WEIGHT_900:
                    fontspec += " Heavy";
                    break;

                case SP_CSS_FONT_WEIGHT_LIGHTER:
                case SP_CSS_FONT_WEIGHT_BOLDER:
                default:
                    g_warning("Unrecognized font_weight.computed value");
                    break;
            }

            switch (style->font_style.computed) {
                case SP_CSS_FONT_STYLE_ITALIC:
                    fontspec += " italic";
                    break;

                case SP_CSS_FONT_STYLE_OBLIQUE:
                    fontspec += " oblique";
                    break;

                case SP_CSS_FONT_STYLE_NORMAL:
                default:
                    //fontspec += " normal";
                    break;
            }

            switch (style->font_stretch.computed) {

                case SP_CSS_FONT_STRETCH_ULTRA_CONDENSED:
                    fontspec += " ultra-condensed";
                    break;

                case SP_CSS_FONT_STRETCH_EXTRA_CONDENSED:
                    fontspec += " extra-condensed";
                    break;

                case SP_CSS_FONT_STRETCH_CONDENSED:
                case SP_CSS_FONT_STRETCH_NARROWER:
                    fontspec += " condensed";
                    break;

                case SP_CSS_FONT_STRETCH_SEMI_CONDENSED:
                    fontspec += " semi-condensed";
                    break;

                case SP_CSS_FONT_STRETCH_NORMAL:
                    //fontspec += " normal";
                    break;

                case SP_CSS_FONT_STRETCH_SEMI_EXPANDED:
                    fontspec += " semi-expanded";
                    break;

                case SP_CSS_FONT_STRETCH_EXPANDED:
                case SP_CSS_FONT_STRETCH_WIDER:
                    fontspec += " expanded";
                    break;

                case SP_CSS_FONT_STRETCH_EXTRA_EXPANDED:
                    fontspec += " extra-expanded";
                    break;

                case SP_CSS_FONT_STRETCH_ULTRA_EXPANDED:
                    fontspec += " ultra-expanded";
                    break;

                default:
                    //fontspec += " normal";
                    break;
            }

            switch (style->font_variant.computed) {

                case SP_CSS_FONT_VARIANT_SMALL_CAPS:
                    fontspec += "small-caps";
                    break;

                default:
                    //fontspec += "normal";
                    break;
            }
        }
    }
    return canonize_fontspec(fontspec);
}


Gtk::TreeModel::Row FontLister::get_row_for_font(Glib::ustring family)
{
    Gtk::TreePath path;

    Gtk::TreeModel::iterator iter = font_list_store->get_iter("0");
    while (iter != font_list_store->children().end()) {

        Gtk::TreeModel::Row row = *iter;

        if (familyNamesAreEqual(family, row[FontList.family])) {
            return row;
        }

        ++iter;
    }

    throw FAMILY_NOT_FOUND;
}

Gtk::TreePath FontLister::get_path_for_font(Glib::ustring family)
{
    return font_list_store->get_path(get_row_for_font(family));
}

Gtk::TreeModel::Row FontLister::get_row_for_style(Glib::ustring style)
{
    Gtk::TreePath path;

    Gtk::TreeModel::iterator iter = style_list_store->get_iter("0");
    while (iter != style_list_store->children().end()) {

        Gtk::TreeModel::Row row = *iter;

        if (familyNamesAreEqual(style, row[FontStyleList.cssStyle])) {
            return row;
        }

        ++iter;
    }

    throw STYLE_NOT_FOUND;
}

static gint compute_distance(const PangoFontDescription *a, const PangoFontDescription *b)
{

    // Weight: multiples of 100
    gint distance = abs(pango_font_description_get_weight(a) -
                        pango_font_description_get_weight(b));

    distance += 10000 * abs(pango_font_description_get_stretch(a) -
                            pango_font_description_get_stretch(b));

    PangoStyle style_a = pango_font_description_get_style(a);
    PangoStyle style_b = pango_font_description_get_style(b);
    if (style_a != style_b) {
        if ((style_a == PANGO_STYLE_OBLIQUE && style_b == PANGO_STYLE_ITALIC) ||
            (style_b == PANGO_STYLE_OBLIQUE && style_a == PANGO_STYLE_ITALIC)) {
            distance += 1000; // Oblique and italic are almost the same
        } else {
            distance += 100000; // Normal vs oblique/italic, not so similar
        }
    }

    // Normal vs small-caps
    distance += 1000000 * abs(pango_font_description_get_variant(a) -
                              pango_font_description_get_variant(b));
    return distance;
}

// This is inspired by pango_font_description_better_match, but that routine
// always returns false if variant or stretch are different. This means, for
// example, that PT Sans Narrow with style Bold Condensed is never matched
// to another font-family with Bold style.
gboolean font_description_better_match(PangoFontDescription *target, PangoFontDescription *old_desc, PangoFontDescription *new_desc)
{
    if (old_desc == NULL)
        return true;
    if (new_desc == NULL)
        return false;

    int old_distance = compute_distance(target, old_desc);
    int new_distance = compute_distance(target, new_desc);
    //std::cout << "font_description_better_match: old: " << old_distance << std::endl;
    //std::cout << "                               new: " << new_distance << std::endl;

    return (new_distance < old_distance);
}

// void
// font_description_dump( PangoFontDescription* target ) {
//   std::cout << "  Font:      " << pango_font_description_to_string( target ) << std::endl;
//   std::cout << "    style:   " << pango_font_description_get_style(   target ) << std::endl;
//   std::cout << "    weight:  " << pango_font_description_get_weight(  target ) << std::endl;
//   std::cout << "    variant: " << pango_font_description_get_variant( target ) << std::endl;
//   std::cout << "    stretch: " << pango_font_description_get_stretch( target ) << std::endl;
//   std::cout << "    gravity: " << pango_font_description_get_gravity( target ) << std::endl;
// }

/* Returns style string */
// TODO: Remove or turn into function to be used by new_font_family.
Glib::ustring FontLister::get_best_style_match(Glib::ustring family, Glib::ustring target_style)
{

#ifdef DEBUG_FONT
    std::cout << "\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
    std::cout << "FontLister::get_best_style_match: " << family << " : " << target_style << std::endl;
#endif

    Glib::ustring fontspec = family + ", " + target_style;

    Gtk::TreeModel::Row row;
    try
    {
        row = get_row_for_font(family);
    }
    catch (...)
    {
        std::cerr << "FontLister::get_best_style_match(): can't find family: " << family << std::endl;
        return (target_style);
    }

    PangoFontDescription *target = pango_font_description_from_string(fontspec.c_str());
    PangoFontDescription *best = NULL;

    //font_description_dump( target );

    GList *styles = default_styles;
    if (row[FontList.onSystem] && !row[FontList.styles]) {
        row[FontList.styles] = font_factory::Default()->GetUIStyles(row[FontList.pango_family]);
        styles = row[FontList.styles];
    }

    for (GList *l = styles; l; l = l->next) {
        Glib::ustring fontspec = family + ", " + ((StyleNames *)l->data)->CssName;
        PangoFontDescription *candidate = pango_font_description_from_string(fontspec.c_str());
        //font_description_dump( candidate );
        //std::cout << "           " << font_description_better_match( target, best, candidate ) << std::endl;
        if (font_description_better_match(target, best, candidate)) {
            pango_font_description_free(best);
            best = candidate;
            //std::cout << "  ... better: " << std::endl;
        } else {
            pango_font_description_free(candidate);
            //std::cout << "  ... not better: " << std::endl;
        }
    }

    Glib::ustring best_style = target_style;
    if (best) {
        pango_font_description_unset_fields(best, PANGO_FONT_MASK_FAMILY);
        best_style = pango_font_description_to_string(best);
    }

    if (target)
        pango_font_description_free(target);
    if (best)
        pango_font_description_free(best);


#ifdef DEBUG_FONT
    std::cout << "  Returning: " << best_style << std::endl;
    std::cout << "FontLister::get_best_style_match: exit" << std::endl;
    std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n" << std::endl;
#endif
    return best_style;
}

const Glib::RefPtr<Gtk::ListStore> FontLister::get_font_list() const
{
    return font_list_store;
}

const Glib::RefPtr<Gtk::ListStore> FontLister::get_style_list() const
{
    return style_list_store;
}

} // namespace Inkscape

// Helper functions

// Separator function (if true, a separator will be drawn)
gboolean font_lister_separator_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer /*data*/)
{
    gchar *text = 0;
    gtk_tree_model_get(model, iter, 0, &text, -1); // Column 0: FontList.family
    return (text && strcmp(text, "#") == 0);
}

void font_lister_cell_data_func(GtkCellLayout * /*cell_layout*/,
                                GtkCellRenderer *cell,
                                GtkTreeModel *model,
                                GtkTreeIter *iter,
                                gpointer /*data*/)
{
    gchar *family;
    gboolean onSystem = false;
    gtk_tree_model_get(model, iter, 0, &family, 2, &onSystem, -1);
    gchar* family_escaped = g_markup_escape_text(family, -1);
    //g_free(family);
    Glib::ustring markup;

    if (!onSystem) {
        markup = "<span foreground='darkblue'>";

        /* See if font-family on system */
        std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("\\s*,\\s*", family_escaped);
        for (size_t i = 0; i < tokens.size(); ++i) {

            Glib::ustring token = tokens[i];

            GtkTreeIter iter;
            gboolean valid;
            gchar *family = 0;
            gboolean onSystem = true;
            gboolean found = false;
            for (valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter);
                 valid;
                 valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter)) {

                gtk_tree_model_get(model, &iter, 0, &family, 2, &onSystem, -1);
                if (onSystem && familyNamesAreEqual(token, family)) {
                    found = true;
                    break;
                }
            }
            if (found) {
                markup += g_markup_escape_text(token.c_str(), -1);
                markup += ", ";
            } else {
                markup += "<span strikethrough=\"true\" strikethrough_color=\"red\">";
                markup += g_markup_escape_text(token.c_str(), -1);
                markup += "</span>";
                markup += ", ";
            }
        }
        // Remove extra comma and space from end.
        if (markup.size() >= 2) {
            markup.resize(markup.size() - 2);
        }
        markup += "</span>";
        // std::cout << markup << std::endl;
    } else {
        markup = family_escaped;
    }

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    int show_sample = prefs->getInt("/tools/text/show_sample_in_list", 1);
    if (show_sample) {

        Glib::ustring sample = prefs->getString("/tools/text/font_sample");
        gchar* sample_escaped = g_markup_escape_text(sample.data(), -1);

        markup += "  <span foreground='gray' font_family='";
        markup += family_escaped;
        markup += "'>";
        markup += sample_escaped;
        markup += "</span>";
        g_free(sample_escaped);
    }

    g_object_set(G_OBJECT(cell), "markup", markup.c_str(), NULL);
    g_free(family_escaped);
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
