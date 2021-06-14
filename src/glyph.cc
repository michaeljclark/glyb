// See LICENSE for license details.

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

#include <hb.h>
#include <hb-ft.h>

#include "binpack.h"
#include "utf8.h"
#include "image.h"
#include "draw.h"
#include "font.h"
#include "glyph.h"
#include "file.h"
#include "logger.h"


/*
 * freetype span rasterization
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
 * font atlas
 */

font_atlas::font_atlas() :
    font_atlas(font_atlas::DEFAULT_WIDTH, font_atlas::DEFAULT_HEIGHT,
    font_atlas::GRAY_DEPTH) {}

font_atlas::font_atlas(size_t width, size_t height, size_t depth) :
    width(width), height(height), depth(depth),
    glyph_map(), pixels(nullptr), uv1x1(1.0f / (float)width),
    bp(bin_point((int)width, (int)height)),
    delta(bin_point((int)width,(int)height),bin_point(0,0)),
    multithreading(false), mutex()
{
    if (width && height && depth) {
        create_pixels();
        clear_pixels();
        uv_pixel();
    }
}

font_atlas::~font_atlas()
{
    if (pixels) {
        delete [] pixels;
        pixels = nullptr;
    }
}

image* font_atlas::get_image()
{
    return img.get();
}

void font_atlas::create_pixels()
{
    /* reserve 0x0 - 1x1 with padding */
    bp.find_region(0, bin_point(2,2));

    /* clear bitmap */
    if (pixels) {
        delete [] pixels;
    }
    pixels = new uint8_t[width * height * depth];

    /* create image handle */
    switch (depth) {
    case 1: img = std::shared_ptr<image>(new image(file_ptr(),
        (uint)width, (uint)height, pixel_format_alpha, pixels)); break;
    case 4: img = std::shared_ptr<image>(new image(file_ptr(),
        (uint)width, (uint)height, pixel_format_rgba, pixels)); break;
    }
}

void font_atlas::clear_pixels()
{
    switch (depth) {
    case 4:
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                //*(uint32_t*)&pixels[(y * width + x) * depth] = 0xff000000;
                *(uint32_t*)&pixels[(y * width + x) * depth] = 0x00000000;
            }
        }
        break;
    default:
        memset(pixels, 0, width * height * depth);
    }
}

void font_atlas::uv_pixel()
{
    uv1x1 = 1.0f / (float)width;

    switch (depth) {
    case 1:
        pixels[0] = 0xff;
        break;
    case 4:
        pixels[0] = 0xff;
        pixels[1] = 0xff;
        pixels[2] = 0xff;
        pixels[3] = 0xff;
        break;
    }
}

void font_atlas::reset_bins()
{
    bp.set_bin_size(bin_point((int)width,(int)height));
    delta = bin_rect(bin_point((int)width,(int)height),bin_point(0,0));
    glyph_map.clear();
}

void font_atlas::reset(size_t width, size_t height, size_t depth)
{
    if (this->width == width &&
        this->height == height &&
        this->depth == depth) {
        return;
    }

    this->width = width;
    this->height = height;
    this->depth = depth;

    reset_bins();
    create_pixels();
    clear_pixels();
    uv_pixel();
}

atlas_entry font_atlas::create(font_face *face, int font_size, int glyph,
    int entry_font_size, int ox, int oy, int w, int h)
{
    float uv[4];
    atlas_entry ae;

    if (multithreading) {
        mutex.lock();
    }

    int bin_id = (int)glyph_map.size();
    auto r = bp.find_region(bin_id, bin_point(w + PADDING , h + PADDING));
    if (!r.first) {
        if (multithreading) {
            mutex.unlock();
        }
        return atlas_entry(-1); /* atlas full */
    }

    /* track minimum update rectangle */
    expand_delta(r.second);

    /* create uv coordinates */
    create_uvs(uv, r.second);

    /* insert into glyph_map */
    auto a = r.second.a;
    auto gi = glyph_map.insert(glyph_map.end(),
        std::pair<glyph_key,atlas_entry>({face->font_id, font_size, glyph},
            {bin_id, entry_font_size, a.x, a.y, ox, oy, w, h, uv}));

    ae = gi->second;

    if (multithreading) {
        mutex.unlock();
    }

    return ae;
}

void font_atlas::create_uvs(float uv[4], bin_rect r)
{
    float x1 = r.a.x,       y1 = r.a.y;
    float x2 = r.b.x - 1.0, y2 = r.b.y - 1.0;

    uv[0] = x1/width;
    uv[1] = y2/width;
    uv[2] = x2/width;
    uv[3] = y1/width;
}

