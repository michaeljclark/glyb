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
#include <numeric>
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
#include "canvas.h"
#include "logger.h"

/*
 * gpu-accelerated shape lists
 *
 * The following shape code is derived from msdfgen/ext/import-font.cpp
 * It has been simplified and adopts a data-oriented programming approach.
 * The context output is arrays formatted suitably to download to a GPU.
 */

typedef const FT_Vector ftvec;

static AContext* uctx(void *u) { return static_cast<AContext*>(u); }
static vec2 pt(ftvec *v) { return vec2(v->x/64.f, v->y/64.f); }

static int ftMoveTo(ftvec *p, void *u) {
    uctx(u)->new_contour();
    uctx(u)->pos = pt(p);
    return 0;
}

static int ftLineTo(ftvec *p, void *u) {
    uctx(u)->new_edge(AEdge{EdgeLinear, { uctx(u)->pos, pt(p) }});
    uctx(u)->pos = pt(p);
    return 0;
}

static int ftConicTo(ftvec *c, ftvec *p, void *u) {
    uctx(u)->new_edge(AEdge{EdgeQuadratic, { uctx(u)->pos, pt(c), pt(p) }});
    uctx(u)->pos = pt(p);
    return 0;
}

static int ftCubicTo(ftvec *c1, ftvec *c2, ftvec *p, void *u) {
    uctx(u)->new_edge(AEdge{EdgeCubic, { uctx(u)->pos, pt(c1), pt(c2), pt(p) }});
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

int AContext::add_glyph(FT_Face ftface, int sz, int dpi, int glyph)
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

    int shape_num = new_shape(offset(m), size(m));

    if ((fterr = FT_Outline_Decompose(&ftface->glyph->outline, &ftfuncs, this))) {
        Panic("error: FT_Outline_Decompose failed: fterr=%d\n", fterr);
    }

    return shape_num;
}

void AContext::print_shape(int shape_num)
{
    AShape &s = shapes[shape_num];

    printf("shape %d (contour count = %d, edge count = %d, "
        "offset = (%d,%d), size = (%d,%d), fill_brush = %d, "
        "stroke_brush = %d, stroke_width = %f\n",
        shape_num, (int)s.contour_count, (int)s.edge_count,
        (int)s.offset.x, (int)s.offset.y, (int)s.size.x, (int)s.size.y,
        (int)s.fill_brush, (int)s.stroke_brush, s.stroke_width);

    for (size_t i = 0; i < (size_t)s.contour_count; i++) {
        AContour &c = contours[(size_t)s.contour_offset + i];
        printf("  contour %zu (edge count = %d)\n", i, (int)c.edge_count);
        for (int j = 0; j < c.edge_count; j++) {
            print_edge((int)c.edge_offset + j);
        }
    }

    if (s.contour_count > 0) return;

    for (int i = 0; i < (size_t)s.edge_count; i++) {
        print_edge((int)s.edge_offset + i);
    }
}

void AContext::print_edge(int edge_num)
{
    AEdge &e = edges[edge_num];
    switch ((int)e.type) {
    case EdgeLinear:
        printf("    edge %d Linear (%f,%f) - (%f, %f)\n",
            edge_num, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y);
        break;
    case EdgeQuadratic:
        printf("    edge %d Quadratic (%f,%f) - [%f, %f]"
            " - (%f, %f)\n",
            edge_num, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y,
            e.p[2].x, e.p[2].y);
        break;
    case EdgeCubic:
        printf("    edge %d Cubic (%f,%f) - [%f, %f]"
            " - [%f, %f] - (%f, %f)\n",
            edge_num, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y,
            e.p[2].x, e.p[2].y, e.p[3].x, e.p[3].y);
        break;
    case PrimitiveCircle:
        printf("    edge %d Circle (%f,%f), r=%f\n",
            edge_num, e.p[0].x, e.p[0].y, e.p[1].x);
        break;
    case PrimitiveEllipse:
        printf("    edge %d Ellipse (%f,%f), r=(%f, %f)\n",
            edge_num, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y);
        break;
    case PrimitiveRectangle:
        printf("    edge %d Rectangle (%f,%f), halfSize=(%f, %f)\n",
            edge_num, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y);
        break;
    case PrimitiveRoundedRectangle:
        printf("    edge %d RoundedRectangle (%f,%f), halfSize=(%f, %f), r=%f\n",
            edge_num, e.p[0].x, e.p[0].y, e.p[1].x, e.p[1].y, e.p[2].x);
        break;
    }
}

