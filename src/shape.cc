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
#include "color.h"
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

static AContext* uctx(void *u) { return static_cast<AContext*>(u); }
static vec2 pt(ftvec *v) { return vec2(v->x/64.f, v->y/64.f); }

static int ftMoveTo(ftvec *p, void *u) {
    uctx(u)->newContour();
    uctx(u)->pos = pt(p);
    return 0;
}

static int ftLineTo(ftvec *p, void *u) {
    uctx(u)->newEdge(AEdge{Linear, { uctx(u)->pos, pt(p) }});
    uctx(u)->pos = pt(p);
    return 0;
}

static int ftConicTo(ftvec *c, ftvec *p, void *u) {
    uctx(u)->newEdge(AEdge{Quadratic, { uctx(u)->pos, pt(c), pt(p) }});
    uctx(u)->pos = pt(p);
    return 0;
}

static int ftCubicTo(ftvec *c1, ftvec *c2, ftvec *p, void *u) {
    uctx(u)->newEdge(AEdge{Cubic, { uctx(u)->pos, pt(c1), pt(c2), pt(p) }});
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

int make_glyph(AContext *ctx, FT_Face ftface, int sz, int dpi, int glyph)
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

void print_edge(AContext &ctx, int edge)
{
    AEdge &e = ctx.edges[edge];
    switch ((int)e.type) {
    case AEdgeType::Linear:
        printf("    edge %d Linear (%f,%f) - (%f, %f)\n",
            edge, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y);
        break;
    case AEdgeType::Quadratic:
        printf("    edge %d Quadratic (%f,%f) - [%f, %f]"
            " - (%f, %f)\n",
            edge, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y,
               e.p[2].x, e.p[2].y);
        break;
    case AEdgeType::Cubic:
        printf("    edge %d Cubic (%f,%f) - [%f, %f]"
            " - [%f, %f] - (%f, %f)\n",
            edge, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y,
               e.p[2].x, e.p[2].y, e.p[3].x, e.p[3].y);
        break;
    case AEdgeType::Circle:
        printf("    edge %d Circle (%f,%f), r=%f\n",
            edge, e.p[0].x, e.p[0].y, e.p[1].x);
        break;
    case AEdgeType::Ellipse:
        printf("    edge %d Ellipse (%f,%f), r=(%f, %f)\n",
            edge, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y);
        break;
    case AEdgeType::Rectangle:
        printf("    edge %d Rectangle (%f,%f), halfSize=(%f, %f)\n",
            edge, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y);
        break;
    case AEdgeType::RoundedRectangle:
        printf("    edge %d RoundedRectangle (%f,%f), halfSize=(%f, %f), r=%f\n",
            edge, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y, e.p[2].x);
        break;
    }
}

void print_shape(AContext &ctx, int shape)
{
    AShape &s = ctx.shapes[shape];
    printf("shape %d (contour count = %d, edge count = %d, "
        "offset = (%d,%d), size = (%d,%d), brush = %d\n",
        shape, (int)s.contour_count, (int)s.edge_count,
        (int)s.offset.x, (int)s.offset.y, (int)s.size.x, (int)s.size.y,
        (int)s.brush);
    for (size_t i = 0; i < (size_t)s.contour_count; i++) {
        AContour &c = ctx.contours[(size_t)s.contour_offset + i];
        printf("  contour %zu (edge count = %d)\n", i, (int)c.edge_count);
        for (int j = 0; j < c.edge_count; j++) {
            print_edge(ctx, (int)c.edge_offset + j);
        }
    }
    if (s.contour_count > 0) return;
    for (int i = 0; i < (size_t)s.edge_count; i++) {
        print_edge(ctx, (int)s.edge_offset + i);
    }
}

void AContext::clear()
{
    shapes.clear();
    contours.clear();
    edges.clear();
}

int AContext::newShape(vec2 offset, vec2 size)
{
    int shape_num = (int)shapes.size();
    shapes.emplace_back(AShape{(float)contours.size(), 0,
        (float)edges.size(), 0, offset, size, (float)currentBrush() });
    return shape_num;
}

int AContext::newContour()
{
    int contour_num = (int)contours.size();
    contours.emplace_back(AContour{(float)edges.size(), 0});
    shapes.back().contour_count++;
    return contour_num;
}

int AContext::newEdge(AEdge e)
{
    int edge_num = (int)edges.size();
    edges.push_back(e);
    if (shapes.back().contour_count > 0) {
        contours.back().edge_count++;
    }
    shapes.back().edge_count++;
    return edge_num;
}

int AContext::newShape(AShape *shape, AEdge *edges)
{
    int shape_num = newShape(shape->offset, shape->size);
    for (int i = 0; i < shape->edge_count; i++) {
        newEdge(edges[i]);
    }
    return shape_num;
}

static int numShapePoints(int edge_type)
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
	return 0;
}

