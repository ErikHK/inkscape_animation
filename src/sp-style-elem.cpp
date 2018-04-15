#include <libcroco/cr-parser.h>
#include "xml/node-event-vector.h"
#include "xml/repr.h"
#include "document.h"
#include "sp-style-elem.h"
#include "sp-root.h"
#include "attributes.h"
#include "style.h"

// For external style sheets
#include "io/resource.h"
#include <iostream>
#include <fstream>

// For font-rule
#include "libnrtype/FontFactory.h"

using Inkscape::XML::TEXT_NODE;

SPStyleElem::SPStyleElem() : SPObject() {
    media_set_all(this->media);
    this->is_css = false;
}

SPStyleElem::~SPStyleElem() {
}

void SPStyleElem::set(unsigned int key, const gchar* value) {
    switch (key) {
        case SP_ATTR_TYPE: {
            if (!value) {
                /* TODO: `type' attribute is required.  Give error message as per
                   http://www.w3.org/TR/SVG11/implnote.html#ErrorProcessing. */
                is_css = false;
            } else {
                /* fixme: determine what whitespace is allowed.  Will probably need to ask on SVG
                  list; though the relevant RFC may give info on its lexer. */
                is_css = ( g_ascii_strncasecmp(value, "text/css", 8) == 0
                           && ( value[8] == '\0' ||
                                value[8] == ';'    ) );
            }
            break;
        }

#if 0 /* unfinished */
        case SP_ATTR_MEDIA: {
            parse_media(style_elem, value);
            break;
        }
#endif

        /* title is ignored. */
        default: {
            SPObject::set(key, value);
            break;
        }
    }
}


static void
child_add_rm_cb(Inkscape::XML::Node *, Inkscape::XML::Node *, Inkscape::XML::Node *,
                void *const data)
{
    SPObject *obj = reinterpret_cast<SPObject *>(data);
    g_assert(data != NULL);
    obj->read_content();
}

static void
content_changed_cb(Inkscape::XML::Node *, gchar const *, gchar const *,
                   void *const data)
{
    SPObject *obj = reinterpret_cast<SPObject *>(data);
    g_assert(data != NULL);
    obj->read_content();
    obj->document->getRoot()->emitModified( SP_OBJECT_MODIFIED_CASCADE );
}

static void
child_order_changed_cb(Inkscape::XML::Node *, Inkscape::XML::Node *,
                       Inkscape::XML::Node *, Inkscape::XML::Node *,
                       void *const data)
{
    SPObject *obj = reinterpret_cast<SPObject *>(data);
    g_assert(data != NULL);
    obj->read_content();
}

Inkscape::XML::Node* SPStyleElem::write(Inkscape::XML::Document* xml_doc, Inkscape::XML::Node* repr, guint flags) {
    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:style");
    }

    if (flags & SP_OBJECT_WRITE_BUILD) {
        g_warning("nyi: Forming <style> content for SP_OBJECT_WRITE_BUILD.");
        /* fixme: Consider having the CRStyleSheet be a member of SPStyleElem, and then
           pretty-print to a string s, then repr->addChild(xml_doc->createTextNode(s), NULL). */
    }
    if (is_css) {
        repr->setAttribute("type", "text/css");
    }
    /* todo: media */

    SPObject::write(xml_doc, repr, flags);

    return repr;
}


/** Returns the concatenation of the content of the text children of the specified object. */
static Glib::ustring
concat_children(Inkscape::XML::Node const &repr)
{
    Glib::ustring ret;
    // effic: Initialising ret to a reasonable starting size could speed things up.
    for (Inkscape::XML::Node const *rch = repr.firstChild(); rch != NULL; rch = rch->next()) {
        if ( rch->type() == TEXT_NODE ) {
            ret += rch->content();
        }
    }
    return ret;
}



/* Callbacks for SAC-style libcroco parser. */

enum StmtType { NO_STMT, FONT_FACE_STMT, NORMAL_RULESET_STMT };

