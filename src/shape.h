// See LICENSE for license details.

#pragma once

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat3 = glm::mat3;
using mat4 = glm::mat4;

/*
 * Accelerator shapes - API for GPU resident shape data structure
 *
 * The following shape code is derived from msdfgen/ext/import-font.cpp
 * It has been simplified and adopts a data-oriented programming approach.
 * The context output is arrays formatted suitably to download to a GPU.
 */

enum AEdgeType {
    Linear = 2,
    Quadratic = 3,
    Cubic = 4,
    Rectangle = 5,
    Circle = 6,
    Ellipse = 7,
    RoundedRectangle = 8,
};

struct AEdge {
    float type;
    vec2 p[4];
};

struct AContour {
    float edge_offset;
    float edge_count;
};

struct AShape {
    float contour_offset;
    float contour_count;
    float edge_offset;
    float edge_count;
    vec2 offset;
    vec2 size;
    float brush;
};

enum ABrushType {
    Axial = 1,
    Radial = 2,
};

struct ABrush {
    float type;
    vec2 p[4];
    vec4 c[4];
};

struct AContext {
    std::vector<AShape> shapes;
    std::vector<AContour> contours;
    std::vector<AEdge> edges;
    std::vector<ABrush> brushes;
    int brush;
    vec2 pos;

    void clear();
    int newShape(vec2 offset, vec2 size);
    int newContour();
    int newEdge(AEdge e);
    int newShape(AShape *shape, AEdge *edges);

    bool brushEquals(ABrush *b0, ABrush *b1);
    int newBrush(ABrush b);
    bool updateBrush(int brush_num, ABrush *b);
    int currentBrush();

    bool shapeEquals(AShape *s0, AEdge *e0, AShape *s1, AEdge *e1);
    int findShape(AShape *s, AEdge *e);
    int addShape(AShape *s, AEdge *e, bool dedup = true);
    bool updateShape(int shape_num, AShape *s, AEdge *e);
};

int make_glyph(AContext *ctx, FT_Face ftface, int sz, int dpi, int glyph);

void print_edge(AContext &ctx, int edge);
void print_shape(AContext &ctx, int shape);

void brush_clear(AContext &ctx);
void brush_set(AContext &ctx, int brush_num);

int make_brush_axial_gradient(AContext &ctx,
    vec2 p0, vec2 p1, color c0, color c1);
int update_brush_axial_gradient(int brush_num, AContext &ctx,
    vec2 p0, vec2 p1, color c0, color c1);

int make_rectangle(AContext &ctx, draw_list &batch, vec2 pos, vec2 halfSize,
    float padding, float z, uint32_t c);
int make_rounded_rectangle(AContext &ctx, draw_list &batch, vec2 pos,
    vec2 halfSize, float radius, float padding, float z, uint32_t c);
int make_circle(AContext &ctx, draw_list &batch, vec2 pos, float radius,
    float padding, float z, uint32_t c);
int make_ellipse(AContext &ctx, draw_list &batch, vec2 pos, vec2 radius,
    float padding, float z, uint32_t c);

int update_rectangle(int shape_num, AContext &ctx, draw_list &batch,
    vec2 pos, vec2 halfSize, float padding, float z, uint32_t c);
int update_rounded_rectangle(int shape_num, AContext &ctx, draw_list &batch,
    vec2 pos, vec2 halfSize, float radius, float padding, float z, uint32_t c);
int update_circle(int shape_num, AContext &ctx, draw_list &batch, vec2 pos,
    float radius, float padding, float z, uint32_t c);
int update_ellipse(int shape_num, AContext &ctx, draw_list &batch, vec2 pos,
    vec2 radius, float padding, float z, uint32_t c);


/*
 * text renderer
 */

struct text_renderer_canvas : text_renderer
{
    AContext &ctx;
    std::map<int,int> &glyph_map;

    text_renderer_canvas(AContext &ctx, std::map<int,int> &glyph_map);
    virtual ~text_renderer_canvas() = default;

    virtual void render(draw_list &batch,
        std::vector<glyph_shape> &shapes,
        text_segment *segment);
};

inline text_renderer_canvas::text_renderer_canvas(AContext &ctx,
    std::map<int,int> &glyph_map) : ctx(ctx), glyph_map(glyph_map) {}