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

#include "linmath.h"
#include "binpack.h"
#include "image.h"
#include "draw.h"
#include "font.h"
#include "glyph.h"
#include "logger.h"
#include "glcommon.h"


/* globals */

enum { tbo_iid = 99 };

static texture_buffer shape_tb, contour_tb, edge_tb;
static program simple, msdf, canvas;
static GLuint vao, vbo, ibo;
static std::map<int,GLuint> tex_map;
static font_manager_ft manager;

static mat4x4 mvp;
static GLFWwindow* window;

static const char *font_path = "fonts/DejaVuSans.ttf";
static const int font_dpi = 72;
static int width = 1024, height = 768;

/* shape */

enum EdgeType { Linear = 2, Quadratic = 3, Cubic = 4, };

struct Edge {
    float type;
    glm::vec2 p[4];
};

struct Contour {
    int edge_offset, edge_count;
};

struct Shape {
    int contour_offset, contour_count, edge_offset, edge_count;
    glm::ivec2 offset, size;
};

struct Context {
    std::vector<Shape> shapes;
    std::vector<Contour> contours;
    std::vector<Edge> edges;
    glm::vec2 pos;

    void newShape(glm::ivec2 offset, glm::ivec2 size) {
        shapes.emplace_back(Shape{(int)contours.size(), 0,
            (int)edges.size(), 0, offset, size });
    }
    void newContour() {
        contours.emplace_back(Contour{(int)edges.size(), 0});
        shapes.back().contour_count++;
    }
    void newEdge(Edge&& e) {
        edges.emplace_back(e);
        contours.back().edge_count++;
        shapes.back().edge_count++;
    }
};

typedef const FT_Vector ftvec;

static Context* ctx(void *u) { return static_cast<Context*>(u); }
static glm::vec2 pt(ftvec *v) { return glm::vec2(v->x/64.f, v->y/64.f); }

static int ftMoveTo(ftvec *p, void *u) {
    ctx(u)->newContour();
    ctx(u)->pos = pt(p);
    return 0;
}

static int ftLineTo(ftvec *p, void *u) {
    ctx(u)->newEdge(Edge{Linear, { ctx(u)->pos, pt(p) }});
    ctx(u)->pos = pt(p);
    return 0;
}

static int ftConicTo(ftvec *c, ftvec *p, void *u) {
    ctx(u)->newEdge(Edge{Quadratic, { ctx(u)->pos, pt(c), pt(p) }});
    ctx(u)->pos = pt(p);
    return 0;
}

static int ftCubicTo(ftvec *c1, ftvec *c2, ftvec *p, void *u) {
    ctx(u)->newEdge(Edge{Cubic, { ctx(u)->pos, pt(c1), pt(c2), pt(p) }});
    ctx(u)->pos = pt(p);
    return 0;
}

static glm::ivec2 offset(FT_Glyph_Metrics *m) {
    return glm::ivec2((int)m->horiBearingX, (int)m->horiBearingY-m->height);
}

