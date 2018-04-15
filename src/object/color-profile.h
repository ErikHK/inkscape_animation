#ifndef SEEN_COLOR_PROFILE_H
#define SEEN_COLOR_PROFILE_H

#include <set>
#include <vector>

#include <glibmm/ustring.h>
#include "cms-color-types.h"

#include "object/sp-object.h"

struct SPColor;

namespace Inkscape {

enum {
    RENDERING_INTENT_UNKNOWN = 0,
    RENDERING_INTENT_AUTO = 1,
    RENDERING_INTENT_PERCEPTUAL = 2,
    RENDERING_INTENT_RELATIVE_COLORIMETRIC = 3,
    RENDERING_INTENT_SATURATION = 4,
    RENDERING_INTENT_ABSOLUTE_COLORIMETRIC = 5
};

class ColorProfileImpl;


/**
 * Color Profile.
 */
class ColorProfile : public SPObject {
public:
    ColorProfile();
    virtual ~ColorProfile();

    bool operator<(ColorProfile const &other) const;

    // we use std::set with pointers to ColorProfile, just having operator< isn't enough to sort these
    struct pointerComparator {
        bool operator()(const ColorProfile * const & a, const ColorProfile * const & b) { return (*a) < (*b); };
    };

    friend cmsHPROFILE colorprofile_get_handle( SPDocument*, unsigned int*, char const* );
    friend class CMSSystem;

    class FilePlusHome {
    public:
        FilePlusHome(Glib::ustring filename, bool isInHome);
        FilePlusHome(const FilePlusHome &filePlusHome);
        bool operator<(FilePlusHome const &other) const;
        Glib::ustring filename;
        bool isInHome;
    };
    class FilePlusHomeAndName: public FilePlusHome {
    public:
        FilePlusHomeAndName(FilePlusHome filePlusHome, Glib::ustring name);
        bool operator<(FilePlusHomeAndName const &other) const;
        Glib::ustring name;
    };

    static std::set<FilePlusHome> getBaseProfileDirs();
    static std::set<FilePlusHome> getProfileFiles();
    static std::set<FilePlusHomeAndName> getProfileFilesWithNames();
#if defined(HAVE_LIBLCMS1) || defined(HAVE_LIBLCMS2)
    //icColorSpaceSignature getColorSpace() const;
    ColorSpaceSig getColorSpace() const;
    //icProfileClassSignature getProfileClass() const;
    ColorProfileClassSig getProfileClass() const;
    cmsHTRANSFORM getTransfToSRGB8();
    cmsHTRANSFORM getTransfFromSRGB8();
    cmsHTRANSFORM getTransfGamutCheck();
    bool GamutCheck(SPColor color);

#endif // defined(HAVE_LIBLCMS1) || defined(HAVE_LIBLCMS2)

    char* href;
    char* local;
    char* name;
    char* intentStr;
    unsigned int rendering_intent; // FIXME: type the enum and hold that instead

protected:
    ColorProfileImpl *impl;

    virtual void build(SPDocument* doc, Inkscape::XML::Node* repr);
    virtual void release();

    virtual void set(unsigned int key, char const* value);

    virtual Inkscape::XML::Node* write(Inkscape::XML::Document* doc, Inkscape::XML::Node* repr, unsigned int flags);
};

} // namespace Inkscape

//#define COLORPROFILE_TYPE (Inkscape::colorprofile_get_type())
#define COLORPROFILE(obj) ((Inkscape::ColorProfile*)obj)
#define IS_COLORPROFILE(obj) (dynamic_cast<const Inkscape::ColorProfile*>((SPObject*)obj))

#endif // !SEEN_COLOR_PROFILE_H

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