void font_atlas::expand_delta(bin_rect b)
{
    /*
     * expand the delta rectangle.
     *
     * This interface is called after find_region with each newly
     * allocated region, to keep track of the minimum update rectangle.
     */
    delta.a.x = std::min(delta.a.x,b.a.x);
    delta.a.y = std::min(delta.a.y,b.a.y);
    delta.b.x = std::max(delta.b.x,b.b.x);
    delta.b.y = std::max(delta.b.y,b.b.y);
}

bin_rect font_atlas::get_delta()
{
    /*
     * return the dela rectangle, and reset it to its smallest value.
     *
     * This interface is called to get the smalled possible update
     * rectangle to use with APIs such as glTexSubImage2D.
     */
    bin_rect r = delta;
    delta = bin_rect(bin_point((int)width,(int)height),bin_point(0,0));
    return r;
}

atlas_entry font_atlas::resize(font_face *face, int font_size, int glyph,
    atlas_entry *tmpl)
{
    float scale = (float)font_size / tmpl->font_size;
    auto gi = glyph_map.insert(glyph_map.end(),
        std::pair<glyph_key,atlas_entry>(
            { face->font_id, font_size, glyph},
            { tmpl->bin_id, font_size, tmpl->x, tmpl->y,
              (short)roundf((float)tmpl->ox * scale),
              (short)roundf((float)tmpl->oy * scale),
              (short)roundf((float)tmpl->w * scale),
              (short)roundf((float)tmpl->h * scale),
              tmpl->uv }));
    return gi->second;
}

atlas_entry font_atlas::lookup(font_face *face, int font_size, int glyph,
    glyph_renderer *renderer)
{
    atlas_entry ae;

    /*
     * lookup atlas to see if the glyph is in the atlas
     */
    auto gi = glyph_map.find({face->font_id, font_size, glyph});
    if (gi != glyph_map.end()) {
        return gi->second;
    }

    /*
     * lookup index with font size 0 which is used for variable
     * sized entries created by signed distance field renderers.
     */
    gi = glyph_map.find({face->font_id, 0, glyph});
    if (gi != glyph_map.end()) {
        return resize(face, font_size, glyph, &gi->second);
    }

    /*
     * rendering a glyph may create a variable size entry, so
     * we check that we got the font size that we requested.
     */
    ae = renderer->render(this, static_cast<font_face_ft*>(face),
        font_size, glyph);
    if (ae.font_size != font_size) {
        return resize(face, font_size, glyph, &ae);
    } else {
        return ae;
    }
}

std::string font_atlas::get_path(font_face *face, file_type type)
{
    switch (type) {
    case csv_file: return face->path + ".atlas.csv";
    case png_file: return face->path + ".atlas.png";
    case ttf_file:
    default: return face->path;
    }
}

#define FLOAT32 "%.9g"

void font_atlas::save_map(font_manager *manager, font_face *face, FILE *out)
{
    std::vector<glyph_key> v;
    for(auto i = glyph_map.begin(); i != glyph_map.end(); i++) {
        v.push_back(i->first);
    }
    sort(v.begin(), v.end(), [&](const glyph_key &a, const glyph_key &b)
        -> bool { return glyph_map[a].bin_id < glyph_map[b].bin_id; });
    for (auto &k : v) {
        auto i = glyph_map.find(k);
        const glyph_key &key = i->first;
        const atlas_entry &ent = i->second;
        fprintf(out, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            ent.bin_id, key.glyph(),
            ent.font_size, ent.x, ent.y, ent.ox, ent.oy, ent.w, ent.h);
    }
}

void font_atlas::load_map(font_manager *manager, font_face *face, FILE *in)
{
    const int num_fields = 9;
    int ret;
    do {
        int glyph;
        atlas_entry ent;
        ret = fscanf(in, "%d,%d,%d,%hd,%hd,%hd,%hd,%hd,%hd\n",
            &ent.bin_id, &glyph, &ent.font_size,
            &ent.x, &ent.y, &ent.ox, &ent.oy, &ent.w, &ent.h);
        if (ret == num_fields) {
            bin_rect r(bin_point(ent.x,ent.y),
                bin_point(ent.x+ent.w+1,ent.y+ent.h+1));
            create_uvs(ent.uv, r);
            bp.create_explicit(ent.bin_id, r);
            auto gi = glyph_map.insert(glyph_map.end(),
                std::pair<glyph_key,atlas_entry>({face->font_id, 0, glyph}, ent));
        }
    } while (ret == num_fields);
}