static int numBrushPoints(int brush_type)
{
    switch (brush_type) {
    case Axial:         return 2;
    case Radial:        return 2;
    }
	return 0;
}

bool AContext::brushEquals(ABrush *b0, ABrush *b1)
{
    if (b0->type != b1->type) return false;
    int match_points = numBrushPoints((int)b0->type);
    for (int i = 0; i < match_points; i++) {
        if (b0->p[i] != b1->p[i]) return false;
        if (b0->c[i] != b1->c[i]) return false;
    }
    return true;
}

int AContext::newBrush(ABrush b)
{
    brush = (int)brushes.size();
    brushes.push_back(b);
    return brush;
}

bool AContext::updateBrush(int brush_num, ABrush *b)
{
    bool update = !brushEquals(&brushes[brush_num], b);
    if (update) {
        brushes[brush_num] = *b;
    }
    return update;
}

int AContext::currentBrush()
{
    return brush;
}

bool AContext::shapeEquals(AShape *s0, AEdge *e0, AShape *s1, AEdge *e1)
{
    if (s0->edge_count != s1->edge_count) return false;
    if (s0->offset != s1->offset) return false;
    if (s0->size != s1->size) return false;
    if (s0->brush != s1->brush) return false;
    if (e0->type != e1->type) return false;
    for (int i = 0; i < s0->edge_count; i++) {
        int match_points = numShapePoints((int)e0[i].type);
        for (int j = 0; j < match_points; j++) {
            if (e0[i].p[j] != e1[i].p[j]) return false;
        }
    }
    return true;
}

int AContext::findShape(AShape *s, AEdge *e)
{
    /* this is really inefficient! */
    for (size_t i = 0; i < shapes.size(); i++) {
        int j = (int)shapes[i].edge_offset;
        if (shapeEquals(&shapes[i], &edges[j], s, e)) {
            return (int)i;
        }
    }
    return -1;
}

int AContext::addShape(AShape *s, AEdge *e, bool dedup)
{
    int shape_num = dedup ? findShape(s, e) : -1;
    if (shape_num == -1) {
        shape_num = newShape(s, e);
    }
    return shape_num;
}