void AContext::print_brush(int brush_num)
{
    ABrush &b = brushes[brush_num];
    switch ((int)b.type) {
    case BrushSolid:
        printf("brush %d Solid color=(%f,%f,%f,%f)\n",
            brush_num, b.c[0].x, b.c[0].y, b.c[0].z, b.c[0].w);
        break;
    case BrushAxial:
        printf("brush %d Axial points=((%f,%f), (%f,%f)) "
            "colors=((%f,%f,%f,%f), (%f,%f,%f,%f))\n",
            brush_num, b.p[0].x, b.p[0].y, b.p[1].x, b.p[1].y,
            b.c[0].x, b.c[0].y, b.c[0].z, b.c[0].w,
            b.c[1].x, b.c[1].y, b.c[1].z, b.c[1].w);
        break;
    case BrushRadial:
        printf("brush %d Radial points=((%f,%f), (%f,%f)) "
            "colors=((%f,%f,%f,%f), (%f,%f,%f,%f))\n",
            brush_num, b.p[0].x, b.p[0].y, b.p[1].x, b.p[1].y,
            b.c[0].x, b.c[0].y, b.c[0].z, b.c[0].w,
            b.c[1].x, b.c[1].y, b.c[1].z, b.c[1].w);
        break;
    case BrushNone: default:
        printf("brush %d None\n", brush_num);
        break;
    }
}

int AEdge::numPoints(int edge_type)
{
    switch (edge_type) {
    case EdgeLinear:                return 2;
    case EdgeQuadratic:             return 3;
    case EdgeCubic:                 return 4;
    case PrimitiveRectangle:        return 2;
    case PrimitiveCircle:           return 2;
    case PrimitiveEllipse:          return 2;
    case PrimitiveRoundedRectangle: return 2;
    }
    return 0;
}

int ABrush::numPoints(int brush_type)
{
    switch (brush_type) {
    case BrushAxial:         return 2;
    case BrushRadial:        return 2;
    }
    return 0;
}

void AContext::clear()
{
    shapes.clear();
    contours.clear();
    edges.clear();
}

int AContext::new_shape(vec2 offset, vec2 size)
{
    int shape_num = (int)shapes.size();
    shapes.emplace_back(AShape{(float)contours.size(), 0,
        (float)edges.size(), 0, offset, size, -1, -1, 0 });
    return shape_num;
}

int AContext::new_contour()
{
    int contour_num = (int)contours.size();
    contours.emplace_back(AContour{(float)edges.size(), 0});
    shapes.back().contour_count++;
    return contour_num;
}

int AContext::new_edge(AEdge e)
{
    int edge_num = (int)edges.size();
    edges.push_back(e);
    if (shapes.back().contour_count > 0) {
        contours.back().edge_count++;
    }
    shapes.back().edge_count++;
    return edge_num;
}

int AContext::copy_shape(AShape *shape, AEdge *edges)
{
    int shape_num = new_shape(shape->offset, shape->size);
    AShape &s = shapes[shape_num];
    s.fill_brush = shape->fill_brush;
    s.stroke_brush = shape->stroke_brush;
    s.stroke_width = shape->stroke_width;
    for (int i = 0; i < shape->edge_count; i++) {
        new_edge(edges[i]);
    }
    return shape_num;
}

bool AContext::brush_equals(ABrush *b0, ABrush *b1)
{
    if (b0->type != b1->type) return false;
    int match_points = ABrush::numPoints((int)b0->type);
    for (int i = 0; i < match_points; i++) {
        if (b0->p[i] != b1->p[i]) return false;
        if (b0->c[i] != b1->c[i]) return false;
    }
    return true;
}

