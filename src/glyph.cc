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

/*
 * locals
 */

static const int font_dpi = 72;
static const char* text_lang = "en";


/*
 * font manger
 */

font_manager::font_manager()
{
    FT_Error fterr;
    if ((fterr = FT_Init_FreeType(&ftlib))) {
        fprintf(stderr, "error: FT_Init_FreeType failed: fterr=%d\n", fterr);
        exit(1);
    }
}

font_manager::~font_manager()
{
    FT_Done_Library(ftlib);
}

font_face* font_manager::lookup_font(std::string path)
{
    FT_Error fterr;
    FT_Face ftface;

    auto fi = path_map.find(path);
    if (fi != path_map.end()) {
        return &faces[fi->second];
    }

    if ((fterr = FT_New_Face(ftlib, path.c_str(), 0, &ftface))) {
        fprintf(stderr, "error: FT_New_Face failed: fterr=%d, path=%s\n",
            fterr, path.c_str());
        exit(1);
    }

    for (int i = 0; i < ftface->num_charmaps; i++)
        if (((ftface->charmaps[i]->platform_id == 0) &&
            (ftface->charmaps[i]->encoding_id == 3))
         || ((ftface->charmaps[i]->platform_id == 3) &&
            (ftface->charmaps[i]->encoding_id == 1))) {
        FT_Set_Charmap(ftface, ftface->charmaps[i]);
        break;
    }

    std::string font_name = FT_Get_Postscript_Name(ftface);
    font_face *face = &*faces.insert(faces.end(),
        font_face(faces.size(), ftface, path, font_name));
    path_map[path] = face->font_id;
    font_name_map[font_name] = face->font_id;

    return face;
}

font_face* font_manager::lookup_font_by_name(std::string font_name)
{
    auto fi = font_name_map.find(font_name);
    if (fi != font_name_map.end()) {
        return &faces[fi->second];
    } else {
        return nullptr;
    }
}

font_face* font_manager::lookup_font_by_id(int font_id)
{
    return &faces[font_id];
}


/*
 * span rasterization
 */

span_measure::span_measure() :
    min_x(INT_MAX), min_y(INT_MAX), max_x(INT_MIN), max_y(INT_MIN) {}

span_vector::span_vector() :
    global_x(0), global_y(0), offset_x(0), offset_y(0),
    width(0), height(0), pixels() {}

void span_vector::reset(int width, int height)
{
    pixels.clear();
    pixels.resize(width * height);
    this->width = width;
    this->height = height;
}

void span_measure_fn(int y, int count, const FT_Span* spans, void *user)
{
    span_measure *d = static_cast<span_measure*>(user);
    d->min_y = (std::min)(d->min_y, y);
    d->max_y = (std::max)(d->max_y, y);
    for (int i = 0; i < count; i++) {
        d->min_x = (std::min)(d->min_x, (int)spans[i].x);
        d->max_x = (std::max)(d->max_x, (int)spans[i].x + spans[i].len);
    }
}

void span_vector_fn(int y, int count, const FT_Span* spans, void *user)
{
    span_vector *d = static_cast<span_vector*>(user);
    int dy = (std::max)((std::min)
        (d->global_y + d->offset_y + y, d->height - 1), 0);
    d->min_y = (std::min)(d->min_y, y);
    d->max_y = (std::max)(d->max_y, y);
    for (int i = 0; i < count; i++) {
        d->min_x = (std::min)(d->min_x, (int)spans[i].x);
        d->max_x = (std::max)(d->max_x, (int)spans[i].x + spans[i].len);
        int dx = (std::max)((std::min)
            (d->global_x + d->offset_x + spans[i].x, d->width), 0);
        int dl = (std::max)((std::min)
            ((int)spans[i].len, d->width - dx), 0);
        if (dl > 0) {
            memset(&d->pixels[dy * d->width + dx], spans[i].coverage, dl);
        }
    }
}


/*
 * font atlas
 */

font_atlas::font_atlas() :
    font_atlas(font_atlas::DEFAULT_WIDTH, font_atlas::DEFAULT_HEIGHT) {}

font_atlas::font_atlas(size_t width, size_t height) :
    width(width), height(height), glyph_map(),
    pixels(), bp(bin_point(width, height)), uv1x1(1.0f / width)
{
    pixels.resize(width * height);
    bp.find_region(0, bin_point(2,2)); /* reserve 0x0 - 1x1 with padding */
    *static_cast<uint32_t*>(static_cast<void*>(&pixels[0])) = 0xffffffff;
}