struct ParseTmp
{
    CRStyleSheet *const stylesheet;
    StmtType stmtType;
    CRStatement *currStmt;
    SPDocument *const document; // Need file location for '@import'
    unsigned magic;
    static unsigned const ParseTmp_magic = 0x23474397;  // from /dev/urandom

    ParseTmp(CRStyleSheet *const stylesheet, SPDocument *const document) :
        stylesheet(stylesheet),
        stmtType(NO_STMT),
        currStmt(NULL),
        document(document),
        magic(ParseTmp_magic)
    { }

    bool hasMagic() const {
        return magic == ParseTmp_magic;
    }

    ~ParseTmp()
    {
        g_return_if_fail(hasMagic());
        magic = 0;
    }
};

CRParser*
parser_init(CRStyleSheet *const stylesheet, SPDocument *const document);

static void
import_style_cb (CRDocHandler *a_handler,
                 GList *a_media_list,
                 CRString *a_uri,
                 CRString *a_uri_default_ns,
                 CRParsingLocation *a_location)
{
    /* a_uri_default_ns is set to NULL and is unused by libcroco */

    // Get document
    g_return_if_fail(a_handler && a_uri);
    g_return_if_fail(a_handler->app_data != NULL);
    ParseTmp &parse_tmp = *static_cast<ParseTmp *>(a_handler->app_data);
    g_return_if_fail(parse_tmp.hasMagic());

    SPDocument* document = parse_tmp.document;
    if (!document) {
        std::cerr << "import_style_cb: No document!" << std::endl;
        return;
    }
    if (!document->style_sheet) {
        std::cerr << "import_style_cb: No document style sheet!" << std::endl;
        return;
    }
    if (!document->getURI()) {
        std::cerr << "import_style_cb: Document URI is NULL" << std::endl;
        return;
    }

    // Get file
    Glib::ustring import_file =
        Inkscape::IO::Resource::get_filename (document->getURI(), a_uri->stryng->str);

    // Parse file
    CRStyleSheet *stylesheet = cr_stylesheet_new (NULL);
    CRParser *parser = parser_init(stylesheet, document);
    CRStatus const parse_status =
        cr_parser_parse_file (parser, reinterpret_cast<const guchar *>(import_file.c_str()), CR_UTF_8);
    if (parse_status == CR_OK) {
        cr_stylesheet_append_import (document->style_sheet, stylesheet);
    } else {
        std::cerr << "import_style_cb: Could not parse: " << import_file << std::endl;
        cr_stylesheet_destroy (stylesheet);
    }

    // Need to delete ParseTmp created by parser_init()
    CRDocHandler *sac_handler = NULL;
    cr_parser_get_sac_handler (parser, &sac_handler);
    ParseTmp *parse_new = reinterpret_cast<ParseTmp *>(sac_handler->app_data);
    cr_parser_destroy(parser);
    delete parse_new;
};

#if 0
/* FIXME: NOT USED, incomplete libcroco implementation */
static void
import_style_result_cb (CRDocHandler *a_this,
                        GList *a_media_list,
                        CRString *a_uri,
                        CRString *a_uri_default_ns,
                        CRStyleSheet *a_sheet)
{
    /* a_uri_default_ns and a_sheet are set to NULL and are unused by libcroco */
    std::cerr << "import_style_result_cb: unimplemented" << std::endl;
};
#endif

