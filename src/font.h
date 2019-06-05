// See LICENSE for license details.

#pragma once

/* Forward declarations. */

typedef struct FT_FaceRec_* FT_Face;
typedef struct FT_LibraryRec_* FT_Library;
typedef struct FT_GlyphSlotRec_* FT_GlyphSlot;
typedef struct FT_Span_ FT_Span;
typedef struct FT_Size_Metrics_ FT_Size_Metrics;

struct font_face;
struct font_manager;
struct font_data;
struct font_spec;

struct font_face_ft;
struct font_manager_ft;

extern const std::string font_family_any;
extern const std::string font_style_any;

/* Font Style */

enum font_style {
    font_style_thin             = 0,
    font_style_thinitalic       = 1,
    font_style_extralight       = 2,
    font_style_extralightitalic = 3,
    font_style_light            = 4,
    font_style_lightitalic      = 5,
    font_style_normal           = 6,
    font_style_normalitalic     = 7,
    font_style_italic           = font_style_normalitalic,
    font_style_medium           = 8,
    font_style_mediumitalic     = 9,
    font_style_seminold         = 10,
    font_style_seminoldItalic   = 11,
    font_style_bold             = 12,
    font_style_bolditalic       = 13,
    font_style_extrabold        = 14,
    font_style_extrabolditalic  = 15,
    font_style_black            = 16,
    font_style_blackitalic      = 17,
    font_style_extrablack       = 18,
    font_style_extrablackitalic = 19,
    font_style_count = 20,
};

/* Font Weight */

enum font_weight {
    font_weight_any,
    font_weight_thin,
    font_weight_extra_light,
    font_weight_ultra_light,
    font_weight_light,
    font_weight_semi_light,
    font_weight_book,
    font_weight_normal,
    font_weight_regular,
    font_weight_medium,
    font_weight_demibold,
    font_weight_semibold,
    font_weight_bold,
    font_weight_extra_bold,
    font_weight_ultra_bold,
    font_weight_black,
    font_weight_heavy,
    font_weight_extra_black,
    font_weight_ultra_black,
    font_weight_count,
};

/* Font Slope */

enum font_slope {
    font_slope_any,
    font_slope_none,
    font_slope_oblique,
    font_slope_italic,
    font_slope_count,
};

/* Font Stretch */

enum font_stretch {
    font_stretch_any,
    font_stretch_ultra_condensed,
    font_stretch_extra_condensed,
    font_stretch_condensed,
    font_stretch_semi_condensed,
    font_stretch_medium,
    font_stretch_semi_expanded,
    font_stretch_expanded,
    font_stretch_extra_expanded,
    font_stretch_ultra_expanded,
    font_stretch_count,
};

/* Font Spacing */

enum font_spacing {
    font_spacing_any,
    font_spacing_normal,
    font_spacing_monospaced,
    font_spacing_count,
};

/* Font Token Type */

enum font_token_type {
    font_token_none,
    font_token_weight,
    font_token_slope,
    font_token_stretch,
    font_token_spacing,
};

/* Font Token Entry */

struct font_token_entry {
    const char* name;
    font_token_type tokenType;
    unsigned tokenEnum;
    bool leftOfHyphen;
    bool eatToken;
    bool caseSensitive;
};

/* Font Spec */

struct font_spec
{
    std::string familyName;
    std::string styleName;
    int fontWeight;
    int fontSlope;
    int fontStretch;
    int fontSpacing;
    
    font_spec(std::string familyName, std::string styleName,
              int fontWeight = 400, int fontSlope = 0,
              int fontStretch = 5, int fontSpacing = 0)
        : familyName(familyName), styleName(styleName),
          fontWeight(fontWeight), fontSlope(fontSlope),
          fontStretch(fontStretch), fontSpacing(fontSpacing) {}

    std::string toString() const;
};

/* Font Data */

struct font_data
{
    std::string familyName;
    std::string styleName;
    font_weight fontWeight;
    font_slope fontSlope;
    font_stretch fontStretch;
    font_spacing fontSpacing;
    
    font_data() = default;
    font_data(std::string familyName, std::string styleName,
              font_weight fontWeight = font_weight_normal,
              font_slope fontSlope = font_slope_none,
              font_stretch fontStretch = font_stretch_medium,
              font_spacing fontSpacing = font_spacing_normal)
    : familyName(familyName), styleName(styleName), fontWeight(fontWeight),
      fontSlope(fontSlope), fontStretch(fontStretch), fontSpacing(fontSpacing) {}
    