bool AContext::updateShape(int shape_num, AShape *s, AEdge *e)
{
    AShape *os = &shapes[shape_num];
    AEdge *oe = &edges[(int)shapes[shape_num].edge_offset];

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
    AContext &ctx, uint shape_num, uint color)
{
    AShape &shape = ctx.shapes[shape_num];
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
 * brush
 */

void brush_clear(AContext &ctx)
{
    ctx.brush = -1;
}

void brush_set(AContext &ctx, int brush_num)
{
    ctx.brush = brush_num;
}

int make_brush_axial_gradient(AContext &ctx, vec2 p0, vec2 p1, color c0, color c1)
{
    vec4 C0{c0.r, c0.g, c0.b, c0.a};
    vec4 C1{c1.r, c1.g, c1.b, c1.a};
    return ctx.newBrush(ABrush{Axial, {p0, p1}, {C0, C1}});
}

int update_brush_axial_gradient(int brush_num, AContext &ctx, vec2 p0, vec2 p1, color c0, color c1)
{
    vec4 C0{c0.r, c0.g, c0.b, c0.a};
    vec4 C1{c1.r, c1.g, c1.b, c1.a};
    ABrush b{Axial, {p0, p1}, {C0, C1}};
    return ctx.updateBrush(brush_num, &b);
}

/*
 * shape create
 */

int make_rectangle(AContext &ctx, draw_list &batch, vec2 pos, vec2 halfSize,
    float padding, float z, uint32_t c)
{
    float brush = (float)ctx.currentBrush();
    AShape shape{0, 0, 0, 1, vec2(0), vec2((halfSize+padding)*2.0f), brush };
    AEdge edge{Rectangle,{halfSize + padding, halfSize}};

    int shape_num = ctx.addShape(&shape, &edge);
    rect(batch, tbo_iid, pos - halfSize - padding, pos + halfSize + padding,
        z, vec2(0,0), (halfSize+padding)*2.0f, c, (float)shape_num);

    return shape_num;
}

int make_rounded_rectangle(AContext &ctx, draw_list &batch, vec2 pos,
    vec2 halfSize, float radius, float padding, float z, uint32_t c)
{
    float brush = (float)ctx.currentBrush();
    AShape shape{0, 0, 0, 1, vec2(0), vec2((halfSize+padding)*2.0f), brush };
    AEdge edge{RoundedRectangle,{halfSize + padding, halfSize, vec2(radius)}};

    int shape_num = ctx.addShape(&shape, &edge);
    rect(batch, tbo_iid, pos - halfSize - padding, pos + halfSize + padding,
        z, vec2(0,0), (halfSize+padding)*2.0f, c, (float)shape_num);

    return shape_num;
}

int make_circle(AContext &ctx, draw_list &batch, vec2 pos, float radius,
    float padding, float z, uint32_t c)
{
    float brush = (float)ctx.currentBrush();
    AShape shape{0, 0, 0, 1, vec2(0), vec2((radius + padding) * 2.0f), brush };
    AEdge edge{Circle,{vec2(radius + padding), vec2(radius)}};

    int shape_num = ctx.addShape(&shape, &edge);
    rect(batch, tbo_iid, pos - radius - padding, pos + radius + padding,
        z, vec2(0,0), vec2((radius+padding)*2.0f), c, (float)shape_num);

    return shape_num;
}

int make_ellipse(AContext &ctx, draw_list &batch, vec2 pos, vec2 radius,
    float padding, float z, uint32_t c)
{
    float brush = (float)ctx.currentBrush();
    AShape shape{0, 0, 0, 1, vec2(0), (radius + padding) * 2.0f, brush };
    AEdge edge{Ellipse,{radius + padding, radius}};

    int shape_num = ctx.addShape(&shape, &edge);
    rect(batch, tbo_iid, pos - radius - padding, pos + radius + padding,
        z, vec2(0,0), (radius+padding)*2.0f, c, (float)shape_num);

    return shape_num;
}

/*
 * shape update
 */

int update_rectangle(int shape_num, AContext &ctx, draw_list &batch, vec2 pos,
    vec2 halfSize, float padding, float z, uint32_t c)
{
    float brush = ctx.shapes[shape_num].brush;
    AShape shape{0, 0, 0, 1, vec2(0), vec2((halfSize+padding)*2.0f), brush };
    AEdge edge{Rectangle,{halfSize + padding, halfSize}};

    int updated = ctx.updateShape(shape_num, &shape, &edge);
    rect(batch, tbo_iid, pos - halfSize - padding, pos + halfSize + padding,
        z, vec2(0,0), (halfSize+padding)*2.0f, c, (float)shape_num);

    return updated;
}

int update_rounded_rectangle(int shape_num, AContext &ctx, draw_list &batch,
    vec2 pos, vec2 halfSize, float radius, float padding, float z, uint32_t c)
{
    float brush = ctx.shapes[shape_num].brush;
    AShape shape{0, 0, 0, 1, vec2(0), vec2((halfSize+padding)*2.0f), brush };
    AEdge edge{RoundedRectangle,{halfSize + padding, halfSize, vec2(radius)}};

    int updated = ctx.updateShape(shape_num, &shape, &edge);
    rect(batch, tbo_iid, pos - halfSize - padding, pos + halfSize + padding,
        z, vec2(0,0), (halfSize+padding)*2.0f, c, (float)shape_num);

    return updated;
}

int update_circle(int shape_num, AContext &ctx, draw_list &batch, vec2 pos,
    float radius, float padding, float z, uint32_t c)
{
    float brush = ctx.shapes[shape_num].brush;
    AShape shape{0, 0, 0, 1, vec2(0), vec2((radius + padding) * 2.0f), brush };
    AEdge edge{Circle,{vec2(radius + padding), vec2(radius)}};

    int updated = ctx.updateShape(shape_num, &shape, &edge);
    rect(batch, tbo_iid, pos - radius - padding, pos + radius + padding,
        z, vec2(0,0), vec2((radius+padding)*2.0f), c, (float)shape_num);

    return updated;
}

int update_ellipse(int shape_num, AContext &ctx, draw_list &batch, vec2 pos,
    vec2 radius, float padding, float z, uint32_t c)
{
    float brush = ctx.shapes[shape_num].brush;
    AShape shape{0, 0, 0, 1, vec2(0), (radius + padding) * 2.0f, brush };
    AEdge edge{Ellipse,{radius + padding, radius}};

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
        } else {
            shape_num = gi->second;
        }
        AShape &shape = ctx.shapes[shape_num];

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