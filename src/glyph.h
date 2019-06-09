#pragma once

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
 * Font Atlas Key
 *
 * Holds the details for a key in the Font Atlas glyph map.
 */
struct atlas_key
{
    uint64_t opaque;

    atlas_key() = default;
    atlas_key(int64_t font_id, int64_t font_size, int64_t glyph);

    bool operator<(const atlas_key &o) const { return opaque < o.opaque; }

    int font_id() const;
    int font_size() const;
    int glyph() const;
};

inline atlas_key::atlas_key(int64_t font_id, int64_t font_size, int64_t glyph) :
    opaque(glyph | (font_size << 20) | (font_id << 40)) {}

inline int atlas_key::font_id() const { return (opaque >> 40) & ((1 << 20)-1); }
inline int atlas_key::font_size() const { return (opaque >> 20) & ((1 << 20)-1); }
inline int atlas_key::glyph() const { return opaque & ((1 << 20)-1); }

/*
 * Font Atlas Entry
 *
 * Holds the details for an entry in the Font Atlas glyph map.
 */
struct atlas_entry
{
    int bin_id;
    short x, y, ox, oy, w, h;
    float uv[4];

    atlas_entry() = default;
    atlas_entry(int bin_id, int x, int y, int ox, int oy, int w, int h,
        float uv[4]);
};

inline atlas_entry::atlas_entry(int bin_id, int x, int y,
    int ox, int oy, int w, int h, float uv[4]) : bin_id(bin_id),
    x(x), y(y), ox(ox), oy(oy), w(w), h(h), uv{uv[0], uv[1], uv[2], uv[3]} {}

/*
 * Font Atlas
 *
 * Font Atlas implementation that uses the MaxRext-BSSF bin packer.
 * The Font Atlas is graphics API agnostic. It is necessary for client
 * code to update an atlas texture after text has been rendered.
 */
struct font_atlas
{
    size_t width, height, depth;
    std::map<atlas_key,atlas_entry> glyph_map;
    std::vector<uint8_t> pixels;
    float uv1x1;
    bin_packer bp;
    bin_rect delta;

    static const int PADDING = 1;
    static const int DEFAULT_WIDTH = 2048;
    static const int DEFAULT_HEIGHT = 2048;
    static const int DEFAULT_DEPTH = 1;

    font_atlas();
    font_atlas(size_t width, size_t height, size_t depth);

    void init();
    void reset(size_t width, size_t height, size_t depth);

    atlas_entry* lookup(int font_id, int font_size, int glyph);
    atlas_entry* create(int font_id, int font_size, int glyph,
        int ox, int oy, int w, int h);

    /* tracking minimum required update rectangle */
    bin_rect get_delta();
    void expand_delta(bin_rect b);

    /* persistance */
    void save_map(FILE *out);
    void load_map(FILE *in);
    void save(std::string basename);
    void load(std::string basename);
};

/*
 * Text Segment
 *
 * Structure to hold a segment of text that uses the same font, font size,
 * and color. Also contains coordinates used by the Text Renderer when
 * outputting vertices.
 */
struct text_segment
{
    std::string text;
    std::string language;
    font_face *face;
    int font_size;
    int x, y;
    int baseline_shift;
    int line_spacing;
    int tracking;
    uint32_t color;

    text_segment() = default;
    text_segment(std::string text, std::string languag);
    text_segment(std::string text, std::string language, font_face *face,
        int font_size, int x, int y, uint32_t color);
};

inline text_segment::text_segment(std::string text, std::string language) :
    text(text), language(language), face(nullptr), font_size(0),
    x(0), y(0), baseline_shift(0), line_spacing(0), tracking(0), color(0) {}

inline text_segment::text_segment(std::string text, std::string language,
    font_face *face, int font_size, int x, int y, uint32_t color) :
    text(text), language(language), face(face), font_size(font_size),
    x(x), y(y), baseline_shift(0), line_spacing(0), tracking(0), color(color) {}

/*
 * Glyph Shape
 *
 * Structure to hold the output of the text shaper.
 */
struct glyph_shape
{
    unsigned glyph;           /* glyph index for the chosen font */
    unsigned cluster;         /* offset within original string */
    int x_offset, y_offset;   /* integer with 6 fraction bits */
    int x_advance, y_advance; /* integer with 6 fraction bits */
};

/*
 * Text Shapers
 *
 * The shaper takes the font face, font size and text fromt a Text Segment
 * and calculates the position of each character. The Text Shaper output is
 * fed into the Text Renderer. This decomposition allows measurement of a
 * text segment before rendering it. Thhere are multiple implementations:
 *
 * - text_shaper_ft - FreeType based text shaper with round to integer kerning.
 *                    (suitable for monospaced fonts)
 * - text_shaper_hb - HarfBuzz based text shaper with round to integer kerning.
 *                    (sutable for any fonts)
 *
 * Note: the Freetype Shaper is not measurably faster than the HarfBuzz shaper,
 * although it could be made faster with google dense hashmap. The main benefit
 * is that it elminates a dependency.
 */

struct text_shaper
{
    virtual void shape(std::vector<glyph_shape> &shapes, text_segment *segment) = 0;
};

struct text_shaper_ft : text_shaper
{
    virtual void shape(std::vector<glyph_shape> &shapes, text_segment *segment);
};

struct text_shaper_hb : text_shaper
{
    virtual void shape(std::vector<glyph_shape> &shapes, text_segment *segment);
};

/*
 * Text Vertex
 *
 * Structure to hold the vertex output of the text renderer.
 */
struct text_vertex
{
    float pos[3];
    float uv[2];
    uint32_t rgba;
};

/*
 * Glyph Renderer
 *
 * Implementation of a simple freetype based glyph renderer. The output
 * of the glyph renderer is a bitmap which is stored in a font atlas.
 */
struct glyph_renderer
{
    font_manager_ft* manager;
    font_atlas* atlas;
    span_vector span;

    glyph_renderer(font_manager_ft* manager, font_atlas* atlas) :
        manager(manager), atlas(atlas) {}

    atlas_entry* render(font_face_ft *face, int font_size, int glyph);
};

/*
 * Text Renderer
 *
 * Implementation of a simple freetype based text renderer. The output
 * of the text renderer is an array of vertices and triangle indices
 * The output is graphics API agnostic and can be utilized by DirectX,
 * OpenGL, Metal or Vulkan.
 */
struct text_renderer
{
    font_manager_ft* manager;
    font_atlas* atlas;
    glyph_renderer renderer;

    text_renderer(font_manager_ft* manager, font_atlas* atlas) :
        manager(manager), atlas(atlas), renderer(manager, atlas) {}

    void render(std::vector<text_vertex> &vertices,
        std::vector<uint32_t> &indices,
        std::vector<glyph_shape> &shapes,
        text_segment *segment);
};