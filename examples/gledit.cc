/*
 * glfw3 text editor demo
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
#include <tuple>
#include <string>
#include <algorithm>
#include <atomic>
#include <mutex>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define CTX_OPENGL_MAJOR 3
#define CTX_OPENGL_MINOR 3

#include "glm/glm.hpp"
#include "glm/ext/matrix_clip_space.hpp"

#include "binpack.h"
#include "image.h"
#include "draw.h"
#include "font.h"
#include "glyph.h"
#include "text.h"
#include "logger.h"
#include "utf8.h"
#include "color.h"
#include "geometry.h"
#include "glcommon.h"

using mat3 = glm::mat3;
using mat4 = glm::mat4;
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using dvec2 = glm::dvec2;

/* constants */

static const color block_color =  color(0.85f, 0.85f, 0.85f, 1.00f);
static const color select_color = color(0.75f, 0.85f, 1.00f, 1.00f);

static const float Inf = std::numeric_limits<float>::infinity();


/* globals */

static GLuint vao, vbo, ibo;
static program simple, msdf;
static draw_list batch;
static std::map<int,GLuint> tex_map;

static mat4 mvp;
static GLFWwindow* window;
static GLFWcursor* beam_cursor;

static bool debug = false;
static int width = 1024, height = 768;
static font_manager_ft manager;

struct zoom_state
{
    float zoom;
    dvec2 mouse_pos;
    vec2 origin;
};

struct text_state
{
    mat3 matrix, inv_matrix;
    std::vector<text_segment> segments;
    std::vector<std::vector<glyph_shape>> segment_shapes;
    text_container container;
};

struct glyph_offset
{
    uint segment;
    uint shape;

    bool operator==(const glyph_offset &o) const {
        return std::tie(segment,shape) == std::tie(o.segment,o.shape); }
    bool operator!=(const glyph_offset &o) const {
        return std::tie(segment,shape) != std::tie(o.segment,o.shape); }
    bool operator<(const glyph_offset &o) const {
        return std::tie(segment,shape) < std::tie(o.segment,o.shape); }
    bool operator<=(const glyph_offset &o) const {
        return std::tie(segment,shape) <= std::tie(o.segment,o.shape); }
    bool operator>(const glyph_offset &o) const {
        return std::tie(segment,shape) > std::tie(o.segment,o.shape); }
    bool operator>=(const glyph_offset &o) const {
        return std::tie(segment,shape) >= std::tie(o.segment,o.shape); }
};

static const float min_zoom = 16.0f, max_zoom = 32768.0f;
static zoom_state state = { 64.0f }, state_save;
static text_state ts;
static bool mouse_left_drag = false;
static bool mouse_middle_drag = false;
static bool mouse_right_drag = false;
static bool display_blocks = false;
static std::pair<glyph_offset,glyph_offset> text_selection;


/* display  */

static void update_uniforms(program *prog)
{
    uniform_matrix_4fv(prog, "u_mvp", (const GLfloat *)&mvp[0][0]);
    uniform_1i(prog, "u_tex0", 0);
}

static program* cmd_shader_gl(int cmd_shader)
{
    switch (cmd_shader) {
    case shader_simple:  return &simple;
    case shader_msdf:    return &msdf;
    default: return nullptr;
    }
}

static void update_buffers();

static void display()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    update_buffers();

    for (auto img : batch.images) {
        auto ti = tex_map.find(img.iid);
        if (ti == tex_map.end()) {
            tex_map[img.iid] = image_create_texture(img);
        }
    }
    glBindVertexArray(vao);
    for (auto cmd : batch.cmds) {
        glUseProgram(cmd_shader_gl(cmd.shader)->pid);
        glBindTexture(GL_TEXTURE_2D, tex_map[cmd.iid]);
        glDrawElements(cmd_mode_gl(cmd.mode), cmd.count, GL_UNSIGNED_INT,
            (void*)(cmd.offset * sizeof(uint)));
    }

    glfwSwapBuffers(window);
}

static void reshape(int width, int height)
{
    mvp = glm::ortho(0.0f, (float)width,(float)height, 0.0f, 0.0f, 100.0f);

    glViewport(0, 0, width, height);

    glUseProgram(simple.pid);
    update_uniforms(&simple);

    glUseProgram(msdf.pid);
    update_uniforms(&msdf);
}

