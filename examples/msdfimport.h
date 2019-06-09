#pragma once

#include "core/arithmetics.hpp"
#include "core/Vector2.h"
#include "core/Scanline.h"
#include "core/Shape.h"
#include "core/BitmapRef.hpp"
#include "core/Bitmap.h"
#include "core/pixel-conversion.hpp"
#include "core/edge-coloring.h"
#include "core/render-sdf.h"
#include "core/rasterization.h"
#include "core/estimate-sdf-error.h"
#include "core/save-bmp.h"
#include "core/save-tiff.h"
#include "core/shape-description.h"
#include "ext/save-png.h"
#include "ext/import-svg.h"
#include "ext/import-font.h"
#include "msdfgen.h"

typedef unsigned uint;

/*
 * we need to open up the msdfgen namespace so that we can access FontHandle
 * and implement a custom version of loadGlyph that accepts glyph indices
 * and applies scaling to the font using FT_Set_Char_Size.
 *
 * The outline shape adaptors are not exposed by the API, so we need to
 * reimplement them. We could declare this outside of the msdfgen namespace,
 * but this would require reimplementing several more methods.
 */
namespace msdfgen {

struct FreetypeHandle {
    FT_Library library;
};

struct FtContext {
    Point2 position;
    Shape *shape;
    Contour *contour;
};

struct FontHandle {
    friend bool loadGlyph(msdfgen::Shape &output, msdfgen::FontHandle *font,
        int glyph, int char_height, int horz_resolution, double *advance);

    FT_Face face;

    std::vector<std::pair<uint,uint>> allCodepointGlyphPairs()
    {
        std::vector<std::pair<uint,uint>> l;
        uint glyph, codepoint = FT_Get_First_Char(face, &glyph);
        do {
            l.push_back(std::pair<uint,uint>(codepoint,glyph));
            codepoint = FT_Get_Next_Char(face, codepoint, &glyph);
        } while (glyph);
        return l;
    }

};

static Point2 ftPoint2(const FT_Vector &vector) {
    return Point2(vector.x/64., vector.y/64.);
}

static int ftMoveTo(const FT_Vector *to, void *user) {
    FtContext *context = reinterpret_cast<FtContext *>(user);
    context->contour = &context->shape->addContour();
    context->position = ftPoint2(*to);
    return 0;
}

static int ftLineTo(const FT_Vector *to, void *user) {
    FtContext *context = reinterpret_cast<FtContext *>(user);
    context->contour->addEdge(new LinearSegment(context->position,
        ftPoint2(*to)));
    context->position = ftPoint2(*to);
    return 0;
}

static int ftConicTo(const FT_Vector *control, const FT_Vector *to, void *user) {
    FtContext *context = reinterpret_cast<FtContext *>(user);
    context->contour->addEdge(new QuadraticSegment(context->position,
        ftPoint2(*control), ftPoint2(*to)));
    context->position = ftPoint2(*to);
    return 0;
}

static int ftCubicTo(const FT_Vector *control1, const FT_Vector *control2,
        const FT_Vector *to, void *user) {
    FtContext *context = reinterpret_cast<FtContext *>(user);
    context->contour->addEdge(new CubicSegment(context->position,
        ftPoint2(*control1), ftPoint2(*control2), ftPoint2(*to)));
    context->position = ftPoint2(*to);
    return 0;
}

/*
 * reimplementation of loadGlyph that uses glyph index instead of unicode
 * codepoint scales glyphs with FT_Set_Char_Size.
 */
bool loadGlyph(msdfgen::Shape &output, msdfgen::FontHandle *font,
    int glyph, int char_height, int horz_resolution, double *advance)
{
    FT_Error error;
    if (!font) {
        return false;
    }
    error = FT_Set_Char_Size(font->face, 0, char_height, horz_resolution,
        horz_resolution);
    if (error) {
        return false;
    }
    error = FT_Load_Glyph(font->face, glyph, 0);
    if (error) {
        return false;
    }
    output.contours.clear();
    output.inverseYAxis = false;
    if (advance)
        *advance = font->face->glyph->advance.x/64.;

    msdfgen::FtContext context = { };
    context.shape = &output;
    FT_Outline_Funcs ftFunctions;
    ftFunctions.move_to = &msdfgen::ftMoveTo;
    ftFunctions.line_to = &msdfgen::ftLineTo;
    ftFunctions.conic_to = &msdfgen::ftConicTo;
    ftFunctions.cubic_to = &msdfgen::ftCubicTo;
    ftFunctions.shift = 0;
    ftFunctions.delta = 0;
    error = FT_Outline_Decompose(&font->face->glyph->outline, &ftFunctions,
        &context);
    return !error;
}

} /* namespace msdfgen */
