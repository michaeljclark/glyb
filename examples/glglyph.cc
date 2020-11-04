/*
 * glfw3 gpu canvas
 */

#ifdef _WIN32
#define _USE_MATH_DEFINES /* required to get the M_PI definition */
#endif

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <climits>
#include <cassert>
#include <cmath>
#include <ctime>

#include <memory>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <string>
#include <algorithm>
#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>
#include <numeric>
#include <initializer_list>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define CTX_OPENGL_MAJOR 3
#define CTX_OPENGL_MINOR 3

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
#include "format.h"
#include "glcommon.h"

using namespace std::chrono;

using vec2 = glm::vec2;

/* globals */

static texture_buffer shape_tb, edge_tb, brush_tb;
static program prog_simple, prog_msdf, prog_canvas;
static GLuint vao, vbo, ibo;
static std::map<int,GLuint> tex_map;
static font_manager_ft manager;
static Canvas canvas(&manager);

static mat4 mvp;
static GLFWwindow* window;
static GLFWcursor* beam_cursor;

static const char *font_path = "fonts/Roboto-Medium.ttf";
static const char *render_text = "qffifflLTA"; // "πάθος λόγος ἦθος";
static const char* text_lang = "en";
static const int stats_font_size = 18;
static int font_size = 256.0f;
static const float min_zoom = 16.0f, max_zoom = 32768.0f;
static std::array<float,4> clear_color = { 1.0f, 1.0f, 1.0f, 1.0f };
static int width = 2560, height = 1440;
static double tl, tn, td;
static bool needs_update = false;
static bool help_text = false;
static bool glyph_meta = false;

/* canvas state */

struct zoom_state {
    float zoom;
    vec2 mouse_pos;
    vec2 origin;
};

static AContext ctx;
static font_face *sans_norm, *mono_norm, *mono_bold;
static draw_list batch;
static zoom_state state = { 128.0f }, state_save;
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

static std::vector<std::string> get_stats(font_face *face, double td)
{
    std::vector<std::string> stats;
    stats.push_back(format("frames-per-second: %5.2f", 1.0/td));
    return stats;
}

static void line(Canvas &canvas, vec2 p1, vec2 p2)
{
    vec2 size = p2 - p1, halfSize = size * 0.5f;
    vec2 pad(1,1);
    Path *p = canvas.new_path(halfSize + pad, size + pad*2.0f);
    p->pos = vec2(p1);
    p->new_line({0,0}, size);
}

