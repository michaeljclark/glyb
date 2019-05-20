/*
 * glfw3 font render demo
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <climits>
#include <cmath>
#include <ctime>

#include <vector>
#include <map>
#include <memory>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include <hb.h>
#include <hb-ft.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>

#define CTX_OPENGL_MAJOR 3
#define CTX_OPENGL_MINOR 2

#include "linmath.h"
#include "glcommon.h"
#include "binpack.h"
#include "glyph.h"


/* globals */

static GLuint program, tex;
static GLuint vao, vbo, ibo;
static std::vector<text_vertex> vertices;
static std::vector<uint32_t> indices;

static mat4x4 mvp;
static GLFWwindow* window;

static const char *font_path = "fonts/RobotoMono-Regular.ttf";
static const char *render_text = "the quick brown fox jumps over the lazy dog";
static const int font_dpi = 72;
static int font_size = 32;
static bool help_text = false;
static bool show_atlas = false;
static bool show_lines = false;
static bool debug = false;
static int width = 1024, height = 256;
static font_manager manager;
static font_atlas atlas;


/* display  */

static void display()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(program);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, (void*)0);

    glfwSwapBuffers(window);
}

static void reshape(int width, int height)
{
    mat4x4_ortho(mvp, 0.0f, (float)width, (float)height, 0.0f, 0.0f, 100.0f);
    uniform_matrix_4fv("u_mvp", (const GLfloat *)mvp);
    glViewport(0, 0, width, height);
}

/* geometry */

static void rect(std::vector<text_vertex> &vertices,
    std::vector<uint> &indices, float x1, float y1, float x2, float y2,
    float z, uint color)
{
    uint o = static_cast<uint>(vertices.size());
    float uv1 = 0.0f, uv2 = atlas.uv1x1;
    vertices.push_back({{x1, y1, z}, {uv1, uv1}, color});
    vertices.push_back({{x2, y1, z}, {uv2, uv1}, color});
    vertices.push_back({{x2, y2, z}, {uv2, uv2}, color});
    vertices.push_back({{x1, y2, z}, {uv1, uv2}, color});
    indices.insert(indices.end(), {o+0, o+3, o+1, o+1, o+3, o+2});
}

static void update_geometry()
{
    font_face *face = manager.lookup_font(font_path);

    int x = 100, y = 100 + font_size;
    uint32_t black = 0xff000000, light_gray = 0xffcccccc;

    std::vector<glyph_rect> rects;
    text_renderer renderer(&manager, &atlas);
    text_segment segment(render_text, face, font_size * 64, x, y, black);

    vertices.clear();
    indices.clear();
    renderer.render(vertices, indices, &segment, &rects);

    if (show_lines) {
        int min_x = INT_MAX, max_x = INT_MIN;
        int height = face->ftface->size->metrics.height >> 6;
        for (auto &r : rects) {
            min_x = (std::min)(min_x,r.x1);
            max_x = (std::max)(max_x,r.x2);
        }
        rect(vertices, indices,
            min_x, y, max_x, y+1, 0, light_gray);
        rect(vertices, indices,
            min_x, y-height/2-1, max_x, y-height/2+1, 0, light_gray);
        rect(vertices, indices,
            min_x, y-height-1, max_x, y-height+1, 0, light_gray);
    }

    if (show_atlas) {
        vertices.clear();
        indices.clear();
        float u1 = 0.0f, v1 = 1.0f;
        float u2 = 1.0f, v2 = 0.0f;
        float x1 = 100, y1 = 100;
        float x2 = 1124, y2 = 1124;
        uint32_t o = vertices.size();
        uint32_t color = 0xff000000;
        vertices.push_back({{x1, y1, 0.f}, {u1, v1}, color});
        vertices.push_back({{x2, y1, 0.f}, {u2, v1}, color});
        vertices.push_back({{x2, y2, 0.f}, {u2, v2}, color});
        vertices.push_back({{x1, y2, 0.f}, {u1, v2}, color});
        indices.insert(indices.end(), {o+0, o+3, o+1, o+1, o+3, o+2});
    }
}

static void vertex_array_config()
{
    vertex_array_pointer("a_pos", 3, GL_FLOAT, 0, &vertex::pos);
    vertex_array_pointer("a_uv0", 2, GL_FLOAT, 0, &vertex::uv);
    vertex_array_pointer("a_color", 4, GL_UNSIGNED_BYTE, 1, &vertex::color);
    vertex_array_1f("a_gamma", 2.0f);
}

static void update_buffers()
{
    static const GLint swizzleMask[] = {GL_RED, GL_RED, GL_RED, GL_RED};

    /* create vertex and index arrays */
    update_geometry();
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    vertex_buffer_create("vbo", &vbo, GL_ARRAY_BUFFER, vertices);
    vertex_buffer_create("ibo", &ibo, GL_ELEMENT_ARRAY_BUFFER, indices);
    vertex_array_config();
    glBindVertexArray(0);

    /* create font atlas texture */
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlas.width, atlas.height, 0,
        GL_RED, GL_UNSIGNED_BYTE, (GLvoid*)&atlas.pixels[0]);
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA_EXT, swizzleMask);
    glActiveTexture(GL_TEXTURE0);
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
    GLuint fsh, vsh;

    /* shader program */
    vsh = compile_shader(GL_VERTEX_SHADER, "shaders/simple.vsh");
    fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/simple.fsh");
    program = link_program(vsh, fsh);

    /* create vertex buffers and font atlas texture */
    update_buffers();

    /* uniforms */
    glUseProgram(program);
    uniform_1i("u_tex0", 0);

    /* pipeline */
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
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
        "  -f, --font <ttf-file>  font file (default %s)\n"
        "  -s, --size <points>    font size (default %d)\n"
        "  -t, --text <string>    text to render (default \"%s\")\n"
        "  -a, --show-atlas       show the atlas instead of the text\n"
        "  -l, --show-lines       show baseline, half-height and height\n"
        "  -d, --debug            print debugging information\n"
        "  -h, --help             command line help\n",
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
        if (match_opt(argv[i], "-d","--debug")) {
            debug = true;
            i++;
        } else if (match_opt(argv[i], "-f","--font")) {
            if (check_param(++i == argc, "--font")) break;
            font_path = argv[i++];
        } else if (match_opt(argv[i], "-s", "--size")) {
            if (check_param(++i == argc, "--size")) break;
            font_size = atoi(argv[i++]);
        } else if (match_opt(argv[i], "-t", "--text")) {
            if (check_param(++i == argc, "--font-size")) break;
            render_text = argv[i++];
        } else if (match_opt(argv[i], "-a", "--show-atlas")) {
            show_atlas = true;
            i++;
        } else if (match_opt(argv[i], "-l", "--show-lines")) {
            show_lines = true;
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
    glfont(argc, argv);
    return 0;
}
