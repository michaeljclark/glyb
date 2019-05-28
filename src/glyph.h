#pragma once

struct font_face
{
    int font_id;
    FT_Face ftface;
    std::string path;
    std::string name;

    font_face() = default;
    font_face(int font_id, FT_Face ftface, std::string path, std::string name) :
        font_id(font_id), ftface(ftface), path(path), name(name) {}
};

struct font_manager
{
    FT_Library ftlib;

    std::vector<font_face> faces;
    std::map<std::string,size_t> path_map;
    std::map<std::string,size_t> font_name_map;

    font_manager();
    ~font_manager();

    font_face* lookup_font(std::string path);
    font_face* lookup_font_by_name(std::string font_name);
    font_face* lookup_font_by_id(int font_id);
};

struct span_measure
{
    int min_x, min_y, max_x, max_y;

    span_measure();
};

struct span_vector : span_measure
{
    int global_x;
    int global_y;
    int offset_x;
    int offset_y;
    int width;
    int height;
    std::vector<uint8_t> pixels;

    span_vector();

    void reset(int width, int height);
};

struct atlas_key
{
    uint64_t opaque;

    atlas_key() = default;
    inline atlas_key(int64_t font_id, int64_t font_size, int64_t glyph) :
        opaque(glyph | (font_size << 20) | (font_id << 40)) {}

    bool operator<(const atlas_key &o) const { return opaque < o.opaque; }
};

struct atlas_entry
{
    int bin_id;
    short x;
    short y;
    short offset_x;
    short offset_y;
    short width;
    short height;
    float uv[4];

    atlas_entry() = default;
    inline atlas_entry(int bin_id, short x, short y,
        short offset_x, short offset_y,
        short width, short height, float uv[4]) : bin_id(bin_id), x(x), y(y),
        offset_x(offset_x), offset_y(offset_y), width(width), height(height),
        uv{uv[0], uv[1], uv[2], uv[3]} {}
};

struct font_atlas
{
    size_t width, height;
    std::map<atlas_key,atlas_entry> glyph_map;
    std::vector<uint8_t> pixels;
    bin_packer bp;
    float uv1x1;

    static const int PADDING = 1;
    static const int DEFAULT_WIDTH = 2048;
    static const int DEFAULT_HEIGHT = 2048;

    font_atlas();
    font_atlas(size_t width, size_t height);

    atlas_entry* lookup(int font_id, int font_size, int glyph);
    atlas_entry* create(int font_id, int font_size, int glyph,
        int ox, int oy, int w, int h);
};

struct text_segment
{
    std::string text;
    font_face *face;
    int font_size;
    int x, y;
    uint32_t color;

    text_segment() = default;
    text_segment(std::string text, font_face *face, int font_size,
        int x, int y, uint32_t color) : text(text), face(face),
        font_size(font_size), x(x), y(y), color(color) {}
};

struct glyph_shape
{
    unsigned glyph;           /* glyph index for the chosen font */
    unsigned cluster;         /* offset within original string */
    int x_offset, y_offset;   /* integer with 6 fraction bits */
    int x_advance, y_advance; /* integer with 6 fraction bits */
};

struct text_shaper
{
    void shape(std::vector<glyph_shape> &shapes, text_segment *segment);
};

struct text_vertex
{
    float pos[3];
    float uv[2];
    uint32_t rgba;
};

struct text_renderer
{
    font_manager* manager;
    font_atlas* atlas;
    span_vector span;

    text_renderer(font_manager* manager, font_atlas* atlas) :
        manager(manager), atlas(atlas) {}

    void render(std::vector<text_vertex> &vertices,
        std::vector<uint32_t> &indices,
        std::vector<glyph_shape> &shapes,
        text_segment *segment);

private:
    atlas_entry* render_glyph(font_face *face, int font_size, int glyph);
};

void span_measure_fn(int y, int count, const FT_Span* spans, void *user);
void span_vector_fn(int y, int count, const FT_Span* spans, void *user);

void print_face(FT_Face ftface);
void print_glyph(FT_GlyphSlot ftglyph, int codepoint, span_measure *d);
