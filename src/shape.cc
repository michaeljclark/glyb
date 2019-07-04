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

#include "glm/glm.hpp"

#include "binpack.h"
#include "image.h"
#include "utf8.h"
#include "draw.h"
#include "font.h"
#include "glyph.h"
#include "shape.h"
#include "logger.h"

/*
 * shape
 *
 * The following shape code is derived from msdfgen/ext/import-font.cpp
 * It has been simplified and adopts a data-oriented programming approach.
 * The context output is arrays formatted suitably to download to a GPU.
 */

typedef const FT_Vector ftvec;

static Context* uctx(void *u) { return static_cast<Context*>(u); }
static vec2 pt(ftvec *v) { return vec2(v->x/64.f, v->y/64.f); }

static int ftMoveTo(ftvec *p, void *u) {
    uctx(u)->newContour();
    uctx(u)->pos = pt(p);
    return 0;
}

static int ftLineTo(ftvec *p, void *u) {
    uctx(u)->newEdge(Edge{Linear, { uctx(u)->pos, pt(p) }});
    uctx(u)->pos = pt(p);
    return 0;
}

static int ftConicTo(ftvec *c, ftvec *p, void *u) {
    uctx(u)->newEdge(Edge{Quadratic, { uctx(u)->pos, pt(c), pt(p) }});
    uctx(u)->pos = pt(p);
    return 0;
}

static int ftCubicTo(ftvec *c1, ftvec *c2, ftvec *p, void *u) {
    uctx(u)->newEdge(Edge{Cubic, { uctx(u)->pos, pt(c1), pt(c2), pt(p) }});
    uctx(u)->pos = pt(p);
    return 0;
}

static vec2 offset(FT_Glyph_Metrics *m) {
    return vec2(floorf(m->horiBearingX/64.0f),
                floorf((m->horiBearingY-m->height)/64.0f));
}

static vec2 size(FT_Glyph_Metrics *m) {
    return vec2(ceilf(m->width/64.0f), ceilf(m->height/64.0f));
}

int make_glyph(Context *ctx, FT_Face ftface, int sz, int dpi, int glyph)
{
    FT_Outline_Funcs ftfuncs = { ftMoveTo, ftLineTo, ftConicTo, ftCubicTo };
    FT_Glyph_Metrics *m = &ftface->glyph->metrics;
    FT_Error fterr;

    if ((fterr = FT_Set_Char_Size(ftface, 0, sz, dpi, dpi))) {
        Panic("error: FT_Set_Char_Size failed: fterr=%d\n", fterr);
    }

    if ((fterr = FT_Load_Glyph(ftface, glyph, 0))) {
        Panic("error: FT_Load_Glyph failed: fterr=%d\n", fterr);
    }

    int shape_num = ctx->newShape(offset(m), size(m));

    if ((fterr = FT_Outline_Decompose(&ftface->glyph->outline, &ftfuncs, ctx))) {
        Panic("error: FT_Outline_Decompose failed: fterr=%d\n", fterr);
    }

    return shape_num;
}

void print_shape(Context &ctx, int shape)
{
    Shape &s = ctx.shapes[shape];
    printf("shape %d (contour count = %d, edge count = %d, "
        "offset = (%d,%d), size = (%d,%d))\n",
        shape, (int)s.contour_count, (int)s.edge_count,
        (int)s.offset.x, (int)s.offset.y, (int)s.size.x, (int)s.size.y);
    for (size_t i = 0; i < (size_t)s.contour_count; i++) {
        Contour &c = ctx.contours[(size_t)s.contour_offset + i];
        printf("  contour %zu (edge count = %d)\n", i, (int)c.edge_count);
        for (size_t j = 0; j < c.edge_count; j++) {
            Edge &e = ctx.edges[(size_t)c.edge_offset + j];
            switch ((int)e.type) {
            case EdgeType::Linear:
                printf("    edge %zu Linear (%f,%f) - (%f, %f)\n",
                    j, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y);
                break;
            case EdgeType::Quadratic:
                printf("    edge %zu Quadratic (%f,%f) - [%f, %f]"
                    " - (%f, %f)\n",
                    j, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y,
                       e.p[2].x, e.p[2].y);
                break;
            case EdgeType::Cubic:
                printf("    edge %zu Cubic (%f,%f) - [%f, %f]"
                    " - [%f, %f] - (%f, %f)\n",
                    j, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y,
                       e.p[2].x, e.p[2].y, e.p[3].x, e.p[3].y);
                break;
            }
        }
    }
}

