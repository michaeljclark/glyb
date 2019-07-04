// See LICENSE for license details.

#pragma once

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using mat3 = glm::mat3;
using mat4 = glm::mat4;

/*
 * shape
 *
 * The following shape code is derived from msdfgen/ext/import-font.cpp
 * It has been simplified and adopts a data-oriented programming approach.
 * The context output is arrays formatted suitably to download to a GPU.
 */

enum EdgeType {
    Linear = 2,
    Quadratic = 3,
    Cubic = 4,
    Rectangle = 5,
    Circle = 6,
    Ellipse = 7,
    RoundedRectangle = 8,
};

struct Edge {
    float type;
    vec2 p[4];
};

struct Contour {
    float edge_offset, edge_count;
};

struct Shape {
    float contour_offset, contour_count, edge_offset, edge_count;
    vec2 offset, size;
};

struct Context {
    std::vector<Shape> shapes;
    std::vector<Contour> contours;
    std::vector<Edge> edges;
    vec2 pos;

    void clear();
    int newShape(vec2 offset, vec2 size);
    int newContour();
    int newEdge(Edge e);
    int newShape(Shape *shape, Edge *edges);

    bool shapeEquals(Shape *s0, Edge *e0, Shape *s1, Edge *e1);
    int findShape(Shape *s, Edge *e);
    int addShape(Shape *s, Edge *e);
    bool updateShape(int shape_num, Shape *s, Edge *e);
};

int make_glyph(Context *ctx, FT_Face ftface, int sz, int dpi, int glyph);
void print_shape(Context &ctx, int shape);

int make_rectangle(Context &ctx, draw_list &batch, vec2 pos, vec2 halfSize,
    float padding, float z, uint32_t c);
int make_rounded_rectangle(Context &ctx, draw_list &batch, vec2 pos,
    vec2 halfSize, float radius, float padding, float z, uint32_t c);
int make_circle(Context &ctx, draw_list &batch, vec2 pos, float radius,
    float padding, float z, uint32_t c);
int make_ellipse(Context &ctx, draw_list &batch, vec2 pos, vec2 radius,
    float padding, float z, uint32_t c);

int update_rectangle(int shape_num, Context &ctx, draw_list &batch,
    vec2 pos, vec2 halfSize, float padding, float z, uint32_t c);
int update_rounded_rectangle(int shape_num, Context &ctx, draw_list &batch,
    vec2 pos, vec2 halfSize, float radius, float padding, float z, uint32_t c);
int update_circle(int shape_num, Context &ctx, draw_list &batch, vec2 pos,
    float radius, float padding, float z, uint32_t c);
int update_ellipse(int shape_num, Context &ctx, draw_list &batch, vec2 pos,
    vec2 radius, float padding, float z, uint32_t c);

/*
 * text renderer
 */

struct text_renderer_canvas : text_renderer
{
    Context &ctx;
    std::map<int,int> &glyph_map;

    text_renderer_canvas(Context &ctx, std::map<int,int> &glyph_map);
    virtual ~text_renderer_canvas() = default;

    virtual void render(draw_list &batch,
        std::vector<glyph_shape> &shapes,
        text_segment *segment);
};

inline text_renderer_canvas::text_renderer_canvas(Context &ctx,
    std::map<int,int> &glyph_map) : ctx(ctx), glyph_map(glyph_map) {}