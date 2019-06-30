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
#include <numeric>

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
#include "glm/ext/matrix_clip_space.hpp"

#include "binpack.h"
#include "shape.h"
#include "image.h"
#include "utf8.h"
#include "draw.h"
#include "font.h"
#include "glyph.h"
#include "logger.h"
#include "glcommon.h"

using namespace std::chrono;

using dvec2 = glm::dvec2;

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
static const char *render_text = "καλως ηρθες";
static const char* text_lang = "en";
static const int font_dpi = 72;
static const int stats_font_size = 18;

static float zoom = 128.0f;
static int width = 1024, height = 768;
static double tl, tn, td;
static bool help_text = false;
static int codepoint = 'g';

/* display  */

Context ctx;
draw_list batch;
vec2 origin;

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

static void rect(draw_list &batch, ivec2 A, ivec2 B, int Z,
    uint shape_num, float padding, uint color)
{
    Shape &shape = ctx.shapes[shape_num];
    auto &size = shape.size;
    auto &offset = shape.offset;
    auto t = mat3(size.x,       0,        offset.x ,
                       0,      -size.y,   size.y + offset.y ,
                       0,       0,        1);
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

static std::map<int,int> glyph_map;

static void draw(double tn, double td)
{
    std::vector<glyph_shape> shapes;
    text_shaper_hb shaper;
    text_renderer_ft renderer(&manager);

    draw_list_clear(batch);

    glfwGetFramebufferSize(window, &width, &height);

    {
        int font_size = (int)zoom;

        auto face = manager.findFontByPath(font_path);
        FT_Face ftface = static_cast<font_face_ft*>(face)->ftface;
        text_segment segment(render_text, text_lang, face,
            font_size * 64, 0, 0, 0);

        shaper.shape(shapes, &segment);
        float text_width = std::accumulate(shapes.begin(), shapes.end(), 0.0f,
            [](float t, glyph_shape& s) { return t + s.x_advance/64.0f; });

        ivec2 screen(width, height), text_size((int)text_width, font_size);
        vec2 tl = vec2(screen - text_size)/2.0f;

        float x = 0;
        size_t shapes_size = ctx.shapes.size();

        for (auto &s : shapes) {
            /* lookup shape num, load glyph if necessary */
            int shape_num = 0;
            auto gi = glyph_map.find(s.glyph);
            if (gi == glyph_map.end()) {
                shape_num = glyph_map[s.glyph] = ctx.shapes.size();
                load_glyph(&ctx, ftface, 1024, font_dpi, s.glyph);
                print_shape(ctx, shape_num);
            } else {
                shape_num = gi->second;
            }
            Shape &shape = ctx.shapes[shape_num];

            /* figure out glyph dimensions */
            float s_scale = font_size * 64.0f / 1024.0f;
            vec2 s_size = shape.size * s_scale;
            vec2 s_offset = shape.offset * s_scale;
            vec2 p1 = tl + vec2(x, font_size-s_size.y) +
                vec2(s_offset.x,-s_offset.y) + origin;
            vec2 p2 = p1 + vec2(s_size.x,s_size.y);

            /* emit geometry and advance */
            rect(batch, p1, p2, /* z = */ 0, /* shape_num = */ shape_num,
                /* pad = */ 0.0f, /* color = */ 0xff000000);
            x += s.x_advance/64.0f + segment.tracking;
        }

        if (shapes_size != ctx.shapes.size()) {
            buffer_texture_create(shape_tb, ctx.shapes, GL_TEXTURE0, GL_R32I);
            buffer_texture_create(edge_tb, ctx.edges, GL_TEXTURE1, GL_R32F);
        }
    }

    /* render stats text */
    int x = 10, y = height - 10;
    auto face = manager.findFontByPath(font_path);
    std::vector<std::string> stats = get_stats(face, td);
    const uint32_t bg_color = 0xbfffffff;
    for (size_t i = 0; i < stats.size(); i++) {
        text_segment stats_segment(stats[i], text_lang, face,
            (int)((float)stats_font_size * 64.0f), x, y, 0xff000000);
        shapes.clear();
        shaper.shape(shapes, &stats_segment);
        font_atlas *atlas = manager.getCurrentAtlas(face);
        renderer.render(batch, shapes, &stats_segment);
        y -= ((float)stats_font_size * 1.334f);
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

/* keyboard callback */

static void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE) {
        exit(0);
    }
}

/* mouse callbacks */

static bool mouse_left_drag = false;
static dvec2 mouse_pos, mouse_pos_drag_start;

static void scroll(GLFWwindow* window, double xoffset, double yoffset)
{
    int quantum = ((int)zoom >> 4);
    if (yoffset < 0 && zoom < 65536) {
        float ratio = 1.0f + (float)quantum / (float)zoom;
        origin *= ratio;
        zoom += quantum;
    } else if (yoffset > 0 && zoom > 64) {
        float ratio = 1.0f + (float)quantum / (float)zoom;
        origin /= ratio;
        zoom -= quantum;
    }
}

static void mouse_button(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        mouse_left_drag = true;
        mouse_pos_drag_start = mouse_pos;
    } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        mouse_left_drag = false;
    }
}

static void cursor_position(GLFWwindow* window, double xpos, double ypos)
{
    mouse_pos = dvec2(xpos, ypos);

    if (mouse_left_drag) {
        origin += mouse_pos - mouse_pos_drag_start;
        mouse_pos_drag_start = mouse_pos;
    }
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
    vertex_array_pointer(p, "x_material", 1, GL_FLOAT, 0, &draw_vertex::material);
    vertex_array_1f(p, "a_gamma", 2.0f);
    glBindVertexArray(0);

    /* create shape and edge buffer textures */
    buffer_texture_create(shape_tb, ctx.shapes, GL_TEXTURE0, GL_R32I);
    buffer_texture_create(edge_tb, ctx.edges, GL_TEXTURE1, GL_R32F);

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
    glfwSetScrollCallback(window, scroll);
    glfwSetKeyCallback(window, keyboard);
    glfwSetMouseButtonCallback(window, mouse_button);
    glfwSetCursorPosCallback(window, cursor_position);
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