static void populate_canvas()
{
    FT_Library ftlib;
    FT_Face ftface;
    FT_Error fterr;
    FT_GlyphSlot ftglyph;

    if (needs_update) {
        canvas.clear();
        needs_update = false;
    } else if (canvas.num_drawables() > 0) {
        return;
    }

    color colors[] = {
        color("#251F39"),
        color("#51413A"),
        color("#9D6F7D"),
        color("#ECB188"),
        color("#CE552F")
    };

    canvas.set_render_mode(Text::render_as_text);
    //canvas.set_render_mode(Text::render_as_contour);

    canvas.set_fill_brush(Brush{BrushSolid, { }, { color(0.5f,0.5f,0.5f,1) }});
    Text *t = canvas.new_text();
    t->set_face(sans_norm);
    t->set_halign(text_halign_center);
    t->set_valign(text_valign_center);
    t->set_text(render_text);
    t->set_lang("en");
    //t->set_position(vec2(0,0));
    t->set_position(vec2(0,-32.0f));
    t->set_size(font_size);

    /* need text size for gradient and rounded rectangle size */
    vec2 text_pos = t->get_position();
    vec2 text_size = t->get_text_size();
    vec2 text_offset = t->get_text_offset();
    font_face_ft *face = static_cast<font_face_ft*>(t->get_face());
    std::vector<glyph_shape> shapes = t->get_glyph_shapes();
    text_segment segment = t->get_text_segment();

    /* freetype library and glyph pointers */
    ftface = face->ftface;
    ftglyph = ftface->glyph;
    ftlib = ftglyph->library;

    /* we need to set up our font metrics */
    face->get_metrics(segment.font_size);

    glm::mat3 m = canvas.get_transform();
    float scale = (m[0][0] + m[1][1]) / 2.0f;

    /* grid frame */
    canvas.set_stroke_brush(Brush{BrushSolid, { }, { color(0,0,0,1) }});
    canvas.set_stroke_width(1.0f);
    float um = 10, lr = 1.25f;
    vec2 grid_size = vec2(text_size.x*0.5f+um, text_size.y*0.5f*lr+um);
    vec2 grid_pos = vec2(text_pos.x, text_pos.y+text_size.y*(lr*0.5f-0.5f));
    float xs = grid_size.x, ys = grid_size.y;
    vec2 p1 = grid_pos + vec2(-xs,-ys), p2 = grid_pos + vec2(xs,-ys);
    vec2 p3 = grid_pos + vec2(-xs,ys),  p4 = grid_pos + vec2(xs,ys);
    line(canvas, p1, p2); line(canvas, p1, p3);
    line(canvas, p3, p4); line(canvas, p2, p4);

    /* grid lines */
    canvas.set_stroke_brush(Brush{BrushSolid, { }, { color(0.7,0.7,0.9,1) }});
    canvas.set_stroke_width(0.5f);
    float v = 20;
    int xlines = floorf(grid_size.x/v)*2+1; /* odd */
    int ylines = floorf(grid_size.y/v)*2+1; /* odd */
    for (int xl = 0; xl < xlines; xl++) {
        float xd = grid_size.x - (float)(xlines-1) * 0.5f * v;
        float x  = grid_pos.x+-grid_size.x + xd + xl * v;
        float y1 = grid_pos.y-grid_size.y, y2 = grid_pos.y+grid_size.y;
        line(canvas, vec2(x, y1), vec2(x, y2));
    }
    for (int yl = 0; yl < ylines; yl++) {
        float yd = grid_size.y - (float)(ylines-1) * 0.5f * v;
        float y  = grid_pos.y+-grid_size.y + yd + yl * v;
        float x1 = grid_pos.x-grid_size.x, x2 = grid_pos.x+grid_size.x;
        line(canvas, vec2(x1, y), vec2(x2, y));
    }

    /* get glyph internal dimensions */
    vec2 pos = text_pos + text_offset;
    for (size_t i = 0; i < shapes.size(); i++)
    {
        auto &shape = shapes[i];

        if ((fterr = FT_Load_Glyph(ftface, shape.glyph, FT_LOAD_NO_BITMAP))) {
            Panic("error: FT_Load_Glyph failed: glyph=%d fterr=%d\n",
                shape.glyph, fterr);
        }

        // segment.baseline_shift
        float x_hbearing = ftglyph->metrics.horiBearingX / 64.0f;
        float y_hbearing = ftglyph->metrics.horiBearingY / 64.0f;
        float x_advance = shape.x_advance / 64.0f;
        float y_advance = shape.y_advance / 64.0f;
        float width = ftglyph->metrics.width / 64.0f;
        float height = ftglyph->metrics.height / 64.0f;
        float descend = (height - y_hbearing);

        if (glyph_meta)
        {
            char hexbuf[64];
            size_t index = shape.cluster;
            size_t end_index = i < (shapes.size()-1) ? shapes[i+1].cluster : index + 1;
            size_t s = 0;
            while (index < end_index)
            {
                struct utf32_code u = utf8_to_utf32_code(render_text + index);
                if (s != 0) s += snprintf(hexbuf+s, sizeof(hexbuf)-s, ",");
                s += snprintf(hexbuf+s, sizeof(hexbuf)-s, "0x%02x", (int)u.code);
                index += u.len;
            }
            canvas.set_fill_brush(Brush{BrushSolid, { }, { color(0.2f,0.2f,0.2f,1) }});
            Text *t = canvas.new_text();
            t->set_face(sans_norm);
            t->set_halign(text_halign_left);
            t->set_valign(text_valign_top);
            t->set_text(hexbuf);
            t->set_lang("en");
            t->set_position(vec2(2,2)+pos);
            t->set_size(12.0f);
        }

        /* glyph box */
        canvas.set_stroke_brush(Brush{BrushSolid, { }, { color(0.3f,0.3f,0.7f,1) }});
        canvas.set_stroke_width(0.75f);
        float px = pos.x+x_hbearing, py = pos.y + descend+text_size.y;
        vec2 p1(px, py),                  p2(px, py-height);
        vec2 p3(px+width, py-height),     p4(px+width, py);
        line(canvas, p2, p1);             line(canvas, p2, p3);
        line(canvas, p3, p4);             line(canvas, p1, p4);

        /* advance box */
        canvas.set_stroke_brush(Brush{BrushSolid, { }, { color(0.7f,0.3f,0.3f,1) }});
        canvas.set_stroke_width(0.75f);
        float qx = pos.x, qy = pos.y;
        vec2 q1(qx, qy+text_size.y),      q2(qx, qy);
        vec2 q3(qx+x_advance,qy),         q4(qx+x_advance, qy+text_size.y);
        line(canvas, q2, q1);             line(canvas, q2, q3);
        line(canvas, q3, q4);             line(canvas, q1, q4);

        pos += vec2(x_advance,0.0f);
    }

    /* trick to move the rounded rectangle behind the text */
    Drawable::Ptr text = std::move(canvas.objects[0]);
    canvas.objects.erase(canvas.objects.begin());
    canvas.objects.push_back(std::move(text));
}

static void render_text_segment(draw_list &batch, font_manager_ft &manager,
    text_segment &segment)
{
    std::vector<glyph_shape> shapes;
    text_shaper_hb shaper;
    text_renderer_ft renderer(&manager);

    shaper.shape(shapes, segment);
    renderer.render(batch, shapes, segment);
}

