/*
 * glfw3 bin_packer demo
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
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define CTX_OPENGL_MAJOR 3
#define CTX_OPENGL_MINOR 2

#include "linmath.h"
#include "glcommon.h"
#include "binpack.h"
#include "util.h"


/* OpenGL objects and buffers */

static GLuint program, tex;
static GLuint line_vao, line_vbo, line_ibo;
static std::vector<vertex> line_vtx;
static std::vector<uint> line_ind;
static GLuint rect_vao, rect_vbo, rect_ibo;
static std::vector<vertex> rect_vtx;
static std::vector<uint> rect_ind;

static mat4x4 m, p, mvp;
static GLFWwindow* window;

/* bin_packer algorithm settings */

static int bin_width = 512, bin_height = 512;
static int rnd_base = 16, rnd_range = 16;
static bin_packer bp(bin_point(bin_width,bin_height));
static bool debug = false;
static size_t seed = 0;
static size_t step_count = 1;

/* bin_packer batch mode globals */

static GLubyte *buffer;
static int width = 1024, height = 768;
static size_t frame_step = 1;
static size_t frame_count = 1;
static char filename[PATH_MAX];;
static const char *filename_template = "video/ppm/binpack-%09zu.ppm";
static bool help_text = false;
static bool batch_mode = false;

/* bin_packer offscreen framebuffer */

static GLuint render_fbo = 0;
static GLuint render_tex = 0;
static GLuint depth_fbo = 0;
static GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };


/* display  */

