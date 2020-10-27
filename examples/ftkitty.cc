/*
 * ftkitty - program that outputs text using kitty image protocol
 *
 * g++ -O2 examples/ftkitty.cc $(pkg-config --libs freetype2 --libs harfbuzz
 *      --cflags freetype2 --cflags harfbuzz) -o build/ftkitty
 */

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <climits>

#include <vector>
#include <unistd.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include <hb.h>
#include <hb-ft.h>

#include "xcolortab.h"

static const char *font_path = "fonts/DejaVuSansMono.ttf";
static const char* text_lang = "en";
static const char *render_text = "hello";
static const char *color_name = "WhiteSmoke";
static const int font_dpi = 72;
static int font_size = 72;
static bool help_text = false;


/*
 * FreeType Span Measurement
 *
 * Measures minimum and maximum x and y coordinates for one glyph.
 * Used as a callback to FT_Outline_Render.
 */

struct span_measure
{
    int min_x, min_y, max_x, max_y;

    span_measure();

    static void fn(int y, int count, const FT_Span* spans, void *user);
};

inline span_measure::span_measure() :
    min_x(INT_MAX), min_y(INT_MAX), max_x(INT_MIN), max_y(INT_MIN) {}

/*
 * FreeType Span Recorder
 *
 * Collects the output of span coverage into an 8-bit grayscale bitmap.
 * Used as a callback to FT_Outline_Render.
 */

struct span_vector : span_measure
{
    int gx, gy, ox, oy, w, h;

    std::vector<uint8_t> pixels;

    span_vector();

    void reset(int width, int height);

    static void fn(int y, int count, const FT_Span* spans, void *user);
};

inline span_vector::span_vector() :
    gx(0), gy(0), ox(0), oy(0), w(0), h(0), pixels() {}

/*
 * FreeType span rasterization
 */

void span_measure::fn(int y, int count, const FT_Span* spans, void *user)
{
    span_measure *s = static_cast<span_measure*>(user);
    s->min_y = std::min(s->min_y, y);
    s->max_y = std::max(s->max_y, y);
    for (int i = 0; i < count; i++) {
        s->min_x = std::min(s->min_x, (int)spans[i].x);
        s->max_x = std::max(s->max_x, (int)spans[i].x + spans[i].len);
    }
}

void span_vector::reset(int width, int height)
{
    pixels.clear();
    pixels.resize(width * height);
    w = width;
    h = height;
}

void span_vector::fn(int y, int count, const FT_Span* spans, void *user)
{
    span_vector *s = static_cast<span_vector*>(user);
    int dy = std::max(std::min(s->gy + s->oy + y, s->h - 1), 0);
    s->min_y = std::min(s->min_y, y);
    s->max_y = std::max(s->max_y, y);
    for (int i = 0; i < count; i++) {
        s->min_x = std::min(s->min_x, (int)spans[i].x);
        s->max_x = std::max(s->max_x, (int)spans[i].x + spans[i].len);
        int dx = std::max(std::min(s->gx + s->ox + spans[i].x, s->w), 0);
        int dl = std::max(std::min((int)spans[i].len, s->w - dx), 0);
        if (dl > 0) {
            memset(&s->pixels[dy * s->w + dx], spans[i].coverage, dl);
        }
    }
}

/*
 * base64.c : base-64 / MIME encode/decode
 * PUBLIC DOMAIN - Jon Mayo - November 13, 2003
 * $Id: base64.c 156 2007-07-12 23:29:10Z orange $
 */

static const uint8_t base64enc_tab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encode(size_t in_len, const unsigned char *in,
    size_t out_len, char *out)
{
    unsigned ii, io;
    uint_least32_t v;
    unsigned rem;

    for(io=0,ii=0,v=0,rem=0;ii<in_len;ii++) {
        unsigned char ch;
        ch=in[ii];
        v=(v<<8)|ch;
        rem+=8;
        while(rem>=6) {
            rem-=6;
            if(io>=out_len) return -1; /* truncation is failure */
            out[io++]=base64enc_tab[(v>>rem)&63];
        }
    }
    if(rem) {
        v<<=(6-rem);
        if(io>=out_len) return -1; /* truncation is failure */
        out[io++]=base64enc_tab[v&63];
    }
    while(io&3) {
        if(io>=out_len) return -1; /* truncation is failure */
        out[io++]='=';
    }
    if(io>=out_len) return -1; /* no room for null terminator */
    out[io]=0;
    return io;
}

