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


/* globals */

static program simple;
static GLuint tex;
static GLuint vao, vbo, ibo;
static draw_list batch;

static mat4x4 mvp;
static GLFWwindow* window;

static const char *font_path = "fonts/RobotoMono-Regular.ttf";
static const char *render_text = "the quick brown fox jumps over the lazy dog";
static const char* text_lang = "en";
static const int font_dpi = 72;
static int font_size = 32;
static int width = 1024, height = 256;
static font_manager_ft manager;
static font_atlas atlas;


/* display  */

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
    uniform_matrix_4fv(&simple, "u_mvp", (const GLfloat *)mvp);
    glViewport(0, 0, width, height);
}

/* geometry */

static void update_geometry()
{
    auto face = manager.findFontByPath(font_path);

    const int x = 100, y = 100 + font_size;
    const uint32_t color = 0xff000000;

    std::vector<glyph_shape> shapes;
    text_shaper_hb shaper;
    text_renderer_ft renderer(&manager, &atlas);
    text_segment segment(render_text, text_lang, face,
        font_size * 64, x, y, color);

    draw_list_clear(batch);
    shaper.shape(shapes, &segment);
    renderer.render(batch, shapes, &segment);
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
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, (GLsizei)atlas.width, (GLsizei)atlas.height,
        0, GL_RED, GL_UNSIGNED_BYTE, (GLvoid*)&atlas.pixels[0]);
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
    glActiveTexture(GL_TEXTURE0);
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

    /* create vertex buffers and font atlas texture */
    update_buffers();

    /* uniforms */
    glUseProgram(simple.pid);
    uniform_1i(&simple, "u_tex0", 0);

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
