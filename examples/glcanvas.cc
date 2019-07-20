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
#include "image.h"
#include "color.h"
#include "utf8.h"
#include "draw.h"
#include "font.h"
#include "glyph.h"
#include "canvas.h"
#include "color.h"
#include "logger.h"
#include "glcommon.h"

using namespace std::chrono;

using dvec2 = glm::dvec2;

/* globals */

static texture_buffer shape_tb, edge_tb, brush_tb;
static program prog_simple, prog_msdf, prog_canvas;
static GLuint vao, vbo, ibo;
static std::map<int,GLuint> tex_map;
static font_manager_ft manager;
static Canvas canvas(&manager);

static mat4 mvp;
static GLFWwindow* window;

static const char *font_path = "fonts/DejaVuSans.ttf";
static const char *render_text = "πάθος λόγος ἦθος";
static const char* text_lang = "en";
static const int font_dpi = 72;
static const int stats_font_size = 18;

static const float min_zoom = 16.0f, max_zoom = 32768.0f;
static int width = 1024, height = 768;
static double tl, tn, td;
static bool help_text = false;
static int codepoint = 'g';

/* canvas state */

struct zoom_state {
    float zoom;
    dvec2 mouse_pos;
    vec2 origin;
};

static AContext ctx;
static font_face *face;
static draw_list batch;
static zoom_state state = { 64.0f }, state_save;
static bool mouse_left_drag = false, mouse_right_drag = false;

/* display  */

static program* cmd_shader_gl(int cmd_shader)
{
    switch (cmd_shader) {
    case shader_simple:  return &prog_simple;
    case shader_msdf:    return &prog_msdf;
    case shader_canvas:  return &prog_canvas;
    default: return nullptr;
    }
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

static std::vector<std::string> get_stats(font_face *face, double td)
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

    if (!face) {
        face = manager.findFontByPath(font_path);
    }

    draw_list_clear(batch);

    /* create text and rounded rectangle with gradient brush */
    if (canvas.num_drawables() == 0) {
        Text *t = canvas.new_text();
        t->set_face(face);
        t->set_halign(text_halign_center);
        t->set_valign(text_valign_center);
        t->set_text(render_text);
        t->set_lang("en");
        t->set_color(color(0,0,0,1));
        t->set_position(vec2(0));
        t->set_size(64.0f);

        /* need text size for gradient and rounded rectangle size */
        vec2 text_size = t->get_text_size();

        /* create rounded rectangle with a gradient fill */
        canvas.set_fill_brush(Brush{
            BrushAxial, { vec2(0,0), vec2(0, text_size.y*2.0f) },
            { color(0.80f,0.80f,0.80f,1.0f), color(0.50f,0.50f,0.50f,1.0f) }
        });
        RoundedRectangle *r = canvas.new_rounded_rectangle(vec2(0),
            vec2(text_size.x/1.85f,text_size.y), text_size.y/2.0f);

        /* trick to move the rounded rectangle behind the text */
        std::swap(canvas.objects[0], canvas.objects[1]);
    }

    /* set up scale/translate matrix */
    glfwGetFramebufferSize(window, &width, &height);
    float s = state.zoom / 64.0f;
    float tx = state.origin.x + width/2.0f;
    float ty = state.origin.y + height/2.0f;
    mat3 m(s, 0, tx,
           0, s, ty,
           0, 0, 1);

    /* emit canvas draw list */
    canvas.emit(batch, m);

    /* synchronize canvas texture buffers */
    if (canvas.dirty) {
        buffer_texture_create(shape_tb, canvas.ctx->shapes, GL_TEXTURE0, GL_R32F);
        buffer_texture_create(edge_tb, canvas.ctx->edges, GL_TEXTURE1, GL_R32F);
        buffer_texture_create(brush_tb, canvas.ctx->brushes, GL_TEXTURE2, GL_R32F);
        canvas.dirty = false;
    }

    /* render stats text */
    float x = 10, y = height - 10;
    std::vector<std::string> stats = get_stats(face, td);
    const uint32_t bg_color = 0xbfffffff;
    for (size_t i = 0; i < stats.size(); i++) {
        text_segment stats_segment(stats[i], text_lang, face,
            (int)((float)stats_font_size * 64.0f), x, y, 0xff000000);
        shapes.clear();
        shaper.shape(shapes, &stats_segment);
        font_atlas *atlas = manager.getCurrentAtlas(face);
        renderer.render(batch, shapes, &stats_segment);
        y -= (int)((float)stats_font_size * 1.334f);
    }

    /* update vertex and index buffers arrays (idempotent) */
    vertex_buffer_create("vbo", &vbo, GL_ARRAY_BUFFER, batch.vertices);
    vertex_buffer_create("ibo", &ibo, GL_ELEMENT_ARRAY_BUFFER, batch.indices);

    /* okay, lets send commands to the GPU */
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* draw list batch with tbo_iid canvas texture buffer special case */
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
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_BUFFER, brush_tb.tex);
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
    uniform_1i(prog, "tb_brush", 2);
}