/* text */

static void create_text(text_container &c)
{
#if 0
    c.append(text_part("Γειά ",
        {{ "tracking", "2" }, { "baseline-shift", "9" }, { "color", "#800000" }}));
    c.append(text_part("σου ",
        {{ "tracking", "2" }, { "baseline-shift", "6" }, { "color", "#008000" }}));
    c.append(text_part("Κόσμε ",
        {{ "tracking", "2" }, { "baseline-shift", "3" }, { "color", "#000080" }}));
    c.append(text_part("Ελευθερία, Ισότητα, Αδελφότητα",
        {{ "font-size", "24" }, { "font-style", "bold" }, { "color", "#000000" }}));
#endif
    c.append(text_part(
        "    Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor "
        "incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud "
        "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure "
        "dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. "
        "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
        "mollit anim id est laborum.",
        {{ "font-size", "18" }, { "font-style", "bold" }, { "color", "#000000" }}));
    c.append(text_part("  Liberty, Equality, Fraternity",
        {{ "font-size", "24" }, { "font-style", "bold" }, { "color", "#000000" }}));
    c.append(text_part(
        "    Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor "
        "incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud "
        "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure "
        "dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. "
        "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
        "mollit anim id est laborum.",
        {{ "font-size", "36" }, { "font-style", "regular" }, { "color", "#1f1f5f" }}));
}

/* geometry */

static vec2 transform(vec2 pos, mat3 matrix)
{
    vec3 v = vec3(pos, 1.0f) * matrix;
    return vec2(v.x / v.z, v.y / v.z);
}

static image* get_pixel()
{
    static std::shared_ptr<image> img;
    static uint8_t pixels[4] = { 0xff, 0xff, 0xff, 0xff };

    if (!img) {
        img = std::shared_ptr<image>
            (new image(file_ptr(), 1, 1, pixel_format_rgba, pixels));
    }

    return img.get();
}

static void rect(draw_list &batch, vec2 A, vec2 B, float z, uint color, glm::mat3 m = glm::mat3(1))
{
    vec2 v1 = transform(A,m), v2 = transform(B,m);
    float uv1 = 0.0f, uv2 = 1.0f;
    uint o = static_cast<uint>(batch.vertices.size());
    uint o0 = draw_list_vertex(batch, {{v1.x, v1.y, z}, {uv1, uv1}, color});
    uint o1 = draw_list_vertex(batch, {{v2.x, v1.y, z}, {uv2, uv1}, color});
    uint o2 = draw_list_vertex(batch, {{v2.x, v2.y, z}, {uv2, uv2}, color});
    uint o3 = draw_list_vertex(batch, {{v1.x, v2.y, z}, {uv1, uv2}, color});
    draw_list_indices(batch, get_pixel()->iid, mode_triangles, shader_simple,
        {o0, o3, o1, o1, o3, o2});
    draw_list_image_delta(batch, get_pixel(),
        bin_rect(bin_point(1,1),bin_point(0,0)), st_clamp | filter_linear);
}

static inline vec2 min(vec2 a, vec2 b) { return vec2(std::min(a.x, b.x), std::min(a.y, b.y)); }
static inline vec2 max(vec2 a, vec2 b) { return vec2(std::max(a.x, b.x), std::max(a.y, b.y)); }

static void print_state(const char* state)
{
    static const char* last;
    if (last == state) return;
    printf("state: %s\n", (last = state));
}

static void text_range_draw_glyph_rects(text_state &ts,
    std::pair<glyph_offset,glyph_offset> r, color block_color, mat3 m)
{
    uint col = block_color.rgba32();
    for (size_t i = r.first.segment; i <= r.second.segment; i++) {
        if (i >= ts.segments.size()) break;
        size_t start_shape = (i == r.first.segment)
            ? r.first.shape : 0;
        size_t end_shape = (i == r.second.segment)
            ? r.second.shape + 1 : ts.segment_shapes[i].size();
        for (size_t j = start_shape; j < end_shape; j++)
        {
            auto &shape = ts.segment_shapes[i][j];
            rect(batch, shape.pos[0], shape.pos[1], 0, col);
        }
    }
}

