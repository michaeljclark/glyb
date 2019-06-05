// See LICENSE for license details.

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <cctype>
#include <cmath>

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <tuple>
#include <algorithm>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include "binpack.h"
#include "font.h"
#include "glyph.h"
#include "util.h"

#ifdef _WIN32
#define strncasecmp _strnicmp
#endif

const std::string font_family_any = "*";
const std::string font_style_any = "*";

/* font manager */

int font_manager::dpi = 72;
bool font_manager::debug = false;

const int font_manager::weightTable[] = {
    -1,     // font_weight_any,
    100,    // font_weight_thin,
    200,    // font_weight_extra_light,
    200,    // font_weight_ultra_light,
    300,    // font_weight_light,
    350,    // font_weight_semi_light,
    350,    // font_weight_book,
    400,    // font_weight_normal,
    400,    // font_weight_regular,
    500,    // font_weight_medium,
    600,    // font_weight_demibold,
    600,    // font_weight_semibold,
    700,    // font_weight_bold,
    800,    // font_weight_extra_bold,
    800,    // font_weight_ultra_bold,
    900,    // font_weight_black,
    900,    // font_weight_heavy,
    950,    // font_weight_extra_black,
    950,    // font_weight_ultra_black,
};

const int font_manager::slopeTable[] = {
    -1,     // font_slope_any,
    0,      // font_slope_none,
    1,      // font_slope_oblique,
    1,      // font_slope_italic,
};

const int font_manager::stretchTable[] = {
    -1,     // font_stretch_any,
    1,      // font_stretch_ultra_condensed,
    2,      // font_stretch_extra_condensed,
    3,      // font_stretch_condensed,
    4,      // font_stretch_semi_condensed,
    5,      // font_stretch_medium,
    6,      // font_stretch_semi_expanded,
    7,      // font_stretch_expanded,
    8,      // font_stretch_extra_expanded,
    9,      // font_stretch_ultra_expanded,
};

const float font_manager::stretchPercentTable[] = {
    -1,     // font_stretch_any,
    50,     // font_stretch_ultra_condensed,
    62.5f,  // font_stretch_extra_condensed,util
    75,     // font_stretch_condensed,
    87.5f,  // font_stretch_semi_condensed,
    100,    // font_stretch_medium,
    112.5f, // font_stretch_semi_expanded,
    125,    // font_stretch_expanded,
    150,    // font_stretch_extra_expanded,
    200,    // font_stretch_ultra_expanded,
};

const int font_manager::spacingTable[] = {
    -1,     // font_spacing_any,
    0,      // font_spacing_normal,
    1,      // font_spacing_monospaced,
};

const char* font_manager::weightName[] = {
    "Any",
    "Thin",
    "ExtraLight",
    "UltraLight",
    "Light",
    "SemiLight",
    "Book",
    "Normal",
    "Regular",
    "Medium",
    "DemiBold",
    "SemiBold",
    "Bold",
    "ExtraBold",
    "UltraBold",
    "Black",
    "Heavy",
    "ExtraBlack",
    "UltraBlack",
    NULL
};

const char* font_manager::slopeName[] = {
    "Any",
    "None",
    "Oblique",
    "Italic",
    NULL
};

const char* font_manager::stretchName[] = {
    "Any",
    "UltraCondensed",
    "ExtraCondensed",
    "Condensed",
    "SemiCondensed",
    "Medium",
    "SemiExpanded",
    "Expanded",
    "ExtraExpanded",
    "UltraExpanded",
    NULL
};

const char* font_manager::spacingName[] = {
    "Any",
    "Normal",
    "Monospaced",
    NULL
};