bool AContext::shape_equals(AShape *s0, AEdge *e0, AShape *s1, AEdge *e1)
{
    if (s0->edge_count != s1->edge_count) return false;
    if (s0->offset != s1->offset) return false;
    if (s0->size != s1->size) return false;
    if (s0->fill_brush != s1->fill_brush) return false;
    if (s0->stroke_brush != s1->stroke_brush) return false;
    if (s0->stroke_width != s1->stroke_width) return false;
    if (e0->type != e1->type) return false;
    for (int i = 0; i < s0->edge_count; i++) {
        int match_points = AEdge::numPoints((int)e0[i].type);
        for (int j = 0; j < match_points; j++) {
            if (e0[i].p[j] != e1[i].p[j]) return false;
        }
    }
    return true;
}

int AContext::new_brush(ABrush b)
{
    int brush = (int)brushes.size();
    brushes.push_back(b);
    return brush;
}

int AContext::find_brush(ABrush *b)
{
    /* this is really inefficient! */
    for (size_t i = 0; i < brushes.size(); i++) {
        if (brush_equals(&brushes[i], b)) {
            return (int)i;
        }
    }
    return -1;
}

int AContext::add_brush(ABrush *b, bool dedup)
{
    int brush_num = dedup ? find_brush(b) : -1;
    if (brush_num == -1) {
        brush_num = new_brush(*b);
    }
    return brush_num;
}

bool AContext::update_brush(int brush_num, ABrush *b)
{
    bool update = !brush_equals(&brushes[brush_num], b);
    if (update) {
        brushes[brush_num] = *b;
    }
    return update;
}

int AContext::find_shape(AShape *s, AEdge *e)
{
    /* this is really inefficient! */
    for (size_t i = 0; i < shapes.size(); i++) {
        int j = (int)shapes[i].edge_offset;
        if (shape_equals(&shapes[i], &edges[j], s, e)) {
            return (int)i;
        }
    }
    return -1;
}

int AContext::add_shape(AShape *s, AEdge *e, bool dedup)
{
    int shape_num = dedup ? find_shape(s, e) : -1;
    if (shape_num == -1) {
        shape_num = copy_shape(s, e);
    }
    return shape_num;
}

