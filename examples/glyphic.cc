/*
 * glyphic logo
 */

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
#include <map>
#include <set>
#include <string>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <chrono>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define CTX_OPENGL_MAJOR 3
#define CTX_OPENGL_MINOR 2

#include "linmath.h"
#include "binpack.h"
#include "image.h"
#include "draw.h"
#include "font.h"
#include "glyph.h"
#include "logger.h"
#include "file.h"
#include "glcommon.h"


/* globals */

static program simple, msdf;
static GLuint vao, vbo, ibo;
static std::map<int,GLuint> tex_map;

static mat4x4 mvp;
static GLFWwindow* window;

static const char *font_path = "fonts/DejaVuSans.ttf";
static const char *render_text = "glyphic";
static const char* text_lang = "en";
static const int font_dpi = 72;
static bool help_text = false;
static bool high_scalability = false;
static int width = 1024, height = 768;
static font_manager_ft manager;

static double tl, tn, td, tb;

static std::vector<std::string> book;

using ns = std::chrono::nanoseconds;
using hires_clock = std::chrono::high_resolution_clock;

/* display  */

static program* cmd_shader_gl(int cmd_shader)
{
    switch (cmd_shader) {
    case shader_simple:  return &simple;
    case shader_msdf:    return &msdf;
    default: return nullptr;
    }
}

static inline int rnd_num(int b, int r)
{
    return b + (int)floorf(((float)rand()/(float)RAND_MAX)*(float)(r));
}