    font_spec fontSpec() const;
    std::string toString() const;
};

/* Font Face */

struct font_face
{
    int font_id;
    std::string path;
    std::string name;
    font_data fontData;

    font_face() = default;
    font_face(int font_id, std::string path, std::string name);
    virtual ~font_face() = default;

    const font_data& getFontData() const { return fontData; }
    std::string get_family_name() const { return fontData.familyName; }
    std::string get_style_name() const { return fontData.styleName; }
};

inline font_face::font_face(int font_id, std::string path, std::string name) :
    font_id(font_id), path(path), name(name) {}


/* Font Manager */

struct font_manager
{
    static bool debug;
    static int dpi;

    static const int weightTable[];
    static const int slopeTable[];
    static const int stretchTable[];
    static const float stretchPercentTable[];
    static const int spacingTable[];

    static const char* weightName[];
    static const char* slopeName[];
    static const char* stretchName[];
    static const char* spacingName[];
    
    static const font_token_entry fontTokens[];
    static const font_data styleMapping[];
    
    static std::string synthesizeFontName(std::string familyName,
        font_weight fontWeight, font_slope fontSlope,
        font_stretch fontStretch, font_spacing fontSpacing);
    static font_data createFontRecord(std::string psName,
        std::string familyName, std::string styleName);

    std::vector<font_face*> allFonts;
    std::map<std::string,size_t> fontPathMap;
    std::map<std::string,size_t> fontNameMap;
    std::map<std::string,std::vector<size_t>> fontFamilyMap;

    font_manager() = default;
    virtual ~font_manager() = default;

    virtual void indexFace(font_face *face);
    virtual void scanFontDir(std::string dir) = 0;
    virtual void scanFontPath(std::string path) = 0;
    virtual size_t fontCount() = 0;
    virtual font_face* findFontById(size_t font_id) = 0;
    virtual font_face* findFontByPath(std::string path);
    virtual font_face* findFontByName(std::string font_name);
    virtual font_face* findFontByFamily(std::string familyName,
        font_style fontStyle);
    virtual font_face* findFontByData(font_data fontRec);
    virtual font_face* findFontBySpec(font_spec fontSpec);
};


/* Font Face (FreeType) */

struct font_face_ft : font_face
{
    FT_Face ftface;

    font_face_ft() = default;
    font_face_ft(int font_id, FT_Face ftface, std::string path);
    virtual ~font_face_ft() = default;

    FT_Size_Metrics* get_metrics(int font_size);
    int get_height(int font_size);
};

/* Font Manager (FreeType) */

struct font_manager_ft : font_manager
{
    FT_Library ftlib;

    std::vector<font_face_ft> faces;

    font_manager_ft(std::string fontDir = "");
    virtual ~font_manager_ft();

    virtual void scanFontDir(std::string dir);
    virtual void scanFontPath(std::string path);
    virtual size_t fontCount();
    virtual font_face* findFontById(size_t font_id);
    virtual font_face* findFontByPath(std::string path);

    const std::vector<font_face_ft>& getFontList() { return faces; }
};

/* Todo: fix me to not use the std namespace */

namespace font_str {
    inline bool compare(std::string s1, std::string s2)
    {
        return ((s1.size() == s2.size()) &&
            std::equal(s1.begin(), s1.end(), s2.begin(), [](char & c1, char & c2) {
                return (c1 == c2 || std::toupper(c1) == std::toupper(c2));
        }));
    }
}