const font_token_entry font_manager::fontTokens[] = {
    { "thin",           font_token_weight,  font_weight_thin,               true, true, false },
    { "extralight",     font_token_weight,  font_weight_extra_light,        true, true, false },
    { "ultralight",     font_token_weight,  font_weight_ultra_light,        true, true, false },
    { "light",          font_token_weight,  font_weight_light,              true, true, false },
    { "semilight",      font_token_weight,  font_weight_semi_light,         true, true, false },
    { "book",           font_token_weight,  font_weight_book,               true, true, false },
    { "normal",         font_token_weight,  font_weight_normal,             true, true, false },
    { "regular",        font_token_weight,  font_weight_regular,            true, true, false },
    { "plain",          font_token_weight,  font_weight_regular,            true, true, false },
    { "roman",          font_token_weight,  font_weight_regular,            true, true, false },
    { "medium",         font_token_weight,  font_weight_medium,             true, true, false },
    { "Med",            font_token_weight,  font_weight_medium,             true, true, true },
    { "demibold",       font_token_weight,  font_weight_demibold,           true, true, false },
    { "semibold",       font_token_weight,  font_weight_semibold,           true, true, false },
    { "extrabold",      font_token_weight,  font_weight_extra_bold,         true, true, false },
    { "ultrabold",      font_token_weight,  font_weight_ultra_bold,         true, true, false },
    { "bold",           font_token_weight,  font_weight_bold,               true, true, false },
    { "heavy",          font_token_weight,  font_weight_heavy,              true, true, false },
    { "extrablack",     font_token_weight,  font_weight_extra_black,        true, true, false },
    { "ultrablack",     font_token_weight,  font_weight_ultra_black,        true, true, false },
    { "black",          font_token_weight,  font_weight_black,              true, true, false },
    { "oblique",        font_token_slope,   font_slope_oblique,             true, true, false },
    { "inclined",       font_token_slope,   font_slope_oblique,             true, true, false },
    { "Ob",             font_token_slope,   font_slope_oblique,             false, true, true },
    { "italic",         font_token_slope,   font_slope_italic,              true, true, false },
    { "It",             font_token_slope,   font_slope_italic,              true, true, true },
    { "ultracondensed", font_token_stretch, font_stretch_ultra_condensed,   true, true, false },
    { "extracondensed", font_token_stretch, font_stretch_extra_condensed,   true, true, false },
    { "semicondensed",  font_token_stretch, font_stretch_semi_condensed,    true, true, false },
    { "condensed",      font_token_stretch, font_stretch_condensed,         true, true, false },
    { "Cond",           font_token_stretch, font_stretch_condensed,         true, true, true },
    { "semiexpanded",   font_token_stretch, font_stretch_semi_expanded,     true, true, false },
    { "extraexpanded",  font_token_stretch, font_stretch_extra_expanded,    true, true, false },
    { "ultraexpanded",  font_token_stretch, font_stretch_ultra_expanded,    true, true, false },
    { "expanded",       font_token_stretch, font_stretch_expanded,          true, true, false },
    { "extended",       font_token_stretch, font_stretch_expanded,          true, true, false },
    { "monospaced",     font_token_spacing, font_spacing_monospaced,        true, true, false },
    { "mono",           font_token_spacing, font_spacing_monospaced,        true, true, false },
    { NULL,             font_token_none,    0,                              false, false, false },
};

const font_data font_manager::styleMapping[] = {
    font_data(font_family_any,  font_style_any,  font_weight_thin,         font_slope_none,    font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_thin,         font_slope_italic,  font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_extra_light,  font_slope_none,    font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_extra_light,  font_slope_italic,  font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_light,        font_slope_none,    font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_light,        font_slope_italic,  font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_regular,      font_slope_none,    font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_regular,      font_slope_italic,  font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_medium,       font_slope_none,    font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_medium,       font_slope_italic,  font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_semibold,     font_slope_none,    font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_semibold,     font_slope_italic,  font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_bold,         font_slope_none,    font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_bold,         font_slope_italic,  font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_extra_bold,   font_slope_none,    font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_extra_bold,   font_slope_italic,  font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_black,        font_slope_none,    font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_black,        font_slope_italic,  font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_extra_black,  font_slope_none,    font_stretch_any, font_spacing_any),
    font_data(font_family_any,  font_style_any,  font_weight_extra_black,  font_slope_italic,  font_stretch_any, font_spacing_any),
};

std::string font_manager::synthesizeFontName(std::string familyName,
    font_weight fontWeight, font_slope fontSlope, font_stretch fontStretch,
    font_spacing fontSpacing)
{
    std::string s;
    s.append(familyName);
    if (fontStretch != font_stretch_medium) {
        s.append(font_manager::stretchName[fontStretch]);
    }
    s.append(font_manager::weightName[fontWeight]);
    if (fontSlope != font_slope_none) {
        s.append(font_manager::slopeName[fontSlope]);
    }
    if (fontSpacing != font_spacing_normal) {
        s.append(font_manager::spacingName[fontSpacing]);
    }
    return s;
}

