#include <cstdio>
#include <cstdlib>

#include <string>

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

const char *font_file = "fonts/Roboto-Regular.ttf";

const char* shades[4][4] = {
    { " ", "▗", "▖", "▄" },
    { "▝", "▐", "▞", "▟" },
    { "▘", "▚", "▌", "▙" },
    { "▀", "▜", "▛", "█" },
};

std::string ansi_color(int r, int g, int b)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1B[38;2;%u;%u;%um", r, g, b);
    return std::string(buf);
}

void render_block(msdfgen::Bitmap<float, 3> &bitmap)
{
    std::vector<std::string> v;
    for (int y = 0; y < bitmap.height(); y += 2) {
        std::string o;
        for (int x = 0; x < bitmap.width(); x += 2) {
            int c[2][2][3] = {0}, g[2][2] = {0}, s[3] = {0}, b[2][2] = {0};
            for (int u = 0; u < 2; u++) {
                for (int v = 0; v < 2; v++) {
                    for (int w = 0; w < 3; w++) {
                        int px = msdfgen::pixelFloatToByte(bitmap(x+u,y+v)[w]);
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
}

int main()
{
    msdfgen::Shape shape;
    int codepoint = 'a';
    double glyphAdvance = 0;

    msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype();
    if (!ft) {
        fprintf(stderr, "error: initializeFreetype failed\n");
        exit(1);
    }
    msdfgen::FontHandle *font = msdfgen::loadFont(ft, font_file);
    if (!font) {
        msdfgen::deinitializeFreetype(ft);
        fprintf(stderr, "error: loadFont failed: filename=%s\n", font_file);
        exit(1);
    }
    if (!msdfgen::loadGlyph(shape, font, codepoint, &glyphAdvance)) {
        msdfgen::destroyFont(font);
        msdfgen::deinitializeFreetype(ft);
        fprintf(stderr, "error: loadGlyph failed : codepoint=%d\n" , codepoint);
        exit(1);
    }
    msdfgen::destroyFont(font);
    msdfgen::deinitializeFreetype(ft);

    int width = 64, height = 64;
    bool overlapSupport = true;
    bool scanlinePass = true;
    double range = 1;
    msdfgen::Vector2 translate;
    msdfgen::Vector2 scale = 3;
    double angleThreshold = 3;
    double edgeThreshold = 1.001;
    unsigned long long coloringSeed = 0;

    msdfgen::edgeColoringSimple(shape, angleThreshold, coloringSeed);
    msdfgen::Bitmap<float, 3> msdf(width, height);
    shape.inverseYAxis = true;
    msdfgen::generateMSDF(msdf, shape, range, scale, translate,
        scanlinePass ? 0 : edgeThreshold, overlapSupport);
    render_block(msdf);
}