static void reshape(int width, int height)
{
    mvp = glm::ortho(0.0f, (float)width,(float)height, 0.0f, 0.0f, 100.0f);

    glViewport(0, 0, width, height);

    glUseProgram(prog_canvas.pid);
    update_uniforms(&prog_canvas);

    glUseProgram(prog_msdf.pid);
    update_uniforms(&prog_msdf);

    glUseProgram(prog_simple.pid);
    update_uniforms(&prog_simple);
}

/* keyboard callback */

static void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE) {
        exit(0);
    }
}

/* mouse callbacks */

static void scroll(GLFWwindow* window, double xoffset, double yoffset)
{
    float quantum = state.zoom / 16.0f;
    float ratio = 1.0f + (float)quantum / (float)state.zoom;
    if (yoffset < 0 && state.zoom < max_zoom) {
        state.origin *= ratio;
        state.zoom += quantum;
    } else if (yoffset > 0 && state.zoom > min_zoom) {
        state.origin /= ratio;
        state.zoom -= quantum;
    }
}

static void mouse_button(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        mouse_left_drag = true;
        state_save = state;
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        mouse_left_drag = false;
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        mouse_right_drag = true;
        state_save = state;
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        mouse_right_drag = false;
    }
}

static void cursor_position(GLFWwindow* window, double xpos, double ypos)
{
    state.mouse_pos = dvec2(xpos, ypos);

    if (mouse_left_drag) {
        state.origin += state.mouse_pos - state_save.mouse_pos;
        state_save.mouse_pos = state.mouse_pos;
    }
    else if (mouse_right_drag) {
        dvec2 delta = state.mouse_pos - state_save.mouse_pos;
        float zoom = state_save.zoom * powf(65.0f/64.0f,(float)-delta.y);
        if (zoom != state.zoom && zoom > min_zoom && zoom < max_zoom) {
            state.zoom = zoom;
            state.origin = state_save.origin * (zoom / state_save.zoom);
        }
    }
}

/* OpenGL initialization */

static void initialize()
{
    GLuint simple_fsh, msdf_fsh, canvas_fsh, vsh;

    std::vector<std::string> attrs = {
        "a_pos", "a_uv0", "a_color", "a_shape", "a_gamma"
    };

    /* shader program */
    vsh = compile_shader(GL_VERTEX_SHADER, "shaders/simple.vsh");
    simple_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/simple.fsh");
    msdf_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/msdf.fsh");
    canvas_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/canvas.fsh");
    link_program(&prog_simple, vsh, simple_fsh, attrs);
    link_program(&prog_msdf, vsh, msdf_fsh, attrs);
    link_program(&prog_canvas, vsh, canvas_fsh, attrs);
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
    program *p = &prog_canvas; /* use any program to get attribute locations */
    vertex_array_pointer(p, "a_pos", 3, GL_FLOAT, 0, &draw_vertex::pos);
    vertex_array_pointer(p, "a_uv0", 2, GL_FLOAT, 0, &draw_vertex::uv);
    vertex_array_pointer(p, "a_color", 4, GL_UNSIGNED_BYTE, 1, &draw_vertex::color);
    vertex_array_pointer(p, "a_shape", 1, GL_FLOAT, 0, &draw_vertex::shape);
    vertex_array_1f(p, "a_gamma", 2.0f);
    glBindVertexArray(0);

    /* create shape and edge buffer textures */
    buffer_texture_create(shape_tb, ctx.shapes, GL_TEXTURE0, GL_R32F);
    buffer_texture_create(edge_tb, ctx.edges, GL_TEXTURE1, GL_R32F);
    buffer_texture_create(brush_tb, ctx.brushes, GL_TEXTURE2, GL_R32F);

    /* pipeline */
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
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
