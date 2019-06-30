/*
 * glfw3 gpu canvas
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <climits>
#include <cassert>
#include <cmath>
#include <ctime>

#include <memory>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <chrono>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define CTX_OPENGL_MAJOR 3
#define CTX_OPENGL_MINOR 2

#include "glm/glm.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_clip_space.hpp"

#include "binpack.h"
#include "image.h"
#include "utf8.h"
#include "draw.h"
#include "font.h"
#include "glyph.h"
#include "logger.h"
#include "glcommon.h"

using namespace std::chrono;
using ivec2 = glm::ivec2;
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using mat3 = glm::mat3;
using mat4 = glm::mat4;

/* globals */

enum { tbo_iid = 99 };

static texture_buffer shape_tb, contour_tb, edge_tb;
static program simple, msdf, canvas;
static GLuint vao, vbo, ibo;
static std::map<int,GLuint> tex_map;
static font_manager_ft manager;

static mat4 mvp;
static GLFWwindow* window;

static const char *font_path = "fonts/DejaVuSans.ttf";
static const char* text_lang = "en";
static const int font_dpi = 72;
static const int font_size_default = 18;

static int width = 1024, height = 768;
static double tl, tn, td;
static bool help_text = false;
static int codepoint = 99;


/*
 * shape
 *
 * The following shape code is derived from msdfgen/ext/import-font.cpp
 * It has been simplified and adopts a data-oriented programming approach.
 * The context output is arrays formatted suitably to download to a GPU.
 */

enum EdgeType { Linear = 2, Quadratic = 3, Cubic = 4, };

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

    void newShape(ivec2 offset, vec2 size) {
        shapes.emplace_back(Shape{(float)contours.size(), 0,
            (float)edges.size(), 0, offset, size });
    }
    void newContour() {
        contours.emplace_back(Contour{(float)edges.size(), 0});
        shapes.back().contour_count++;
    }
    void newEdge(Edge&& e) {
        edges.emplace_back(e);
        contours.back().edge_count++;
        shapes.back().edge_count++;
    }
};

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
    return vec2(m->horiBearingX/64.0f, (m->horiBearingY-m->height)/64.0f);
}

static vec2 size(FT_Glyph_Metrics *m) {
    return vec2(m->width/64.0f, m->height/64.0f);
}

static void load_glyph(Context *ctx, FT_Face ftface, int sz, int dpi, int glyph)
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

    ctx->newShape(offset(m), size(m));

    if ((fterr = FT_Outline_Decompose(&ftface->glyph->outline, &ftfuncs, ctx))) {
        Panic("error: FT_Outline_Decompose failed: fterr=%d\n", fterr);
    }
}

