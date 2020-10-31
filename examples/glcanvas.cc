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

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define CTX_OPENGL_MAJOR 3
#define CTX_OPENGL_MINOR 3

#include "glm/glm.hpp"
#include "glm/ext/matrix_clip_space.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

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

static const char *sans_norm_font_path = "fonts/DejaVuSans.ttf";
static const char *mono_norm_font_path = "fonts/RobotoMono-Regular.ttf";
static const char *mono_bold_font_path = "fonts/RobotoMono-Bold.ttf";
static const char *render_text = "πάθος λόγος ἦθος";
static const char* text_lang = "en";
static const int stats_font_size = 18;

static const float min_zoom = 16.0f, max_zoom = 32768.0f;
static std::array<float,4> clear_color = { 1.0f, 1.0f, 1.0f, 1.0f };
static int width = 1024, height = 768;
static double tl, tn, td;
static bool help_text = false;

/* canvas state */

struct zoom_state {
    float zoom;
    dvec2 mouse_pos;
    vec2 origin;
};

static AContext ctx;
static font_face *sans_norm, *mono_norm, *mono_bold;
static draw_list batch;
static zoom_state state = { 64.0f }, state_save;
static bool mouse_left_drag = false, mouse_right_drag = false;
static int current_example = 4;

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
    std::vector<char> buf;
    int len;
    va_list ap;

    va_start(ap, fmt);
    len = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);

    buf.resize(len+1);
    va_start(ap, fmt);
    vsnprintf(buf.data(), len+1, fmt, ap);
    va_end(ap);

    return std::string(buf.data(), len);
}

static std::vector<std::string> get_stats(font_face *face, double td)
{
    std::vector<std::string> stats;
    stats.push_back(format_string("frames-per-second: %5.2f", 1.0/td));
    return stats;
}

