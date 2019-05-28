#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <climits>
#include <cmath>

#include <memory>
#include <vector>
#include <map>
#include <tuple>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include <hb.h>
#include <hb-ft.h>

#include "binpack.h"
#include "utf8.h"
#include "glyph.h"

static const char* ascii_palette = " .:-=+*#%@";
static const char *font_path = "fonts/NotoSans-Regular.ttf";
static const char* text_lang = "en";
static const char *render_text = "hello";
static const int font_dpi = 72;
static int font_size = 32;
static bool help_text = false;
static bool use_block = false;
static bool debug = false;

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

void render_block(span_vector *d)
{
    for (int y = 0; y < d->height; y += 2) {
        std::string s;
        size_t b1 = (d->height - y - 1) * d->width;
        size_t b2 = (std::max)((d->height - y - 2), 0) * d->width;
        for (int x = 0; x < d->width; x += 2) {
            uint8_t c00 = d->pixels[b1 + x];
            uint8_t c01 = d->pixels[b1 + (std::min)(x + 1, d->width - 1)];
            uint8_t c10 = d->pixels[b2 + x];
            uint8_t c11 = d->pixels[b2 + (std::min)(x + 1, d->width - 1)];
            bool b00 = c00 > 128, b01 = c01 > 128;
            bool b10 = c10 > 128, b11 = c11 > 128;
            int sum = (c00 + c01 + c10 + c11) / 4;
            s.append(ansi_color(sum, sum, sum));
            s.append(shades[b00 << 1 | b01][b10 << 1 | b11]);
        }
        printf("%s\n", s.c_str());
    }
}

void render_ascii(span_vector *d)
{
    for (int y = 0; y < d->height; y++) {
        std::string s;
        for (int x = 0; x < d->width; x++) {
            uint8_t c = d->pixels[(d->height - y - 1) * d->width + x];
            s.append(1, ascii_palette[c/(255/(strlen(ascii_palette)-1))]);
        }
        printf("%s\n", s.c_str());
    }
}

void print_face(FT_Face ftface)
{
    FT_Size_Metrics*  metrics = &ftface->size->metrics;

    printf("family_name         = \"%s\"\n", ftface->family_name);
    printf("style_name          = \"%s\"\n", ftface->style_name);
    printf("psname              = \"%s\"\n", FT_Get_Postscript_Name(ftface));
    printf("face_index          = % 9d\n", (int)ftface->face_index);
    printf("num_glyphs          = % 9d\n", (int)ftface->num_glyphs);

    printf("height              = %9.3f   units_per_EM        = % 9d\n",
        (float)ftface->height/64.0f,
        (int)ftface->units_per_EM);
    printf("metrics.x_ppem      = % 9d   metrics.y_ppem      = % 9d\n",
        (int)metrics->x_ppem,
        (int)metrics->y_ppem);
    printf("metrics.x_scale     = %9.3f   metrics.y_scale     = %9.3f\n",
        (float)metrics->x_scale/65536.0f,
        (float)metrics->y_scale/65536.0f);
    printf("metrics.ascender    = %9.3f   metrics.descender   = %9.3f\n",
        (float)metrics->ascender/64.0f,
        (float)metrics->descender/64.0f);
    printf("metrics.height      = %9.3f   metrics.max_advance = %9.3f\n",
        (float)metrics->height/64.0f,
        (float)metrics->max_advance/64.0f);
    printf("bbox.xMin           = %9.3f   bbox.xMax           = %9.3f\n",
        (float)ftface->bbox.xMin/64.0f,
        (float)ftface->bbox.xMax/64.0f);
    printf("bbox.yMin           = %9.3f   bbox.yMax           = %9.3f\n",
        (float)ftface->bbox.yMin/64.0f,
        (float)ftface->bbox.yMax/64.0f);
    printf("ascender            = %9.3f   descender           = %9.3f\n",
        (float)ftface->ascender/64.0f,
        (float)ftface->descender/64.0f);
    printf("max_advance_width   = %9.3f   max_advance_height  = %9.3f\n",
        (float)ftface->max_advance_width/64.0f,
        (float)ftface->max_advance_height/64.0f);
    printf("\n");
}