bool AContext::update_shape(int shape_num, AShape *s, AEdge *e)
{
    AShape *os = &shapes[shape_num];
    AEdge *oe = &edges[(int)shapes[shape_num].edge_offset];

    if (shape_equals(os, oe, s, e)) {
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
 * text renderer
 */

void text_renderer_canvas::render(draw_list &batch,
        std::vector<glyph_shape> &shapes,
        text_segment *segment)
{
    FT_Face ftface = static_cast<font_face_ft*>(segment->face)->ftface;
    int font_size = segment->font_size;
    int dpi = font_manager::dpi;
    float x_offset = 0;

    for (auto &s : shapes) {
        auto gi = glyph_map.find(s.glyph);
        if (gi != glyph_map.end()) continue;
        glyph_map[s.glyph] = ctx.add_glyph(ftface, glyph_size, dpi, s.glyph);
    }

    for (auto &s : shapes) {
        /* lookup shape num */
        int shape_num = glyph_map[s.glyph];
        AShape &shape = ctx.shapes[shape_num];

        /* figure out glyph dimensions */
        float s_scale = (float)font_size / (float)glyph_size;
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


/*
 * Canvas objects
 */

/* Edge */

EdgeType Edge::type() {
    return (EdgeType)(int)canvas->ctx->edges[edge_num].type;
}

size_t Edge::num_points() {
    return AEdge::numPoints(type());
}

vec2 Edge::get_point(size_t offset) {
    return canvas->ctx->edges[edge_num].p[offset];
}

void Edge::set_point(size_t offset, vec2 val) {
    if (val != canvas->ctx->edges[edge_num].p[offset]) {
        canvas->ctx->edges[edge_num].p[offset] = val;
        canvas->dirty = true;
    }
}

/* Contour */

size_t Contour::num_edges() {
    return contour_num == -1 ?
        (int)canvas->ctx->shapes[shape_num].edge_count :
        (int)canvas->ctx->contours[contour_num].edge_count;
}

Edge Contour::get_edge(size_t offset) {
    int edge_offset = contour_num == -1 ?
        (int)canvas->ctx->shapes[shape_num].edge_offset :
        (int)canvas->ctx->contours[contour_num].edge_offset;
    return Edge{canvas,contour_num,edge_offset + (int)offset};
}

/* Shape */

vec2 Shape::get_offset() {
    return canvas->ctx->shapes[shape_num].offset;
}

vec2 Shape::get_size() {
    return canvas->ctx->shapes[shape_num].size;
}

Brush Shape::get_fill_brush() {
    return canvas->get_brush((int)canvas->ctx->shapes[shape_num].fill_brush);
}

Brush Shape::get_stroke_brush() {
    return canvas->get_brush((int)canvas->ctx->shapes[shape_num].stroke_brush);
}

float Shape::get_stroke_width() {
    return canvas->ctx->shapes[shape_num].stroke_width;
}

void Shape::set_offset(vec2 offset) {
    if (offset != canvas->ctx->shapes[shape_num].offset) {
        canvas->ctx->shapes[shape_num].offset = offset;
        canvas->dirty = true;
    }
}

void Shape::set_size(vec2 size) {
    if (size != canvas->ctx->shapes[shape_num].size) {
        canvas->ctx->shapes[shape_num].size = size;
        canvas->dirty = true;
    }
}

void Shape::set_fill_brush(Brush fill_brush) {
    int fill_brush_num = canvas->get_brush_num(fill_brush);
    canvas->ctx->shapes[shape_num].fill_brush = (float)fill_brush_num;
}

void Shape::set_stroke_brush(Brush stroke_brush) {
    int stroke_brush_num = canvas->get_brush_num(stroke_brush);
    canvas->ctx->shapes[shape_num].stroke_brush = (float)stroke_brush_num;
}

void Shape::set_stroke_width(float stroke_width) {
    canvas->ctx->shapes[shape_num].stroke_width = stroke_width;
}

size_t Shape::num_contours() {
    /* fake contour count for primitive shapes without contours */
    return std::max((int)canvas->ctx->shapes[shape_num].contour_count,1);
}

Contour Shape::get_contour(size_t offset) {
    /* use contour offset of -1 for primitive shapes without contours */
    if (canvas->ctx->shapes[shape_num].contour_count) {
        return Contour{canvas,-1};
    } else {
        return Contour{canvas,(int)canvas->ctx->shapes[shape_num].contour_offset};
    }
}

/* Drawable */

size_t Drawable::num_edges() {
    if (ll_shape_num) {
        return (int)canvas->ctx->shapes[ll_shape_num].edge_count;
    } else {
        return 0;
    }
}

Edge Drawable::get_edge(size_t edge_num) {
    /* we use -1 to go via edge vs contours array */
    return Edge{canvas,-1,0};
}

Shape Drawable::get_shape() { return Shape{canvas,ll_shape_num}; }

float Drawable::get_z() { return z; }
vec2 Drawable::get_position() { return pos; }
float Drawable::get_scale() { return scale; }

void Drawable::set_z(float z) { this->z = z; }
void Drawable::set_position(vec2 pos) { this->pos = pos; }
void Drawable::set_scale(float scale) { this->scale = scale; }

/* Patch */

size_t Patch::num_contours() {
    return get_shape().num_contours();
}

Contour Patch::get_contour(size_t contour_num) {
    return Contour{canvas,ll_shape_num,(int)contour_num};
}

void Patch::new_contour() {
    canvas->ctx->new_contour();
}

void Patch::new_line(vec2 p1, vec2 p2) {
    canvas->ctx->new_edge(AEdge{EdgeLinear, { p1, p2 }});
}

void Patch::new_quadratic_curve(vec2 p1, vec2 c1, vec2 p2) {
    canvas->ctx->new_edge(AEdge{EdgeQuadratic, { p1, c1, p2 }});
}

void Patch::new_cubic_curve(vec2 p1, vec2 c1, vec2 c2, vec2 p2) {
    canvas->ctx->new_edge(AEdge{EdgeCubic, { p1, c1, c2, p2 }});
}

/* Text */

float Text::get_size() { return size; }
font_face* Text::get_face() { return face; }
text_halign Text::get_halign() { return halign; }
text_valign Text::get_valign() { return valign; }
std::string Text::get_text() { return text; }
std::string Text::get_lang() { return lang; }
color Text::get_color() { return col; }

void Text::set_size(float size) { shapes.clear(); this->size = size; }
void Text::set_face(font_face *face) { shapes.clear(); this->face = face; }
void Text::set_halign(text_halign halign) { this->halign = halign; }
void Text::set_valign(text_valign valign) { this->valign = valign; }
void Text::set_text(std::string text) { shapes.clear(); this->text = text; }
void Text::set_lang(std::string lang) { shapes.clear(); this->lang = lang; }
void Text::set_color(color col) { this->col = col; }

text_segment& Text::get_text_segment() {
    segment = text_segment(text, lang, face, size * 64.0f, 0, 0, col.rgba32());
    return segment;
}

std::vector<glyph_shape>& Text::get_glyph_shapes() {
    if (shapes.size() == 0) {
        canvas->text_shaper.shape(shapes, &get_text_segment());
    }
    return shapes;
}

vec2 Text::get_text_size() {
    std::vector<glyph_shape> &shapes = get_glyph_shapes();
    float text_width = std::accumulate(shapes.begin(), shapes.end(), 0.0f,
        [](float t, glyph_shape& s) { return t + s.x_advance/64.0f; });
    return vec2(text_width,size);
}

/*
 * Primitive subclasses are single edge shapes with no contours
 */

vec2 Primitive::get_vec(size_t offset) {
    size_t edge_offset = (size_t)canvas->ctx->shapes[ll_shape_num].edge_offset;
    return canvas->ctx->edges[edge_offset].p[offset];
}

void Primitive::set_vec(size_t offset, vec2 val) {
    size_t edge_offset = (size_t)canvas->ctx->shapes[ll_shape_num].edge_offset;
    if (val != canvas->ctx->edges[edge_offset].p[offset]) {
        canvas->ctx->edges[edge_offset].p[offset] = val;
        canvas->dirty = true;
    }
}

/* Circle */

vec2 Circle::get_origin() { return get_vec(0); }
void Circle::set_origin(vec2 origin)  { set_vec(0, origin); }
float Circle::get_radius() { return get_vec(1)[0]; }
void Circle::set_radius(float radius) { set_vec(1, vec2(radius)); }
void Circle::update_circle(vec2 pos, float radius) {
    set_position(pos);
    get_shape().set_size(vec2(radius * 2.0f));
    set_vec(0, vec2(radius));
    set_vec(1, vec2(radius));
}

/* Ellipse */

vec2 Ellipse::get_origin() { return get_vec(0); }
void Ellipse::set_origin(vec2 origin)  { set_vec(0, origin); }
vec2 Ellipse::get_halfsize() { return get_vec(1); }
void Ellipse::set_halfsize(vec2 halfSize) { set_vec(1, halfSize); }
void Ellipse::update_ellipse(vec2 pos, vec2 half_size) {
    set_position(pos);
    get_shape().set_size(half_size*2.0f);
    set_vec(0, half_size);
    set_vec(1, half_size);
}

/* Rectangle */

vec2 Rectangle::get_origin() { return get_vec(0); }
void Rectangle::set_origin(vec2 origin)  { set_vec(0, origin); }
vec2 Rectangle::get_halfsize() { return get_vec(1); }
void Rectangle::set_halfsize(vec2 halfSize) { set_vec(1, halfSize); }
void Rectangle::update_rectangle(vec2 pos, vec2 half_size) {
    set_position(pos);
    get_shape().set_size(half_size*2.0f);
    set_vec(0, half_size);
    set_vec(1, half_size);
}

/* RoundedRectangle */

vec2 RoundedRectangle::get_origin() { return get_vec(0); }
void RoundedRectangle::set_origin(vec2 origin)  { set_vec(0, origin); }
vec2 RoundedRectangle::get_halfsize() { return get_vec(1); }
float RoundedRectangle::get_radius() { return get_vec(2)[0]; }
void RoundedRectangle::set_halfsize(vec2 halfSize) { set_vec(1, halfSize); }
void RoundedRectangle::set_radius(float radius) { set_vec(2, vec2(radius)); }
void RoundedRectangle::update_rounded_rectangle(vec2 pos, vec2 half_size, float radius) {
    set_position(pos);
    get_shape().set_size(half_size*2.0f);
    set_vec(0, half_size);
    set_vec(1, half_size);
    set_vec(2, vec2(radius));
}

/*
 * Canvas API
 */

Canvas::Canvas(font_manager* manager) :
    objects(), glyph_map(), ctx(std::make_unique<AContext>()),
    text_renderer(*ctx, glyph_map), manager(manager),
    dirty(false),
    fill_brush{BrushSolid, {vec2(0)}, {color(0,0,0,1)}},
    stroke_brush{BrushSolid, {vec2(0)}, {color(0,0,0,1)}},
    stroke_width(0.0f) {}

int Canvas::get_brush_num(Brush p)
{
    if (p.brush_type == BrushNone) {
        return -1;
    } else {
        color *c = p.colors;
        vec2 *P = p.points;
        vec4 C[4] = {
            { c[0].r, c[0].g, c[0].b, c[0].a },
            { c[1].r, c[1].g, c[1].b, c[1].a },
            { c[2].r, c[2].g, c[2].b, c[2].a },
            { c[3].r, c[3].g, c[3].b, c[3].a },
        };
        ABrush tmpl{(float)(int)p.brush_type,
            { P[0], P[1], P[2], P[3] }, { C[0], C[1], C[2], C[3] } };
        return ctx->add_brush(&tmpl);
    }
}

Brush Canvas::get_brush(int brush_num) {
    if (brush_num == -1) {
        return Brush{BrushNone, { vec2(0,0)}, { color(0,0,0,1) } };
    } else {
        ABrush &b = ctx->brushes[brush_num];
        return Brush{(BrushType)(int)b.type,
            { b.p[0], b.p[1], b.p[2], b.p[3]},
            { color(b.c[0].r, b.c[0].g, b.c[0].b, b.c[0].a),
              color(b.c[1].r, b.c[1].g, b.c[1].b, b.c[1].a),
              color(b.c[2].r, b.c[2].g, b.c[2].b, b.c[2].a),
              color(b.c[3].r, b.c[3].g, b.c[3].b, b.c[3].a) }
        };
    }
}

Brush Canvas::get_fill_brush() { return fill_brush; }
Brush Canvas::get_stroke_brush() { return stroke_brush; }
float Canvas::get_stroke_width() { return stroke_width; }

void Canvas::set_fill_brush(Brush brush) { fill_brush = brush; }
void Canvas::set_stroke_brush(Brush brush) { stroke_brush = brush; }
void Canvas::set_stroke_width(float width) { stroke_width = width; }

size_t Canvas::num_shapes() { return ctx->shapes.size(); }
size_t Canvas::num_contours() { return ctx->contours.size(); }
size_t Canvas::num_edges() { return ctx->edges.size(); }
size_t Canvas::num_drawables() { return objects.size(); }

Shape Canvas::get_shape(int shape_num) {
    return Shape{this,shape_num};
}

Contour Canvas::get_contour(int shape_num, int contour_num) {
    return Contour{this,shape_num,contour_num};
}

Edge Canvas::get_edge(int shape_num, int contour_num, int edge_num) {
    return Edge{this,shape_num,contour_num,edge_num};
}

Drawable* Canvas::get_drawable(size_t offset) {
    return objects[offset].get();
}

Patch* Canvas::new_patch(vec2 offset, vec2 size) {
    int shape_num = ctx->new_shape(offset, size);
    auto o = new Patch{this, drawable_patch, (int)objects.size(), shape_num};
    objects.push_back(std::unique_ptr<Drawable>(o));
    dirty = true;
    return o;
}

Text* Canvas::new_text() {
    auto o = new Text{this, drawable_text, (int)objects.size(), -1};
    objects.push_back(std::unique_ptr<Drawable>(o));
    return o;
}

Circle* Canvas::new_circle(vec2 pos, float radius) {
    int fill_brush_num = get_brush_num(fill_brush);
    int stroke_brush_num = get_brush_num(stroke_brush);
    AShape shape{0, 0, 0, 1, vec2(0), vec2(radius * 2.0f),
        (float)fill_brush_num, (float)stroke_brush_num, stroke_width };
    AEdge edge{PrimitiveCircle,{vec2(radius), vec2(radius)}};
    auto o = new Circle{this, drawable_circle,
        (int)objects.size(), ctx->add_shape(&shape, &edge), pos, 0.0f, 0.0f};
    objects.push_back(std::unique_ptr<Drawable>(o));
    dirty = true;
    return o;
}

Ellipse* Canvas::new_ellipse(vec2 pos, vec2 half_size) {
    int fill_brush_num = get_brush_num(fill_brush);
    int stroke_brush_num = get_brush_num(stroke_brush);
    AShape shape{0, 0, 0, 1, vec2(0), half_size * 2.0f,
        (float)fill_brush_num, (float)stroke_brush_num, stroke_width };
    AEdge edge{PrimitiveEllipse,{half_size, half_size}};
    auto o = new Ellipse{this, drawable_ellipse,
        (int)objects.size(), ctx->add_shape(&shape, &edge), pos, 0.0f, 0.0f};
    objects.push_back(std::unique_ptr<Drawable>(o));
    dirty = true;
    return o;
}

Rectangle* Canvas::new_rectangle(vec2 pos, vec2 half_size) {
    int fill_brush_num = get_brush_num(fill_brush);
    int stroke_brush_num = get_brush_num(stroke_brush);
    AShape shape{0, 0, 0, 1, vec2(0), vec2(half_size*2.0f),
        (float)fill_brush_num, (float)stroke_brush_num, stroke_width };
    AEdge edge{PrimitiveRectangle,{half_size, half_size}};
    auto o = new Rectangle{this, drawable_rectangle,
        (int)objects.size(), ctx->add_shape(&shape, &edge), pos, 0.0f, 0.0f};
    objects.push_back(std::unique_ptr<Drawable>(o));
    dirty = true;
    return o;
}

RoundedRectangle* Canvas::new_rounded_rectangle(vec2 pos, vec2 half_size, float radius) {
    int fill_brush_num = get_brush_num(fill_brush);
    int stroke_brush_num = get_brush_num(stroke_brush);
    AShape shape{0, 0, 0, 1, vec2(0), half_size * 2.0f,
        (float)fill_brush_num, (float)stroke_brush_num, stroke_width };
    AEdge edge{PrimitiveRoundedRectangle,{half_size, half_size, vec2(radius)}};
    auto o = new RoundedRectangle{this, drawable_rounded_rectangle,
        (int)objects.size(), ctx->add_shape(&shape, &edge), pos, 0.0f, 0.0f};
    objects.push_back(std::unique_ptr<Drawable>(o));
    dirty = true;
    return o;
}

void Canvas::emit(draw_list &batch) {
    for (auto &o : objects) {
        switch (o->drawable_type) {
        case drawable_patch: {
            auto shape = static_cast<Patch*>(o.get());
            AShape &llshape = ctx->shapes[shape->ll_shape_num];
            vec2 pos = shape->pos + llshape.offset * shape->scale;
            vec2 halfSize = (llshape.size * shape->scale) / 2.0f;
            float padding = ceil(llshape.stroke_width/2.0f);
            Brush fill_brush = get_brush((int)llshape.fill_brush);
            uint32_t c = fill_brush.colors[0].rgba32();
            rect(batch, tbo_iid,
                pos - halfSize - padding, pos + halfSize + padding,
                shape->get_z(), -vec2(padding), halfSize * 2.0f + padding, c,
                (float)o->ll_shape_num);
            break;
        }
        case drawable_text: {
            auto shape = static_cast<Text*>(o.get());
            size_t s = glyph_map.size();
            text_segment &segment = shape->get_text_segment();
            std::vector<glyph_shape> &shapes = shape->get_glyph_shapes();
            vec2 text_size = shape->get_text_size();
            switch (shape->halign) {
            case text_halign_left:   segment.x = shape->pos.x; break;
            case text_halign_center: segment.x = shape->pos.x - text_size.x/2.0f; break;
            case text_halign_right:  segment.x = shape->pos.x - text_size.x; break;
            }
            switch (shape->valign) {
            case text_valign_top:    segment.y = shape->pos.y - text_size.y; break;
            case text_valign_center: segment.y = shape->pos.y - text_size.y/2.0f; break;
            case text_valign_bottom: segment.y = shape->pos.y; break;
            }
            text_renderer.render(batch, shapes, &segment);
            dirty |= (s != glyph_map.size());
            break;
        }
        case drawable_circle: {
            auto shape = static_cast<Circle*>(o.get());
            AShape &llshape = ctx->shapes[shape->ll_shape_num];
            vec2 pos = shape->get_position();
            float radius = shape->get_radius();
            float padding = ceil(llshape.stroke_width/2.0f);
            Brush fill_brush = get_brush((int)llshape.fill_brush);
            uint32_t c = fill_brush.colors[0].rgba32();
            rect(batch, tbo_iid,
                pos - radius - padding, pos + radius + padding,
                shape->get_z(), -vec2(padding), vec2(radius) * 2.0f + padding, c,
                (float)o->ll_shape_num);
            break;
        }
        case drawable_ellipse: {
            auto shape = static_cast<Ellipse*>(o.get());
            AShape &llshape = ctx->shapes[shape->ll_shape_num];
            vec2 pos = shape->get_position();
            vec2 halfSize = shape->get_halfsize();
            float padding = ceil(llshape.stroke_width/2.0f);
            Brush fill_brush = get_brush((int)llshape.fill_brush);
            uint32_t c = fill_brush.colors[0].rgba32();
            rect(batch, tbo_iid,
                pos - halfSize - padding, pos + halfSize + padding,
                shape->get_z(), -vec2(padding), halfSize * 2.0f + padding, c,
                (float)o->ll_shape_num);
            break;
        }
        case drawable_rectangle: {
            auto shape = static_cast<Rectangle*>(o.get());
            AShape &llshape = ctx->shapes[shape->ll_shape_num];
            vec2 pos = shape->get_position();
            vec2 halfSize = shape->get_halfsize();
            float padding = ceil(llshape.stroke_width/2.0f);
            Brush fill_brush = get_brush((int)llshape.fill_brush);
            uint32_t c = fill_brush.colors[0].rgba32();
            rect(batch, tbo_iid,
                pos - halfSize - padding, pos + halfSize + padding,
                shape->get_z(), -vec2(padding), halfSize * 2.0f + padding, c,
                (float)o->ll_shape_num);
            break;
        }
        case drawable_rounded_rectangle: {
            auto shape = static_cast<RoundedRectangle*>(o.get());
            AShape &llshape = ctx->shapes[shape->ll_shape_num];
            vec2 pos = shape->get_position();
            vec2 halfSize = shape->get_halfsize();
            float padding = ceil(llshape.stroke_width/2.0f);
            Brush fill_brush = get_brush((int)llshape.fill_brush);
            uint32_t c = fill_brush.colors[0].rgba32();
            rect(batch, tbo_iid,
                pos - halfSize - padding, pos + halfSize + padding,
                shape->get_z(), -vec2(padding), halfSize * 2.0f + padding, c,
                (float)o->ll_shape_num);
            break;
        }
        }
    }
}