static void do_example_text1()
{
    static size_t item_current = 0;
    static font_face *face = nullptr;
    static const auto &font_list = manager.getFontList();

    clear_color = { 1.0f, 1.0f, 1.0f, 1.0f };

    /* controller - cl */

    if (ImGui::BeginCombo("font", font_list[item_current]->name.c_str(), 0)) {
        for (size_t i = 0; i < font_list.size(); i++) {
            bool is_selected = (item_current == i);
            if (ImGui::Selectable(font_list[i]->name.c_str(), is_selected)) {
                item_current = i;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (face != font_list[item_current].get()) {
        face = font_list[item_current].get();
        canvas.clear();
    }

    if (canvas.num_drawables() > 0) return;

    canvas.set_fill_brush(Brush{BrushSolid, { }, { color(0,0,0,1) }});
    Text *t = canvas.new_text();
    t->set_face(face);
    t->set_halign(text_halign_center);
    t->set_valign(text_valign_center);
    t->set_text(render_text);
    t->set_lang("en");
    t->set_position(vec2(0));
    t->set_size(64.0f);

    /* need text size for gradient and rounded rectangle size */
    vec2 text_size = t->get_text_size();

    /* create rounded rectangle with a gradient fill */
    canvas.set_fill_brush(Brush{
        BrushAxial, { vec2(0,0), vec2(0, text_size.y*2.0f) },
        { color(0.80f,0.80f,0.80f,1.0f), color(0.50f,0.50f,0.50f,1.0f) }
    });
    Rectangle *r = canvas.new_rounded_rectangle(vec2(0),
        vec2(text_size.x/1.85f,text_size.y), text_size.y/2.0f);

    /* trick to move the rounded rectangle behind the text */
    std::swap(canvas.objects[0], canvas.objects[1]);
}

static void do_example_circle1()
{
    static float rot = 0.0f;

    clear_color = { 1.0f, 1.0f, 1.0f, 1.0f };

    if (ImGui::SliderFloat("rotation", &rot, 0.0f, 360.0f)) {
        canvas.clear();
    }

    if (canvas.num_drawables() > 0) return;

    color colors[] = {
        color("#251F39"),
        color("#51413A"),
        color("#9D6F7D"),
        color("#ECB188"),
        color("#CE552F")
    };

    float l = 200.0f, r = 90.0f;

    canvas.set_fill_brush(Brush{BrushSolid, { }, { color(0.5f,0.5f,0.5f,1.0f) }});
    canvas.set_stroke_brush(Brush{BrushSolid, { }, { color(0.3f,0.3f,0.3f,1.0f) }});
    canvas.set_stroke_width(5.0f);
    canvas.new_circle(vec2(0), r);

    for (size_t i = 0; i < 5; i++) {
        float phi = (float)i * (float)M_PI * (2.0f/5.0f) - rot * (float)M_PI / 180.0f;
        canvas.set_fill_brush(Brush{BrushSolid, { }, { colors[i] }});
        canvas.set_stroke_brush(Brush{BrushSolid, { }, { colors[i].brighten(0.5) }});
        canvas.new_circle(vec2(sinf(phi) * l, cosf(phi) * l), r);
    }
}

static void do_example_curve1()
{
    clear_color = { 1.0f, 1.0f, 1.0f, 1.0f };

    if (canvas.num_drawables() > 0) return;

    canvas.set_fill_brush(Brush{BrushSolid, { }, { color(0.0f,0.0f,0.0f,0.0f) }});
    canvas.set_stroke_brush(Brush{BrushSolid, { }, { color(0.3f,0.3f,0.3f,1.0f) }});
    canvas.set_stroke_width(5.0f);

    for (size_t x = 0; x < 4; x++)
    {
        for (size_t y = 0; y < 3; y++)
        {
            Path *p1 = canvas.new_path({0.0f,0.0f},{100.0f,100.0f});
            p1->pos = { -380.0f + x * 220.0f, -270.0f + y * 220.0f };
            p1->new_quadratic_curve({0.0f,0.0f}, {0.0f,50.0f}, {50.0f,50.0f});
            p1->new_quadratic_curve({50.0f,50.0f}, {100.0f,50.0f}, {100.0f,100.0f});

            Path *p2 = canvas.new_path({0.0f,0.0f},{100.0f,100.0f});
            p2->pos = { -270.0f + x * 220.0f, -160.0f + y * 220.0f };
            p2->new_quadratic_curve({0.0f,0.0f}, {50.0f,0.0f}, {50.0f,50.0f});
            p2->new_quadratic_curve({50.0f,50.0f}, {50.0f,100.0f}, {100.0f,100.0f});
        }
    }
}

static void do_example_node1()
{
    clear_color = { 0.1f, 0.1f, 0.1f, 1.0f };

    if (canvas.num_drawables() > 0) return;

    if (!mono_norm) {
        mono_norm = manager.findFontByPath(mono_norm_font_path);
        mono_bold = manager.findFontByPath(mono_bold_font_path);
    }

    TextStyle text_style_default{
        12.0f,
        mono_norm,
        text_halign_left,
        text_valign_center,
        "en",
        Brush{BrushSolid, { }, { color(1.0f,1.0f,1.0f,1.0f) }},
        Brush{BrushNone, { }, { }}
    };

    canvas.set_fill_brush(Brush{BrushSolid, { }, { color(0.15f,0.15f,0.15f,1.0f) }});
    canvas.set_stroke_brush(Brush{BrushSolid, { }, { color(0.7f,0.7f,0.7f,1.0f) }});
    canvas.set_stroke_width(1.0f);

    float x = -300.0f, y = 20.0f;
    size_t num_reg = 32;
    float cellh = 18.0f, regx = 70.0f, regw = 140.0f;
    float w = 250.0f, h = cellh * num_reg + 50.0f;
    Rectangle *r1 = canvas.new_rounded_rectangle(vec2(0), vec2(w/2,h/2), 5.0f);
    r1->pos = { x, y };

    Text *th1 = canvas.new_text(text_style_default);
    th1->pos = { x - w/2 + 10.0f, y - h/2 + 20.0f };
    th1->set_face(mono_bold);
    th1->set_text("Register File");

    vec2 out_c[32];

    for (size_t i = 0; i < num_reg; i++) {

        float yo = y - h/2 + 50.0f + i * cellh;

        std::string regname = std::string("x") + std::to_string(i);

        Text *t1 = canvas.new_text(text_style_default);
        t1->pos = { x - w/2 + 10.0f, yo };
        t1->set_text(regname);

        canvas.set_fill_brush(Brush{BrushSolid, { }, { color(0.3f,0.3f,0.3f,1.0f) }});

        Rectangle *r2 = canvas.new_rectangle(vec2(0), vec2(regw/2,7.0f));
        r2->pos = { x - w/2 + regx + regw/2, yo + 1 };

        Text *t2 = canvas.new_text(text_style_default);
        t2->pos = { x - w/2 + regx + 5.0f, yo };
        t2->set_text("0xffffffffffffffff");

        canvas.set_fill_brush(Brush{BrushSolid, { }, { color(0.15f,0.15f,0.15f,1.0f) }});

        out_c[i] = { x + w/2 - 20.0f, yo };
        Circle *c1 = canvas.new_circle(vec2(0), 5.0f);
        c1->pos = out_c[i];
    }

    x = 20.0f, y = -100.0f;
    w = 300.0f, h = 75.0f;
    regx = 150.0f;
    Rectangle *r2 = canvas.new_rounded_rectangle(vec2(0), vec2(w/2,h/2), 5.0f);
    r2->pos = { x, y };

    Text *th2 = canvas.new_text(text_style_default);
    th2->pos = { x - w/2 + 10.0f, y - h/2 + 20.0f };
    th2->set_face(mono_bold);
    th2->set_text("Register Monitor");

    vec2 in_c[1];
    {
        float yo = y + 10.0f;

        in_c[0] = { x - w/2 + 20.0f, yo };
        Circle *c1 = canvas.new_circle(vec2(0), 5.0f);
        c1->pos = in_c[0];

        Text *t1 = canvas.new_text(text_style_default);
        t1->pos = { x - w/2 + 40.0f, yo };
        t1->set_text("Match Value");

        canvas.set_fill_brush(Brush{BrushSolid, { }, { color(0.3f,0.3f,0.3f,1.0f) }});

        Rectangle *r2 = canvas.new_rectangle(vec2(0), vec2(regw/2,7.0f));
        r2->pos = { x - w/2 + regx + regw/2, yo + 1 };

        Text *t2 = canvas.new_text(text_style_default);
        t2->pos = { x - w/2 + regx + 5.0f, yo };
        t2->set_text("0xdeadbeaffeedbeef");

        canvas.set_fill_brush(Brush{BrushSolid, { }, { color(0.15f,0.15f,0.15f,1.0f) }});
    }

    canvas.set_stroke_brush(Brush{BrushSolid, { }, { color(0.7f,0.7f,0.7f,1.0f) }});
    canvas.set_stroke_width(3.0f);

    vec2 delta_c = in_c[0] - out_c[0];
    float pw = delta_c.x, ph = delta_c.y;
    pw += 20; ph += 20;
    Path *p1 = canvas.new_path({0.0f,0.0f},{pw,ph});
    p1->new_quadratic_curve({10,10}, {pw/2,10}, {pw/2,ph/2});
    p1->new_quadratic_curve({pw/2,ph/2}, {pw/2,ph-10}, {pw-10,ph-10});
    p1->pos = out_c[0] + delta_c/2.0f;
}

std::string to_binary(uint64_t v, size_t n)
{
    std::string s;
    s.resize(n);
    for (size_t i = 0; i < n; i++) {
        s[i] = '0' + ((v >> (n-i-1)) & 1);
    }
    return s;
}

static void make_path(Canvas &canvas, vec2 from, vec2 to,
    float nr = 0.4f, float sr = 0.6f, float cr = 0.9f)
{
    vec2 delta_c = to - from;
    float pw = delta_c.x, ph = delta_c.y;
    float p = 10;
    if (ph < 0) {
        float a = (-ph+2*p), b = (pw+2*p), c = a/2.0f, d = b * nr;
        float e = c - (ph/2.0f*cr), f = c + (ph/2.0f*cr);
        Path *p1 = canvas.new_path({0.0f,0.0f}, {b,a});
        p1->new_quadratic_curve({p,a-p}, {d,e}, {d,c});
        p1->new_quadratic_curve({d,c}, {d,f}, {b-p,p});
        p1->pos = from + delta_c/2.0f;
    } else {
        float a = (ph+2*p), b = (pw+2*p), c = a/2.0f, d = b * sr;
        float e = c - (ph/2.0f*cr), f = c + (ph/2.0f*cr);
        Path *p1 = canvas.new_path({0.0f,0.0f}, {b,a});
        p1->new_quadratic_curve({p,p}, {d,e}, {d,c});
        p1->new_quadratic_curve({d,c}, {d,f}, {b-p,a-p});
        p1->pos = from + delta_c/2.0f;
    }
}

static void wire_bits(Canvas &canvas,
    float x, float y, int xi, int yi, int tyi,
    float w, float h, float xs, float ys, int ss, int ds,
    float sw = 3.0f, float cr = 5.0f, float cs = 1.0f, float l = 10.0f)
{
    vec2 from = { x + xs*(xi-1) + w/2 - l, y + ys*yi     + ss * h/4 };
    vec2 to   = { x + xs*xi     - w/2 + l, y + ys*tyi    + ds * h/4 };
    canvas.set_stroke_width(cs);
    Circle *c1 = canvas.new_circle(vec2(0), cr);
    Circle *c2 = canvas.new_circle(vec2(0), cr);
    c1->pos = from; c2->pos = to;
    canvas.set_stroke_width(sw);
    make_path(canvas, from, to);
}

static void do_example_shuffle1()
{
    clear_color = { 1.0f, 1.0f, 1.0f, 1.0f };

    if (canvas.num_drawables() > 0) return;

    TextStyle text_style_default{
        12.0f,
        sans_norm,
        text_halign_center,
        text_valign_center,
        "en",
        Brush{BrushSolid, { }, { color(0.0f,0.0f,0.0f,1.0f) }},
        Brush{BrushNone, { }, { }}
    };
    canvas.set_fill_brush(Brush{BrushSolid, { }, { color(0.85f,0.85f,0.85f,1.0f) }});
    canvas.set_stroke_brush(Brush{BrushSolid, { }, { color(0.65f,0.65f,0.65f,1.0f) }});

    int xlim = 3, ylim = 4;
    float x = -250.0f, y = -200.0f, w = 100.0f, h = 100.0, xs = 250.0f, ys = 150.0;

    for (int xi = 0; xi < xlim; xi++) {
        for (int yi = 0; yi < ylim; yi++) {
            canvas.set_stroke_width(1.0f);
            Rectangle *r1 = canvas.new_rounded_rectangle(vec2(0), vec2(w/2,h/2), 5.0f);
            Text *h1 = canvas.new_text(text_style_default);
            vec2 pos = { x + xs*xi , y + ys*yi  };
            r1->pos = h1->pos = pos;
            h1->set_text(std::string("ABC").substr(xi,1) + std::to_string(yi+1));
        }
    }
    canvas.set_fill_brush(Brush{BrushSolid, { }, { color(0.0f,0.0f,0.0f,1.0f) }});
    for (int xi = 0; xi < xlim; xi++) {
        for (int yi = 0; yi < ylim; yi++) {
            {   int tyi = (yi * 2 + 1) % ylim, ss = 1, ds = yi * 2 + 1 >= 4 ? 1 : -1;
                wire_bits(canvas, x, y, xi, yi, tyi, w, h, xs, ys, ss, ds); }
            {   int tyi = (yi * 2) % ylim, ss = -1, ds = yi * 2 >= 4 ? 1 : -1;
                wire_bits(canvas, x, y, xi, yi, tyi, w, h, xs, ys, ss, ds); }
        }
    }
    for (int yi = 0; yi < ylim; yi++) {
        wire_bits(canvas, x, y, xlim, yi, yi, w, h, xs, ys, 1, 1);
        wire_bits(canvas, x, y, xlim, yi, yi, w, h, xs, ys, -1, -1);
    }
    for (int yi = 0; yi < ylim; yi++) {
        for (int yj = 0; yj < 2; yj++) {
            std::string t = to_binary((yi << 1) + yj, 3);
            Text *tl = canvas.new_text(text_style_default);
            Text *tr = canvas.new_text(text_style_default);
            tr->set_halign(text_halign_right);
            tl->set_halign(text_halign_left);
            tl->set_text(t);
            tr->set_text(t);
            tl->pos = { x + xs*(-1) + w/2 - 50.0f, y + ys*yi + (-1+2*yj) * h/4 };
            tr->pos = { x + xs*xlim - w/2 + 50.0f, y + ys*yi + (-1+2*yj) * h/4 };
        }
    }
}

static void populate_canvas()
{
    ImGui::SetNextWindowPos(ImVec2(10.0f,10.0f));
    ImGui::Begin("Controller", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

    const char* items[] = { "text1", "circle1", "curve1", "node1", "shuffle1" };
    if (ImGui::Combo("example", &current_example, items, IM_ARRAYSIZE(items))) {
        canvas.clear();
    }

    switch (current_example) {
        case 0: do_example_text1(); break;
        case 1: do_example_circle1(); break;
        case 2: do_example_curve1(); break;
        case 3: do_example_node1(); break;
        case 4: do_example_shuffle1(); break;
    }

    ImGui::End();
}

static void update_texture_buffers()
{
    /* synchronize canvas texture buffers */
    buffer_texture_create(shape_tb, canvas.ctx->shapes, GL_TEXTURE0, GL_R32F);
    buffer_texture_create(edge_tb, canvas.ctx->edges, GL_TEXTURE1, GL_R32F);
    buffer_texture_create(brush_tb, canvas.ctx->brushes, GL_TEXTURE2, GL_R32F);
}

static void display()
{
    std::vector<glyph_shape> shapes;
    text_shaper_hb shaper;
    text_renderer_ft renderer(&manager);

    if (!sans_norm) {
        sans_norm = manager.findFontByPath(sans_norm_font_path);
    }

    auto t = high_resolution_clock::now();

    tl = tn;
    tn = (double)duration_cast<nanoseconds>(t.time_since_epoch()).count()/1e9;
    td = tn - tl;

    draw_list_clear(batch);

    /* create new frame */
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

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
    update_texture_buffers();

    /* render stats text */
    float x = 10.0f, y = height - 10.0f;
    std::vector<std::string> stats = get_stats(sans_norm, td);
    uint32_t c = clear_color[0] == 1.0 ? 0xff404040 : 0xffc0c0c0;
    for (size_t i = 0; i < stats.size(); i++) {
        text_segment stats_segment(stats[i], text_lang, sans_norm,
            (int)((float)stats_font_size * 64.0f), x, y, c);
        shapes.clear();
        shaper.shape(shapes, stats_segment);
        font_atlas *atlas = manager.getCurrentAtlas(sans_norm);
        renderer.render(batch, shapes, stats_segment);
        y -= (int)((float)stats_font_size * 1.334f);
    }

    /* update vertex and index buffers arrays (idempotent) */
    vertex_buffer_create("vbo", &vbo, GL_ARRAY_BUFFER, batch.vertices);
    vertex_buffer_create("ibo", &ibo, GL_ELEMENT_ARRAY_BUFFER, batch.indices);

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

    /* render overlay */
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
    case GLFW_KEY_1:
    case GLFW_KEY_2:
    case GLFW_KEY_3:
    case GLFW_KEY_4:
    case GLFW_KEY_5:
        current_example = key - GLFW_KEY_1; canvas.clear();
        break;
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
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

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
    //manager.msdf_autoload = true;
    //manager.msdf_enabled = true;
    manager.scanFontDir("fonts");

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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    initialize();
    reshape(width, height);
    while (!glfwWindowShouldClose(window)) {
        display();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();
}

/* help text */

void print_help(int argc, char **argv)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  -h, --help            command line help\n", argv[0]);
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
