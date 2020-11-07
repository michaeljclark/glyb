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
#include "geometry.h"
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

static const char *stats_font_path = "fonts/Roboto-Regular.ttf";
static const char *display_font_path = "fonts/Roboto-Regular.ttf";
static const char *render_text = "qffifflLTA"; // "πάθος λόγος ἦθος";
static const char* text_lang = "en";
static const int stats_font_size = 18;
static int font_size = 256.0f;
static const float min_zoom = 16.0f, max_zoom = 32768.0f;
static std::array<float,4> clear_color = { 1.0f, 1.0f, 1.0f, 1.0f };
static int window_width = 2560, window_height = 1440;
static int framebuffer_width, framebuffer_height;
static double tl, tn, td;
static bool draw_grid = false, draw_boxes = false, draw_meta = false;
static bool help_text = false, needs_update = false;

/* canvas state */

struct zoom_state {
    float zoom;
    vec2 mouse_pos;
    vec2 origin;
};

static AContext ctx;
static font_face *display_face, *stats_face;
static draw_list batch;
static zoom_state state = { 128.0f }, state_save;
static bool mouse_left_drag = false;
static bool mouse_middle_drag = false;
static bool mouse_right_drag = false;

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

static std::vector<std::string> get_stats(double td)
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
    p->pos = p1;
    p->new_line({0,0}, size);
}

static void rect(Canvas &canvas, vec2 p1, vec2 p2)
{
    vec2 size = p2 - p1, halfSize = size * 0.5f;
    Rectangle *r = canvas.new_rounded_rectangle(vec2(0), halfSize, 20.0f);
    r->pos = p2 - halfSize;
}

static inline vec2 min(const vec2 &a, const vec2 &b)
{
    return { std::min(a.x, b.x), std::min(a.y, b.y) };
}

static inline vec2 max(const vec2 &a, const vec2 &b)
{
    return { std::max(a.x, b.x), std::max(a.y, b.y) };
}

static vec3 transform(vec2 p)
{
    return vec3(p, 1.0f) * canvas.get_transform();
}

static const float Inf = std::numeric_limits<float>::infinity();

static const color meta_color         = color(0.20f, 0.20f, 0.20f, 1.00f);
static const color text_color         = color(0.50f, 0.50f, 0.50f, 1.00f);
static const color frame_color        = color(0.00f, 0.00f, 0.00f, 1.00f);
static const color grid_color         = color(0.70f, 0.70f, 0.90f, 1.00f);
static const color glyphbox_color     = color(0.30f, 0.30f, 0.70f, 1.00f);
static const color advancebox_color   = color(0.70f, 0.30f, 0.30f, 1.00f);
static const color select_color       = color(0.75f, 0.85f, 1.00f, 1.00f);

static std::vector<glyph_shape> shapes;

static std::pair<int,int> text_selection = { -1, -1 };