static void text_range_draw_selection_rects(text_state &ts,
    std::pair<glyph_offset,glyph_offset> r, color select_color, mat3 m)
{
    float xmin = Inf, xmax = -Inf;
    float xminf = Inf, xminl = -Inf;
    uint col = select_color.rgba32();
    for (size_t i = r.first.segment; i <= r.second.segment; i++) {
        if (i >= ts.segments.size()) break;
        size_t start_shape = (i == r.first.segment)
            ? r.first.shape : 0;
        size_t end_shape = (i == r.second.segment)
            ? r.second.shape + 1 : ts.segment_shapes[i].size();
        for (size_t j = start_shape; j < end_shape; j++)
        {
            auto &shape = ts.segment_shapes[i][j];

            /* skip empty bounding box */
            if (shape.pos[1] - shape.pos[0] == vec3(0)) continue;

            /* find bounds */
            xmin = std::min(xmin, shape.pos[0].x);
            xmax = std::max(xmax, shape.pos[1].x);

            /* find start of first line */
            if (i == r.first.segment) {
                xminf = std::min(xminf, shape.pos[0].x);
            }
            /* find end of last line */
            if (i == r.second.segment) {
                xminl = std::max(xminl, shape.pos[1].x);
            }
        }
    }
    for (size_t i = r.first.segment; i <= r.second.segment; i++) {
        if (i >= ts.segments.size()) break;
        auto &segment = ts.segments[i];
        auto &shape0 = ts.segment_shapes[i][0];

        /*
         * segment coordinates are pre-transform.
         * multiply them by the active transformation matrix.
         */
        float sx = segment.x;
        float fs = segment.font_size/64.0f;
        float sy1 = segment.y - fs - segment.baseline_shift;
        float sy2 = segment.y + fs/4.0f - segment.baseline_shift;
        vec3 vy1 = vec3(sx, sy1, 1.0f) * m;
        vec3 vy2 = vec3(sx, sy2, 1.0f) * m;
        float y1 = vy1.y / vy1.z;
        float y2 = vy2.y / vy2.z;

        if (i == r.first.segment && r.first.segment != r.second.segment)
        {
            rect(batch, vec2(xminf, y1), vec2(xmax,  y2), 0, col);
        }
        else if (i == r.second.segment && r.first.segment != r.second.segment)
        {
            rect(batch, vec2(xmin,  y1), vec2(xminl, y2), 0, col);
        }
        else {
            rect(batch, vec2(xmin,  y1), vec2(xmax,  y2), 0, col);
        }
    }
}

static auto find_glyph_selection(text_state &ts, vec2 s[2])
{
    /*
     * Text selection needs to handle script direction rules
     * for LTR (Left-To-Right) and RTL (Right-to-left) text.
     *
     * Text direction can be inferred from language tags on the
     * text segment or by content analysis. The rules are simple
     * when the lines only contain one script direction:
     *
     * Single-line:
     *
     *            a      b                     b      a
     *            v      v                     v      v
     *   ---- --- [ ssss ] --- ----   ---- --- [ ssss ] --- ----
     *
     * LTR (Left-to-right):
     *
     *            a                                   a
     *            v                                   v
     *   ---- --- [ ssss sssss ssss   ---- ----- ---- [ sss ssss
     *   sssss ssss sssss ssss ssss   sssss ssss sssss ssss ssss
     *   ssss sssss ssss ] --- ----   ssss sss ] ---- ----- ----
     *                   ^                     ^
     *                   b                     b
     *
     * RTL (Right-to-left):
     *
     *            a                                   a
     *            v                                   v
     *   ssss sss ] ---- ----- ----   ssss sssss ssss ] --- ----
     *   sssss ssss sssss ssss ssss   sssss ssss sssss ssss ssss
     *   ---- ----- ---- [ sss ssss   ---- --- [ ssss sssss ssss
     *                   ^                     ^
     *                   b                     b
     *
     * Code here is *broken* direction agnostic geometric coverage.
     */
    size_t num_segments = ts.segments.size();
    glyph_offset min{0,0}, max{0,0};
    for (size_t i = 0; i < num_segments; i++) {
        if (i >= ts.segments.size()) break;
        size_t num_shapes = ts.segment_shapes[i].size();
        auto &segment = ts.segments[i];
        for (size_t j = 0; j < num_shapes; j++) {
            auto &shape = ts.segment_shapes[i][j];
            auto &p = shape.pos;
            bool c = std::min(p[0].x, p[1].x) <= std::max(s[0].x, s[1].x) &&
                     std::max(p[0].x, p[1].x) >= std::min(s[0].x, s[1].x) &&
                     std::min(p[0].y, p[1].y) <= std::max(s[0].y, s[1].y) &&
                     std::max(p[0].y, p[1].y) >= std::min(s[0].y, s[1].y);
            if (c) {
                glyph_offset c{(uint)i,(uint)j};
                if (min == glyph_offset{0,0} || c < min) min = c;
                if (max == glyph_offset{0,0} || c > max) max = c;
            }
        }
    }
    return std::pair<glyph_offset,glyph_offset>(min,max);
}

