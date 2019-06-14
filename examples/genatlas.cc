#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <climits>
#include <cassert>
#include <cstring>

#include <map>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <chrono>

#include "binpack.h"
#include "draw.h"
#include "font.h"
#include "glyph.h"
#include "logger.h"
#include "image.h"
#include "file.h"
#include "utf8.h"
#include "util.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include "msdfimport.h"

using namespace std::chrono;

typedef unsigned uint;

static const char *font_path = nullptr;
static const char *output_path = nullptr;
static const char *scan_path = nullptr;
static const int dpi = 72;
static unsigned glyph_limit = 0x7f; /* 0x10ffff; */
static double range = 8;
static int glyph = 0;
static int font_size = 128;
static bool help_text = false;
static bool quiet = false;
static bool verbose = false;
static bool batch_render = true;
static bool display_ansi = false;
static bool clear_ansi = false;

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

static void render_block(msdfgen::Bitmap<float, 3> &bitmap)
{
    int w = bitmap.width(), h = bitmap.height();
    assert(w == rupeven(w));
    assert(h == rupeven(h));
    std::vector<std::string> v;
    if (clear_ansi) {
        printf("\033[2J");
    }
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
    printf("%s", ansi_color(255,255,255).c_str());
}

static std::string ae_dim_str(atlas_entry *ae)
{
    std::string s;
    if (ae->ox >= 0) s+= "+";
    s += std::to_string(ae->ox);
    if (ae->oy >= 0) s+= "+";
    s += std::to_string(ae->oy);
    s += " ";
    s += std::to_string(ae->x);
    s += "x";
    s += std::to_string(ae->y);
    return s;
}

atlas_entry* gen(msdfgen::FontHandle *font, font_atlas *atlas,
    int size, int dpi, int glyph)
{
    msdfgen::Shape shape;
    FT_GlyphSlot ftglyph;
    atlas_entry *ae;
    msdfgen::Vector2 translate, scale = { 1, 1 };

    bool overlapSupport = true;
    bool scanlinePass = true;
    double angleThreshold = 3;
    double edgeThreshold = 1.001;
    double glyphAdvance = 0;
    uint long long coloringSeed = 0;
    msdfgen::FillRule fillRule = msdfgen::FILL_NONZERO;

    if (!msdfgen::loadGlyph(shape, font, glyph, size, dpi, &glyphAdvance)) {
        return nullptr;
    }

    /* font dimensions */
    ftglyph = font->face->glyph;
    int ox = (int)floorf((float)ftglyph->metrics.horiBearingX / 64.0f) - 1;
    int oy = (int)floorf((float)(ftglyph->metrics.horiBearingY -
        ftglyph->metrics.height) / 64.0f) - 1;
    int w = (int)ceilf(ftglyph->metrics.width / 64.0f) + 2;
    int h = (int)ceilf(ftglyph->metrics.height / 64.0f) + 2;
    w = rupeven(w), h = rupeven(h); /* render_block needs even rows and cols */
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

    if (display_ansi) {
        render_block(msdf);
    }

    ae = atlas->create(0, 0, glyph, ox, oy, w, h);
    if (ae) {
        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) {
                int r = msdfgen::pixelFloatToByte(msdf(x,y)[0]);
                int g = msdfgen::pixelFloatToByte(msdf(x,y)[1]);
                int b = msdfgen::pixelFloatToByte(msdf(x,y)[2]);
                size_t dst = ((ae->y + y) * atlas->width + ae->x + x) * 4;
                uint32_t color = r | g << 8 | b << 16 | 0xff000000;
                *(uint32_t*)&atlas->pixels[dst] = color;
            }
        }
    }

    return ae;
}

/*
 * ftrender -render text using freetype2, and display metrics
 *
 * e.g.  ./build/bin/ftrender --font fonts/Roboto-Bold.ttf \
 *           --size 32 --text 'ABCDabcd1234' --block
 */

void print_help(int argc, char **argv)
{
    fprintf(stderr,
        "\n"
        "Usage: %s [options] --font <path>\n"
        "\n"
        "Options:\n"
        "  -h, --help             display this help text\n"
        "  -q, --quiet            supress all output messages\n"
        "  -v, --verbose          intclude per glyph output messages\n"
        "  -d, --display          display glyphs (ANSI console)\n"
        "  -c, --clear            send clear before glyph (ANSI console)\n"
        "  -r, --range <float>    signed distance range (default %f)\n"
        "  -g, --glyph <glyph>    display single glyph (ANSI console)\n"
        "  -s, --size <pixels>    font size (default %d)\n"
        "  -l, --limit <integer>  render atlas glyph limit (default %d)\n"
        "  -o, --output <path>    output path (defaults to <path>.atlas)\n"
        "  -f, --font <ttf-file>  font path (mandatory)\n"
        "  -a, --scan <font-dir>  convert all fonts in directory\n",
        argv[0], range, font_size, glyph_limit);
}


bool check_param(bool cond, const char *param)
{
    if (cond) {
        printf("error: %s requires parameter\n", param);
    }
    return (help_text = cond);
}

bool match_opt(const char *arg, const char *opt, const char *longopt)
{
    return strcmp(arg, opt) == 0 || strcmp(arg, longopt) == 0;
}