static void display()
{
    glClearColor(0.2f, 0.2f, 0.2f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(program);
    glBindVertexArray(rect_vao);
    glDrawElements(GL_TRIANGLES, rect_ind.size(), GL_UNSIGNED_INT, (void*)0);

    glBindVertexArray(line_vao);
    glDrawElements(GL_LINES, line_ind.size(), GL_UNSIGNED_INT, (void*)0);
    glfwSwapBuffers(window);
}

static void reshape(int width, int height)
{
    vec3 eye{0.0f, 0.0f, 2.2f};
    vec3 center{0.0f, 0.0f, 0.0f};
    vec3 up{0.0f, 1.0f, 0.0f};

    mat4x4_perspective(p, 45, (float) width / (float) height, 1, 10);
    mat4x4_look_at(m, eye, center, up);
    mat4x4_mul(mvp, p, m);

    uniform_matrix_4fv("u_mvp", (const GLfloat *)mvp);

    glViewport(0, 0, width, height);
}

/* geometry */

enum op_type {
    op_fill,
    op_stroke
};

static void rect(op_type op, std::vector<vertex> &vertices,
    std::vector<uint> &indices, float x1, float y1, float x2, float y2,
    float z, uint color)
{
    uint o = static_cast<uint>(vertices.size());
    vertices.push_back({{x1, y1, z}, {0, 0}, color});
    vertices.push_back({{x2, y1, z}, {0, 0}, color});
    vertices.push_back({{x2, y2, z}, {0, 0}, color});
    vertices.push_back({{x1, y2, z}, {0, 0}, color});
    switch (op) {
    case op_fill:
        indices.insert(indices.end(), {o+0, o+1, o+3, o+1, o+2, o+3});
        break;
    case op_stroke:
        indices.insert(indices.end(), {o+0, o+1, o+1, o+2, o+2, o+3, o+3, o+0});
        break;
    }    
}

static uint color(float base, float range, int shift1, int shift2, float value)
{
    uint c = (int)floorf(base + value * range);
    return 0xff000000 | c << shift1 | c << shift2;
}

static void update_geometry()
{
    const float dx = (2.0f/(float)bp.total.width());
    const float dy = (2.0f/(float)bp.total.height());

    line_ind.clear();
    line_vtx.clear();
    rect_ind.clear();
    rect_vtx.clear();

    for (auto i : bp.alloc_map) {
        bin_rect c = i.second;
        float x1 = ((float)c.a.x*dx)-1.0f, y1 = ((float)c.a.y*dy)-1.0f;
        float x2 = ((float)c.b.x*dx)-1.0f, y2 = ((float)c.b.y*dy)-1.0f;
        rect(op_fill, rect_vtx, rect_ind, x1, y1, x2, y2, 0.0f,
            color(64.0f, 32.0f, 8, 16,
                (float)i.first / (float)(bp.alloc_map.size())));
        rect(op_stroke, line_vtx, line_ind, x1, y1, x2, y2, 0.000001,
            color(128.0f, 32.0f, 8, 16,
                (float)i.first / (float)(bp.alloc_map.size())));
    }

    for (size_t i = 0; i < bp.free_list.size(); i++) {
        bin_rect c = bp.free_list[i];
        float x1 = ((float)c.a.x*dx)-1.0f, y1 = ((float)c.a.y*dy)-1.0f;
        float x2 = ((float)c.b.x*dx)-1.0f, y2 = ((float)c.b.y*dy)-1.0f;
        rect(op_fill, rect_vtx, rect_ind, x1, y1, x2, y2, 0.0f,
            color(64.0f, 32.0f, 0, 8,
                (float)i / (float)(bp.free_list.size())));
        rect(op_stroke, line_vtx, line_ind, x1, y1, x2, y2, 0.000001,
            color(128.0f, 32.0f, 0, 8,
                (float)i / (float)(bp.free_list.size())));
    }    
}

static void vertex_array_config()
{
    vertex_array_pointer("a_pos", 3, GL_FLOAT, 0, &vertex::pos);
    vertex_array_pointer("a_uv0", 2, GL_FLOAT, 0, &vertex::uv);
    vertex_array_pointer("a_color", 4, GL_UNSIGNED_BYTE, 1, &vertex::color);
    vertex_array_1f("a_gamma", 1.0f);
}

static void update_buffers()
{
    update_geometry();

    glGenVertexArrays(1, &line_vao);
    glBindVertexArray(line_vao);
    vertex_buffer_create("line_vbo", &line_vbo, GL_ARRAY_BUFFER, line_vtx);
    vertex_buffer_create("line_ibo", &line_ibo, GL_ELEMENT_ARRAY_BUFFER, line_ind);
    vertex_array_config();
    glBindVertexArray(0);

    glGenVertexArrays(1, &rect_vao);
    glBindVertexArray(rect_vao);
    vertex_buffer_create("rect_vbo", &rect_vbo, GL_ARRAY_BUFFER, rect_vtx);
    vertex_buffer_create("rect_ibo", &rect_ibo, GL_ELEMENT_ARRAY_BUFFER, rect_ind);
    vertex_array_config();
    glBindVertexArray(0);
}

/* algorithm step */

static inline int rnd_num(int b, int r)
{
    return b + (int)floorf(((float)rand()/(float)RAND_MAX)*(float)(r));
}

static inline bin_point rnd_size()
{
    return bin_point(rnd_num(rnd_base, rnd_range),rnd_num(rnd_base, rnd_range));
}

enum step_type {
    step_single,
    step_fill
};

static void step(step_type st)
{
    srand(seed);
    bp.reset();

    size_t i = 0;

    switch (st) {
    case step_single:
        while (i < step_count) {
            bp.find_region(i++,rnd_size());
        }
        break;
    case step_fill:
        while (bp.find_region(i++,rnd_size()).first);
        step_count = i - 1;
        break;
    }

    if (debug) {
        bp.dump();
        bp.verify();
    }
}

/* bin_packer stats  */

static void stats()
{
    int alloc_area = 0;
    for (auto i : bp.alloc_map) {
        alloc_area += i.second.area();
    }
    float alloc_percent = ((float)alloc_area/(float)bp.total.area()) * 100.0f;
    printf("------------------------------\n");
    printf("free list node count = %zu\n", bp.free_list.size());
    printf("alloc map node count = %zu\n", bp.alloc_map.size());
    printf("bin dimensions       = %d,%d\n", bp.total.width(), bp.total.height());
    printf("bin total area       = %d\n", bp.total.area());
    printf("bin allocated area   = %d\n", alloc_area);
    printf("bin utilization      = %4.1f%%\n", alloc_percent);
}

/* keyboard callback */

static void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
        if (step_count > 0) step_count--;
        step(step_single);
        update_buffers();
    }
    if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
        step_count++;
        step(step_single);
        update_buffers();
    }
    if (key == GLFW_KEY_D && action == GLFW_PRESS) {
        debug = !debug;
        printf("debug: %s\n", debug ? "enabled" : "disabled");
    }
    if (key == GLFW_KEY_E && action == GLFW_PRESS) {
        step_count = 1;
        step(step_single);
        update_buffers();
    }
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        step(step_fill);
        update_buffers();
    }
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        seed = time(NULL);
        step(step_fill);
        update_buffers();
    }
    if (key == GLFW_KEY_S && action == GLFW_PRESS) {
        stats();
    }
}