static std::string to_string(glyph_offset o)
{
    return std::string("{") + std::to_string(o.segment) +
        std::string(",") + std::to_string(o.shape) + std::string("}");
}

static std::string text_range_to_string(text_state &ts,
    std::pair<glyph_offset,glyph_offset> range)
{
    std::string s;
    for (size_t i = range.first.segment; i <= range.second.segment; i++) {
        size_t start_shape = (i == range.first.segment)
            ? range.first.shape : 0;
        size_t end_shape = (i == range.second.segment)
            ? range.second.shape + 1 : ts.segment_shapes[i].size();
        for (size_t j = start_shape; j < end_shape; j++) {
            auto &segment = ts.segments[i];
            auto &shape = ts.segment_shapes[i][j];
            size_t l = utf8_codelen(segment.text.data() + shape.cluster);
            s += std::string(segment.text.data() + shape.cluster, l);
        }
    };
    return s;
}

static std::pair<glyph_offset,glyph_offset> text_range_all(text_state &ts)
{
    size_t segments = ts.segments.size();
    size_t glyphs = ts.segments.size() ? ts.segment_shapes.back().size() : 0;

    return { glyph_offset{0,0}, glyph_offset{ (uint)segments, (uint)glyphs} };
}

static void update_geometry()
{
    text_shaper_hb shaper;
    text_renderer_ft renderer(&manager);
    text_layout layout(&manager, &shaper, &renderer);

    /* set up scale/translate matrix */
    glfwGetFramebufferSize(window, &width, &height);
    float s = state.zoom / 64.0f;
    float tx = state.origin.x;
    float ty = state.origin.y;
    mat3 m(s,  0,  tx,
           0,  s,  ty,
           0,  0,  1);

    ts.matrix = m;
    ts.inv_matrix = glm::inverseTranspose(m);

    draw_list_clear(batch);

    if (mouse_left_drag) {
        vec2 a = min(state.mouse_pos, state_save.mouse_pos);
        vec2 b = max(state.mouse_pos, state_save.mouse_pos);
        rect(batch, a, b, 0, 0xffeeeeee);
    }
    if (text_selection.second != glyph_offset{0,0}) {
        text_range_draw_selection_rects(ts, text_selection, select_color, m);
    }
    if (display_blocks) {
        text_range_draw_glyph_rects(ts, text_range_all(ts), block_color, m);
    }

    ts.segments.clear();
    layout.layout(ts.segments, &ts.container, 50, 50, 900, 700);
    size_t num_segments = ts.segments.size();
    ts.segment_shapes.resize(num_segments);
    for (size_t i = 0; i < num_segments; i++) {
        ts.segment_shapes[i].clear();
        shaper.shape(ts.segment_shapes[i], ts.segments[i]);
        renderer.render(batch, ts.segment_shapes[i], ts.segments[i], m);
    }
}

static void vertex_array_config(program *prog)
{
    vertex_array_pointer(prog, "a_pos", 3, GL_FLOAT, 0, &draw_vertex::pos);
    vertex_array_pointer(prog, "a_uv0", 2, GL_FLOAT, 0, &draw_vertex::uv);
    vertex_array_pointer(prog, "a_color", 4, GL_UNSIGNED_BYTE, 1, &draw_vertex::color);
    vertex_array_1f(prog, "a_gamma", 2.0f);
}

