/*
 * glfw3 graph ui
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
#include "file.h"
#include "format.h"
#include "glcommon.h"

#include "ui9.h"

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

static const char *sans_norm_font_path = "fonts/DejaVuSans.ttf";
static const char *mono_norm_font_path = "fonts/RobotoMono-Regular.ttf";
static const char *mono_bold_font_path = "fonts/RobotoMono-Bold.ttf";
static const char *render_text = "πάθος λόγος ἦθος";
static const char* text_lang = "en";
static const int stats_font_size = 12;

static const float min_zoom = 16.0f, max_zoom = 32768.0f;
static std::array<float,4> clear_color = { 1.0f, 1.0f, 1.0f, 1.0f };
static float xscale, yscale;
static int window_width = 800, window_height = 600;
static int framebuffer_width, framebuffer_height;
static double tl, tn, td;
static bool help_text = false;
static bool overlay_stats = false;

/* canvas state */

struct zoom_state {
    float zoom;
    vec2 mouse_pos;
    vec2 origin;
};

static AContext ctx;
static font_face *sans_norm, *mono_norm, *mono_bold;
static draw_list batch;
static zoom_state state = { 64.0f }, state_save;
static bool mouse_left_drag = false, mouse_right_drag = false;
static int current_example;
static ui9::Root root(&manager);

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

static void create_layout(ui9::Root &root)
{
    auto frame1 = new ui9::Frame();
    frame1->set_text("Simulation Settings");
    frame1->set_position(vec3(0,0,0));
    root.add_child(frame1);

    auto grid1 = new ui9::Grid();
    grid1->set_rows_homogeneous(false);
    grid1->set_cols_homogeneous(false);
    frame1->add_child(grid1);

    const char* label_names[] = {
        "Damping",
        "Center Attract",
        "Time Step",
        "Maximum Speed",
        "Stopping Energy"
    };

    for (size_t i = 0; i < 5; i++)
    {
        auto l1 = new ui9::Label();
        l1->set_text(label_names[i]);
        l1->set_preferred_size({100,50,0});
        grid1->add_child(l1, 0, i);

        auto s1 = new ui9::Slider();
        s1->set_value((1.0f/6.0f) * (i+1));
        s1->set_preferred_size({300,50,0});
        grid1->add_child(s1, 1, i, 1, 1, { ui9::ratio, ui9::dynamic }, {1,1});

        auto l2 = new ui9::Label();
        l2->set_text(std::to_string(s1->get_value()));
        l2->set_preferred_size({100,50,0});
        grid1->add_child(l2, 2, i);

        s1->set_callback([=](float v) { l2->set_text(std::to_string(v)); });
    }

}

static void populate_canvas()
{
    if (canvas.num_drawables() > 0) return;

    if (!mono_norm) {
        mono_norm = manager.findFontByPath(mono_norm_font_path);
        mono_bold = manager.findFontByPath(mono_bold_font_path);
    }

    if (root.has_children()) return;

    create_layout(root);
    root.layout(&canvas);
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
    float x = 10.0f, y = window_height - 10.0f;
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
    glfwGetWindowSize(window, &window_width, &window_height);
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    float s = state.zoom / 64.0f;
    float tx = state.origin.x + window_width/2.0f;
    float ty = state.origin.y + window_height/2.0f;
    canvas.set_transform(mat3(s,  0,  tx,
                              0,  s,  ty,
                              0,  0,  1));
    canvas.set_scale(sqrtf((framebuffer_width * framebuffer_height) /
        (float)(window_width * window_height)));

    /* emit canvas draw list */
    root.layout(&canvas);
    canvas.emit(batch);

    /* render stats text */
    if (overlay_stats) {
        render_stats_text(batch, manager);
    }

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

static void reshape(int framebuffer_width, int framebuffer_height)
{
    glfwGetWindowSize(window, &window_width, &window_height);
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

    mvp = glm::ortho(0.0f, (float)window_width, (float)window_height,
        0.0f, 0.0f, 100.0f);

    glViewport(0, 0, framebuffer_width, framebuffer_height);

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
    case GLFW_KEY_S: overlay_stats = !overlay_stats; break;
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

static char q;
static char b;

static bool mouse_button_ui9(int button, int action, int mods, vec3 pos)
{
    switch(button) {
    case GLFW_MOUSE_BUTTON_LEFT:  b = ui9::left_button;  break;
    case GLFW_MOUSE_BUTTON_RIGHT: b = ui9::right_button; break;
    }
    switch(action) {
    case GLFW_PRESS:   q = ui9::pressed;  break;
    case GLFW_RELEASE: q = ui9::released; break;
    }
    vec3 v = canvas.get_inverse_transform() * pos;
    ui9::MouseEvent evt{{ui9::mouse, q}, b, v};
    return root.dispatch(&evt.header);
}

static void mouse_button(GLFWwindow* window, int button, int action, int mods)
{
    if (mouse_button_ui9(button, action, mods, vec3(state.mouse_pos, 1))) {
        return;
    }

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

static bool mouse_motion_ui9(vec3 pos)
{
    vec3 v = canvas.get_inverse_transform() * pos;
    ui9::MouseEvent evt{{ui9::mouse, ui9::motion}, b, v};
    return root.dispatch(&evt.header);
}

static void cursor_position(GLFWwindow* window, double xpos, double ypos)
{
    state.mouse_pos = vec2(xpos, ypos);

    if (mouse_motion_ui9(vec3(state.mouse_pos, 1))) {
        return;
    }

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
    sans_norm = manager.findFontByPath(sans_norm_font_path);

    /* pipeline */
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
}

/* GLFW GUI entry point */

static void resize(GLFWwindow* window, int framebuffer_width, int framebuffer_height)
{
    reshape(framebuffer_width, framebuffer_height);
}

static void glcanvas(int argc, char **argv)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, CTX_OPENGL_MAJOR);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, CTX_OPENGL_MINOR);

    window = glfwCreateWindow(window_width, window_height, argv[0], NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL();
    glfwSwapInterval(1);
    glfwSetScrollCallback(window, scroll);
    glfwSetKeyCallback(window, keyboard);
    glfwSetMouseButtonCallback(window, mouse_button);
    glfwSetCursorPosCallback(window, cursor_position);
    glfwSetFramebufferSizeCallback(window, resize);
    glfwGetWindowSize(window, &window_width, &window_height);
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

    initialize();
    reshape(framebuffer_width, framebuffer_height);
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
        "  -h, --help            command line help\n"
        "  -y, --overlay-stats   show statistics overlay\n",
        argv[0]);
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
        } else if (match_opt(argv[i], "-y", "--overlay-stats")) {
            overlay_stats = true;
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