static std::vector<std::string> read_file(file_ptr rsrc)
{
    char buf[1024];
    char *line;

    std::vector<std::string> lines;

    rsrc->open(0);
    while ((line = rsrc->readLine(buf, sizeof(buf)))) {
        lines.push_back(line);
    }
    rsrc->close();

    return lines;
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

static std::vector<std::string> get_stats(font_face *face)
{
    std::vector<std::string> stats;

    size_t glyph_count = 0, memory_allocated = 0;
    size_t area_used = 0, area_total = 0;
    std::set<int> font_size_set;
    for (auto atlas : manager.faceAtlasMap[face]) {
        area_total += atlas->width * atlas->height;
        memory_allocated += atlas->width * atlas->height * atlas->depth;
        for (auto ai : atlas->glyph_map) {
            const glyph_key &key = ai.first;
            const atlas_entry &ent = ai.second;
            if (atlas->depth == 4 && key.font_size() != 0) continue;
            glyph_count += 1;
            area_used += ent.w * ent.h;
            font_size_set.insert(font_size_set.begin(), ent.font_size);
        }
    }

    stats.push_back(format_string("frames-per-second: %5.2f",
        1.0/td));
    stats.push_back(format_string("memory-allocated: %d KiB",
        memory_allocated/1024));
    stats.push_back(format_string("atlas-utilization: %5.2f %%",
        100.0f*(float)area_used / (float)area_total));
    stats.push_back(format_string("atlas-font-sizes: %zu",
        font_size_set.size()));
    stats.push_back(format_string("atlas-glyph-count: %zu",
        glyph_count));
    stats.push_back(format_string("atlas-count: %zu",
        manager.everyAtlas.size()));

    return stats;
}

static int text_width(std::vector<glyph_shape> &shapes, text_segment &segment)
{
    float tw = 0;
    for (auto &s : shapes) {
        tw += s.x_advance/64.0f;
        tw += segment.tracking;
    }
    return tw;
}

struct wisdom {
    float x, y, w, s;
    int font_size;
    uint32_t color;
    std::string book_wisdom;
};

static std::vector<wisdom> wise;

static void display()
{
    draw_list batch;
    std::vector<glyph_shape> shapes;
    text_shaper_hb shaper;
    text_renderer_ft renderer(&manager);

    auto face = manager.findFontByPath(font_path);
    uint32_t color1 = 0xff808080;
    uint32_t color2 = 0xff000000;
    size_t wisdom_count = high_scalability ? 36 : 9;
    float tw;
    auto t = hires_clock::now();

    glfwGetFramebufferSize(window, &width, &height);

    tl = tn;
    tn = (double)std::chrono::duration_cast<ns>(t.time_since_epoch()).count()/1e9;
    td = tn - tl;

    /* render wisdom */
    for (size_t j = 0; j < wisdom_count; j++) {
        if (wise.size() == 0) {
            wise.resize(wisdom_count);
        }

        if (wise[j].x + wise[j].w <= 0) {
            wise[j].x = width + rnd_num(0,width);
            wise[j].y = 80 + j * (high_scalability ? 20 : 80);
            wise[j].s = rnd_num(100, 100);
            wise[j].font_size = 12 + rnd_num(0, high_scalability ? 5 : 10) * 4;
            int c = rnd_num(64,127);
            wise[j].color = 0xff000000 | c << 16 | c << 8 | c;
            do {
                wise[j].book_wisdom = book[rnd_num(32,book.size()-33)];
            } while (wise[j].book_wisdom.size() < 20);
        }

        text_segment wisdom_segment(wise[j].book_wisdom, text_lang, face,
            wise[j].font_size * 64, 0, 0, wise[j].color);

        shapes.clear();
        shaper.shape(shapes, &wisdom_segment);
        wise[j].w = text_width(shapes, wisdom_segment);
        wisdom_segment.x = wise[j].x;
        wisdom_segment.y = wise[j].y;
        renderer.render(batch, shapes, &wisdom_segment);

        wise[j].x -= wise[j].s * td;
    }

    /* render pulsating glyphic text */
    float s = sin(fmod(tn, 1.f) * 4.0f * M_PI);
    float font_size = 160 + (int)(s * 25);
    text_segment glyphic_segment(render_text, text_lang, face,
        font_size * 64, 0, 0, color2);
    glyphic_segment.tracking = -5;

    shapes.clear();
    shaper.shape(shapes, &glyphic_segment);
    tw = text_width(shapes, glyphic_segment);
    glyphic_segment.x = (width - (int)tw) / 2 - 10;
    glyphic_segment.y =  height/2 + font_size*0.33f;
    renderer.render(batch, shapes, &glyphic_segment);

    /* render stats text */
    int x = 10, y = height - 10;
    std::vector<std::string> stats = get_stats(face);
    for (size_t i = 0; i < stats.size(); i++) {
        text_segment stats_segment(stats[i], text_lang, face,
            18 * 64, x, y, color2);
        shapes.clear();
        shaper.shape(shapes, &stats_segment);
        renderer.render(batch, shapes, &stats_segment);
        y -= 24;
    }

    /* updates buffers */
    program *prog = manager.msdf_enabled ? &msdf : &simple;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    vertex_buffer_create("vbo", &vbo, GL_ARRAY_BUFFER, batch.vertices);
    vertex_buffer_create("ibo", &ibo, GL_ELEMENT_ARRAY_BUFFER, batch.indices);
    vertex_array_pointer(prog, "a_pos", 3, GL_FLOAT, 0, &draw_vertex::pos);
    vertex_array_pointer(prog, "a_uv0", 2, GL_FLOAT, 0, &draw_vertex::uv);
    vertex_array_pointer(prog, "a_color", 4, GL_UNSIGNED_BYTE, 1, &draw_vertex::color);
    vertex_array_1f(prog, "a_gamma", 2.0f);
    glBindVertexArray(0);

    /* update textures */
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

    /* draw */
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindVertexArray(vao);
    for (auto cmd : batch.cmds) {
        glUseProgram(cmd_shader_gl(cmd.shader)->pid);
        glBindTexture(GL_TEXTURE_2D, tex_map[cmd.iid]);
        glDrawElements(cmd_mode_gl(cmd.mode), cmd.count, GL_UNSIGNED_INT,
            (void*)(cmd.offset * sizeof(uint)));
    }

    glfwSwapBuffers(window);
}

static void update_uniforms(program *prog)
{
    uniform_matrix_4fv(prog, "u_mvp", (const GLfloat *)mvp);
    uniform_1i(prog, "u_tex0", 0);
}

static void reshape(int width, int height)
{
    mat4x4_ortho(mvp, 0.0f, (float)width, (float)height, 0.0f, 0.0f, 100.0f);
    uniform_matrix_4fv(&msdf, "u_mvp", (const GLfloat *)mvp);
    glViewport(0, 0, width, height);

    glUseProgram(msdf.pid);
    update_uniforms(&msdf);

    glUseProgram(simple.pid);
    update_uniforms(&simple);
}

/* OpenGL initialization */

static void initialize()
{
    GLuint simple_fsh, msdf_fsh, vsh;

    book = read_file(file::getResource("data/pg5827.txt"));

    /* shader program */
    vsh = compile_shader(GL_VERTEX_SHADER, "shaders/simple.vsh");
    simple_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/simple.fsh");
    msdf_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/msdf.fsh");
    link_program(&simple, vsh, simple_fsh);
    link_program(&msdf, vsh, msdf_fsh);
    glDeleteShader(vsh);
    glDeleteShader(simple_fsh);
    glDeleteShader(msdf_fsh);

    /*
     * we need to scan font directory for caching to work, as it uses
     * font ids assigned during scanning. this also means that if the
     * font directory has changed, then cached font ids will be wrong
     */
    if (manager.msdf_enabled) {
        manager.scanFontDir("fonts");
    }

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

static void glyphic(int argc, char **argv)
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
        "\n"
        "Options:\n"
        "  -f, --font <ttf-file>  font file (default %s)\n"
        "  -m, --enable-msdf      enable MSDF font rendering\n"
        "  -q, --quadruple        quadruple the object count\n"
        "  -h, --help             command line help\n",
        argv[0], font_path);
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
        if (match_opt(argv[i], "-f","--font")) {
            if (check_param(++i == argc, "--font")) break;
            font_path = argv[i++];
        } else if (match_opt(argv[i], "-m", "--enable-msdf")) {
            manager.msdf_enabled = true;
            manager.msdf_autoload = true;
            i++;
        } else if (match_opt(argv[i], "-q", "--quadruple")) {
            high_scalability = true;
            i++;
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
    glyphic(argc, argv);
    return 0;
}