static void update_buffers()
{
    /* create vertex and index arrays */
    update_geometry();
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    vertex_buffer_create("vbo", &vbo, GL_ARRAY_BUFFER, batch.vertices);
    vertex_buffer_create("ibo", &ibo, GL_ELEMENT_ARRAY_BUFFER, batch.indices);
    vertex_array_config(&simple);
    glBindVertexArray(0);
}

/* keyboard callback */

static void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS) return;
    switch(key) {
    case GLFW_KEY_ESCAPE: exit(0); break;
    case GLFW_KEY_B:
        if (action == GLFW_PRESS) display_blocks = !display_blocks;
        break;
    }
}

/* mouse callbacks */

static void scroll(GLFWwindow* window, double xoffset, double yoffset)
{
    float quantum = state.zoom / 16.0f;
    float ratio = (float)quantum / (float)state.zoom;
    vec2 origin_delta = (state.origin - vec2(state.mouse_pos));
    if (yoffset < 0 && state.zoom < max_zoom) {
        state.origin += origin_delta * ratio;
        state.zoom += quantum;
    } else if (yoffset > 0 && state.zoom > min_zoom) {
        state.origin -= origin_delta * ratio;
        state.zoom -= quantum;
    }
}

static void mouse_text_select(text_state &ts, vec2 span[2])
{
    auto range = find_glyph_selection(ts, span);

    if (range == text_selection) return; /* unchanged */
    text_selection = range;

    if (debug) {
        printf("mouse_text_select: (%s - %s) text=\"%s\"\n",
            to_string(range.first).c_str(), to_string(range.second).c_str(),
            text_range_to_string(ts, range).c_str());
    }
}

static void mouse_button(GLFWwindow* window, int button, int action, int mods)
{
    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
        mouse_left_drag = (action == GLFW_PRESS);
        state_save = state;
        glfwSetCursor(window, mouse_left_drag ? beam_cursor : NULL);
        break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        mouse_middle_drag = (action == GLFW_PRESS);
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
    state.mouse_pos = dvec2(xpos, ypos);

    if (mouse_left_drag) {
        vec2 span[2] = { vec2(state_save.mouse_pos), vec2(state.mouse_pos) };
        mouse_text_select(ts, span);
    }
    else if (mouse_middle_drag) {
        state.origin += state.mouse_pos - state_save.mouse_pos;
        state_save.mouse_pos = state.mouse_pos;
    }
    else if (mouse_right_drag) {
        dvec2 delta = state.mouse_pos - state_save.mouse_pos;
        float zoom = state_save.zoom * powf(65.0f/64.0f,(float)-delta.y);
        if (zoom != state.zoom && zoom > min_zoom && zoom < max_zoom) {
            vec2 origin_delta = (state.origin - vec2(state.mouse_pos));
            float zoom_diff = (zoom / state.zoom);
            state.origin -= origin_delta * (1.0f - zoom_diff);
            state.zoom = zoom;
        }
    }
}


/* OpenGL initialization */

static void initialize()
{
    GLuint simple_fsh, msdf_fsh, vsh;

    /* shader program */
    vsh = compile_shader(GL_VERTEX_SHADER, "shaders/simple.vsh");
    simple_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/simple.fsh");
    msdf_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/msdf.fsh");
    link_program(&simple, vsh, simple_fsh);
    link_program(&msdf, vsh, msdf_fsh);
    glDeleteShader(vsh);
    glDeleteShader(simple_fsh);
    glDeleteShader(msdf_fsh);

    /* get font list */
    manager.msdf_autoload = true;
    manager.msdf_enabled = true;
    manager.scanFontDir("fonts");

    /* pipeline */
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glLineWidth(1.0);

    /* create text */
    create_text(ts.container);
}

/* GLFW GUI entry point */

static void resize(GLFWwindow* window, int width, int height)
{
    reshape(width, height);
}

static void glfont(int argc, char **argv)
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
    glfont(argc, argv);
    return 0;
}