static void render_stats_text(draw_list &batch, font_manager_ft &manager)
{
    float x = 10.0f, y = height - 10.0f;
    std::vector<std::string> stats = get_stats(sans_norm, td);
    uint32_t c = clear_color[0] == 1.0 ? 0xff404040 : 0xffc0c0c0;
    for (size_t i = 0; i < stats.size(); i++) {
        text_segment segment(stats[i], text_lang, sans_norm,
            (int)((float)stats_font_size * 64.0f), x, y, c);
        render_text_segment(batch, manager, segment);
        y -= (int)((float)stats_font_size * 1.334f);
    }
}

static void update()
{
    auto t = high_resolution_clock::now();
    tl = tn;
    tn = (double)duration_cast<nanoseconds>(t.time_since_epoch()).count()/1e9;
    td = tn - tl;

    /* start frame with empty draw list */
    draw_list_clear(batch);

    /* create canvas and overlay */
    populate_canvas();

    /* set up scale/translate matrix */
    glfwGetFramebufferSize(window, &width, &height);
    float s = state.zoom / 64.0f;
    float tx = state.origin.x + width/2.0f;
    float ty = state.origin.y + height/2.0f;
    canvas.set_transform(mat3(s,  0,  tx,
                              0,  s,  ty,
                              0,  0,  1));

    /* emit canvas draw list */
    canvas.emit(batch);

    /* render stats text */
    render_stats_text(batch, manager);

    /* synchronize canvas texture buffers */
    buffer_texture_create(shape_tb, canvas.ctx->shapes, GL_TEXTURE0, GL_R32F);
    buffer_texture_create(edge_tb, canvas.ctx->edges, GL_TEXTURE1, GL_R32F);
    buffer_texture_create(brush_tb, canvas.ctx->brushes, GL_TEXTURE2, GL_R32F);

    /* update vertex and index buffers arrays (idempotent) */
    vertex_buffer_create("vbo", &vbo, GL_ARRAY_BUFFER, batch.vertices);
    vertex_buffer_create("ibo", &ibo, GL_ELEMENT_ARRAY_BUFFER, batch.indices);
}

static void display()
{
    /* okay, lets send commands to the GPU */
    glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* draw list batch with tbo_iid canvas texture buffer special case */
    for (auto img : batch.images) {
        auto ti = tex_map.find(img.iid);
        if (ti == tex_map.end()) {
            tex_map[img.iid] = image_create_texture(img);
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
    if (action != GLFW_PRESS) return;
    switch(key) {
    case GLFW_KEY_ESCAPE: exit(0); break;
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
    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
        mouse_left_drag = (action == GLFW_PRESS);
        state_save = state;
        break;
    case GLFW_MOUSE_BUTTON_RIGHT:
        mouse_right_drag = (action == GLFW_PRESS);
        state_save = state;
        break;
    }
}

static void cursor_position(GLFWwindow* window, double xpos, double ypos)
{
    state.mouse_pos = vec2(xpos, ypos);

    if (mouse_left_drag) {
        state.origin += state.mouse_pos - state_save.mouse_pos;
        state_save.mouse_pos = state.mouse_pos;
    }
    else if (mouse_right_drag) {
        vec2 delta = state.mouse_pos - state_save.mouse_pos;
        float zoom = state_save.zoom * powf(65.0f/64.0f,(float)-delta.y);
        if (zoom != state.zoom && zoom > min_zoom && zoom < max_zoom) {
            state.zoom = zoom;
            state.origin = (state.origin * (zoom / state.zoom));
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
    vertex_array_1f(p, "a_gamma", 1.0f);
    glBindVertexArray(0);

    /* get font list */
    manager.msdf_autoload = true;
    manager.msdf_enabled = true;
    manager.scanFontDir("fonts");
    sans_norm = manager.findFontByPath(font_path);

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

    beam_cursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    glfwSetCursor(window, beam_cursor);

    initialize();
    reshape(width, height);
    while (!glfwWindowShouldClose(window)) {
        update();
        display();
        glfwSwapBuffers(window);
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
        "  -f, --font <ttf-file>     font file (default %s)\n"
        "  -s, --size <integer>      font size (default %d)\n"
        "  -t, --text <string>       text to render (default \"%s\")\n"
        "  -m, --meta                display glyph metadata\n"
        "  -h, --help            command line help\n",
        argv[0], font_path, font_size, render_text);
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
        if (match_opt(argv[i], "-h", "--help")) {
            help_text = true;
            i++;
        } else if (match_opt(argv[i], "-m", "--meta")) {
            glyph_meta = true;
            i++;
        } else if (match_opt(argv[i], "-f","--font")) {
            if (check_param(++i == argc, "--font")) break;
            font_path = argv[i++];
        } else if (match_opt(argv[i], "-s", "--size")) {
            if (check_param(++i == argc, "--size")) break;
            font_size = atoi(argv[i++]);
        } else if (match_opt(argv[i], "-t", "--text")) {
            if (check_param(++i == argc, "--text")) break;
            render_text = argv[i++];
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