void print_glyph(FT_GlyphSlot ftglyph, int codepoint, span_measure *span)
{
    printf("glyph               = % 9d\n", codepoint);

    printf("linearHoriAdvance   = %9.3f   linearVertAdvance   = %9.3f\n",
        (float)ftglyph->linearHoriAdvance/65536.0f,
        (float)ftglyph->linearVertAdvance/65536.0f);
    printf("metrics.width       = %9.3f   metrics.height      = %9.3f\n",
        (float)ftglyph->metrics.width/64.0f,
        (float)ftglyph->metrics.height/64.0f);
    printf("metrics.hBearingX   = %9.3f   metrics.hBearingY   = %9.3f\n",
        (float)ftglyph->metrics.horiBearingX/64.0f,
        (float)ftglyph->metrics.horiBearingY/64.0f);
    printf("metrics.hAdvance    = %9.3f   metrics.vAdvance    = %9.3f\n",
        (float)ftglyph->metrics.horiAdvance/64.0f,
        (float)ftglyph->metrics.vertAdvance/64.0f);
    printf("metrics.vBearingX   = %9.3f   metrics.vBearingY   = %9.3f\n",
        (float)ftglyph->metrics.vertBearingX/64.0f,
        (float)ftglyph->metrics.vertBearingY/64.0f);

    printf("span.min_x          = % 9d   span.min_y          = % 9d\n",
        span->min_x, span->min_y);
    printf("span.max_x          = % 9d   span.max_y          = % 9d\n",
        span->max_x, span->max_y);
    printf("span.(width)        = % 9d   span.(height)       = % 9d\n",
        span->max_x - span->min_x, span->max_y - span->min_y);
    printf("comp.(min_x)        = % 9d   comp.(min_y)        = % 9d\n",
        (int)floorf((float)ftglyph->metrics.horiBearingX/64.0f),
        (int)floorf((float)(ftglyph->metrics.horiBearingY -
            ftglyph->metrics.height)/64.0f));
    printf("comp.(max_x)        = % 9d   comp.(max_y)        = % 9d\n",
        (int)ceilf((float)(ftglyph->metrics.horiBearingX +
            ftglyph->metrics.width)/64.0f),
        (int)ceilf((float)(ftglyph->metrics.horiBearingY)/64.0f - 1.0f));
    printf("\n");
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
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  -f, --font <ttf-file>  font file (default %s)\n"
        "  -s, --size <points>    font size (default %d)\n"
        "  -t, --text <string>    text to render (default \"%s\")\n"
        "  -b, --block            render with unicode block characters\n"
        "  -d, --debug            print debugging information\n"
        "  -h, --help             command line help\n",
        argv[0], font_path, font_size, render_text);
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
        if (match_opt(argv[i], "-d","--debug")) {
            debug = true;
            i++;
        }
        else if (match_opt(argv[i], "-f","--file")) {
            if (check_param(++i == argc, "--file")) break;
            font_path = argv[i++];
        }
        else if (match_opt(argv[i], "-s", "--size")) {
            if (check_param(++i == argc, "--size")) break;
            font_size = atoi(argv[i++]);
        }
        else if (match_opt(argv[i], "-t", "--text")) {
            if (check_param(++i == argc, "--font-size")) break;
            render_text = argv[i++];
        } else if (match_opt(argv[i], "-b", "--block")) {
            use_block = true;
            i++;
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

    if (debug) {
        print_face(ftface);
    }

    hbfont  = hb_ft_font_create(ftface, NULL);
    hblang = hb_language_from_string(text_lang, strlen(text_lang));

    hb_buffer_t *buf = hb_buffer_create();
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hblang);
    hb_buffer_add_utf8(buf, render_text, strlen(render_text), 0, strlen(render_text));

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
    rp.gray_spans = span_vector_fn;

    size_t width = 0;
    for (size_t i = 0; i < glyph_count; i++) {
        width += glyph_pos[i].x_advance/64;
    }

    /* set global offset and size the render buffer */
    span.global_x = 0;
    span.global_y = -ftface->size->metrics.descender/64;
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

        span.offset_x = glyph_pos[i].x_offset/64;
        span.offset_y = glyph_pos[i].y_offset/64;

        if ((fterr = FT_Outline_Render(ftlib, &ftface->glyph->outline, &rp))) {
            printf("error: FT_Outline_Render failed: fterr=%d\n", fterr);
            exit(1);
        }

        if (debug) {
            print_glyph(ftface->glyph, glyph_info[i].codepoint, &span);
        }

        span.global_x += glyph_pos[i].x_advance/64;
        span.global_y += glyph_pos[i].y_advance/64;
    }

    /* display rendered output */
    if (use_block) {
        render_block(&span);
    } else {
        render_ascii(&span);
    }

    hb_buffer_destroy(buf);
    hb_font_destroy(hbfont);
    FT_Done_Face(ftface);
    FT_Done_Library(ftlib);
}
