/**
 * @file-update
 * Operations to bump files from the pre-0.92 era into the 0.92+ era
 * (90dpi vs 96dpi, line height problems, and related bugs)
 */
/* Authors:
 * Tavmjong Bah
 * Marc Jeanmougin
 * su_v
 */
#include "file.h"
#include "sp-root.h"
#include "sp-text.h"
#include "sp-tspan.h"
#include "sp-flowdiv.h"
#include "sp-flowtext.h"
#include "sp-object.h"
#include "sp-item.h"
#include "style.h"
#include "document.h"
#include <string>
#include <clocale>
#include "text-editing.h"

using namespace std;

bool is_line(SPObject *i)
{
    if (!(i->getAttribute("sodipodi:role")))
        return false;
    return !strcmp(i->getAttribute("sodipodi:role"), "line");
}


void fix_blank_line(SPObject *o)
{
    if (SP_IS_TEXT(o))
        ((SPText *)o)->rebuildLayout();
    else if (SP_IS_FLOWTEXT(o))
        ((SPFlowtext *)o)->rebuildLayout();

    SPIFontSize fontsize = o->style->font_size;
    SPILengthOrNormal lineheight = o->style->line_height;
    vector<SPObject *> cl = o->childList(false);
    bool beginning = true;
    for (vector<SPObject *>::const_iterator ci = cl.begin(); ci != cl.end(); ++ci) {
        SPObject *i = *ci;
        if ((SP_IS_TSPAN(i) && is_line(i)) || SP_IS_FLOWPARA(i)) {
            if (sp_text_get_length((SPItem *)i) <= 1) { // empty line
                Inkscape::Text::Layout::iterator pos = te_get_layout((SPItem*)(o))->charIndexToIterator(
                        (SP_IS_FLOWPARA(i)?0:((ci==cl.begin())?0:1)) + sp_text_get_length_upto(o,i) );
                sp_te_insert((SPItem *)o, pos, "\u00a0"); //"\u00a0"
                gchar *l = g_strdup_printf("%f", lineheight.value);
                gchar *f = g_strdup_printf("%f", fontsize.value);
                i->style->line_height.readIfUnset(l);
                if (!beginning)
                    i->style->font_size.read(f);
                else
                    i->style->font_size.readIfUnset(f);
                g_free(l);
                g_free(f);
            } else {
                beginning = false;
                fontsize = i->style->font_size;
                lineheight = o->style->line_height;
            }
        }
    }
}

void fix_line_spacing(SPObject *o)
{
    SPILengthOrNormal lineheight = o->style->line_height;
    bool inner = false;
    vector<SPObject *> cl = o->childList(false);
    for (vector<SPObject *>::const_iterator ci = cl.begin(); ci != cl.end(); ++ci) {
        SPObject *i = *ci;
        if ((SP_IS_TSPAN(i) && is_line(i)) || SP_IS_FLOWPARA(i)) {
            // if no line-height attribute, set it
            gchar *l = g_strdup_printf("%f", lineheight.value);
            i->style->line_height.readIfUnset(l);
            g_free(l);
        }
        inner = true;
    }
    if (inner) {
        if (SP_IS_TEXT(o)) {
            o->style->line_height.read("0.00%");
        } else {
            o->style->line_height.read("0.01%");
        }
    }
}

void fix_font_name(SPObject *o)
{
    vector<SPObject *> cl = o->childList(false);
    for (vector<SPObject *>::const_iterator ci = cl.begin(); ci != cl.end(); ++ci)
        fix_font_name(*ci);
    string prev = o->style->font_family.value ? o->style->font_family.value : o->style->font_family.value_default;
    if (prev == "Sans")
        o->style->font_family.read("sans-serif");
    else if (prev == "Serif")
        o->style->font_family.read("serif");
    else if (prev == "Monospace")
        o->style->font_family.read("monospace");
}


void fix_font_size(SPObject *o)
{
    SPIFontSize fontsize = o->style->font_size;
    if (!fontsize.set)
        return;
    bool inner = false;
    vector<SPObject *> cl = o->childList(false);
    for (vector<SPObject *>::const_iterator ci = cl.begin(); ci != cl.end(); ++ci) {
        SPObject *i = *ci;
        fix_font_size(i);
        if ((SP_IS_TSPAN(i) && is_line(i)) || SP_IS_FLOWPARA(i)) {
            inner = true;
            gchar *s = g_strdup_printf("%f", fontsize.value);
            if (fontsize.set)
                i->style->font_size.readIfUnset(s);
            g_free(s);
        }
    }
    if (inner && (SP_IS_TEXT(o) || SP_IS_FLOWTEXT(o)))
        o->style->font_size.clear();
}



// helper function
void sp_file_text_run_recursive(void (*f)(SPObject *), SPObject *o)
{
    if (SP_IS_TEXT(o) || SP_IS_FLOWTEXT(o))
        f(o);
    else {
        vector<SPObject *> cl = o->childList(false);
        for (vector<SPObject *>::const_iterator ci = cl.begin(); ci != cl.end(); ++ci)
            sp_file_text_run_recursive(f, *ci);
    }
}

void fix_update(SPObject *o) { 
    o->style->write();
    o->updateRepr();
}

void sp_file_convert_text_baseline_spacing(SPDocument *doc)
{
    setlocale(LC_NUMERIC,"C");
    sp_file_text_run_recursive(fix_blank_line, doc->getRoot());
    sp_file_text_run_recursive(fix_line_spacing, doc->getRoot());
    sp_file_text_run_recursive(fix_font_size, doc->getRoot());
    sp_file_text_run_recursive(fix_update, doc->getRoot());
    setlocale(LC_NUMERIC,"");
}

void sp_file_convert_font_name(SPDocument *doc)
{
    sp_file_text_run_recursive(fix_font_name, doc->getRoot());
    sp_file_text_run_recursive(fix_update, doc->getRoot());
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