static void
start_selector_cb(CRDocHandler *a_handler,
                  CRSelector *a_sel_list)
{
    g_return_if_fail(a_handler && a_sel_list);
    g_return_if_fail(a_handler->app_data != NULL);
    ParseTmp &parse_tmp = *static_cast<ParseTmp *>(a_handler->app_data);
    g_return_if_fail(parse_tmp.hasMagic());
    if ( (parse_tmp.currStmt != NULL)
         || (parse_tmp.stmtType != NO_STMT) ) {
        g_warning("Expecting currStmt==NULL and stmtType==0 (NO_STMT) at start of ruleset, but found currStmt=%p, stmtType=%u",
                  static_cast<void *>(parse_tmp.currStmt), unsigned(parse_tmp.stmtType));
        // fixme: Check whether we need to unref currStmt if non-NULL.
    }
    CRStatement *ruleset = cr_statement_new_ruleset(parse_tmp.stylesheet, a_sel_list, NULL, NULL);
    g_return_if_fail(ruleset && ruleset->type == RULESET_STMT);
    parse_tmp.stmtType = NORMAL_RULESET_STMT;
    parse_tmp.currStmt = ruleset;
}

static void
end_selector_cb(CRDocHandler *a_handler,
                CRSelector *a_sel_list)
{
    g_return_if_fail(a_handler && a_sel_list);
    g_return_if_fail(a_handler->app_data != NULL);
    ParseTmp &parse_tmp = *static_cast<ParseTmp *>(a_handler->app_data);
    g_return_if_fail(parse_tmp.hasMagic());
    CRStatement *const ruleset = parse_tmp.currStmt;
    if (parse_tmp.stmtType == NORMAL_RULESET_STMT
        && ruleset
        && ruleset->type == RULESET_STMT
        && ruleset->kind.ruleset->sel_list == a_sel_list)
    {
        parse_tmp.stylesheet->statements = cr_statement_append(parse_tmp.stylesheet->statements,
                                                               ruleset);
    } else {
        g_warning("Found stmtType=%u, stmt=%p, stmt.type=%u, ruleset.sel_list=%p, a_sel_list=%p.",
                  unsigned(parse_tmp.stmtType),
                  ruleset,
                  unsigned(ruleset->type),
                  ruleset->kind.ruleset->sel_list,
                  a_sel_list);
    }
    parse_tmp.currStmt = NULL;
    parse_tmp.stmtType = NO_STMT;
}

static void
start_font_face_cb(CRDocHandler *a_handler,
                   CRParsingLocation *)
{
    g_return_if_fail(a_handler->app_data != NULL);
    ParseTmp &parse_tmp = *static_cast<ParseTmp *>(a_handler->app_data);
    g_return_if_fail(parse_tmp.hasMagic());
    if (parse_tmp.stmtType != NO_STMT || parse_tmp.currStmt != NULL) {
        g_warning("Expecting currStmt==NULL and stmtType==0 (NO_STMT) at start of @font-face, but found currStmt=%p, stmtType=%u",
                  static_cast<void *>(parse_tmp.currStmt), unsigned(parse_tmp.stmtType));
        // fixme: Check whether we need to unref currStmt if non-NULL.
    }
    CRStatement *font_face_rule = cr_statement_new_at_font_face_rule (parse_tmp.stylesheet, NULL);
    g_return_if_fail(font_face_rule && font_face_rule->type == AT_FONT_FACE_RULE_STMT);
    parse_tmp.stmtType = FONT_FACE_STMT;
    parse_tmp.currStmt = font_face_rule;
}