void Context::clear()
{
    shapes.clear();
    contours.clear();
    edges.clear();
}

int Context::newShape(vec2 offset, vec2 size)
{
    int shape_num = (int)shapes.size();
    shapes.emplace_back(Shape{(float)contours.size(), 0,
        (float)edges.size(), 0, offset, size });
    return shape_num;
}

int Context::newContour()
{
    int contour_num = (int)contours.size();
    contours.emplace_back(Contour{(float)edges.size(), 0});
    shapes.back().contour_count++;
    return contour_num;
}

int Context::newEdge(Edge e)
{
    int edge_num = (int)edges.size();
    edges.push_back(e);
    if (shapes.back().contour_count > 0) {
        contours.back().edge_count++;
    }
    shapes.back().edge_count++;
    return edge_num;
}

int Context::newShape(Shape *shape, Edge *edges)
{
    int shape_num = newShape(shape->offset, shape->size);
    for (int i = 0; i < shape->edge_count; i++) {
        newEdge(edges[i]);
    }
    return shape_num;
}

static int numPoints(int edge_type)
{
    switch (edge_type) {
    case Linear:           return 2;
    case Quadratic:        return 3;
    case Cubic:            return 4;
    case Rectangle:        return 2;
    case Circle:           return 2;
    case Ellipse:          return 2;
    case RoundedRectangle: return 2;
    }
}

bool Context::shapeEquals(Shape *s0, Edge *e0, Shape *s1, Edge *e1)
{
    if (s0->edge_count != s1->edge_count) return false;
    if (s0->offset != s1->offset) return false;
    if (s0->size != s1->size) return false;
    if (e0->type != e1->type) return false;
    for (int i = 0; i < s0->edge_count; i++) {
        int match_points = numPoints(e0[i].type);
        for (int j = 0; j < match_points; j++) {
            if (e0[i].p[j] != e1[i].p[j]) return false;
        }
    }
    return true;
}

int Context::findShape(Shape *s, Edge *e)
{
    /* this is really inefficient! */
    for (size_t i = 0; i < shapes.size(); i++) {
        int j = shapes[i].edge_offset;
        if (shapeEquals(&shapes[i], &edges[j], s, e)) {
            return (int)i;
        }
    }
    return -1;
}

int Context::addShape(Shape *s, Edge *e)
{
    int shape_num = findShape(s, e);
    if (shape_num == -1) {
        shape_num = newShape(s, e);
    }
    return shape_num;
}

bool Context::updateShape(int shape_num, Shape *s, Edge *e)
{
    Shape *os = &shapes[shape_num];
    Edge *oe = &edges[shapes[shape_num].edge_offset];

    if (shapeEquals(os, oe, s, e)) {
        return false;
    }

    s->contour_offset = os->contour_offset;
    s->contour_count  = os->contour_count;
    s->edge_offset    = os->edge_offset;

    shapes[shape_num] = s[0];
    for (int i = 0; i < s->edge_count; i++) {
        edges[i] = e[i];
    }

    return true;
}

/*
 * draw list utility
 */