static glm::ivec2 size(FT_Glyph_Metrics *m) {
    return glm::ivec2((int)m->width, m->height);
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
        shape, s.contour_count, s.edge_count,
        s.offset.x, s.offset.y, s.size.x, s.size.y);
    for (size_t i = 0; i < s.contour_count; i++) {
        Contour &c = ctx.contours[s.contour_offset + i];
        printf("  contour %zu (edge count = %d)\n", i, c.edge_count);
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

static program* cmd_shader_gl(int cmd_shader)
{
    switch (cmd_shader) {
    case shader_simple:  return &simple;
    case shader_msdf:    return &msdf;
    case shader_canvas:  return &canvas;
    default: return nullptr;
    }
}

static void rect(draw_list &batch, glm::ivec2 A, glm::ivec2 B, int Z, uint col)
{
    uint o = static_cast<uint>(batch.vertices.size());

    glm::vec2 a = A, b = B;
    float z = Z;

    uint o0 = draw_list_vertex(batch, {{a.x, a.y, (float)z}, {0, 0}, col});
    uint o1 = draw_list_vertex(batch, {{b.x, a.y, (float)z}, {1, 0}, col});
    uint o2 = draw_list_vertex(batch, {{b.x, b.y, (float)z}, {1, 1}, col});
    uint o3 = draw_list_vertex(batch, {{a.x, b.y, (float)z}, {0, 1}, col});

    draw_list_indices(batch, tbo_iid, mode_triangles, shader_canvas,
        {o0, o3, o1, o1, o3, o2});
}

static void display()
{
    draw_list batch;
    program *prog = &canvas;

    glfwGetFramebufferSize(window, &width, &height);

    glm::ivec2 screen(width, height), size((std::min)(width, height));
    glm::ivec2 p1 = (screen - size)/2, p2 = p1 + size;

    rect(batch, p1, p2, 0, 0xff000000);

    if (!vao) {
        glGenVertexArrays(1, &vao);
    }
    glBindVertexArray(vao);
    vertex_buffer_create("vbo", &vbo, GL_ARRAY_BUFFER, batch.vertices);
    vertex_buffer_create("ibo", &ibo, GL_ELEMENT_ARRAY_BUFFER, batch.indices);
    vertex_array_pointer(prog, "a_pos", 3, GL_FLOAT, 0, &draw_vertex::pos);
    vertex_array_pointer(prog, "a_uv0", 2, GL_FLOAT, 0, &draw_vertex::uv);
    vertex_array_pointer(prog, "a_color", 4, GL_UNSIGNED_BYTE, 1, &draw_vertex::color);
    vertex_array_1f(prog, "a_gamma", 2.0f);
    glBindVertexArray(0);

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (auto img : batch.images) {
        auto ti = tex_map.find(img.iid);
        if (ti == tex_map.end()) {
            GLuint tex;
            image_create_texture(&tex, img);
            tex_map[img.iid] = tex;
        }
    }
    glBindVertexArray(vao);
    for (auto cmd : batch.cmds) {
        glUseProgram(cmd_shader_gl(cmd.shader)->pid);
        if (cmd.iid == tbo_iid) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_BUFFER, shape_tb.tex);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_BUFFER, contour_tb.tex);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_BUFFER, edge_tb.tex);
        } else {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex_map[cmd.iid]);
        }
        glDrawElements(cmd_mode_gl(cmd.mode), cmd.count, GL_UNSIGNED_INT,
            (void*)(cmd.offset * sizeof(uint)));
    }

    glfwSwapBuffers(window);
}

static void update_uniforms(program *prog)
{
    uniform_matrix_4fv(prog, "u_mvp", (const GLfloat *)mvp);
    uniform_1i(prog, "u_tex0", 0);
    uniform_1i(prog, "u_tex1", 1);
    uniform_1i(prog, "u_tex2", 2);
}

static void reshape(int width, int height)
{
    mat4x4_ortho(mvp, 0.0f, (float)width, (float)height, 0.0f, 0.0f, 100.0f);
    uniform_matrix_4fv(&simple, "u_mvp", (const GLfloat *)mvp);
    glViewport(0, 0, width, height);

    glUseProgram(canvas.pid);
    update_uniforms(&canvas);

    glUseProgram(msdf.pid);
    update_uniforms(&msdf);

    glUseProgram(simple.pid);
    update_uniforms(&simple);
}

void create_tbo()
{
    Context ctx;
    font_face *face = manager.findFontByPath(font_path);
    FT_Face ftface = static_cast<font_face_ft*>(face)->ftface;

    load_glyph(&ctx, ftface, 128 * 64, font_dpi, FT_Get_Char_Index(ftface, 'a'));
    print_shape(ctx, 0);

    buffer_texture_create(shape_tb, ctx.shapes, GL_TEXTURE0, GL_R32I);
    buffer_texture_create(contour_tb, ctx.contours, GL_TEXTURE1, GL_R32I);
    buffer_texture_create(edge_tb, ctx.edges, GL_TEXTURE2, GL_R32F);
}

/* OpenGL initialization */

static void initialize()
{
    GLuint simple_fsh, msdf_fsh, canvas_fsh, vsh;

    /* shader program */
    vsh = compile_shader(GL_VERTEX_SHADER, "shaders/simple.vsh");
    simple_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/simple.fsh");
    msdf_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/msdf.fsh");
    canvas_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/canvas.fsh");
    link_program(&simple, vsh, simple_fsh);
    link_program(&msdf, vsh, msdf_fsh);
    link_program(&canvas, vsh, canvas_fsh);
    glDeleteShader(vsh);
    glDeleteShader(simple_fsh);
    glDeleteShader(msdf_fsh);
    glDeleteShader(canvas_fsh);

    create_tbo();

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

/* entry point */

int main(int argc, char **argv)
{
    glcanvas(argc, argv);
    return 0;
}
