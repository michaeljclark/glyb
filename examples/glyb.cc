/*
 * glyb logo
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
#define CTX_OPENGL_MINOR 3

#include "glm/glm.hpp"
#include "glm/ext/matrix_clip_space.hpp"

#include "binpack.h"
#include "image.h"
#include "draw.h"
#include "font.h"
#include "glyph.h"
#include "logger.h"
#include "file.h"
#include "format.h"
#include "glcommon.h"

using namespace std::chrono;
using mat4 = glm::mat4;

/* globals */

static program simple, msdf;
static GLuint vao, vbo, ibo;
static std::map<int,GLuint> tex_map;
static mat4 mvp;
static GLFWwindow* window;
static font_manager_ft manager;

static const char *font_path = "fonts/DejaVuSans.ttf";
static const char *render_text = "glyb";
static const char* text_lang = "en";
static const int stats_font_fize = 18;

static float load_factor = 1.0;
static bool help_text = false;
static bool overlay_stats = false;
static int width = 1024, height = 384;
static double tl, tn, td, tb;

static GLuint render_fbo = 0;
static GLuint render_tex = 0;
static GLuint depth_fbo = 0;
static GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };

static bool batch_mode = false;
static size_t frame_rate = 60;
static size_t frame_skip = 15 * frame_rate;
static size_t frame_count = 15 * frame_rate;
static const char *filename_template = "video/ppm/glyb-%09zu.ppm";
static const char *atlas_dump_template = nullptr;


/* shader enum to program */

static program* cmd_shader_gl(int cmd_shader)
{
    switch (cmd_shader) {
    case shader_simple:  return &simple;
    case shader_msdf:    return &msdf;
    default: return nullptr;
    }
}

/* utility functions */

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

/* on-screen stats*/

static std::vector<std::string> get_stats(font_face *face, float td)
{
    std::vector<std::string> stats;

    if (!overlay_stats) return stats;

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

    stats.push_back(format("frames-per-second: %5.2f",
        1.0/td));
    stats.push_back(format("memory-allocated: %d KiB",
        memory_allocated/1024));
    stats.push_back(format("atlas-utilization: %5.2f %%",
        100.0f*(float)area_used / (float)area_total));
    stats.push_back(format("atlas-font-sizes: %zu",
        font_size_set.size()));
    stats.push_back(format("atlas-glyph-count: %zu",
        glyph_count));
    stats.push_back(format("atlas-count: %zu",
        manager.everyAtlas.size()));

    return stats;
}

static float text_width(std::vector<glyph_shape> &shapes, text_segment &segment)
{
    float tw = 0;
    for (auto &s : shapes) {
        tw += s.x_advance/64.0f;
        tw += segment.tracking;
    }
    return tw;
}

static inline int r(int base, int range)
{
    return base + (int)floorf(((float)rand()/(float)RAND_MAX)*(float)(range));
}

struct wisdom {
    float x, y, w, s;
    int font_size;
    uint32_t color;
    std::string book_wisdom;
};

static draw_list batch;
static std::vector<std::string> book;
static std::vector<wisdom> wise;

/* display  */

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