/* OpenGL initialization */

static void initialize(bool offscreen)
{
    const GLuint pixel = 0xffffffff;
    GLuint fsh, vsh;

    /* shader program */
    vsh = compile_shader(GL_VERTEX_SHADER, "shaders/simple.vsh");
    fsh = compile_shader(GL_FRAGMENT_SHADER,
        offscreen ? "shaders/offscreen.fsh" : "shaders/simple.fsh");
    program = link_program(vsh, fsh);

    /* create vertex buffers */
    step(step_single);
    update_buffers();

    /* texture */
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
        (GLvoid*)&pixel);
    glActiveTexture(GL_TEXTURE0);

    /* uniforms */
    glUseProgram(program);
    uniform_1i("u_tex0", 0);

    /* pipeline */
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glLineWidth(1.0);
}

/* GLFW GUI entry point */

static void resize(GLFWwindow* window, int width, int height)
{
    reshape(width, height);
}

static void binpack_gui(int argc, char **argv)
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

    initialize(false);
    reshape(width, height);
    while (!glfwWindowShouldClose(window)) {
        display();
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}

/* offscreen framebuffer */

static void fbo_initialize()
{
    glGenFramebuffers(1, &render_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, render_fbo);
    printf("render_fbo = %d\n", render_fbo);

    glGenTextures(1, &render_tex);
    glBindTexture(GL_TEXTURE_2D, render_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
        GL_UNSIGNED_BYTE, buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    printf("render_tex = %d\n", render_tex);

    glGenRenderbuffers(1, &depth_fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_fbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER, depth_fbo);
    printf("depth_fbo = %d\n", depth_fbo);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, render_tex, 0);
    glDrawBuffers(1, DrawBuffers);
    printf("fbo status = %s\n",
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE ?
        "happy" : "sad");

    glBindFramebuffer(GL_FRAMEBUFFER, render_fbo);
}

/* write image to file */

static void write_ppm(const char *filename, const uint8_t *buffer, int width, int height)
{
   FILE *f = fopen(filename, "wb");
   if (!f) {
      fprintf(stderr, "error: fopen failed: %s\n", strerror(errno));
      exit(1);
   }
   fprintf(f,"P6\n");
   fprintf(f,"# ppm-file\n");
   fprintf(f,"%i %i\n", width, height);
   fprintf(f,"255\n");
   uint8_t *line = (uint8_t*)malloc(width * 3);
   for (int y = height - 1; y >= 0; y--) {
      const uint8_t *in = &buffer[y * width * 4];
      uint8_t *out = line;
      for (int x = 0; x < width; x++) {
         *out++ = *in++; /* red */
         *out++ = *in++; /* green */
         *out++ = *in++; /* blue */
         in++;           /* skip alpha */
      }
      fwrite(line, width, 3, f);
   }
   fclose(f);
   free(line);
}