font_data font_manager::createFontRecord(std::string psName,
    std::string familyName, std::string styleName)
{
    font_stretch fontStretch = font_stretch_medium;
    font_weight fontWeight = font_weight_regular;
    font_slope fontSlope = font_slope_none;
    font_spacing fontSpacing = font_spacing_normal;
    std::string fontNameTemp = psName;
    int tokensProcessed;
    bool foundHyphen = false;
    
    size_t offset = fontNameTemp.length(), tokenLen;
    do {
        tokensProcessed = 0;
        if (offset > 1 && fontNameTemp.c_str()[offset - 1] == '-') {
            foundHyphen = true;
            fontNameTemp.erase(offset - 1, 1);
            offset--;
            tokensProcessed++;
        }
        const font_token_entry *token = fontTokens;
        while (token->name != NULL) {
            if ((token->leftOfHyphen || !foundHyphen) && offset >
                (tokenLen = strlen(token->name)) &&
                ((!token->caseSensitive && strncasecmp(fontNameTemp.c_str() +
                    offset - tokenLen, token->name, tokenLen) == 0) ||
                 (token->caseSensitive && strncmp(fontNameTemp.c_str() +
                    offset - tokenLen, token->name, tokenLen) == 0)) )
            {
                switch (token->tokenType) {
                    case font_token_weight:
                        fontWeight = (font_weight)token->tokenEnum;
                        break;
                    case font_token_slope:
                        fontSlope = (font_slope)token->tokenEnum;
                        break;
                    case font_token_stretch:
                        fontStretch = (font_stretch)token->tokenEnum;
                        break;
                    case font_token_spacing:
                        fontSpacing = (font_spacing)token->tokenEnum;
                        break;
                    default:
                        break;
                }
                if (token->eatToken) {
                    fontNameTemp.erase(offset - tokenLen, tokenLen);
                }
                offset -= tokenLen;
                tokensProcessed++;
            }
            token++;
        }
    } while (tokensProcessed > 0);
    
    if (styleName.length() == 0) {
        styleName = synthesizeFontName(familyName, fontWeight, fontSlope,
            fontStretch, fontSpacing);
    }
    
    return font_data(familyName, styleName, fontWeight, fontSlope,
        fontStretch, fontSpacing);
}

void font_manager::indexFace(font_face *face)
{
    fontPathMap[face->path] = face->font_id;
    fontNameMap[face->name] = face->font_id;
    auto ffi = fontFamilyMap.find(face->fontData.familyName);
    if (ffi == fontFamilyMap.end()) {
        ffi = fontFamilyMap.insert(std::pair<std::string,std::vector<size_t>>
            (face->fontData.familyName, std::vector<size_t>())).first;
    }
    (*ffi).second.push_back(face->font_id);
    allFonts.push_back(face);
    if (debug) {
        fprintf(stderr, "%s %s\n", __func__, face->fontData.toString().c_str());
    }
}

font_face* font_manager::findFontByPath(std::string path)
{
    auto fi = fontPathMap.find(path);
    if (fi != fontPathMap.end()) {
        return findFontById(fi->second);
    } else {
        return nullptr;
    }
}

font_face* font_manager::findFontByName(std::string fontName)
{
    auto fi = fontNameMap.find(fontName);
    if (fi != fontNameMap.end()) {
        return findFontById(fi->second);
    } else {
        return nullptr;
    }
}

font_face* font_manager::findFontByFamily(std::string familyName,
    font_style fontStyle)
{
    auto ffi = fontFamilyMap.find(familyName);
    if (ffi != fontFamilyMap.end()) {
        auto &fontList = (*ffi).second;
        font_spec fontSpec = styleMapping[fontStyle].fontSpec();
        std::equal_to<font_spec> test;
        for (auto fli = fontList.begin(); fli != fontList.end(); fli++) {
            font_spec matchSpec = findFontById(*fli)->fontData.fontSpec();
            if (test(fontSpec, matchSpec)) {
                return findFontById(*fli);
            }
        }
    }
    return nullptr;
}

font_face* font_manager::findFontByData(font_data fontRec)
{
    std::equal_to<font_data> m;
    for (size_t i = 0; i < fontCount(); i++) {
        font_face *face = findFontById(i);
        font_data matchRec = face->getFontData();
        if (m(matchRec, fontRec)) return face;
    }
    return nullptr;
}

font_face* font_manager::findFontBySpec(font_spec fontSpec)
{
    std::equal_to<font_spec> m;
    for (size_t i = 0; i < fontCount(); i++) {
        font_face *face = findFontById(i);
        font_spec matchSpec = face->getFontData().fontSpec();
        if (m(matchSpec, fontSpec)) return face;
    }
    return nullptr;
}

