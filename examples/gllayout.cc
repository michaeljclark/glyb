/*
 * glfw3 simple font render demo
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

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define CTX_OPENGL_MAJOR 3
#define CTX_OPENGL_MINOR 2

#include "linmath.h"
#include "draw.h"
#include "glcommon.h"
#include "binpack.h"
#include "font.h"
#include "glyph.h"
#include "text.h"


/* globals */

static GLuint tex;
static GLuint vao, vbo, ibo;
static program simple;
static draw_list batch;

static mat4x4 mvp;
static GLFWwindow* window;

static int width = 1024, height = 768;
static font_manager_ft manager;
static font_atlas atlas;


/* display  */

static void update_uniforms(program *prog)
{
    uniform_matrix_4fv(prog, "u_mvp", (const GLfloat *)mvp);
    uniform_1i(prog, "u_tex0", 0);
}

static void display()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(simple.pid);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)batch.indices.size(), GL_UNSIGNED_INT, (void*)0);

    glfwSwapBuffers(window);
}

static void reshape(int width, int height)
{
    mat4x4_ortho(mvp, 0.0f, (float)width, (float)height, 0.0f, 0.0f, 100.0f);
    glViewport(0, 0, width, height);

    glUseProgram(simple.pid);
    update_uniforms(&simple);
}

/* geometry */

static void update_geometry()
{
    std::vector<text_segment> segments;
    std::vector<glyph_shape> shapes;

    text_shaper_hb shaper;
    text_renderer_ft renderer(&manager, &atlas);
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

    draw_list_clear(batch);
    layout.layout(segments, &c, 50, 50, 900, 700);
    for (auto &segment : segments) {
        shapes.clear();
        shaper.shape(shapes, &segment);
        renderer.render(batch, shapes, &segment);
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
    static const GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};

    /* create vertex and index arrays */
    update_geometry();
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    vertex_buffer_create("vbo", &vbo, GL_ARRAY_BUFFER, batch.vertices);
    vertex_buffer_create("ibo", &ibo, GL_ELEMENT_ARRAY_BUFFER, batch.indices);
    vertex_array_config(&simple);
    glBindVertexArray(0);

    /* create font atlas texture */
    image_create_texture(&tex, atlas.width, atlas.height, atlas.depth,
        &atlas.pixels[0], atlas.depth == 4 ? GL_LINEAR : GL_NEAREST);
}

/* OpenGL initialization */

static void initialize()
{
    GLuint fsh, vsh;

    /* shader program */
    vsh = compile_shader(GL_VERTEX_SHADER, "shaders/simple.vsh");
    fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/simple.fsh");
    link_program(&simple, vsh, fsh);
    glDeleteShader(vsh);
    glDeleteShader(fsh);

    /* load font metadata */
    manager.scanFontDir("fonts");

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
