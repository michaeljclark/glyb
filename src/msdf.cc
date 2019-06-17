#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <cassert>
#include <cctype>
#include <cmath>

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <tuple>
#include <algorithm>
#include <atomic>
#include <mutex>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

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

#include "binpack.h"
#include "utf8.h"
#include "draw.h"
#include "font.h"
#include "image.h"
#include "glyph.h"
#include "msdf.h"


struct FtContext {
    msdfgen::Shape *shape;
    msdfgen::Point2 position;
    msdfgen::Contour *contour;
};

static msdfgen::Point2 ftPoint2(const FT_Vector &vector) {
    return msdfgen::Point2(vector.x/64., vector.y/64.);
}

static int ftMoveTo(const FT_Vector *to, void *user) {
    FtContext *context = static_cast<FtContext *>(user);
    context->contour = &context->shape->addContour();
    context->position = ftPoint2(*to);
    return 0;
}

static int ftLineTo(const FT_Vector *to, void *user) {
    FtContext *context = static_cast<FtContext *>(user);
    context->contour->addEdge(new msdfgen::LinearSegment(context->position,
        ftPoint2(*to)));
    context->position = ftPoint2(*to);
    return 0;
}

static int ftConicTo(const FT_Vector *control, const FT_Vector *to, void *user) {
    FtContext *context = static_cast<FtContext *>(user);
    context->contour->addEdge(new msdfgen::QuadraticSegment(context->position,
        ftPoint2(*control), ftPoint2(*to)));
    context->position = ftPoint2(*to);
    return 0;
}

static int ftCubicTo(const FT_Vector *control1, const FT_Vector *control2,
        const FT_Vector *to, void *user) {
    FtContext *context = static_cast<FtContext *>(user);
    context->contour->addEdge(new msdfgen::CubicSegment(context->position,
        ftPoint2(*control1), ftPoint2(*control2), ftPoint2(*to)));
    context->position = ftPoint2(*to);
    return 0;
}


/*
 * glyph_renderer_msdf
 */

atlas_entry glyph_renderer_msdf::render(font_atlas *atlas, font_face_ft *face,
	int font_size, int glyph)
{
    msdfgen::Shape shape;
    msdfgen::Vector2 translate, scale = { 1, 1 };
    FT_GlyphSlot ftglyph;
    FT_Error error;
    FT_Outline_Funcs ftFunctions;
    FtContext context = { &shape };
    atlas_entry ae;

    int char_height = 128 * 64; /* magic - shader uses textureSize() */
    int horz_resolution = font_manager::dpi;
	double range = 8;
    bool overlapSupport = true;
    bool scanlinePass = true;
    double angleThreshold = 3;
    double edgeThreshold = 1.001;
    uint long long coloringSeed = 0;
    msdfgen::FillRule fillRule = msdfgen::FILL_NONZERO;

    error = FT_Set_Char_Size(face->ftface, 0, char_height, horz_resolution,
        horz_resolution);
    if (error) {
        return atlas_entry(-1);
    }
    error = FT_Load_Glyph(face->ftface, glyph, 0);
    if (error) {
        return atlas_entry(-1);
    }

    ftFunctions.move_to = ftMoveTo;
    ftFunctions.line_to = ftLineTo;
    ftFunctions.conic_to = ftConicTo;
    ftFunctions.cubic_to = ftCubicTo;
    ftFunctions.shift = 0;
    ftFunctions.delta = 0;

    error = FT_Outline_Decompose(&face->ftface->glyph->outline, &ftFunctions,
    	&context);
    if (error) {
        return atlas_entry(-1);
    }

    /* font dimensions */
    ftglyph = face->ftface->glyph;
    int ox = (int)floorf((float)ftglyph->metrics.horiBearingX / 64.0f) - 1;
    int oy = (int)floorf((float)(ftglyph->metrics.horiBearingY -
        ftglyph->metrics.height) / 64.0f) - 1;
    int w = (int)ceilf(ftglyph->metrics.width / 64.0f) + 2;
    int h = (int)ceilf(ftglyph->metrics.height / 64.0f) + 2;
    translate.x = -ox;
    translate.y = -oy;

    msdfgen::Bitmap<float, 3> msdf(w, h);
    msdfgen::edgeColoringSimple(shape, angleThreshold, coloringSeed);
    msdfgen::generateMSDF(msdf, shape, range, scale, translate,
        scanlinePass ? 0 : edgeThreshold, overlapSupport);
    msdfgen::distanceSignCorrection(msdf, shape, scale, translate, fillRule);
    if (edgeThreshold > 0) {
        msdfgen::msdfErrorCorrection(msdf, edgeThreshold/(scale*range));
    }

    /*
     * note: we create a template font atlas entry with font size 0,
     * which indicates the atlas entry for for a variable size entry,
     * which is the main reason we use signed distance fields. the
     * atlas contains special logic to create font size specific
     * entries from the template entry.
     *
     * note: we use the multithreaded interface
	 */
    ae = atlas->create(face, 0, glyph, char_height, ox, oy, w, h);
    if (ae.bin_id >= 0) {
        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) {
                int r = msdfgen::pixelFloatToByte(msdf(x,y)[0]);
                int g = msdfgen::pixelFloatToByte(msdf(x,y)[1]);
                int b = msdfgen::pixelFloatToByte(msdf(x,y)[2]);
                size_t dst = ((ae.y + y) * atlas->width + ae.x + x) * 4;
                uint32_t color = r | g << 8 | b << 16 | 0xff000000;
                *(uint32_t*)&atlas->pixels[dst] = color;
            }
        }
    }

    /* clients expect font metrics for the font size to be loaded */
    face->get_metrics(font_size);

    /*
     * note: we recurse into the font atlas lookup code to trigger
     * generation of an atlas entry for the requested font size.
     */
    return ae;
}