atlas_entry* font_atlas::lookup(int font_id, int point_size, int glyph_index)
{
    auto gi = glyph_map.find(atlas_key(font_id, point_size, glyph_index));
    if (gi != glyph_map.end()) {
        return &gi->second;
    } else {
        return nullptr;
    }
}

atlas_entry* font_atlas::create(int font_id, int point_size, int glyph_index,
    int ox, int oy, int w, int h)
{
    int bin_id = glyph_map.size();
    auto r = bp.find_region(bin_id, bin_point(w + PADDING , h + PADDING));
    if (!r.first) {
        return nullptr; /* atlas full */
    }

    /* create uv coordinates */
    float x1 = 0.5f + r.second.a.x,     y1 = 0.5f + r.second.a.y;
    float x2 = 0.5f + r.second.b.x - 1, y2 = 0.5f + r.second.b.y - 1;
    float uv[4] = { x1/width, y2/width, x2/width, y1/width };

    /* insert into glyph_map */
    auto gi = glyph_map.insert(glyph_map.end(),
        std::pair<atlas_key,atlas_entry>(
            atlas_key(font_id, point_size, glyph_index),
            atlas_entry(bin_id, r.second.a.x, r.second.a.y, ox, oy, w, h, uv)));

    return &gi->second;
}


/*
 * text shaper
 */

void text_shaper::shape(std::vector<glyph_shape> &shapes, text_segment *segment)
{
    font_face *face = segment->face;
    int point_size = segment->point_size;;
    FT_Face ftface = face->ftface;
    FT_Size_Metrics *metrics = &face->ftface->size->metrics;

    hb_font_t *hbfont;
    hb_language_t hblang;
    hb_glyph_info_t *glyph_info;
    hb_glyph_position_t *glyph_pos;
    unsigned glyph_count;

    /* get metrics for our point size */
    int points = (int)(point_size * metrics->x_scale) / ftface->units_per_EM;
    if (metrics->x_scale != metrics->y_scale || point_size != points) {
        FT_Set_Char_Size(ftface, 0, point_size, font_dpi, font_dpi);
    }

    /* get text to render */
    const char* text = segment->text.c_str();
    size_t text_len = segment->text.size();

    /* shape text */
    hbfont  = hb_ft_font_create(ftface, NULL);
    hblang = hb_language_from_string(text_lang, strlen(text_lang));

    hb_buffer_t *buf = hb_buffer_create();
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hblang);
    hb_buffer_add_utf8(buf, text, text_len, 0, text_len);

    hb_shape(hbfont, buf, NULL, 0);
    glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

    for (size_t i = 0; i < glyph_count; i++) {
        shapes.push_back({
            glyph_info[i].codepoint, glyph_info[i].cluster,
            glyph_pos[i].x_offset, glyph_pos[i].y_offset,
            glyph_pos[i].x_advance, glyph_pos[i].y_advance
        });
    }

    hb_buffer_destroy(buf);
}

/*
 * text renderer
 */