/*
 * kitty image protocol
 *
 * outputs base64 encoding of image data in a span_vector
 */

static uint32_t shade_color(uint32_t c, uint32_t col)
{
    uint32_t r = ((col >> 0 ) & 0xff) * c / 0xff;
    uint32_t g = ((col >> 8 ) & 0xff) * c / 0xff;
    uint32_t b = ((col >> 16) & 0xff) * c / 0xff;
    return (r << 0) | (g << 8)| (b << 16) | (0xff << 24);
}

static void render_kitty(span_vector *s, uint32_t rgba_col)
{
    const size_t chunk_limit = 4096;

    size_t pixel_count = s->w * s->h;
    size_t total_size = pixel_count << 2;
    size_t base64_size = ((total_size + 2) / 3) * 4;
    uint32_t *color_pixels = (uint32_t*)malloc(total_size);
    uint8_t *base64_pixels = (uint8_t*)malloc(base64_size+1);

    /* convert pixel data to RGBA */
    for (int y = 0; y < s->h; y++) {
        for (int x = 0; x < s->w; x++) {
            uint8_t c = s->pixels[(s->h - y - 1) * s->w + x];
            color_pixels[y * s->w + x] = shade_color(c, rgba_col);
        }
    }

    /* base64 encode the data */
    int ret = base64_encode(total_size, (const uint8_t*)color_pixels,
        base64_size+1, (char*)base64_pixels);
    if (ret < 0) {
        fprintf(stderr, "error: base64_encode failed: ret=%d\n", ret);
        exit(1);
    }

    /*
     * write kitty protocol RGBA image in chunks no greater than 4096 bytes
     *
     * <ESC>_Gf=32,s=<w>,v=<h>,m=1;<encoded pixel data first chunk><ESC>\
     * <ESC>_Gm=1;<encoded pixel data second chunk><ESC>\
     * <ESC>_Gm=0;<encoded pixel data last chunk><ESC>\
     */

    size_t sent_bytes = 0;
    while (sent_bytes < base64_size) {
        size_t chunk_size = base64_size - sent_bytes < chunk_limit
            ? base64_size - sent_bytes : chunk_limit;
        int cont = !!(sent_bytes + chunk_size < base64_size);
        if (sent_bytes == 0) {
            fprintf(stdout,"\x1B_Gf=32,a=T,s=%d,v=%d,m=%d;", s->w, s->h, cont);
        } else {
            fprintf(stdout,"\x1B_Gm=%d;", cont);
        }
        fwrite(base64_pixels + sent_bytes, chunk_size, 1, stdout);
        fprintf(stdout, "\x1B\\");
        sent_bytes += chunk_size;
    }
    fprintf(stdout, "\n");
    fflush(stdout);

    free(color_pixels);
    free(base64_pixels);
}

/*
 * ftkitty -render text using freetype2, and display metrics
 *
 * e.g.  ./build/bin/ftkitty --font fonts/Roboto-Bold.ttf \
 *           --size 32 --text 'ABCDabcd1234'
 */

static void print_help(int argc, char **argv)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  -f, --font <ttf-file>  font file (default %s)\n"
        "  -s, --size <points>    font size (default %d)\n"
        "  -t, --text <string>    text to render (default \"%s\")\n"
        "  -c, --color <string>   color to render (default \"%s\")\n"
        "  -h, --help             command line help\n",
        argv[0], font_path, font_size, render_text, color_name);
}

static uint32_t xcolor_by_name(const char *xcolor_name, uint32_t notfound)
{
    for (struct xcolor *colp = xcolortab; colp->name; colp++) {
        if (strcmp(xcolor_name, colp->name) == 0) {
            return colp->rgba;
        }
    }
    return notfound;
}

static bool check_param(bool cond, const char *param)
{
    if (cond) {
        printf("error: %s requires parameter\n", param);
    }
    return (help_text = cond);
}

static bool match_opt(const char *arg, const char *opt, const char *longopt)
{
    return strcmp(arg, opt) == 0 || strcmp(arg, longopt) == 0;
}