void font_atlas::save(font_manager *manager, font_face *face)
{
    std::string img_path = get_path(face, png_file);
    std::string csv_path = get_path(face, csv_file);
    FILE *fcsv = fopen(csv_path.c_str(), "w");
    if (fcsv == nullptr) {
        Error("error: fopen: %s: %s\n", csv_path.c_str(), strerror(errno));
        exit(1);
    }
    save_map(manager, face, fcsv);
    fclose(fcsv);
    image::saveToFile(img_path, img);
}

void font_atlas::load(font_manager *manager, font_face *face)
{
    std::string img_path = get_path(face, png_file);
    std::string csv_path = get_path(face, csv_file);
    if (!file::fileExists(img_path) || !file::fileExists(csv_path)) {
        return;
    }
    FILE *fcsv = fopen(csv_path.c_str(), "r");
    if (fcsv == nullptr) {
        Error("error: fopen: %s: %s\n", csv_path.c_str(), strerror(errno));
        exit(1);
    }
    image_ptr load_img = image::createFromFile(img_path);
    if (!load_img) {
        return;
    }
    img = load_img;
    pixels = img->move();
    width = img->getWidth();
    height = img->getHeight();
    depth = img->getBytesPerPixel();
    reset_bins();
    uv_pixel();
    load_map(manager, face, fcsv);
    fclose(fcsv);
}


/*
 * text shaper (FreeType)
 */

void text_shaper_ft::shape(std::vector<glyph_shape> &shapes,
    text_segment &segment)
{
    font_face_ft *face = static_cast<font_face_ft*>(segment.face);
    FT_Face ftface = face->ftface;
    FT_GlyphSlot ftglyph = ftface->glyph;
    FT_Error fterr;

    /* we need to set up our font metrics */
    face->get_metrics(segment.font_size);

    /* shape text with FreeType (does not apply glyph-glyph kerning) */
    const char* text = segment.text.c_str();
    size_t text_len = segment.text.size();
    for (size_t i = 0; i < text_len; i += utf8_codelen(text + i)) {

        uint32_t codepoint = utf8_to_utf32(text + i);
        uint32_t glyph = FT_Get_Char_Index(ftface, codepoint);

        if (ftface->num_fixed_sizes > 0) {
            //FT_Bitmap_Size *bsize = &ftface->available_sizes[ftface->num_fixed_sizes-1];
            //bsize->width; bsize->height;
            shapes.push_back({
                glyph, (unsigned)i,
                0,
                0,
                segment.font_size * 10 / 8,
                0
            });
        } else {
            if ((fterr = FT_Load_Glyph(ftface, glyph,
                FT_LOAD_NO_BITMAP | FT_LOAD_COMPUTE_METRICS | FT_LOAD_NO_HINTING))) {
                Error("error: FT_Load_Glyph failed: glyph=%d fterr=%d\n",
                    glyph, fterr);
                return;
            }
            if (ftface->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
                Error("error: FT_Load_Glyph format is not outline: format=\n",
                    ftface->glyph->format);
                return;
            }

            shapes.push_back({
                glyph, (unsigned)i,
                (int)ftglyph->metrics.horiBearingX,
                0,
                (int)ftglyph->advance.x,
                (int)ftglyph->advance.y
            });
        }
    }
}

/*
 * text shaper (HarfBuff)
 */

struct shape_data
{
    uint glyph;
    int x_offset;
    int y_offset;
    int x_advance;
    int y_advance;
};

void text_shaper_hb::shape(std::vector<glyph_shape> &shapes, text_segment &segment)
{
    font_face_ft *face = static_cast<font_face_ft*>(segment.face);

    hb_font_t *hbfont;
    hb_language_t hblang;
    hb_glyph_info_t *glyph_info;
    hb_glyph_position_t *glyph_pos;
    unsigned glyph_count;

    /* we need to set up font metrics */
    face->get_metrics(segment.font_size);

    /* get text to render */
    const char* text = segment.text.c_str();
    size_t text_len = segment.text.size();

    /* get font and language */
    hbfont = face->get_hbfont(segment.font_size);
    hblang = hb_language_from_string(segment.language.c_str(),
        (int)segment.language.size());

    /* create text buffers */
    hb_buffer_t *buf = hb_buffer_create();
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hblang);
    hb_buffer_add_utf8(buf, text, (int)text_len, 0, (int)text_len);

    /* shape text with HarfBuzz */
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
 * glyph renderer (outlines)
 */