namespace std {
    template <> struct equal_to <font_spec> {
        inline bool operator()(const font_spec &f1, const font_spec &f2) const {
            return ((font_str::compare(f1.familyName,f2.familyName) || f1.familyName == "*" || f2.familyName == "*") &&
                    (font_str::compare(f1.styleName, f2.styleName) || f1.styleName == "*" || f2.styleName == "*") &&
                    (f1.fontWeight == f2.fontWeight || f1.fontWeight == -1 || f2.fontWeight == -1) &&
                    (f1.fontSlope == f2.fontSlope || f1.fontSlope == -1 || f2.fontSlope == -1) &&
                    (f1.fontStretch == f2.fontStretch || f1.fontStretch == -1 || f2.fontStretch == -1) &&
                    (f1.fontSpacing == f2.fontSpacing || f1.fontSpacing == -1 || f2.fontSpacing == -1));
        }
    };
    template <> struct less <font_spec> {
        inline bool operator()(const font_spec &f1, const font_spec &f2) const {
            if (f1.familyName < f2.familyName && f1.familyName != font_family_any && f2.familyName != font_family_any) return true;
            if (font_str::compare(f1.familyName,f2.familyName) || f1.familyName == font_family_any || f2.familyName == font_family_any) {
                if (f1.styleName < f2.styleName && f1.styleName != font_style_any && f2.styleName != font_style_any) return true;
                if (font_str::compare(f1.styleName, f2.styleName) || f1.styleName == font_style_any || f2.styleName == font_style_any) {
                    if (f1.fontWeight < f2.fontWeight && f1.fontWeight != -1 && f2.fontWeight != -1) return true;
                    if (f1.fontWeight == f2.fontWeight || f1.fontWeight == -1 || f2.fontWeight == -1) {
                        if (f1.fontSlope < f2.fontSlope && f1.fontSlope != -1 && f2.fontSlope != -1) return true;
                        if (f1.fontSlope == f2.fontSlope|| f1.fontSlope == -1 || f2.fontSlope == -1) {
                            if (f1.fontStretch < f2.fontStretch && f1.fontStretch != -1 && f2.fontStretch != -1) return true;
                            if (f1.fontStretch == f2.fontStretch || f1.fontStretch == -1 || f2.fontStretch == -1) {
                                if (f1.fontSpacing < f2.fontSpacing && f1.fontSpacing != -1 && f2.fontSpacing != -1) return true;
                            }
                        }
                    }
                }
            }
            return false;
        }
    };
    template <> struct equal_to <font_data> {
        inline bool operator()(const font_data &f1, const font_data &f2) const {
            return ((font_str::compare(f1.familyName, f2.familyName) || f1.familyName == font_family_any || f2.familyName == font_family_any) &&
                    (font_str::compare(f1.styleName, f2.styleName) || f1.styleName == font_style_any || f2.styleName == font_style_any) &&
                    (f1.fontWeight == f2.fontWeight || f1.fontWeight == font_weight_any || f2.fontWeight == font_weight_any) &&
                    (f1.fontSlope == f2.fontSlope || f1.fontSlope == font_slope_any || f2.fontSlope == font_slope_any) &&
                    (f1.fontStretch == f2.fontStretch || f1.fontStretch == font_stretch_any || f2.fontStretch == font_stretch_any) &&
                    (f1.fontSpacing == f2.fontSpacing || f1.fontSpacing == font_spacing_any || f2.fontSpacing == font_spacing_any));
        }
    };
    template <> struct less <font_data> {
        inline bool operator()(const font_data &f1, const font_data &f2) const {
            if (f1.familyName < f2.familyName && f1.familyName != font_family_any && f2.familyName != font_family_any) return true;
            if (font_str::compare(f1.familyName, f2.familyName) || f1.familyName == font_family_any || f2.familyName == font_family_any) {
                if (f1.styleName < f2.styleName && f1.styleName != font_style_any && f2.styleName != font_style_any) return true;
                if (font_str::compare(f1.styleName, f2.styleName) || f1.styleName == font_style_any || f2.styleName == font_style_any) {
                    if (f1.fontWeight < f2.fontWeight && f1.fontWeight != font_weight_any && f2.fontWeight != font_weight_any) return true;
                    if (f1.fontWeight == f2.fontWeight || f1.fontWeight == font_weight_any || f2.fontWeight == font_weight_any) {
                        if (f1.fontSlope < f2.fontSlope && f1.fontSlope != font_slope_any && f2.fontSlope != font_slope_any) return true;
                        if (f1.fontSlope == f2.fontSlope || f1.fontSlope == font_slope_any || f2.fontSlope == font_slope_any) {
                            if (f1.fontStretch < f2.fontStretch && f1.fontStretch != font_stretch_any && f2.fontStretch != font_stretch_any) return true;
                            if (f1.fontStretch == f2.fontStretch || f1.fontStretch == font_stretch_any || f2.fontStretch == font_stretch_any) {
                                if (f1.fontSpacing < f2.fontSpacing && f1.fontSpacing != font_spacing_any && f2.fontSpacing != font_spacing_any) return true;
                            }
                        }
                    }
                }
            }
            return false;
        }
    };
}