/* batch mode entry point */

static void binpack_offline(int argc, char **argv)
{
    buffer = new GLubyte[width * height * 4 * sizeof(GLubyte)];
    if (!buffer) {
        fprintf(stderr, "error: memory allocation failed\n");
        exit(1);
    }

    /* create window */
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, CTX_OPENGL_MAJOR);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, CTX_OPENGL_MINOR);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    window = glfwCreateWindow(width, height, argv[0], NULL, NULL);
    glfwMakeContextCurrent(window);

    fbo_initialize();
    initialize(true);
    reshape(width, height);

    for (size_t i = 1; i <= frame_count; i++) {
        update_buffers();
        display();
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        snprintf(filename, sizeof(filename), filename_template, i);
        write_ppm(filename, buffer, width, height);
        printf("wrote output to %s\n", filename);
        for (size_t j = 0; j < frame_step; j++) {
            if (!bp.find_region(++step_count,rnd_size()).first) goto out;
        }
    }
out:

    delete [] buffer;
}

/* help text */

void print_help(int argc, char **argv)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Offline options:\n"
        "  -o, --offline                      start in offline batch mode\n"
        "  -t, --template <format-%%05d.ppm>  offline output template\n"
        "  -s, --frame-size <width>x<height>  window or image size\n"
        "  -i, --frame-step <integer>         algorithm steps per frame\n"
        "  -c, --frame-count <integer>        output frame count limit\n"
        "\n"
        "Common options:\n"
        "  -r, --random <base>,<range>        random params (default 16,16)\n"
        "  -b, --bin-size <width>x<height>    bin size (default 512x512)\n"
        "  -h, --help                         command line help\n", argv[0]);
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
        if (strcmp(argv[i], "-o") == 0 ||
            strcmp(argv[i], "--offline") == 0) {
            i++;
            batch_mode = true;
        }
        else if (match_opt(argv[i], "-t", "--template")) {
            if (check_param(++i == argc, "--template")) break;
            filename_template = argv[i++];
        }
        else if (match_opt(argv[i], "-b", "--bin-size")) {
            if (check_param(++i == argc, "-bin-size")) break;
            sscanf(argv[i++], "%dx%d", &bin_width, &bin_height);
            bp.set_bin_size(bin_point(bin_width, bin_height));
        }
        else if (match_opt(argv[i], "-r", "--random")) {
            if (check_param(++i == argc, "--random")) break;
            sscanf(argv[i++], "%d,%d", &rnd_base, &rnd_range);
        }
        else if (match_opt(argv[i], "-s", "--frame-size")) {
            if (check_param(++i == argc, "--frame-size")) break;
            sscanf(argv[i++], "%dx%d", &width, &height);
        }
        else if (match_opt(argv[i], "-i","--frame-step")) {
            if (check_param(++i == argc, "--frame-step")) break;
            frame_step = atoi(argv[i++]);
        }
        else if (match_opt(argv[i], "-c", "--frame-count")) {
            if (check_param(++i == argc, "--frame-count")) break;
            frame_count = atoi(argv[i++]);
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

    printf("bin size = %dx%d\n", bin_width, bin_height);
    printf("image size = %dx%d\n", width, height);
    printf("random params = %d + rnd(%d)\n", rnd_base, rnd_range);
    printf("batch mode = %s\n", batch_mode ? "true" : "false");

    if (batch_mode) {
        printf("template = %s\n", filename_template);
        printf("frame step = %zu\n", frame_step);
        printf("frame count = %zu\n", frame_count);
    }
}

/* entry point */

int main(int argc, char **argv)
{
    parse_options(argc, argv);

    if (batch_mode) {
        binpack_offline(argc, argv);
    } else {
        binpack_gui(argc, argv);
    }

    return 0;
}