atlas_entry glyph_renderer_outline_ft::render(font_atlas *atlas, font_face_ft *face,
    int font_size, int glyph)
{
    FT_Library ftlib;
    FT_Face ftface;
    FT_Error fterr;
    FT_GlyphSlot ftglyph;
    FT_Raster_Params rp;
    int ox, oy, w, h;
    atlas_entry ae;

    /* freetype library and glyph pointers */
    ftface = face->ftface;
    ftglyph = ftface->glyph;
    ftlib = ftglyph->library;

    /* we need to set up our font metrics */
    face->get_metrics(font_size);

    /* load glyph */
    if ((fterr = FT_Load_Glyph(ftface, glyph, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING))) {
        Error("error: FT_Load_Glyph failed: glyph=%d fterr=%d\n",
            glyph, fterr);
        return atlas_entry(-1);
    }
    if (ftface->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
        Error("error: FT_Load_Glyph format is not outline: format=\n",
            ftface->glyph->format);
        return atlas_entry(-1);
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
        return atlas_entry(-1);
    }

    if (span.min_x == INT_MAX && span.min_y == INT_MAX) {
        /* create atlas entry for white space glyph with zero dimensions */
        ae = atlas->create(face, font_size, glyph, font_size, 0, 0, 0, 0);
    } else {
        /* create atlas entry for glyph using dimensions from span */
        ae = atlas->create(face, font_size, glyph, font_size, ox, oy, w, h);

        /* copy pixels from span to atlas */
        if (ae.bin_id >= 0) {
            for (int i = 0; i < span.h; i++) {
                size_t src = i * span.w;
                size_t dst = (ae.y + i) * atlas->width + ae.x;
                memcpy(&atlas->pixels[dst], &span.pixels[src], span.w);
            }
        }
    }

    return ae;
}

/*
 * glyph renderer (bitmap)
 */

atlas_entry glyph_renderer_color_ft::render(font_atlas *atlas, font_face_ft *face,
    int font_size, int glyph)
{
    FT_Library ftlib;
    FT_Face ftface;
    FT_Error fterr;
    FT_GlyphSlot ftglyph;
    FT_Raster_Params rp;
    FT_Bitmap *bitmap;
    int ox, oy, w, h;
    atlas_entry ae;
    FT_Size_Metrics* metrics;

    /* freetype library and glyph pointers */
    ftface = face->ftface;
    ftglyph = ftface->glyph;
    ftlib = ftglyph->library;
    bitmap = &ftglyph->bitmap;
    metrics = &ftface->size->metrics;

    /* we need to set up our font metrics */
    face->get_metrics(font_size);

    /* get glyph bitmap size */
    float bitmap_size = 0;
    if (ftface->num_fixed_sizes > 0) {
        if ((fterr = FT_Load_Glyph(ftface, glyph, FT_LOAD_COLOR | FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING))) {
            Error("error: FT_Load_Glyph failed: glyph=%d fterr=%d\n",
                glyph, fterr);
            return atlas_entry(-1);
        }
        FT_Bitmap_Size *bsize = &ftface->available_sizes[ftface->num_fixed_sizes-1];
        FT_Set_Pixel_Sizes(ftface, bsize->x_ppem/64, bsize->y_ppem/64);
        bitmap_size = bsize->x_ppem;
    }

    /* load glyph */
    if ((fterr = FT_Load_Glyph(ftface, glyph, FT_LOAD_COLOR))) {
        Error("error: FT_Load_Glyph failed: glyph=%d fterr=%d\n",
            glyph, fterr);
        return atlas_entry(-1);
    }

    /* render glyph */
    if ((fterr = FT_Render_Glyph(ftglyph, FT_RENDER_MODE_NORMAL))) {
        Error("error: FT_Render_Glyph error: fterr=%d\n", fterr);
        return atlas_entry(-1);
    }

    /* font dimensions */
    ox = ftglyph->bitmap_left;
    oy = ftglyph->bitmap_top;
    h = bitmap->rows;
    w = bitmap->width;

    //printf("[% 5d]\t+%d+%d %dx%d\n", glyph, ox, oy, w, h);

    /* create atlas entry for glyph using dimensions from bitmap */
    if (bitmap_size > 0) {
        ae = atlas->create(face, 0, glyph, bitmap_size, ox, oy-h, w, h);
    } else {
        ae = atlas->create(face, font_size, glyph, font_size, ox, oy-h, w, h);
    }

    /* copy pixels from bitmap to atlas  */
    if (ae.bin_id >= 0) {
        int bit_depth;
        switch (bitmap->pixel_mode) {
        case FT_PIXEL_MODE_MONO: bit_depth = 1; break;
        case FT_PIXEL_MODE_GRAY:
        case FT_PIXEL_MODE_LCD: bit_depth = 8; break;
        case FT_PIXEL_MODE_BGRA: bit_depth = 32; break;
        default: abort();
        }
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                size_t src = (h-i-1) * bitmap->pitch + j * (bit_depth>>3);
                size_t dst = (ae.y + i) * atlas->width * atlas->depth + (ae.x + j) * atlas->depth;
                uint32_t rgba = 0xffffffff;
                switch (bit_depth) {
                case 1:
                    rgba = ((bitmap->buffer[src] >> (j & 7)) & 1)
                            ? 0xffffffff : 0x00000000;
                    break;
                case 8:
                    rgba = ((uint32_t)bitmap->buffer[src] << 24) |
                           ((uint32_t)bitmap->buffer[src] << 16) |
                           ((uint32_t)bitmap->buffer[src] << 8) |
                           (uint32_t)bitmap->buffer[src];
                    break;
                case 32:
                    rgba = ((uint32_t)bitmap->buffer[src+2]) |
                           ((uint32_t)bitmap->buffer[src+1] << 8) |
                           ((uint32_t)bitmap->buffer[src+0] << 16) |
                           ((uint32_t)bitmap->buffer[src+3] << 24);
                    break;
                }
                switch (atlas->depth) {
                case 1: atlas->pixels[dst] = (uint8_t)rgba; break;
                case 4: *(uint32_t*)&atlas->pixels[dst] = rgba; break;
                }
            }
        }
    }

    return ae;
}