static void print_shape(Context &ctx, int shape)
{
    Shape &s = ctx.shapes[shape];
    printf("shape %d (contour count = %d, edge count = %d, "
        "offset = (%d,%d), size = (%d,%d))\n",
        shape, (int)s.contour_count, (int)s.edge_count,
        (int)s.offset.x, (int)s.offset.y, (int)s.size.x, (int)s.size.y);
    for (size_t i = 0; i < s.contour_count; i++) {
        Contour &c = ctx.contours[s.contour_offset + i];
        printf("  contour %zu (edge count = %d)\n", i, (int)c.edge_count);
        for (size_t j = 0; j < c.edge_count; j++) {
            Edge &e = ctx.edges[c.edge_offset + j];
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

/* display  */

Context ctx;
draw_list batch;

static program* cmd_shader_gl(int cmd_shader)
{
    switch (cmd_shader) {
    case shader_simple:  return &simple;
    case shader_canvas:  return &canvas;
    default: return nullptr;
    }
}

static void rect(draw_list &b, uint iid, ivec2 A, ivec2 B, int Z,
    vec2 UV0, vec2 UV1, uint c, float m)
{
    uint o = static_cast<uint>(b.vertices.size());

    vec2 j = A, k = B;
    float z = Z;

    uint o0 = draw_list_vertex(b, {{j.x, j.y, (float)z}, {UV0.x, UV0.y}, c, m});
    uint o1 = draw_list_vertex(b, {{k.x, j.y, (float)z}, {UV1.x, UV0.y}, c, m});
    uint o2 = draw_list_vertex(b, {{k.x, k.y, (float)z}, {UV1.x, UV1.y}, c, m});
    uint o3 = draw_list_vertex(b, {{j.x, k.y, (float)z}, {UV0.x, UV1.y}, c, m});

    draw_list_indices(b, iid, mode_triangles, shader_canvas,
        {o0, o3, o1, o1, o3, o2});
}

static mat3 shape_transform(Shape &shape, float padding)
{
    vec2 size = vec2(shape.size) + padding;
    float scale = (std::max)(size.x,size.y);
    vec2 rem = vec2(shape.size) - vec2(scale);
    return mat3(scale,       0,       0 + rem.x/2 + shape.offset.x ,
                0,      -scale,   scale + rem.y/2 + shape.offset.y ,
                0,           0,   scale);
}

static void rect(draw_list &batch, ivec2 A, ivec2 B, int Z,
    uint shape_num, float padding, uint color)
{
    Shape &shape = ctx.shapes[shape_num];
    auto t = shape_transform(shape, padding);
    auto UV0 = vec3(0,0,1) * t;
    auto UV1 = vec3(1,1,1) * t;
    rect(batch, tbo_iid, A, B, Z, UV0, UV1, color, shape_num);
}

static std::string format_string(const char* fmt, ...)
{
    std::vector<char> buf(128);
    va_list ap;

    va_start(ap, fmt);
    int len = vsnprintf(buf.data(), buf.capacity(), fmt, ap);
    va_end(ap);

    std::string str;
    if (len >= (int)buf.capacity()) {
        buf.resize(len + 1);
        va_start(ap, fmt);
        vsnprintf(buf.data(), buf.capacity(), fmt, ap);
        va_end(ap);
    }
    str = buf.data();

    return str;
}

static std::vector<std::string> get_stats(font_face *face, float td)
{
    std::vector<std::string> stats;
    stats.push_back(format_string("frames-per-second: %5.2f", 1.0/td));
    return stats;
}

static void draw(double tn, double td)
{
    std::vector<glyph_shape> shapes;
    text_shaper_hb shaper;
    text_renderer_ft renderer(&manager);

    draw_list_clear(batch);

    glfwGetFramebufferSize(window, &width, &height);

    ivec2 screen(width, height), size((std::min)(width, height));
    ivec2 p1 = (screen - size)/2, p2 = p1 + size;

    rect(batch, p1, p2, /* z = */ 0, /* shape_num = */ 0,
        /* pad = */ 8.0f, /* color = */ 0xff000000);

    /* render stats text */
    int x = 10, y = height - 10;
    auto face = manager.findFontByPath(font_path);
    std::vector<std::string> stats = get_stats(face, td);
    const uint32_t bg_color = 0xbfffffff;
    for (size_t i = 0; i < stats.size(); i++) {
        text_segment stats_segment(stats[i], text_lang, face,
            (int)((float)font_size_default * 64.0f), x, y, 0xff000000);
        shapes.clear();
        shaper.shape(shapes, &stats_segment);
        font_atlas *atlas = manager.getCurrentAtlas(face);
        renderer.render(batch, shapes, &stats_segment);
        y -= ((float)font_size_default * 1.334f);
    }

    /* update vertex and index buffers arrays (idempotent) */
    vertex_buffer_create("vbo", &vbo, GL_ARRAY_BUFFER, batch.vertices);
    vertex_buffer_create("ibo", &ibo, GL_ELEMENT_ARRAY_BUFFER, batch.indices);

    /* okay, lets send commands to the GPU */
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (auto img : batch.images) {
        auto ti = tex_map.find(img.iid);
        if (ti == tex_map.end()) {
            GLuint tex;
            image_create_texture(&tex, img);
            tex_map[img.iid] = tex;
        } else {
            image_update_texture(tex_map[img.iid], img);
        }
    }
    for (auto cmd : batch.cmds) {
        glUseProgram(cmd_shader_gl(cmd.shader)->pid);
        if (cmd.iid == tbo_iid) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_BUFFER, shape_tb.tex);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_BUFFER, edge_tb.tex);
        } else {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex_map[cmd.iid]);
        }
        glBindVertexArray(vao);
        glDrawElements(cmd_mode_gl(cmd.mode), cmd.count, GL_UNSIGNED_INT,
            (void*)(cmd.offset * sizeof(uint)));
    }

    glfwSwapBuffers(window);
}

static void display()
{
    auto t = high_resolution_clock::now();

    tl = tn;
    tn = (double)duration_cast<nanoseconds>(t.time_since_epoch()).count()/1e9;
    td = tn - tl;

    draw(tn, td);
}

static void update_uniforms(program *prog)
{
    uniform_matrix_4fv(prog, "u_mvp", (const GLfloat *)&mvp[0][0]);
    uniform_1i(prog, "u_tex0", 0);
    uniform_1i(prog, "tb_shape", 0);
    uniform_1i(prog, "tb_edge", 1);
}

static void reshape(int width, int height)
{
    mvp = glm::ortho(0.0f, (float)width,(float)height, 0.0f, 0.0f, 100.0f);

    glViewport(0, 0, width, height);

    glUseProgram(canvas.pid);
    update_uniforms(&canvas);

    glUseProgram(msdf.pid);
    update_uniforms(&msdf);

    glUseProgram(simple.pid);
    update_uniforms(&simple);
}