static void rect(draw_list &b, uint iid, vec2 A, vec2 B, float Z,
    vec2 UV0, vec2 UV1, uint c, float m)
{
    uint o = static_cast<uint>(b.vertices.size());

    uint o0 = draw_list_vertex(b, {{A.x, A.y, Z}, {UV0.x, UV0.y}, c, m});
    uint o1 = draw_list_vertex(b, {{B.x, A.y, Z}, {UV1.x, UV0.y}, c, m});
    uint o2 = draw_list_vertex(b, {{B.x, B.y, Z}, {UV1.x, UV1.y}, c, m});
    uint o3 = draw_list_vertex(b, {{A.x, B.y, Z}, {UV0.x, UV1.y}, c, m});

    draw_list_indices(b, iid, mode_triangles, shader_canvas,
        {o0, o3, o1, o1, o3, o2});
}

static void rect(draw_list &batch, vec2 A, vec2 B, float Z,
    Context &ctx, uint shape_num, uint color)
{
    Shape &shape = ctx.shapes[shape_num];
    auto &size = shape.size;
    auto &offset = shape.offset;
    auto t = mat3(size.x,       0,        offset.x ,
                       0,      -size.y,   size.y + offset.y ,
                       0,       0,        1);
    auto UV0 = vec3(0,0,1) * t;
    auto UV1 = vec3(1,1,1) * t;
    rect(batch, tbo_iid, A, B, Z, UV0, UV1, color, (float)shape_num);
}

/*
 * shape create
 */

int make_rectangle(Context &ctx, draw_list &batch, vec2 pos, vec2 halfSize,
    float padding, float z, uint32_t c)
{
    Shape shape{0, 0, 0, 1, vec2(0), vec2((halfSize+padding)*2.0f) };
    Edge edge{Rectangle,{halfSize + padding, halfSize}};

    int shape_num = ctx.addShape(&shape, &edge);
    rect(batch, tbo_iid, pos - halfSize - padding, pos + halfSize + padding,
        z, vec2(0,0), (halfSize+padding)*2.0f, c, (float)shape_num);

    return shape_num;
}

int make_rounded_rectangle(Context &ctx, draw_list &batch, vec2 pos,
    vec2 halfSize, float radius, float padding, float z, uint32_t c)
{
    Shape shape{0, 0, 0, 1, vec2(0), vec2((halfSize+padding)*2.0f) };
    Edge edge{RoundedRectangle,{halfSize + padding, halfSize, vec2(radius)}};

    int shape_num = ctx.addShape(&shape, &edge);
    rect(batch, tbo_iid, pos - halfSize - padding, pos + halfSize + padding,
        z, vec2(0,0), (halfSize+padding)*2.0f, c, (float)shape_num);

    return shape_num;
}

int make_circle(Context &ctx, draw_list &batch, vec2 pos, float radius,
    float padding, float z, uint32_t c)
{
    Shape shape{0, 0, 0, 1, vec2(0), vec2((radius + padding) * 2.0f) };
    Edge edge{Circle,{vec2(radius + padding), vec2(radius)}};

    int shape_num = ctx.addShape(&shape, &edge);
    rect(batch, tbo_iid, pos - radius - padding, pos + radius + padding,
        z, vec2(0,0), vec2((radius+padding)*2.0f), c, (float)shape_num);

    return shape_num;
}

int make_ellipse(Context &ctx, draw_list &batch, vec2 pos, vec2 radius,
    float padding, float z, uint32_t c)
{
    Shape shape{0, 0, 0, 1, vec2(0), (radius + padding) * 2.0f };
    Edge edge{Ellipse,{radius + padding, radius}};

    int shape_num = ctx.addShape(&shape, &edge);
    rect(batch, tbo_iid, pos - radius - padding, pos + radius + padding,
        z, vec2(0,0), (radius+padding)*2.0f, c, (float)shape_num);

    return shape_num;
}

/*
 * shape update
 */