static void draw(double tn, double td, bool skip = false)
{
    std::vector<glyph_shape> shapes;
    text_shaper_hb shaper;
    text_renderer_ft renderer(&manager);

    auto face = manager.findFontByPath(font_path);
    uint32_t color1 = 0xff808080;
    uint32_t color2 = 0xff000000;
    size_t wisdom_count = (size_t)((float)9 * load_factor);
    float scale = sqrtf((float)width*(float)height)/sqrtf(1024.0f*768.0f);
    float tw;

    draw_list_clear(batch);

    glfwGetFramebufferSize(window, &width, &height);

    /* render wisdom */
    for (size_t j = 0; j < wisdom_count; j++) {
        if (wise.size() == 0) {
            wise.resize(wisdom_count);
        }

        if (wise[j].x + wise[j].w <= 0) {
            wise[j].x = (float)width + (float)r(0,width);
            wise[j].y = (float)(j + 1) * (80 / load_factor);
            wise[j].s = (float)r(100, 100);
            wise[j].font_size = 12 + r(0, 5) * 4;

            wise[j].s = wise[j].s * scale;
            wise[j].y = wise[j].y * scale;
            wise[j].font_size = (int)((float)wise[j].font_size * scale);

            int c = r(64,127);
            wise[j].color = 0xff000000 | c << 16 | c << 8 | c;
            do {
                wise[j].book_wisdom = book.size() > 0 ?
                book[r(32,(int)book.size()-33)] :
                "wisdom wisdom wisdom wisdom wisdom wisdom wisdom wisdom wisdom";
            } while (wise[j].book_wisdom.size() < 20);
        }

        text_segment wisdom_segment(wise[j].book_wisdom, text_lang, face,
            wise[j].font_size * 64, 0, 0, wise[j].color);

        wise[j].w = text_width(shapes, wisdom_segment);
        wisdom_segment.x = wise[j].x;
        wisdom_segment.y = wise[j].y;
        wise[j].x -= wise[j].s * (float)td;

        if (!skip) {
            shapes.clear();
            shaper.shape(shapes, wisdom_segment);
            renderer.render(batch, shapes, wisdom_segment);
        }
    }

    if (skip) return;

    /* render pulsating glyb text */
    float s = sinf(fmodf((float)tn, 1.f) * 4.0f * (float)M_PI);
    float font_size = 300.0f + floorf(s * 25);
    text_segment glyb_segment(render_text, text_lang, face,
        (int)((float)font_size * scale * 64.0f), 0, 0, color2);
    glyb_segment.tracking = -5.0f;

    shapes.clear();
    shaper.shape(shapes, glyb_segment);
    tw = text_width(shapes, glyb_segment);
    glyb_segment.x = (float)(width - (int)tw) / 2.0f - 10.0f;
    glyb_segment.y =  (float)height/2.0f + (float)font_size*0.225f;
    renderer.render(batch, shapes, glyb_segment);

    /* render stats text */
    float x = 10.0f, y = height - 10.0f;
    std::vector<std::string> stats = get_stats(face, (float)td);
    const uint32_t bg_color = 0xbfffffff;
    for (size_t i = 0; i < stats.size(); i++) {
        text_segment stats_segment(stats[i], text_lang, face,
            (int)((float)stats_font_fize * scale * 64.0f), x, y, color2);
        shapes.clear();
        shaper.shape(shapes, stats_segment);
        tw = text_width(shapes, stats_segment);
        font_atlas *atlas = manager.getCurrentAtlas(face);
        float h = ((float)stats_font_fize * 1.0f * scale);
        float h2 = ((float)stats_font_fize * 0.334f * scale);
        rect(batch, atlas, x, y-h-h2/2.0f, x+tw, y+h2/2.0f, -0.0001f, bg_color);
        renderer.render(batch, shapes, stats_segment);
        y -= ((float)stats_font_fize * scale * 1.334f);
    }

    /* updates buffers */
    vertex_buffer_create("vbo", &vbo, GL_ARRAY_BUFFER, batch.vertices);
    vertex_buffer_create("ibo", &ibo, GL_ELEMENT_ARRAY_BUFFER, batch.indices);

    /* update textures */
    for (auto img : batch.images) {
        auto ti = tex_map.find(img.iid);
        if (ti == tex_map.end()) {
            tex_map[img.iid] = image_create_texture(img);
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

static void display()
{
    auto t = high_resolution_clock::now();

    tl = tn;
    tn = (double)duration_cast<nanoseconds>(t.time_since_epoch()).count()/1e9;
    td = tn - tl;

    draw(tn, td);
}

static void update_uniforms(program *prog)
{
    uniform_matrix_4fv(prog, "u_mvp", (const GLfloat *)&mvp[0][0]);
    uniform_1i(prog, "u_tex0", 0);
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

/* OpenGL initialization */

static void initialize()
{
    GLuint simple_fsh, msdf_fsh, vsh;

    book = read_file(file::getFile("data/pg5827.txt"));

    /* shader program */
    vsh = compile_shader(GL_VERTEX_SHADER, "shaders/simple.vsh");
    simple_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/simple.fsh");
    msdf_fsh = compile_shader(GL_FRAGMENT_SHADER, "shaders/msdf.fsh");
    link_program(&simple, vsh, simple_fsh);
    link_program(&msdf, vsh, msdf_fsh);
    glDeleteShader(vsh);
    glDeleteShader(simple_fsh);
    glDeleteShader(msdf_fsh);

    /* create vertex and index buffers arrays */
    vertex_buffer_create("vbo", &vbo, GL_ARRAY_BUFFER, batch.vertices);
    vertex_buffer_create("ibo", &ibo, GL_ELEMENT_ARRAY_BUFFER, batch.indices);

    /* configure vertex array object */
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    program *p = &simple;
    vertex_array_pointer(p, "a_pos", 3, GL_FLOAT, 0, &draw_vertex::pos);
    vertex_array_pointer(p, "a_uv0", 2, GL_FLOAT, 0, &draw_vertex::uv);
    vertex_array_pointer(p, "a_color", 4, GL_UNSIGNED_BYTE, 1, &draw_vertex::color);
    vertex_array_1f(p, "a_gamma", 2.0f);
    glBindVertexArray(0);

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
    glLineWidth(1.0);
}

static void resize(GLFWwindow* window, int width, int height)
{
    reshape(width, height);
}

static void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) {
            exit(0);
        } else if (key == GLFW_KEY_S) {
            overlay_stats = !overlay_stats;
        }
    }
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
        GL_UNSIGNED_BYTE, nullptr);
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

/* GLFW GUI entry point */

static void glyb_gui(int argc, char **argv)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, CTX_OPENGL_MAJOR);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, CTX_OPENGL_MINOR);

    window = glfwCreateWindow(width, height, argv[0], NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL();
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, keyboard);
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