void create_tbo(int codepoint)
{
    font_face *face = manager.findFontByPath(font_path);
    FT_Face ftface = static_cast<font_face_ft*>(face)->ftface;
    int glyph = FT_Get_Char_Index(ftface, codepoint);

    load_glyph(&ctx, ftface, 128 * 64, font_dpi, glyph);
    print_shape(ctx, 0);

    buffer_texture_create(shape_tb, ctx.shapes, GL_TEXTURE0, GL_R32I);
    buffer_texture_create(edge_tb, ctx.edges, GL_TEXTURE1, GL_R32F);
}

/* OpenGL initialization */

static void initialize()
{
    GLuint simple_fsh, msdf_fsh, canvas_fsh, vsh;

    std::vector<std::string> attrs = {
        "a_pos", "a_uv0", "a_color", "x_material", "a_gamma"
    };

    /* shader program */
    vsh = compile_shader(GL_VERTEX_SHADER, "shaders/simple.vsh");
    simple_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/simple.fsh");
    msdf_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/msdf.fsh");
    canvas_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/canvas.fsh");
    link_program(&simple, vsh, simple_fsh, attrs);
    link_program(&msdf, vsh, msdf_fsh, attrs);
    link_program(&canvas, vsh, canvas_fsh, attrs);
    glDeleteShader(vsh);
    glDeleteShader(simple_fsh);
    glDeleteShader(msdf_fsh);
    glDeleteShader(canvas_fsh);

    /* create vertex and index buffers arrays */
    vertex_buffer_create("vbo", &vbo, GL_ARRAY_BUFFER, batch.vertices);
    vertex_buffer_create("ibo", &ibo, GL_ELEMENT_ARRAY_BUFFER, batch.indices);

    /* configure vertex array object */
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    program *p = &canvas; /* use any program to get attribute locations */
    vertex_array_pointer(p, "a_pos", 3, GL_FLOAT, 0, &draw_vertex::pos);
    vertex_array_pointer(p, "a_uv0", 2, GL_FLOAT, 0, &draw_vertex::uv);
    vertex_array_pointer(p, "a_color", 4, GL_UNSIGNED_BYTE, 1, &draw_vertex::color);
    vertex_array_pointer(p, "a_material", 1, GL_FLOAT, 0, &draw_vertex::material);
    vertex_array_1f(p, "a_gamma", 2.0f);
    glBindVertexArray(0);

    create_tbo(codepoint);

    /* pipeline */
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glLineWidth(1.0);
}

/* GLFW GUI entry point */

static void resize(GLFWwindow* window, int width, int height)
{
    reshape(width, height);
}

static void glcanvas(int argc, char **argv)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, CTX_OPENGL_MAJOR);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, CTX_OPENGL_MINOR);

    window = glfwCreateWindow(width, height, argv[0], NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL();
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, resize);
    glfwGetFramebufferSize(window, &width, &height);

    initialize();
    reshape(width, height);
    while (!glfwWindowShouldClose(window)) {
        display();
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}

/* help text */

void print_help(int argc, char **argv)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  -g, --glyph <glyph>   glyph to render (default '%c')\n"
        "  -h, --help            command line help\n", argv[0], codepoint);
}

/* option parsing */

bool check_param(bool cond, const char *param)
{
    if (cond) {
        printf("error: %s requires parameter\n", param);
    }
    return (help_text = cond);
}

bool match_opt(const char *arg, const char *opt, const char *longopt)
{
    return strcmp(arg, opt) == 0 || strcmp(arg, longopt) == 0;
}

void parse_options(int argc, char **argv)
{
    int i = 1;
    while (i < argc) {
        if (match_opt(argv[i], "-g", "--glyph")) {
            if (check_param(++i == argc, "--glyph")) break;
            const char *codepoint_str = argv[i++];
            if (utf8_codelen(codepoint_str) == strlen(codepoint_str)) {
                codepoint = utf8_to_utf32(codepoint_str);
            } else if (!(codepoint = atoi(codepoint_str))) {
                printf("error:--glyph must be a single character or integer\n");
                help_text = true;
                break;
            }
        } else if (match_opt(argv[i], "-h", "--help")) {
            help_text = true;
            i++;
         } else {
            fprintf(stderr, "error: unknown option: %s\n", argv[i]);
            help_text = true;
            break;
        }
    }

    if (help_text) {
        print_help(argc, argv);
        exit(1);
    }

}

/* entry point */

int main(int argc, char **argv)
{
    parse_options(argc, argv);
    glcanvas(argc, argv);
    return 0;
}