static void
end_font_face_cb(CRDocHandler *a_handler)
{
    g_return_if_fail(a_handler->app_data != NULL);
    ParseTmp &parse_tmp = *static_cast<ParseTmp *>(a_handler->app_data);
    g_return_if_fail(parse_tmp.hasMagic());

    CRStatement *const font_face_rule = parse_tmp.currStmt;
    if (parse_tmp.stmtType == FONT_FACE_STMT
        && font_face_rule
        && font_face_rule->type == AT_FONT_FACE_RULE_STMT)
    {
        parse_tmp.stylesheet->statements = cr_statement_append(parse_tmp.stylesheet->statements,
                                                               font_face_rule);
    } else {
        g_warning("Found stmtType=%u, stmt=%p, stmt.type=%u.",
                  unsigned(parse_tmp.stmtType),
                  font_face_rule,
                  unsigned(font_face_rule->type));
    }

    std::cout << "end_font_face_cb: font face rule limited support." << std::endl;
    cr_declaration_dump (font_face_rule->kind.font_face_rule->decl_list, stdout, 2, TRUE);
    printf ("\n");

    // Get document
    SPDocument* document = parse_tmp.document;
    if (!document) {
        std::cerr << "end_font_face_cb: No document!" << std::endl;
        return;
    }
    if (!document->getURI()) {
        std::cerr << "end_font_face_cb: Document URI is NULL" << std::endl;
        return;
    }

    // Add ttf or otf fonts.
    CRDeclaration const *cur = NULL;
    for (cur = font_face_rule->kind.font_face_rule->decl_list; cur; cur = cur->next) {
        if (cur->property &&
            cur->property->stryng &&
            cur->property->stryng->str &&
            strcmp(cur->property->stryng->str, "src") == 0 ) {

            if (cur->value &&
                cur->value->content.str &&
                cur->value->content.str->stryng &&
                cur->value->content.str->stryng->str) {

                Glib::ustring value = cur->value->content.str->stryng->str;

                if (value.rfind("ttf") == (value.length() - 3) ||
                    value.rfind("otf") == (value.length() - 3)) {

                    // Get file
                    Glib::ustring ttf_file =
                        Inkscape::IO::Resource::get_filename (document->getURI(), value);

                    if (!ttf_file.empty()) {
                        font_factory *factory = font_factory::Default();
                        factory->AddFontFile( ttf_file.c_str() );
                        std::cout << "end_font_face_cb: Added font: " << ttf_file << std::endl;

                        // FIX ME: Need to refresh font list.
                    } else {
                        std::cout << "end_font_face_cb: Failed to add: " << value << std::endl;
                    }
                }
            }
        }
    }

    parse_tmp.currStmt = NULL;
    parse_tmp.stmtType = NO_STMT;

}

static void
property_cb(CRDocHandler *const a_handler,
            CRString *const a_name,
            CRTerm *const a_value, gboolean const a_important)
{
    // std::cout << "property_cb: Entrance: " << a_name->stryng->str << ": " << cr_term_to_string(a_value) << std::endl;
    g_return_if_fail(a_handler && a_name);
    g_return_if_fail(a_handler->app_data != NULL);
    ParseTmp &parse_tmp = *static_cast<ParseTmp *>(a_handler->app_data);
    g_return_if_fail(parse_tmp.hasMagic());

    CRStatement *const ruleset = parse_tmp.currStmt;
    g_return_if_fail(ruleset);

    CRDeclaration *const decl = cr_declaration_new (ruleset, cr_string_dup(a_name), a_value);
    g_return_if_fail(decl);
    decl->important = a_important;

    switch (parse_tmp.stmtType) {

        case NORMAL_RULESET_STMT: {
            g_return_if_fail (ruleset->type == RULESET_STMT);
            CRStatus const append_status = cr_statement_ruleset_append_decl (ruleset, decl);
            g_return_if_fail (append_status == CR_OK);
            break;
        }
        case FONT_FACE_STMT: {
            g_return_if_fail (ruleset->type == AT_FONT_FACE_RULE_STMT);
            CRDeclaration *new_decls = cr_declaration_append (ruleset->kind.font_face_rule->decl_list, decl);
            g_return_if_fail (new_decls);
            ruleset->kind.font_face_rule->decl_list = new_decls;
            break;
        }
        default:
            g_warning ("property_cb: Unhandled stmtType: %u", parse_tmp.stmtType);
            return;
    }
}