static std::pair<int,int> find_glyph_selection(vec2 s[2])
{
    // find min and max bounds of text layout
    vec2 minbox(Inf), maxbox(-Inf);
    for (size_t i = 0; i < shapes.size(); i++) {
        auto &shape = shapes[i];
        auto &p = shape.pos;
        minbox = min(minbox, min(p[0],p[1]));
        maxbox = max(maxbox, max(p[0],p[1]));
    }

    // find left-to-right or right-to-left intersection bounds
    rect_2d lbox, rbox;
    vec2 min1 = min(s[0], s[1]), max1 = max(s[0], s[1]);
    if (min1.y < minbox.y) {
        /* handle cursor upwards - select from start of line */
        rbox = {{ minbox.x, min1.y }, { s[0].x,   max1.y }};
        lbox = {{ s[0].x, min1.y }, { s[0].x, max1.y }};
    } else if (max1.y > maxbox.y) {
        /* handle cursor downwards - select to end of line */
        rbox = {{ s[0].x, min1.y }, { max1.x, max1.y }};
        lbox = {{ s[0].x,   min1.y }, { maxbox.x, max1.y }};
    } else {
        lbox = rbox = rect_2d{ min1, max1 };
    }

    // find glyph at start and end of the intersection bounds
    int gs = -1, ge = -1;
    for (size_t i = 0; i < shapes.size(); i++)
    {
        auto &shape = shapes[i];
        auto &p = shape.pos;
        rect_2d gr{ min(p[0], p[1]), max(p[0], p[1]) };
        int li = intersect(gr, lbox), ri = intersect(gr, rbox);
        if (li & intersect_2d::inner && (ge == -1 || i > ge)) ge = i;
        if (ri & intersect_2d::inner && (gs == -1 || i < gs)) gs = i;
    }

    return {gs, ge};
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

    canvas.set_render_mode(Text::render_as_text);
    //canvas.set_render_mode(Text::render_as_contour);

    canvas.set_fill_brush(Brush{BrushSolid, { }, { text_color }});
    Text *t = canvas.new_text();
    t->set_face(display_face);
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
    shapes = t->get_glyph_shapes();
    text_segment segment = t->get_text_segment();

    /* freetype library and glyph pointers */
    ftface = face->ftface;
    ftglyph = ftface->glyph;
    ftlib = ftglyph->library;

    /* we need to set up our font metrics */
    face->get_metrics(segment.font_size);

    float um = 10;    /* margin */
    float lr = 1.25f; /* leading ratio*/

    vec2 adv_size = vec2(text_size.x, text_size.y*lr);
    vec2 grid_size = vec2(adv_size.x*0.5f+um, adv_size.y*0.5f+um);
    vec2 grid_pos = vec2(text_pos.x, text_pos.y+text_size.y*(lr*0.5f-0.5f));

    /* find and store text selection bounds */
    vec2 tl_ss, br_se, p = text_pos + text_offset;
    for (size_t i = 0; i < shapes.size(); i++)
    {
        float x_advance = shapes[i].x_advance / 64.0f;
        vec2 tl(p.x, p.y), br(p.x + x_advance, p.y + adv_size.y);

        size_t idx = shapes[i].cluster;
        size_t end = i < (shapes.size()-1) ? shapes[i+1].cluster
                                           : strlen(render_text);
        size_t glyph_count = 0;
        while (idx < end) {
            utf32_code u = utf8_to_utf32_code(render_text + idx);
            idx += u.len;
            glyph_count++;
        }
        if (glyph_count > 1) {
            /* handle ligature subdivision */
        }

        if (i == text_selection.first) tl_ss = tl;
        if (i == text_selection.second) br_se = br;

        shapes[i].pos[0] = transform(tl);
        shapes[i].pos[1] = transform(br);

        p += vec2(x_advance,0.0f);
    }

    /* draw selection rect */
    if (text_selection.first >= 0 && text_selection.second >= 0) {
        canvas.set_stroke_brush(Brush{BrushNone, { }, { }});
        canvas.set_fill_brush(Brush{BrushSolid, { }, { select_color }});
        rect(canvas, tl_ss, br_se);
    }

    /* draw grid frame */
    if (draw_grid) {
        canvas.set_stroke_brush(Brush{BrushSolid, { }, { frame_color }});
        canvas.set_stroke_width(1.0f);
        float xs = grid_size.x, ys = grid_size.y;
        vec2 p1 = grid_pos + vec2(-xs,-ys), p2 = grid_pos + vec2(xs,-ys);
        vec2 p3 = grid_pos + vec2(-xs,ys),  p4 = grid_pos + vec2(xs,ys);
        line(canvas, p1, p2); line(canvas, p1, p3);
        line(canvas, p3, p4); line(canvas, p2, p4);
    }

    /* draw grid lines */
    if (draw_grid) {
        canvas.set_stroke_brush(Brush{BrushSolid, { }, { grid_color }});
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
    }

    /* draw glyph boxes */
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

        if (draw_meta)
        {
            char hexbuf[64];
            size_t idx = shape.cluster;
            size_t end = i < (shapes.size()-1) ? shapes[i+1].cluster
                                               : strlen(render_text);
            size_t s = 0;
            while (idx < end) {
                utf32_code u = utf8_to_utf32_code(render_text + idx);
                if (s != 0) s += snprintf(hexbuf+s, sizeof(hexbuf)-s, ",");
                s += snprintf(hexbuf+s, sizeof(hexbuf)-s, "0x%02x", (int)u.code);
                idx += u.len;
            }
            canvas.set_fill_brush(Brush{BrushSolid, { }, { meta_color }});
            Text *t = canvas.new_text();
            t->set_face(stats_face);
            t->set_halign(text_halign_left);
            t->set_valign(text_valign_top);
            t->set_text(hexbuf);
            t->set_lang("en");
            t->set_position(vec2(2,2)+pos);
            t->set_size(12.0f);
        }

        if (draw_boxes) {

            /* glyph bounding box bottom left */
            float px = pos.x+x_hbearing, py = pos.y + descend+text_size.y;

            /* glyph advance box top left*/
            float qx = pos.x, qy = pos.y;

            /* glyph bounding box */
            vec2 p1(px, py),                p3(px+width, py);
            vec2 p2(px, py-height),         p4(px+width, py-height);

            /* glyph advance box and leading footer */
            vec2 q1(qx, qy+text_size.y),    q3(qx+x_advance, qy+text_size.y);
            vec2 q2(qx, qy),                q4(qx+x_advance, qy);
            vec2 q5(qx, qy+adv_size.y),     q6(qx+x_advance, qy+adv_size.y);

            /* draw glyph box lines */
            canvas.set_stroke_brush(Brush{BrushSolid, { }, { glyphbox_color }});
            canvas.set_stroke_width(0.75f);
            line(canvas, p2, p1);           line(canvas, p2, p4);
            line(canvas, p4, p3);           line(canvas, p1, p3);

            /* draw advance box lines */
            canvas.set_stroke_brush(Brush{BrushSolid, { }, { advancebox_color }});
            canvas.set_stroke_width(0.75f);
            line(canvas, q2, q1);           line(canvas, q2, q4);
            line(canvas, q4, q3);           line(canvas, q1, q3);

            /* draw leading box lines */
            line(canvas, q1, q5);           line(canvas, q3, q6);
            line(canvas, q5, q6);
        }

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
    float x = 10.0f, y = window_height - 10.0f;
    std::vector<std::string> stats = get_stats(td);
    uint32_t c = clear_color[0] == 1.0 ? 0xff404040 : 0xffc0c0c0;
    for (size_t i = 0; i < stats.size(); i++) {
        text_segment segment(stats[i], text_lang, stats_face,
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

    /* create canvas and overlay */
    populate_canvas();

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
        needs_update = true;
    } else if (yoffset > 0 && state.zoom > min_zoom) {
        state.origin /= ratio;
        state.zoom -= quantum;
        needs_update = true;
    }
}

static void mouse_text_select(vec2 span[2])
{
    auto range = find_glyph_selection(span);

    if (range == text_selection) return; /* unchanged */

    text_selection = range;
    needs_update = true;
}

static void mouse_button(GLFWwindow* window, int button, int action, int mods)
{
    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
        mouse_left_drag = (action == GLFW_PRESS);
        state_save = state;
        break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        /* FIXME - don't use middle button */
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
    state.mouse_pos = vec2(xpos, ypos);

    if (mouse_left_drag) {
        vec2 span[2] = { vec2(state_save.mouse_pos), vec2(state.mouse_pos) };
        mouse_text_select(span);
    }
    else if (mouse_middle_drag) {
        state.origin += state.mouse_pos - state_save.mouse_pos;
        state_save.mouse_pos = state.mouse_pos;
    }
    else if (mouse_right_drag) {
        vec2 delta = state.mouse_pos - state_save.mouse_pos;
        float zoom = state_save.zoom * powf(65.0f/64.0f,(float)-delta.y);
        if (zoom != state.zoom && zoom > min_zoom && zoom < max_zoom) {
            state.zoom = zoom;
            state.origin = (state.origin * (zoom / state.zoom));
            needs_update = true;
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
    display_face = manager.findFontByPath(display_font_path);
    stats_face = manager.findFontByPath(stats_font_path);

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

    beam_cursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    glfwSetCursor(window, beam_cursor);

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
        "  -f, --font <ttf-file>     font file (default %s)\n"
        "  -s, --size <integer>      font size (default %d)\n"
        "  -t, --text <string>       text to render (default \"%s\")\n"
        "  -G, --grid                display grid behind text\n"
        "  -B, --boxes               display glyph boxes\n"
        "  -M, --meta                display glyph metadata\n"
        "  -h, --help                command line help\n",
        argv[0], display_font_path, font_size, render_text);
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
        } else if (match_opt(argv[i], "-G", "--grid")) {
            draw_grid = true;
            i++;
        } else if (match_opt(argv[i], "-B", "--boxes")) {
            draw_boxes = true;
            i++;
        } else if (match_opt(argv[i], "-M", "--meta")) {
            draw_meta = true;
            i++;
        } else if (match_opt(argv[i], "-f","--font")) {
            if (check_param(++i == argc, "--font")) break;
            display_font_path = argv[i++];
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
