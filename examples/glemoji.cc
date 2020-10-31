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
#include "logger.h"
#include "glcommon.h"

using mat4 = glm::mat4;


/* globals */

static program simple;
static GLuint vao, vbo, ibo;
static draw_list batch;
static std::map<int,GLuint> tex_map;

static mat4 mvp;
static GLFWwindow* window;

static int font_size = 109;
static int width = 1024, height = 768;
static font_manager_ft manager;


/* display  */

static void display()
{
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (auto img : batch.images) {
        auto ti = tex_map.find(img.iid);
        if (ti == tex_map.end()) {
            tex_map[img.iid] = image_create_texture(img);
        } else {
            image_update_texture(tex_map[img.iid], img);
        }
    }
    glBindVertexArray(vao);
    for (auto cmd : batch.cmds) {
        glUseProgram(simple.pid);
        glBindTexture(GL_TEXTURE_2D, tex_map[cmd.iid]);
        glDrawElements(cmd_mode_gl(cmd.mode), cmd.count, GL_UNSIGNED_INT,
            (void*)(cmd.offset * sizeof(uint)));
    }

    glfwSwapBuffers(window);
}

static void reshape(int width, int height)
{
    mvp = glm::ortho(0.0f, (float)width,(float)height, 0.0f, 0.0f, 100.0f);
    uniform_matrix_4fv(&simple, "u_mvp", (const GLfloat *)&mvp[0][0]);
    glViewport(0, 0, width, height);
}

/* geometry */

static void update_geometry()
{
    auto face = manager.findFontByPath("fonts/NotoColorEmoji.ttf");

    const float x = 170.0f, y = 440.0f;
    const uint32_t color = 0xffffffff;

    std::vector<glyph_shape> shapes;
    text_shaper_ft shaper;
    text_renderer_ft renderer(&manager);
    //text_segment segment("📑📄📃📁📂📦", "en", face, font_size << 6, x, y, color);
    //text_segment segment("🐁🐇🐈🐎🐄", "en", face, font_size << 6, x, y, color);
    text_segment segment("🙃😙😃😜😍", "en", face, font_size << 6, x, y, color);

    draw_list_clear(batch);
    shaper.shape(shapes, segment);
    renderer.render(batch, shapes, segment);
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
    manager.color_enabled = true;
    if (argc == 2) font_size = atoi(argv[1]);
    glfont(argc, argv);
    return 0;
}

