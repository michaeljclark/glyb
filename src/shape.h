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

    int newShape(vec2 offset, vec2 size) {
        int shape_num = (int)shapes.size();
        shapes.emplace_back(Shape{(float)contours.size(), 0,
            (float)edges.size(), 0, offset, size });
        return shape_num;
    }
    int newContour() {
        int contour_num = (int)contours.size();
        contours.emplace_back(Contour{(float)edges.size(), 0});
        shapes.back().contour_count++;
        return contour_num;
    }
    int newEdge(Edge&& e) {
        int edge_num = (int)edges.size();
        edges.emplace_back(e);
        contours.back().edge_count++;
        shapes.back().edge_count++;
        return edge_num;
    }
    int newCircle(vec2 center, float radius, float padding) {
        int shape_num = newShape(vec2(0), vec2((radius+padding)*2.0f));
        edges.emplace_back(Edge{Circle,{center + padding, vec2(radius)}});
        shapes.back().edge_count++;
        return shape_num;
    }
    int newEllipse(vec2 center, vec2 radius, float padding) {
        int shape_num = newShape(vec2(0), (radius+padding)*2.0f);
        edges.emplace_back(Edge{Ellipse,{center + padding, radius}});
        shapes.back().edge_count++;
        return shape_num;
    }
    int newShape(EdgeType edge_type, vec2 offset, vec2 size, vec2 p0,
            vec2 p1 = vec2(0), vec2 p2 = vec2(0), vec2 p3 = vec2(0)) {
        int shape_num = newShape(offset, size);
        edges.emplace_back(Edge{(float)edge_type,{p0, p1, p2, p3}});
        shapes.back().edge_count++;
        return shape_num;
    }
};

void load_glyph(Context *ctx, FT_Face ftface, int sz, int dpi, int glyph);
void print_shape(Context &ctx, int shape);
void rectangle(Context &ctx, draw_list &batch, vec2 pos, vec2 halfSize,
    float padding, uint32_t c);
void rounded_rectangle(Context &ctx, draw_list &batch, vec2 pos, vec2 halfSize,
    float radius, float padding, uint32_t c);
void circle(Context &ctx, draw_list &batch, vec2 pos, float radius,
    float padding, uint32_t c);
void ellipse(Context &ctx, draw_list &batch, vec2 pos, vec2 radius,
    float padding, uint32_t c);

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