static void parse_options(int argc, char **argv)
{
    int i = 1;
    while (i < argc) {
        if (match_opt(argv[i], "-f","--font")) {
            if (check_param(++i == argc, "--font")) break;
            font_path = argv[i++];
        }
        else if (match_opt(argv[i], "-s", "--size")) {
            if (check_param(++i == argc, "--size")) break;
            font_size = atoi(argv[i++]);
        }
        else if (match_opt(argv[i], "-t", "--text")) {
            if (check_param(++i == argc, "--text")) break;
            render_text = argv[i++];
        }
        else if (match_opt(argv[i], "-c", "--color")) {
            if (check_param(++i == argc, "--color")) break;
            color_name = argv[i++];
        } else if (match_opt(argv[i], "-h", "--help")) {
            help_text = true;
            i++;
        } else {
            fprintf(stderr, "error: unknown option: %s\n", argv[i]);
            help_text = true;
            break;
        }
    }

    if (help_text) {
        print_help(argc, argv);
        exit(1);
    }
}

/*
 * ftkitty main program
 */

int main(int argc, char **argv)
{
    FT_Library ftlib;
    FT_Face ftface;
    FT_Error fterr;
    hb_font_t *hbfont;
    hb_language_t hblang;
    unsigned int glyph_count;
    hb_glyph_info_t *glyph_info;
    hb_glyph_position_t *glyph_pos;
    span_vector span;

    parse_options(argc, argv);

    if ((fterr = FT_Init_FreeType(&ftlib))) {
        fprintf(stderr, "error: FT_Init_FreeType failed: fterr=%d\n", fterr);
        exit(1);
    }

    if ((fterr = FT_New_Face(ftlib, font_path, 0, &ftface))) {
        fprintf(stderr, "error: FT_New_Face failed: fterr=%d, path=%s\n",
            fterr, font_path);
        exit(1);
    }

    FT_Set_Char_Size(ftface, 0, font_size * 64, font_dpi, font_dpi);

    hbfont  = hb_ft_font_create(ftface, NULL);
    hblang = hb_language_from_string(text_lang, (int)strlen(text_lang));

    hb_buffer_t *buf = hb_buffer_create();
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hblang);
    hb_buffer_add_utf8(buf, render_text, (int)strlen(render_text),
        0, (int)strlen(render_text));

    hb_shape(hbfont, buf, NULL, 0);
    glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

    FT_Raster_Params rp;
    rp.target = 0;
    rp.flags = FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_AA;
    rp.user = &span;
    rp.black_spans = 0;
    rp.bit_set = 0;
    rp.bit_test = 0;
    rp.gray_spans = span_vector::fn;

    int width = 0;
    for (size_t i = 0; i < glyph_count; i++) {
        width += glyph_pos[i].x_advance/64;
    }

    /* set global offset and size the render buffer */
    span.gx = 0;
    span.gy = -ftface->size->metrics.descender/64;
    span.reset(width, (ftface->size->metrics.ascender -
        ftface->size->metrics.descender)/64);

    /* render glyphs */
    for (size_t i = 0; i < glyph_count; i++)
    {
        if ((fterr = FT_Load_Glyph(ftface, glyph_info[i].codepoint, 0))) {
            fprintf(stderr, "error: FT_Load_Glyph failed: codepoint=%d fterr=%d\n",
                glyph_info[i].codepoint, fterr);
            exit(1);
        }
        if (ftface->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
            fprintf(stderr, "error: FT_Load_Glyph format is not outline\n");
            exit(1);
        }

        span.min_x = INT_MAX;
        span.min_y = INT_MAX;
        span.max_x = INT_MIN;
        span.max_y = INT_MIN;

        span.ox = glyph_pos[i].x_offset/64;
        span.oy = glyph_pos[i].y_offset/64;

        if ((fterr = FT_Outline_Render(ftlib, &ftface->glyph->outline, &rp))) {
            printf("error: FT_Outline_Render failed: fterr=%d\n", fterr);
            exit(1);
        }

        span.gx += glyph_pos[i].x_advance/64;
        span.gy += glyph_pos[i].y_advance/64;
    }

    /* display rendered output */
    render_kitty(&span, xcolor_by_name(color_name, 0xffffffff));

    /* free our buffers */
    hb_buffer_destroy(buf);
    hb_font_destroy(hbfont);
    FT_Done_Face(ftface);
    FT_Done_Library(ftlib);
}