atlas_entry* text_renderer::render_glyph(font_face *face, int point_size,
    int glyph_index)
{
    FT_Library ftlib;
    FT_Face ftface;
    FT_Error fterr;
    FT_GlyphSlot ftglyph;
    FT_Raster_Params rp;
    int offset_x, offset_y;
    int width, height;
    atlas_entry *entry;

    /* get freetype handles */
    ftlib = manager->ftlib;
    ftface = face->ftface;
    ftglyph = ftface->glyph;

    /* load glyph */
    if ((fterr = FT_Load_Glyph(ftface, glyph_index, 0))) {
        fprintf(stderr, "error: FT_Load_Glyph failed: glyph=%d fterr=%d\n",
            glyph_index, fterr);
        exit(1);
    }
    if (ftface->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
        fprintf(stderr, "error: FT_Load_Glyph format is not outline\n");
        exit(1);
    }

    /* set up render parameters */
    rp.target = 0;
    rp.flags = FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_AA;
    rp.user = &span;
    rp.black_spans = 0;
    rp.bit_set = 0;
    rp.bit_test = 0;
    rp.gray_spans = span_vector_fn;

    /* font dimensions */
    offset_x = (int)floorf((float)ftglyph->metrics.horiBearingX / 64.0f) - 1;
    offset_y = (int)floorf((float)(ftglyph->metrics.horiBearingY -
        ftglyph->metrics.height) / 64.0f) - 1;
    width = (int)ceilf(ftglyph->metrics.width / 64.0f) + 2;
    height = (int)ceilf(ftglyph->metrics.height / 64.0f) + 2;

    /* set up span vector dimensions */
    span.global_x = 0;
    span.global_y = 0;
    span.offset_x = -offset_x;
    span.offset_y = -offset_y;
    span.min_x = INT_MAX;
    span.min_y = INT_MAX;
    span.max_x = INT_MIN;
    span.max_y = INT_MIN;
    span.reset(width, height);

    /* rasterize glyph */
    if ((fterr = FT_Outline_Render(ftlib, &ftface->glyph->outline, &rp))) {
        printf("error: FT_Outline_Render failed: fterr=%d\n", fterr);
        return nullptr;
    }

    if (span.min_x == INT_MAX && span.min_y == INT_MAX) {
        /* create atlas entry for white space glyph with zero dimensions */
        entry = atlas->create(face->font_id, point_size, glyph_index,
            0, 0, 0, 0);
    } else {
        /* create atlas entry for glyph using dimensions from span */
        entry = atlas->create(face->font_id, point_size, glyph_index,
            offset_x, offset_y, width, height);
        /* copy pixels from span to atlas */
        if(entry) {
            for (int i = 0; i < span.height; i++) {
                size_t src = i * span.width;
                size_t dst = (entry->y + i) * atlas->width + entry->x;
                memcpy(&atlas->pixels[dst], &span.pixels[src], span.width);
            }
        }
    }

    return entry;
}

void text_renderer::render(std::vector<text_vertex> &vertices,
    std::vector<uint32_t> &indices,
    std::vector<glyph_shape> &shapes,
    text_segment *segment)
{
    font_face *face = segment->face;
    int point_size = segment->point_size;;
    FT_Face ftface = face->ftface;
    FT_Size_Metrics *metrics = &face->ftface->size->metrics;

    /* get metrics for our point size */
    int points = (int)(point_size * metrics->x_scale) / ftface->units_per_EM;
    if (metrics->x_scale != metrics->y_scale || point_size != points) {
        FT_Set_Char_Size(ftface, 0, point_size, font_dpi, font_dpi);
    }

    /* lookup glyphs in font atlas, creating them if they don't exist */
    int dx = 0, dy = 0;
    for (auto shape : shapes) {
        atlas_entry *entry = atlas->lookup
            (face->font_id, point_size, shape.glyph_index);
        if (!entry) {
            /* render glyph and create entry in atlas */
            if (!(entry = render_glyph(face, point_size, shape.glyph_index))) {
                continue;
            }
            /* apply harfbuzz offsets */
            entry->offset_x += shape.x_offset/64;
            entry->offset_y += shape.y_offset/64;
        }
        /* create polygons in vertex array */
        int x1 = segment->x + entry->offset_x + dx;
        int x2 = x1 + entry->width;
        int y1 = segment->y - entry->offset_y + dy - entry->height;
        int y2 = y1 + entry->height;
        if (entry->width > 0 && entry->height > 0) {
            float x1p = 0.5f + x1, x2p = 0.5f + x2;
            float y1p = 0.5f + y1, y2p = 0.5f + y2;
            float u1 = entry->uv[0], v1 = entry->uv[1];
            float u2 = entry->uv[2], v2 = entry->uv[3];
            uint32_t o = vertices.size();
            vertices.push_back({{x1p, y1p, 0.f}, {u1, v1}, segment->color});
            vertices.push_back({{x2p, y1p, 0.f}, {u2, v1}, segment->color});
            vertices.push_back({{x2p, y2p, 0.f}, {u2, v2}, segment->color});
            vertices.push_back({{x1p, y2p, 0.f}, {u1, v2}, segment->color});
            indices.insert(indices.end(), {o+0, o+3, o+1, o+1, o+3, o+2});
        }
        /* advance */
        dx += shape.x_advance/64;
        dy += shape.y_advance/64;
    }
}


/*
 * print face details
 */

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


/*
 * print glyph details
 */

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