/* Font Data */

font_spec font_data::fontSpec() const
{
    return font_spec(familyName, styleName,
        font_manager::weightTable[fontWeight],
        font_manager::slopeTable[fontSlope],
        font_manager::stretchTable[fontStretch],
        font_manager::spacingTable[fontSpacing]);
}

std::string font_data::toString() const
{
    std::string s;
    s.append("{ font_data");
    s.append(" familyName=\"" + familyName);
    s.append("\", styleName=\"" + styleName);
    s.append("\", weight=\"" + std::string(font_manager::weightName[fontWeight]));
    s.append("\", slope=\"" + std::string(font_manager::slopeName[fontSlope]));
    s.append("\", stretch=\"" + std::string(font_manager::stretchName[fontStretch]));
    s.append("\", spacing=\"" + std::string(font_manager::spacingName[fontSpacing]));
    s.append("\" }");
    return s;
}


/* Font Spec */

std::string font_spec::toString() const
{
    std::string s;
    s.append("{ font_spec");
    s.append(" familyName=\"" + familyName);
    s.append("\", styleName=\"" + styleName);
    s.append("\", weight=" + std::to_string(fontWeight));
    s.append(", slope=" + std::to_string(fontSlope));
    s.append(", stretch=" + std::to_string(fontStretch));
    s.append(", spacing=" + std::to_string(fontSpacing));
    s.append(" }");
    return s;
}


/* Font Manager (FreeType) */

font_manager_ft::font_manager_ft(std::string fontDir) : font_manager()
{
    FT_Error fterr;
    if ((fterr = FT_Init_FreeType(&ftlib))) {
        fprintf(stderr, "error: FT_Init_FreeType failed: fterr=%d\n", fterr);
        exit(1);
    }
    if (fontDir.size() > 0) {
        scanFontDir(fontDir);
    }
}

font_manager_ft::~font_manager_ft()
{
    FT_Done_Library(ftlib);
}

void font_manager_ft::scanFontDir(std::string dir)
{
    for (auto &p : sortList(endsWith(listFiles(dir), ".ttf"))) {
        scanFontPath(p);
    }
}

void font_manager_ft::scanFontPath(std::string path)
{
    FT_Error fterr;
    FT_Face ftface;

    if ((fterr = FT_New_Face(ftlib, path.c_str(), 0, &ftface))) {
        fprintf(stderr, "error: FT_New_Face failed: fterr=%d, path=%s\n",
            fterr, path.c_str());
        return;
    }

    for (int i = 0; i < ftface->num_charmaps; i++)
        if (((ftface->charmaps[i]->platform_id == 0) &&
            (ftface->charmaps[i]->encoding_id == 3))
         || ((ftface->charmaps[i]->platform_id == 3) &&
            (ftface->charmaps[i]->encoding_id == 1))) {
        FT_Set_Charmap(ftface, ftface->charmaps[i]);
        break;
    }

    size_t font_id = faces.size();
    faces.emplace_back(font_face_ft((int)faces.size(), ftface, path));
    indexFace(&faces[font_id]);
}

size_t font_manager_ft::fontCount() { return faces.size(); }

font_face* font_manager_ft::findFontById(size_t font_id)
{
    return &faces[font_id];
}

font_face* font_manager_ft::findFontByPath(std::string path)
{
    font_face *face = font_manager::findFontByPath(path);
    if (!face) {
        scanFontPath(path);
        face = font_manager::findFontByPath(path);
    }
    return face;
}


/* Font Face (FreeType) */

font_face_ft::font_face_ft(int font_id, FT_Face ftface, std::string path) :
    font_face(font_id, path, FT_Get_Postscript_Name(ftface)), ftface(ftface)
{
    fontData = font_manager::createFontRecord(name,
        ftface->family_name, ftface->style_name);
}

FT_Size_Metrics* font_face_ft::get_metrics(int font_size)
{
    /* get metrics for our point size */
    int font_dpi = font_manager::dpi;
    FT_Size_Metrics *metrics = &ftface->size->metrics;
    int points = (int)(font_size * metrics->x_scale) / ftface->units_per_EM;
    if (metrics->x_scale != metrics->y_scale || font_size != points) {
        FT_Set_Char_Size(ftface, 0, font_size, font_dpi, font_dpi);
    }
    return &ftface->size->metrics;
}

int font_face_ft::get_height(int font_size)
{
    return get_metrics(font_size)->height;
}