void parse_options(int argc, char **argv)
{
    int i = 1;
    while (i < argc) {
        if (match_opt(argv[i], "-h", "--help")) {
            help_text = true;
            i++;
        }
        else if (match_opt(argv[i], "-o","--output")) {
            if (check_param(++i == argc, "--output")) break;
            output_path = argv[i++];
        }
        else if (match_opt(argv[i], "-f","--font")) {
            if (check_param(++i == argc, "--font")) break;
            font_path = argv[i++];
        }
        else if (match_opt(argv[i], "-a","--scan")) {
            if (check_param(++i == argc, "--scan")) break;
            scan_path = argv[i++];
        }
        else if (match_opt(argv[i], "-s", "--size")) {
            if (check_param(++i == argc, "--size")) break;
            font_size = atoi(argv[i++]);
        }
        else if (match_opt(argv[i], "-r", "--range")) {
            if (check_param(++i == argc, "--range")) break;
            range = (float)atof(argv[i++]);
        }
        else if (match_opt(argv[i], "-g", "--glyph")) {
            if (check_param(++i == argc, "--glyph")) break;
            const char *glyphstr = argv[i++];
            int codepoint = utf8_to_utf32(glyphstr);
            size_t codelen = utf8_codelen(glyphstr);
            if (strlen(glyphstr) != codelen) {
                printf("error: invalid glyph: %s\n", glyphstr);
                help_text = true;
                break;
            } else {
                glyph = codepoint;
                display_ansi = true;
                batch_render = false;
            }
        }
        else if (match_opt(argv[i], "-l", "--limit")) {
            if (check_param(++i == argc, "--limit")) break;
            glyph_limit = atoi(argv[i++]);
        }
        else if (match_opt(argv[i], "-q", "--quiet")) {
            quiet = true;
            i++;
        }
        else if (match_opt(argv[i], "-v", "--verbose")) {
            verbose = true;
            i++;
        }
        else if (match_opt(argv[i], "-d", "--display")) {
            display_ansi = true;
            i++;
        }
        else if (match_opt(argv[i], "-c", "--clear")) {
            clear_ansi = true;
            i++;
        }
        else {
            fprintf(stderr, "error: unknown option: %s\n", argv[i]);
            help_text = true;
            break;
        }
    }

    if (font_path == nullptr && scan_path == nullptr) {
        fprintf(stderr, "error: need to specify --font <path>\n");
        help_text = true;
    }

    if (help_text) {
        print_help(argc, argv);
        exit(1);
    }
}

uint64_t process_one_file(const char *font_path, const char *output_path)
{
    font_atlas atlas(2048, 2048, 4);

    msdfgen::FreetypeHandle *ft = nullptr;
    msdfgen::FontHandle *font = nullptr;

    const auto t1 = high_resolution_clock::now();

    /*
     * load font
     */

    if (!(ft = msdfgen::initializeFreetype())) {
        fprintf(stderr, "error: initializeFreetype failed\n");
        exit(1);
    }

    if (!(font = msdfgen::loadFont(ft, font_path))) {
        msdfgen::deinitializeFreetype(ft);
        fprintf(stderr, "error: loadFont failed: filename=%s\n", font_path);
        exit(1);
    }

    /*
     * select single or multiple glyphs
     */
    std::vector<std::pair<uint,uint>> allGlyphs;
    if (glyph) {
        allGlyphs.push_back({ glyph, FT_Get_Char_Index(font->face, glyph)});
    } else {
        allGlyphs = font->allCodepointGlyphPairs();
    }

    /*
     * loop through chosen glyphs and create atlas
     */
    size_t area = 0, count = 0;
    for (auto pair : allGlyphs) {
        uint codepoint = pair.first, glyph = pair.second;
        atlas_entry *ae;

        if (codepoint >= glyph_limit) continue;

        if (!(ae = gen(font, &atlas, font_size * 64, dpi, glyph))) {
            if (verbose) {
                printf("ATLAS FULL (codepoint: %u, glyph: %u)\n",
                    codepoint, glyph);
            }
            break;
        }

        if (verbose) {
            printf("[%zu/%zu] %20s (codepoint: %u, glyph: %u)\n",
                count, allGlyphs.size(), ae_dim_str(ae).c_str(),
                codepoint, glyph);
        }

        area += ae->w * ae->h;
        count++;
    }

    if (batch_render) {
        atlas.save(output_path);
    }

    /*
     * atlas statistics
     */
    if (verbose) {
        printf("---\n");
        printf("font-path        : %s\n", font_path);
        printf("total-glyphs     : %zu\n", allGlyphs.size());
        printf("glyphs-processed : %zu\n", count);
        printf("total-area       : %zu (%d squared)\n", area, (int)sqrtf(area));
        printf("utilization      : %5.3f%%\n",
            100.0f*(float)area / (float)(atlas.width * atlas.height));
    }

    msdfgen::destroyFont(font);
    msdfgen::deinitializeFreetype(ft);

    const auto t2 = high_resolution_clock::now();

    return duration_cast<nanoseconds>(t2 - t1).count();
}

int main(int argc, char **argv)
{
    parse_options(argc, argv);

    /* gather files */
    std::vector<std::pair<std::string,std::string>> pathList;
    if (scan_path) {
        for (auto &path : sortList(endsWith(listFiles(scan_path), ".ttf"))) {
            pathList.push_back(std::pair<std::string,std::string>(path, path));
        }
    } else {
        pathList.push_back(std::pair<std::string,std::string>(
            font_path, output_path ? output_path : font_path));
    }

    /* process them */
    for (auto &path : pathList) {
        uint64_t d = process_one_file(path.first.c_str(), path.second.c_str());
        if (verbose) {
            printf("processing time  : %5.3f seconds\n---\n", (float)d/ 1e9f);
        } else if (!quiet) {
            printf("%-40s (%5.3f seconds)\n", path.first.c_str(), (float)d/ 1e9f);
        }
    }
}