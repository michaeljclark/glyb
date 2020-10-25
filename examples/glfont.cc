/*
 * glfw3 font render demo
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
#include <string>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

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
#include "msdf.h"
#include "multi.h"
#include "logger.h"
#include "glcommon.h"

using mat4 = glm::mat4;


/* globals */

static GLuint vao, vbo, ibo;
static program simple, msdf;
static draw_list batch;
static std::map<int,GLuint> tex_map;

static mat4 mvp;
static GLFWwindow* window;

static const char *font_path = "fonts/RobotoMono-Regular.ttf";
static const char *render_text = "the quick brown fox jumps over the lazy dog";
static const char* text_lang = "en";
static int font_size_min = 12;
static int font_size_max = 32;
static bool help_text = false;
static bool show_atlas = false;
static bool show_lines = false;
static bool debug = false;
static bool use_multithread = true;
static int width = 1024, height = 768;
static font_manager_ft manager;


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

static void display()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

    glUseProgram(msdf.pid);
    update_uniforms(&msdf);

    glUseProgram(simple.pid);
    update_uniforms(&simple);
}

/* geometry */

static void rect(draw_list &batch, font_atlas *atlas,
    float x1, float y1, float x2, float y2, float z, uint color)
{
    int atlas_iid = atlas->get_image()->iid;
    int atlas_shader = atlas->depth == 4 ? shader_msdf : shader_simple;
    int atlas_flags = st_clamp | atlas_image_filter(atlas);

    draw_list_image(batch, atlas->get_image(), atlas_flags);

    uint o = static_cast<uint>(batch.vertices.size());
    float uv1 = 0.0f, uv2 = atlas->uv1x1;
    uint o0 = draw_list_vertex(batch, {{x1, y1, z}, {uv1, uv1}, color});
    uint o1 = draw_list_vertex(batch, {{x2, y1, z}, {uv2, uv1}, color});
    uint o2 = draw_list_vertex(batch, {{x2, y2, z}, {uv2, uv2}, color});
    uint o3 = draw_list_vertex(batch, {{x1, y2, z}, {uv1, uv2}, color});

    draw_list_indices(batch, atlas_iid, mode_triangles, atlas_shader,
        {o0, o3, o1, o1, o3, o2});
}

