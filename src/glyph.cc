#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <climits>
#include <cstring>
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
#include "font.h"
#include "glyph.h"


/*
 * freetype span rasterization
 */

void span_measure::fn(int y, int count, const FT_Span* spans, void *user)
{
    span_measure *s = static_cast<span_measure*>(user);
    s->min_y = (std::min)(s->min_y, y);
    s->max_y = (std::max)(s->max_y, y);
    for (int i = 0; i < count; i++) {
        s->min_x = (std::min)(s->min_x, (int)spans[i].x);
        s->max_x = (std::max)(s->max_x, (int)spans[i].x + spans[i].len);
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
    int dy = (std::max)((std::min)(s->gy + s->oy + y, s->h - 1), 0);
    s->min_y = (std::min)(s->min_y, y);
    s->max_y = (std::max)(s->max_y, y);
    for (int i = 0; i < count; i++) {
        s->min_x = (std::min)(s->min_x, (int)spans[i].x);
        s->max_x = (std::max)(s->max_x, (int)spans[i].x + spans[i].len);
        int dx = (std::max)((std::min)(s->gx + s->ox + spans[i].x, s->w), 0);
        int dl = (std::max)((std::min)((int)spans[i].len, s->w - dx), 0);
        if (dl > 0) {
            memset(&s->pixels[dy * s->w + dx], spans[i].coverage, dl);
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

atlas_entry* font_atlas::lookup(int font_id, int font_size, int glyph)
{
    auto gi = glyph_map.find({font_id, font_size, glyph});
    if (gi != glyph_map.end()) {
        return &gi->second;
    } else {
        return nullptr;
    }
}

atlas_entry* font_atlas::create(int font_id, int font_size, int glyph,
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
    auto a = r.second.a;
    auto gi = glyph_map.insert(glyph_map.end(),
        std::pair<atlas_key,atlas_entry>({font_id, font_size, glyph},
            {bin_id, a.x, a.y, ox, oy, w, h, uv}));

    return &gi->second;
}


/*
 * text shaper
 */

void text_shaper::shape(std::vector<glyph_shape> &shapes, text_segment *segment)
{
    font_face_ft *face = static_cast<font_face_ft*>(segment->face);
    FT_Face ftface = face->ftface;
    int font_size = segment->font_size;;

    hb_font_t *hbfont;
    hb_language_t hblang;
    hb_glyph_info_t *glyph_info;
    hb_glyph_position_t *glyph_pos;
    unsigned glyph_count;

    /* we need to set up our font metrics */
    face->get_metrics(font_size);

    /* get text to render */
    const char* text = segment->text.c_str();
    size_t text_len = segment->text.size();

    /* create text buffers */
    hbfont  = hb_ft_font_create(ftface, NULL);
    hblang = hb_language_from_string(segment->language.c_str(),
        segment->language.size());

    hb_buffer_t *buf = hb_buffer_create();
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hblang);
    hb_buffer_add_utf8(buf, text, text_len, 0, text_len);

    /* shape text */
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
 * glyph renderer
 */

atlas_entry* glyph_renderer::render(font_face_ft *face, int font_size,
    int glyph)
{
    FT_Library ftlib;
    FT_Face ftface;
    FT_Error fterr;
    FT_GlyphSlot ftglyph;
    FT_Raster_Params rp;
    int ox, oy, w, h;
    atlas_entry *ae;

    /* freetype library and glyph pointers */
    ftface = face->ftface;
    ftglyph = ftface->glyph;
    ftlib = ftglyph->library;

    /* load glyph */
    if ((fterr = FT_Load_Glyph(ftface, glyph, 0))) {
        fprintf(stderr, "error: FT_Load_Glyph failed: glyph=%d fterr=%d\n",
            glyph, fterr);
        return nullptr;
    }
    if (ftface->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
        fprintf(stderr, "error: FT_Load_Glyph format is not outline\n");
        return nullptr;
    }

    /* set up render parameters */
    rp.target = 0;
    rp.flags = FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_AA;
    rp.user = &span;
    rp.black_spans = 0;
    rp.bit_set = 0;
    rp.bit_test = 0;
    rp.gray_spans = span_vector::fn;

    /* font dimensions */
    ox = (int)floorf((float)ftglyph->metrics.horiBearingX / 64.0f) - 1;
    oy = (int)floorf((float)(ftglyph->metrics.horiBearingY -
        ftglyph->metrics.height) / 64.0f) - 1;
    w = (int)ceilf(ftglyph->metrics.width / 64.0f) + 2;
    h = (int)ceilf(ftglyph->metrics.height / 64.0f) + 2;

    /* set up span vector dimensions */
    span.gx = 0;
    span.gy = 0;
    span.ox = -ox;
    span.oy = -oy;
    span.min_x = INT_MAX;
    span.min_y = INT_MAX;
    span.max_x = INT_MIN;
    span.max_y = INT_MIN;
    span.reset(w, h);

    /* rasterize glyph */
    if ((fterr = FT_Outline_Render(ftlib, &ftface->glyph->outline, &rp))) {
        printf("error: FT_Outline_Render failed: fterr=%d\n", fterr);
        return nullptr;
    }

    if (span.min_x == INT_MAX && span.min_y == INT_MAX) {
        /* create atlas entry for white space glyph with zero dimensions */
        ae = atlas->create(face->font_id, font_size, glyph, 0, 0, 0, 0);
    } else {
        /* create atlas entry for glyph using dimensions from span */
        ae = atlas->create(face->font_id, font_size, glyph, ox, oy, w, h);
        /* copy pixels from span to atlas */
        if(ae) {
            for (int i = 0; i < span.h; i++) {
                size_t src = i * span.w;
                size_t dst = (ae->y + i) * atlas->width + ae->x;
                memcpy(&atlas->pixels[dst], &span.pixels[src], span.w);
            }
        }
    }

    return ae;
}

/*
 * text renderer
 */

void text_renderer::render(std::vector<text_vertex> &vertices,
    std::vector<uint32_t> &indices,
    std::vector<glyph_shape> &shapes,
    text_segment *segment)
{
    font_face_ft *face = static_cast<font_face_ft*>(segment->face);
    int font_size = segment->font_size;
    int baseline_shift = segment->baseline_shift;
    int tracking = segment->tracking;

    /* lookup glyphs in font atlas, creating them if they don't exist */
    float dx = 0, dy = 0;
    for (auto shape : shapes) {
        atlas_entry *ae = atlas->lookup(face->font_id, font_size, shape.glyph);
        if (!ae) {
            /* render glyph and create entry in atlas */
            if (!(ae = renderer.render(face, font_size, shape.glyph))) {
                continue;
            }
            /* apply harfbuzz offsets to the newly created entry */
            ae->ox += shape.x_offset/64;
            ae->oy += shape.y_offset/64;
        }
        /* create polygons in vertex array */
        int x1 = segment->x + ae->ox + roundf(dx);
        int x2 = x1 + ae->w;
        int y1 = segment->y - ae->oy + roundf(dy) - ae->h - baseline_shift;
        int y2 = y1 + ae->h;
        if (ae->w > 0 && ae->h > 0) {
            float x1p = 0.5f + x1, x2p = 0.5f + x2;
            float y1p = 0.5f + y1, y2p = 0.5f + y2;
            float u1 = ae->uv[0], v1 = ae->uv[1];
            float u2 = ae->uv[2], v2 = ae->uv[3];
            uint32_t o = vertices.size();
            vertices.push_back({{x1p, y1p, 0.f}, {u1, v1}, segment->color});
            vertices.push_back({{x2p, y1p, 0.f}, {u2, v1}, segment->color});
            vertices.push_back({{x2p, y2p, 0.f}, {u2, v2}, segment->color});
            vertices.push_back({{x1p, y2p, 0.f}, {u1, v2}, segment->color});
            indices.insert(indices.end(), {o+0, o+3, o+1, o+1, o+3, o+2});
        }
        /* advance */
        dx += shape.x_advance/64.0f + tracking;
        dy += shape.y_advance/64.0f;
    }
}