CRParser*
parser_init(CRStyleSheet *const stylesheet, SPDocument *const document) {

    ParseTmp *parse_tmp = new ParseTmp(stylesheet, document);

    CRDocHandler *sac_handler = cr_doc_handler_new();
    sac_handler->app_data = parse_tmp;
    sac_handler->import_style = import_style_cb;
    sac_handler->start_selector = start_selector_cb;
    sac_handler->end_selector = end_selector_cb;
    sac_handler->start_font_face = start_font_face_cb;
    sac_handler->end_font_face = end_font_face_cb;
    sac_handler->property = property_cb;

    CRParser *parser = cr_parser_new (NULL);
    cr_parser_set_sac_handler(parser, sac_handler);

    return parser;
}

void update_style_recursively( SPObject *object ) {
    if (object) {
        // std::cout << "update_style_recursively: "
        //           << (object->getId()?object->getId():"null") << std::endl;
        if (object->style) {
            object->style->readFromObject( object );
        }
        for (auto& child : object->children) {
            update_style_recursively( &child );
        }
    }
}

void SPStyleElem::read_content() {

    // This won't work when we support multiple style sheets in a file.
    // We'll need to identify which style sheet this element corresponds to
    // and replace just that part of the total style sheet. (The first
    // style element would correspond to document->style_sheet, while
    // laters ones are chained on using style_sheet->next).

    document->style_sheet = cr_stylesheet_new (NULL);
    CRParser *parser = parser_init(document->style_sheet, document);

    CRDocHandler *sac_handler = NULL;
    cr_parser_get_sac_handler (parser, &sac_handler);
    ParseTmp *parse_tmp = reinterpret_cast<ParseTmp *>(sac_handler->app_data);

    //XML Tree being used directly here while it shouldn't be.
    Glib::ustring const text = concat_children(*getRepr());
    CRStatus const parse_status =
        cr_parser_parse_buf (parser, reinterpret_cast<const guchar *>(text.c_str()), text.bytes(), CR_UTF_8);

    // std::cout << "SPStyeElem::read_content: result:" << std::endl;
    // const gchar* string = cr_stylesheet_to_string (document->style_sheet);
    // std::cout << (string?string:"Null") << std::endl;

    if (parse_status == CR_OK) {
        // Also destroys old style sheet:
        cr_cascade_set_sheet (document->style_cascade, document->style_sheet, ORIGIN_AUTHOR);
    } else {
        cr_stylesheet_destroy (document->style_sheet);
        document->style_sheet = NULL;
        if (parse_status != CR_PARSING_ERROR) {
            g_printerr("parsing error code=%u\n", unsigned(parse_status));
            /* Better than nothing.  TODO: Improve libcroco's error handling.  At a minimum, add a
               strerror-like function so that we can give a string rather than an integer. */
            /* TODO: Improve error diagnosis stuff in inkscape.  We'd like a panel showing the
               errors/warnings/unsupported features of the current document. */
        }
    }

    cr_parser_destroy(parser);
    delete parse_tmp;

    // If style sheet has changed, we need to cascade the entire object tree, top down
    // Get root, read style, loop through children
    update_style_recursively( (SPObject *)document->getRoot() );
    // cr_stylesheet_dump (document->style_sheet, stdout);
}

/**
 * Does addListener(fns, data) on \a repr and all of its descendents.
 */
static void
rec_add_listener(Inkscape::XML::Node &repr,
                 Inkscape::XML::NodeEventVector const *const fns, void *const data)
{
    repr.addListener(fns, data);
    for (Inkscape::XML::Node *child = repr.firstChild(); child != NULL; child = child->next()) {
        rec_add_listener(*child, fns, data);
    }
}

void SPStyleElem::build(SPDocument *document, Inkscape::XML::Node *repr) {
    read_content();

    readAttr( "type" );
    readAttr( "media" );

    static Inkscape::XML::NodeEventVector const nodeEventVector = {
        child_add_rm_cb,   // child_added
        child_add_rm_cb,   // child_removed
        NULL,   // attr_changed
        content_changed_cb,   // content_changed
        child_order_changed_cb,   // order_changed
    };
    rec_add_listener(*repr, &nodeEventVector, this);

    SPObject::build(document, repr);
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