int update_rectangle(int shape_num, Context &ctx, draw_list &batch, vec2 pos,
    vec2 halfSize, float padding, float z, uint32_t c)
{
    Shape shape{0, 0, 0, 1, vec2(0), vec2((halfSize+padding)*2.0f) };
    Edge edge{Rectangle,{halfSize + padding, halfSize}};

    int updated = ctx.updateShape(shape_num, &shape, &edge);
    rect(batch, tbo_iid, pos - halfSize - padding, pos + halfSize + padding,
        z, vec2(0,0), (halfSize+padding)*2.0f, c, (float)shape_num);

    return updated;
}

int update_rounded_rectangle(int shape_num, Context &ctx, draw_list &batch,
    vec2 pos, vec2 halfSize, float radius, float padding, float z, uint32_t c)
{
    Shape shape{0, 0, 0, 1, vec2(0), vec2((halfSize+padding)*2.0f) };
    Edge edge{RoundedRectangle,{halfSize + padding, halfSize, vec2(radius)}};

    int updated = ctx.updateShape(shape_num, &shape, &edge);
    rect(batch, tbo_iid, pos - halfSize - padding, pos + halfSize + padding,
        z, vec2(0,0), (halfSize+padding)*2.0f, c, (float)shape_num);

    return updated;
}

int update_circle(int shape_num, Context &ctx, draw_list &batch, vec2 pos,
    float radius, float padding, float z, uint32_t c)
{
    Shape shape{0, 0, 0, 1, vec2(0), vec2((radius + padding) * 2.0f) };
    Edge edge{Circle,{vec2(radius + padding), vec2(radius)}};

    int updated = ctx.updateShape(shape_num, &shape, &edge);
    rect(batch, tbo_iid, pos - radius - padding, pos + radius + padding,
        z, vec2(0,0), vec2((radius+padding)*2.0f), c, (float)shape_num);

    return updated;
}

int update_ellipse(int shape_num, Context &ctx, draw_list &batch, vec2 pos,
    vec2 radius, float padding, float z, uint32_t c)
{
    Shape shape{0, 0, 0, 1, vec2(0), (radius + padding) * 2.0f };
    Edge edge{Ellipse,{radius + padding, radius}};

    int updated = ctx.updateShape(shape_num, &shape, &edge);
    rect(batch, tbo_iid, pos - radius - padding, pos + radius + padding,
        z, vec2(0,0), (radius+padding)*2.0f, c, (float)shape_num);

    return updated;
}


/*
 * text renderer
 */

static const int glyph_load_size = 64;

void text_renderer_canvas::render(draw_list &batch,
        std::vector<glyph_shape> &shapes,
        text_segment *segment)
{
    font_face_ft *face = static_cast<font_face_ft*>(segment->face);
    FT_Face ftface = face->ftface;
    int font_size = segment->font_size;
    int font_dpi = font_manager::dpi;

    float x_offset = 0;
    for (auto &s : shapes) {
        /* lookup shape num, load glyph if necessary */
        int shape_num = 0;
        auto gi = glyph_map.find(s.glyph);
        if (gi == glyph_map.end()) {
            shape_num = glyph_map[s.glyph] = (int)ctx.shapes.size();
            make_glyph(&ctx, ftface, glyph_load_size << 6, font_dpi, s.glyph);
            print_shape(ctx, shape_num);
        } else {
            shape_num = gi->second;
        }
        Shape &shape = ctx.shapes[shape_num];

        /* figure out glyph dimensions */
        float s_scale = font_size / (float)(glyph_load_size << 6);
        vec2 s_size = shape.size * s_scale;
        vec2 s_offset = shape.offset * s_scale;
        float y_offset = (font_size / 64.0f) - s_size.y;
        vec2 p1 = vec2(segment->x, segment->y) + vec2(x_offset, y_offset) +
            vec2(s_offset.x,-s_offset.y);
        vec2 p2 = p1 + vec2(s_size.x,s_size.y);

        /* emit geometry and advance */
        rect(batch, p1, p2, 0, ctx, shape_num, segment->color);
        x_offset += s.x_advance/64.0f + segment->tracking;
    }
}