/* write image to file */

static void write_ppm(const char *filename, const uint8_t *buffer, int width, int height)
{
   FILE *f = fopen(filename, "wb+");
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

static void glyb_offline(int argc, char **argv)
{
    GLubyte *buffer = new GLubyte[width * height * 4 * sizeof(GLubyte)];
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
    gladLoadGL();

    fbo_initialize();
    initialize();
    reshape(width, height);

    size_t frame_num = 0;
    for (size_t i = 1; i <= frame_skip; i++) {
        draw(frame_num/(float)frame_rate, 1/(float)frame_rate, true);
        frame_num++;
    }
    if (atlas_dump_template != nullptr) {
        auto face = manager.findFontByPath(font_path);
        size_t i = 0;
        for (auto atlas : manager.faceAtlasMap[face]) {
            std::string filename = format(atlas_dump_template, i++);
            image::saveToFile(std::string(filename), atlas->img, &image::PNG);
            printf("frame-%09zu : wrote atlas to %s\n", frame_num, filename.c_str());
        }
    }
    for (size_t i = 1; i <= frame_count; i++) {
        draw(frame_num/(float)frame_rate, 1/(float)frame_rate);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        std::string filename = format(filename_template, i++);
        write_ppm(filename.c_str(), buffer, width, height);
        printf("frame-%09zu : wrote output to %s\n", frame_num, filename.c_str());
        frame_num++;
    }

    delete [] buffer;
}

/* help text */

void print_help(int argc, char **argv)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "\n"
        "Offline options:\n"
        "  -o, --offline                      start in offline batch mode\n"
        "  -t, --template <name-%%05d.ppm>     offline output template\n"
        "  -s, --frame-size <width>x<height>  window or image size\n"
        "  -r, --frame-rate <integer>         output frame rate\n"
        "  -k, --frame-skip <integer>         output frame count start\n"
        "  -c, --frame-count <integer>        output frame count limit\n"
        "  -d, --dump-atlases <name-%%d.png>   dump atlas textures to png files\n"
        "\n"
        "Common options:\n"
        "  -f, --font <ttf-file>              font file (default %s)\n"
        "  -l, --load-factor <float>          text load (default %f)\n"
        "  -r, --render-text <string>         render text (default \"%s\")\n"
        "  -y, --overlay-stats                show statistics overlay\n"
        "  -m, --enable-msdf                  enable MSDF font rendering\n"
        "  -q, --quadruple                    quadruple the object count\n"
        "  -h, --help                         command line help\n",
        argv[0], font_path, load_factor, render_text);
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
        } else if (match_opt(argv[i], "-t", "--template")) {
            if (check_param(++i == argc, "--template")) break;
            filename_template = argv[i++];
        } else if (match_opt(argv[i], "-f","--font")) {
            if (check_param(++i == argc, "--font")) break;
            font_path = argv[i++];
        } else if (match_opt(argv[i], "-y", "--overlay-stats")) {
            overlay_stats = true;
            i++;
        } else if (match_opt(argv[i], "-m", "--enable-msdf")) {
            manager.msdf_enabled = true;
            manager.msdf_autoload = true;
            i++;
        } else if (match_opt(argv[i], "-l", "--load-factor")) {
            if (check_param(++i == argc, "--load-factor")) break;
            load_factor = atof(argv[i++]);
        } else if (match_opt(argv[i], "-r", "--render-text")) {
            if (check_param(++i == argc, "--render-text")) break;
            render_text = argv[i++];
        } else if (match_opt(argv[i], "-q", "--quadruple")) {
            load_factor *= 4.0f;
            i++;
        } else if (match_opt(argv[i], "-s", "--frame-size")) {
            if (check_param(++i == argc, "--frame-size")) break;
            sscanf(argv[i++], "%dx%d", &width, &height);
        } else if (match_opt(argv[i], "-r", "--frame-rate")) {
            if (check_param(++i == argc, "--frame-rate")) break;
            frame_rate = atoi(argv[i++]);
        } else if (match_opt(argv[i], "-k", "--frame-skip")) {
            if (check_param(++i == argc, "--frame-skip")) break;
            frame_skip = atoi(argv[i++]);
        } else if (match_opt(argv[i], "-c", "--frame-count")) {
            if (check_param(++i == argc, "--frame-count")) break;
            frame_count = atoi(argv[i++]);
        } else if (match_opt(argv[i], "-d", "--dump-atlases")) {
            if (check_param(++i == argc, "--dump-atlases")) break;
            atlas_dump_template = argv[i++];
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

    if (batch_mode) {
        glyb_offline(argc, argv);
    } else {
        glyb_gui(argc, argv);
    }

    return 0;
}