/*
 * text renderer
 */

void text_renderer_ft::render(draw_list &batch,
    std::vector<glyph_shape> &shapes,
    text_segment &segment, glm::mat3 m)
{
    font_face_ft *face = static_cast<font_face_ft*>(segment.face);
    //float scale = glm::determinant(m);
    float scale = (m[0][0] + m[1][1]) / 2.0f;
    int font_size = (int)roundf(segment.font_size * scale);
    float baseline_shift = segment.baseline_shift * scale;
	float tracking = segment.tracking * scale;

    /* lookup glyphs in font atlas, creating them if they don't exist */
    float dx = 0, dy = 0;
    for (auto &shape : shapes) {
        glyph_entry *ge = manager->lookup(face, font_size, shape.glyph);
        if (!ge) continue;
        /* create polygons in vertex array */
        glm::vec3 v = glm::vec3(segment.x, segment.y, 1.0f) * m;
        float x1 = v.x / v.z + ge->ox + dx + shape.x_offset/64.0f;
        float x2 = x1 + ge->w;
        float y1 = v.y / v.z - ge->oy + dy + shape.y_offset/64.0f -
            ge->h - baseline_shift;
        float y2 = y1 + ge->h;
        if (ge->w > 0 && ge->h > 0) {
            float u1 = ge->uv[0], v1 = ge->uv[1];
            float u2 = ge->uv[2], v2 = ge->uv[3];
            uint o = (int)batch.vertices.size();
            uint c = segment.color; /* black -> white for emoji */
            shape.pos[0] = {x1, y1, 0};
            shape.pos[1] = {x2, y2, 0};
            uint o0 = draw_list_vertex(batch, {{x1, y1, 0}, {u1, v1}, c});
            uint o1 = draw_list_vertex(batch, {{x2, y1, 0}, {u2, v1}, c});
            uint o2 = draw_list_vertex(batch, {{x2, y2, 0}, {u2, v2}, c});
            uint o3 = draw_list_vertex(batch, {{x1, y2, 0}, {u1, v2}, c});
            draw_list_indices(batch, ge->atlas->get_image()->iid, mode_triangles,
                ge->atlas->depth == 4 ? shader_msdf : shader_simple,
                {o0, o3, o1, o1, o3, o2});
            draw_list_image_delta(batch, ge->atlas->get_image(), ge->atlas->get_delta(),
                st_clamp | atlas_image_filter(ge->atlas));
        }
        /* TODO - 1/4th pixel glyph caching and sub-pixel advance precision */
        dx += shape.x_advance/64.0f * scale + tracking;
        dy += shape.y_advance/64.0f * scale;

        if (text_renderer::debug) {
            Debug("S U-%04d offsets=(%8.3f,%8.3f) advances=(%8.3f,%8.3f) "
                "position=(%8.3f,%8.3f)-(%8.3f,%8.3f)\n", shape.glyph,
                shape.x_offset/64.0f, shape.y_offset/64.0f,
                shape.x_advance/64.0f, shape.y_advance/64.0f,
                shape.pos[0].x, shape.pos[0].y, shape.pos[1].x, shape.pos[1].y);
        }
    }
}