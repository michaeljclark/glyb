#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <climits>
#include <cstring>

#include <map>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>

#include "binpack.h"
#include "utf8.h"
#include "font.h"
#include "glyph.h"
#include "logger.h"
#include "image.h"
#include "file.h"

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

#include "core/edge-selectors.h"
#include "core/contour-combiners.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

typedef unsigned uint;

static const char *font_file = "fonts/Roboto-Regular.ttf";

static const char* shades[4][4] = {
    { " ", "▗", "▖", "▄" },
    { "▝", "▐", "▞", "▟" },
    { "▘", "▚", "▌", "▙" },
    { "▀", "▜", "▛", "█" },
};

static std::string ansi_color(int r, int g, int b)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1B[38;2;%u;%u;%um", r, g, b);
    return std::string(buf);
}

static int rupeven(int n) { return (n + 1) & ~1; }

static void render_block(msdfgen::Bitmap<float, 3> &bitmap, int w, int h)
{
    w = rupeven(w), h = rupeven(h);
    std::vector<std::string> v;
    for (int y = 0; y < h; y += 2) {
        std::string o;
        for (int x = 0; x < w; x += 2) {
            int c[2][2][3] = {0}, g[2][2] = {0}, s[3] = {0}, b[2][2] = {0};
            for (int u = 0; u < 2; u++) {
                for (int v = 0; v < 2; v++) {
                    for (int w = 0; w < 3; w++) {
                        int px = msdfgen::pixelFloatToByte(bitmap(x+u,h-y-v-1)[w]);
                        c[u][v][w] = px;
                        g[v][u] += c[u][v][w];
                        s[w] += c[u][v][w];
                    }
                    b[v][u] = (g[v][u] / 3) > 64;
                }
            }
            o.append(ansi_color(s[0]>>2,s[1]>>2,s[2]>>2));
            o.append(shades[b[0][0]<<1|b[0][1]][b[1][0]<<1|b[1][1]]);
        }
        printf("%s\n", o.c_str());
    }
    printf("%s\n", ansi_color(255,255,255).c_str());
}

static void copy_bitmap(image_ptr img, msdfgen::Bitmap<float, 3> &bitmap)
{
    for (int y = 0; y < img->width; y++) {
        uint8_t *row = img->getData() + y * img->width * 4;
        for (int x = 0; x < img->height; x++) {
            uint8_t *col = row + x * 4;
            for (int w = 0; w < 3; w++) {
                col[w] = msdfgen::pixelFloatToByte(bitmap(x,y)[w]);
            }
            col[3] = 0xff;
        }
    }
}

int main(int argc, char **argv)
{
    int font_id = 0;
    const char *file = nullptr;
    bool console = false;

    int dpi = 72;
    int width = 64;
    int height = 64;
    unsigned codepoint = 'a';

    bool overlapSupport = true;
    bool scanlinePass = true;
    double range = 1;
    double angleThreshold = 3;
    double edgeThreshold = 1.001;
    double glyphAdvance = 0;
    uint long long coloringSeed = 0;

    msdfgen::Shape shape;
    msdfgen::Bitmap<float, 3> msdf(width, height);
    msdfgen::Vector2 translate, scale = { 3, 3 };

    msdfgen::FreetypeHandle *ft = nullptr;
    msdfgen::FontHandle *font = nullptr;

    /*
     * parse command line arguments
     */

    if (! ((argc == 3 && (strcmp(argv[1], "-file") == 0 && (file = argv[2]))) ||
           (argc == 2 && (console = strcmp(argv[1], "-console") == 0))) ) {
        fprintf(stderr, "usage: %s [-console|-file <output.png>]\n", argv[0]);
        exit(1);
    }

    /*
     * load font
     */

    if (!(ft = msdfgen::initializeFreetype())) {
        fprintf(stderr, "error: initializeFreetype failed\n");
        exit(1);
    }

    if (!(font = msdfgen::loadFont(ft, font_file))) {
        msdfgen::deinitializeFreetype(ft);
        fprintf(stderr, "error: loadFont failed: filename=%s\n", font_file);
        exit(1);
    }

    /*
     * generate multi-channel SDF
     */

    if (!msdfgen::loadGlyph(shape, font, codepoint, &glyphAdvance)) {
        msdfgen::deinitializeFreetype(ft);
        fprintf(stderr, "error: loadGlyph failed: codepoint=%d\n", codepoint);
        exit(1);
    }

    msdfgen::edgeColoringSimple(shape, angleThreshold, coloringSeed);
    msdfgen::generateMSDF(msdf, shape, range, scale, translate,
        scanlinePass ? 0 : edgeThreshold, overlapSupport);


    if (console) {
        render_block(msdf, width, height);
    }

    if (file) {
        image_ptr img = image::createBitmap(msdf.width(), msdf.height(),
            pixel_format_rgba);
        copy_bitmap(img, msdf);
        image::saveToFile(file, img);
    }

    msdfgen::destroyFont(font);
    msdfgen::deinitializeFreetype(ft);
}