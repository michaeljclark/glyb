/*
 * glfw3 simple font render demo
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

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>

#define CTX_OPENGL_MAJOR 3
#define CTX_OPENGL_MINOR 2

#include "linmath.h"
#include "glcommon.h"
#include "binpack.h"
#include "font.h"
#include "glyph.h"
#include "text.h"


/* globals */

static GLuint program, tex;
static GLuint vao, vbo, ibo;
static std::vector<text_vertex> vertices;
static std::vector<uint32_t> indices;

static mat4x4 mvp;
static GLFWwindow* window;

static const int font_dpi = 72;
static int width = 1024, height = 768;
static font_manager_ft manager;
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

static void update_geometry()
{
    std::vector<text_segment> segments;
    std::vector<glyph_shape> shapes;

    text_shaper shaper;
    text_renderer renderer(&manager, &atlas);
    text_layout layout(&manager, &atlas, &shaper, &renderer);
    text_container c;

    c.append(text_part("Γειά ",
        {{ "tracking", "2" }, { "baseline-shift", "9" }, { "color", "#800000" }}));
    c.append(text_part("σου ",
        {{ "tracking", "2" }, { "baseline-shift", "6" }, { "color", "#008000" }}));
    c.append(text_part("Κόσμε ",
        {{ "tracking", "2" }, { "baseline-shift", "3" }, { "color", "#000080" }}));
    c.append(text_part(
        "    Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor "
        "incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud "
        "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure "
        "dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. "
        "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
        "mollit anim id est laborum.    ",
        {{ "font-size", "18" }, { "font-style", "regular" }, { "color", "#000040" }}));
    c.append(text_part(
        "    Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor "
        "incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud "
        "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure "
        "dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. "
        "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
        "mollit anim id est laborum.    ",
        {{ "font-size", "36" }, { "font-style", "bold" }, { "color", "#7f7f9f" }}));

    vertices.clear();
    indices.clear();
    layout.layout(segments, &c, 50, 50, 900, 700);
    for (auto &segment : segments) {
        shapes.clear();
        shaper.shape(shapes, &segment);
        renderer.render(vertices, indices, shapes, &segment);
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

/* OpenGL initialization */

static void initialize()
{
    GLuint fsh, vsh;

    /* shader program */
    vsh = compile_shader(GL_VERTEX_SHADER, "shaders/simple.vsh");
    fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/simple.fsh");
    program = link_program(vsh, fsh);

    /* load font metadata */
    manager.scanFontDir("fonts");

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
    glEnable(GL_DEPTH_TEST);
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

/* entry point */

int main(int argc, char **argv)
{
    glfont(argc, argv);
    return 0;
}