static void update_geometry()
{
    const uint32_t black = 0xff000000, light_gray = 0xffcccccc;

    text_shaper_hb shaper;
    text_renderer_ft renderer(&manager);
    font_face *face = manager.findFontByPath(font_path);
    font_atlas *atlas = manager.getCurrentAtlas(face);

    float x = 100, y = 100;

    draw_list_clear(batch);

    struct render_item {
        std::unique_ptr<text_segment> segment;
        std::unique_ptr<std::vector<glyph_shape>> shapes;
    };
    std::vector<render_item> items;

    for (int sz = font_size_min; sz <= font_size_max; sz++)
    {
        y += sz;
        std::string size_text = std::to_string(sz);
        auto size_segment = std::make_unique<text_segment>
            (size_text, text_lang, face, 12 * 64, x - 50.0f, y, black);
        auto render_segment = std::make_unique<text_segment>
            (render_text, text_lang, face, sz * 64, x, y, black);
        auto size_shapes = std::make_unique<std::vector<glyph_shape>>();
        auto render_shapes = std::make_unique<std::vector<glyph_shape>>();
        if (show_lines) {
            int width = 0;
            int height = static_cast<font_face_ft*>
                (face)->get_height(render_segment->font_size) >> 6;
            std::vector<glyph_shape> shapes;
            shaper.shape(shapes, *render_segment);
            for (auto &s : shapes) {
                width += s.x_advance/64;
            }
            float x1 = (float)x, x2 = (float)x + (float)width;
            float y1 = (float)y - 1.0f, y2 = (float)y1 + 2.0f;
            float y3 = (float)y - (float)height / 2.0f - 1.0f, y4 = y3 + 2.0f;
            float y5 = (float)y - (float)height - 1.0f, y6 = y5 + 2.0f;
            float fwidth = (float)width, fheight = (float)height;
            rect(batch, atlas, x1, y1, x2, y2, 0, light_gray);
            rect(batch, atlas, x1, y3, x2, y4, 0, light_gray);
            rect(batch, atlas, x1, y5, x2, y6, 0, light_gray);
        }
        items.push_back({std::move(size_segment),std::move(size_shapes)});
        items.push_back({std::move(render_segment),std::move(render_shapes)});
    }

    /*
     * perform distinct stages for shaping and rendering. we do this so
     * that we can accumulate all glyphs used for multithreaded rendering.
     * submitting all glyphs together is more efficient becuase we need to
     * wait for the glyph render results before starting text rendering.
     */

    for (auto &item : items) {
        shaper.shape(*item.shapes, *item.segment);
    }

    if (use_multithread && manager.msdf_enabled) {
        glyph_renderer_factory_impl<glyph_renderer_msdf> renderer_factory;
        glyph_renderer_multi multithreaded_renderer(&manager,
            renderer_factory, std::thread::hardware_concurrency());
        for (auto &item : items) {
            multithreaded_renderer.add(*item.shapes, item.segment.get());
        }
        multithreaded_renderer.run();
    }

    for (auto &item : items) {
        renderer.render(batch, *item.shapes, *item.segment);
    }

    if (show_atlas) {
        draw_list_clear(batch);
        float u1 = 0.0f, v1 = 1.0f;
        float u2 = 1.0f, v2 = 0.0f;
        float x1 = 100, y1 = 100;
        float x2 = 1124, y2 = 1124;
        uint32_t o = (uint32_t)batch.vertices.size();
        uint32_t color = 0xff000000;
        batch.vertices.push_back({{x1, y1, 0.f}, {u1, v1}, color});
        batch.vertices.push_back({{x2, y1, 0.f}, {u2, v1}, color});
        batch.vertices.push_back({{x2, y2, 0.f}, {u2, v2}, color});
        batch.vertices.push_back({{x1, y2, 0.f}, {u1, v2}, color});
        batch.indices.insert(batch.indices.end(), {o+0, o+3, o+1, o+1, o+3, o+2});
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
    auto face = manager.findFontByPath(font_path);
    auto atlas = manager.getCurrentAtlas(face);

    /* create vertex and index arrays */
    update_geometry();
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    vertex_buffer_create("vbo", &vbo, GL_ARRAY_BUFFER, batch.vertices);
    vertex_buffer_create("ibo", &ibo, GL_ELEMENT_ARRAY_BUFFER, batch.indices);
    vertex_array_config(manager.msdf_enabled ? &msdf : &simple);
    glBindVertexArray(0);
}

/* keyboard callback */

static void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        exit(1);
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

    /*
     * we need to scan font directory for caching to work, as it uses
     * font ids assigned during scanning. this also means that if the
     * font directory has changed, then cached font ids will be wrong
     */
    if (manager.msdf_enabled) {
        manager.scanFontDir("fonts");
    }

    /* create vertex buffers and font atlas texture */
    update_buffers();

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

static void glfont(int argc, char **argv)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, CTX_OPENGL_MAJOR);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, CTX_OPENGL_MINOR);

    window = glfwCreateWindow(width, height, argv[0], NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL();
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, resize);
    glfwSetKeyCallback(window, keyboard);
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
        "  -f, --font <ttf-file>     font file (default %s)\n"
        "  -min, --min-size <points> font size minimum (default %d)\n"
        "  -max, --max-size <points> font size maximum (default %d)\n"
        "  -s, --size <points>       font size (both minimum and maximum)\n"
        "  -t, --text <string>       text to render (default \"%s\")\n"
        "  -a, --show-atlas          show the atlas instead of the text\n"
        "  -l, --show-lines          show baseline, half-height and height\n"
        "  -m, --disable-msdf        disable MSDF font rendering\n"
        "  -M, --disable-autoload    disable MSDF atlas autoloading\n"
        "  -T, --disable-multithread disable multithreaded atlas generation\n"
        "  -d, --debug               print debugging information\n"
        "  -h, --help                command line help\n",
        argv[0], font_path, font_size_min, font_size_max, render_text);
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
        if (match_opt(argv[i], "-d","--debug")) {
            debug = true;
            i++;
        } else if (match_opt(argv[i], "-f","--font")) {
            if (check_param(++i == argc, "--font")) break;
            font_path = argv[i++];
        } else if (match_opt(argv[i], "-s", "--size")) {
            if (check_param(++i == argc, "--size")) break;
            font_size_min = font_size_max = atoi(argv[i++]);
        } else if (match_opt(argv[i], "-min", "--min-size")) {
            if (check_param(++i == argc, "--size")) break;
            font_size_min  = atoi(argv[i++]);
        } else if (match_opt(argv[i], "-max", "--max-size")) {
            if (check_param(++i == argc, "--size")) break;
            font_size_max = atoi(argv[i++]);
        } else if (match_opt(argv[i], "-t", "--text")) {
            if (check_param(++i == argc, "--font-size")) break;
            render_text = argv[i++];
        } else if (match_opt(argv[i], "-a", "--show-atlas")) {
            show_atlas = true;
            i++;
        } else if (match_opt(argv[i], "-l", "--show-lines")) {
            show_lines = true;
            i++;
        } else if (match_opt(argv[i], "-c", "--enable-color")) {
            manager.color_enabled = true;
            manager.msdf_enabled = false;
            i++;
        } else if (match_opt(argv[i], "-m", "--disable-msdf")) {
            manager.msdf_enabled = false;
            i++;
        } else if (match_opt(argv[i], "-M", "--disable-autoload")) {
            manager.msdf_autoload = false;
            i++;
        } else if (match_opt(argv[i], "-T", "--disable-multithread")) {
            use_multithread = false;
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
    /*
     * enable MSDF font atlases. note: this requires that client
     * code configures MSDF shaders, so it is disabled by default.
     */
    manager.msdf_enabled = true;
    manager.msdf_autoload = true;

    parse_options(argc, argv);
    glfont(argc, argv);
    return 0;
}
