# glyb

glyb is an experimental graphics API agnostic text layout
and rendering library that builds on FreeType and HarfBuzz.

- simple graphics library agnostic draw lists
- embedded font matching and text layout engine
- offline and online multi-threaded font-atlas generation
- resolution independent signed distance field font rendering
- scalable and transparent handling of multiple glyph atlases
- gpu-accelerated distance field based 2D canvas

![glyb](/images/glyb.png)

## Introduction

glyb supports simple cross-platform text rendering and vector graphics
for C++ apps targeting OpenGL or Vulkan.

glyb contains high level interfaces for measuring and rendering text as
well as rudimentary vector graphics. glyb outputs font atlas bitmaps,
vertex arrays and index arrays which can be used with OpenGL or Vulkan.

glyb code-base is largely a proof-of-concept for antialiased text zoom,
and contains experimental quality code that answers questions regarding
cross platform GPU-based rendering of scalable vector graphics.

#### Font Atlas Management

glyb makes managing multiple font atlases with complete Unicode coverage
simple by emitting draw lists containing image switch and update commands,
so that any number of Unicode faces and glyphs can be used at one time.

glyb has online and offline font atlas glyph renderers using HarfBuzz
and FreeType. Render time is less than one microsecond per glyph.
glyb's font atlas uses an online 2D _**MAXRECTS-BSSF**_ derived bin
packing algorithm, as outlined in _"A Thousand Ways to Pack the Bin - A
Practical Approach to Two-Dimensional Rectangle Bin Packing, Jukka Jylänki_.

#### Signed Distance Field Fonts

glyb includes an MSDF (multi-channel signed distance field) glyph
renderer that uses the [msdfgen](https://github.com/Chlumsky/msdfgen)
library to create _variable-size_ MSDF font atlases. MSDF font atlases
are CPU-intensive to produce so an offline tool `genatlas` is included
to pregenerate MSDF font atlases. The advantage of MSDF font atlases is
that glyphs only need to be rendered for one size. After the atlas has
been generated, text renderering becomes extremely fast. 

glyb includes an online multi-threaded MSDF renderer. This allows
online MSDF atlas generation with any truetype font. Rendering signed
distance field font atlases from truetype contours online is typically
prohibitive due to CPU requirements, however, when spread over 8 to 16
cores, the latency becomes acceptable for real-time use.

The project also contains several OpenGL examples using the library.

## Project Structure

- `src`
  - `binpack` - _bin packing algorithm use by the font atlas_
  - `canvas` - _gpu-accelerated distance field based 2D canvas_
  - `file` - _simple filesystem abstraction_
  - `font` - _font manager, font face and font attributes_
  - `glyph` - _font atlas, text shaper and text renderer_
  - `image` - _image with support for PNG loading and saving_
  - `text` - _text container, text layout and text part_
  - `utf8` - _UTF-8 <-> UTF-32 conversion_
- `examples`
  - `fontdb` - _example demonstrates scanning font metadata_
  - `ftrender` - _example renders glyphs to the console_
  - `genatlas` - _example MSDF font atlas batch generator_
  - `glbinpack` - _visualization of the bin packing algorithm_
  - `glcanvas` - _example GPU-acceralated Bézier font rendering_
  - `glfont` - _example that displays sample text at multiple sizes_
  - `gllayout` - _example showing text layout with line breaks_
  - `glsimple` - _simplest possible example for OpenGL_
  - `glyb` - _scalability test for regular and MSDF font atlases_
- `third_party`
  - `freetype` - _font rendering engine_
  - `glad` - _OpenGL extension loader used by examples_
  - `glfw` - _OpenGL window library used by examples_
  - `harfbuzz` - _text shaping engine_
  - `imgui` - _immediate Mode Graphical User interface for C++_
  - `libpng` - _the official PNG reference library_
  - `msdfgen` - _multi-channel signed distance field generator_
  - `zlib` - _massively spiffy yet delicately unobtrusive compression library_

### Dependencies

glyb links to: 
[FreeType](https://github.com/aseprite/freetype2)
[†](https://www.freetype.org/),
[HarfBuzz](https://github.com/harfbuzz/harfbuzz)
[†](https://www.freedesktop.org/wiki/Software/HarfBuzz/),
[msdfgen](https://github.com/Chlumsky/msdfgen)
[†](https://github.com/Chlumsky/msdfgen/files/3050967/thesis.pdf),
[zlib](https://github.com/madler/zlib.git)
[†](http://zlib.net/),
[libpng](https://github.com/glennrp/libpng)
[†](http://www.libpng.org/pub/png/libpng.html),
and the examples link to:
[GLAD](https://github.com/Dav1dde/glad) [†](https://glad.dav1d.de/),
[GLFW](https://github.com/glfw/glfw) [†](https://www.glfw.org/).
[ImGui](https://github.com/ocornut/imgui) [†](https://www.patreon.com/imgui),

## Example Code

The following code snippet shows glyb's high level text layout interface:

```
{
    std::vector<text_segment> segments;
    std::vector<glyph_shape> shapes;
    std::vector<draw_vertex> vertices;
    std::vector<uint32_t> indices;

    text_shaper shaper;
    text_renderer renderer(&manager);
    text_layout layout(&manager, &shaper, &renderer);
    text_container c;

    c.append(text_part(
        "    Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed "
        "do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
        "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris "
        "nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in "
        "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
        "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
        "culpa qui officia deserunt mollit anim id est laborum.    ",
        {{ "font-size", "36" },
         { "font-style", "bold" },
         { "color", "#7f7f9f" },
         { "line-spacing", "48" }}));

    layout.layout(segments, &c, 50, 50, 900, 700);
    for (auto &segment : segments) {
        shapes.clear();
        shaper.shape(shapes, &segment);
        renderer.render(vertices, indices, shapes, &segment);
    }
}
```

Simple GLSL vertex shader:

```
attribute vec3 a_pos;
attribute vec2 a_uv0;
attribute vec4 a_color;
attribute float a_gamma;

uniform mat4 u_mvp;

varying vec4 v_color;
varying vec2 v_uv0;
varying float v_gamma;

void main() {
    v_color = a_color;
    v_gamma = a_gamma;
    v_uv0 = a_uv0;
    gl_Position = u_mvp * vec4(a_pos.xyz, 1.0);
}
```

Simple GLSL fragment shader:

```
varying vec4 v_color;
varying vec2 v_uv0;
varying float v_gamma;

uniform sampler2D u_tex0;

void main() {
    vec4 t_color = texture2D(u_tex0, v_uv0);
    gl_FragColor = v_color * vec4(pow(t_color.rgb, vec3(1.0/v_gamma)), t_color.a);
}
```

## Draw Lists

The render interface is abstracted using a graphics API agnostic draw list.
The draw list includes information to create all necessary textures. A single
batch can use several fonts, access multiple font atlas bitmaps and include
other 2D geometry such as line and rectangles. The batch draw commands
add vertices and indicies to a pair of vertex array and index array. Client
code can accumulate into one large draw batch or alternatively create many
if they are invalidated at different frequencies. The following diagram shows
the draw list model relationships.

![model](/images/model.png)
_**Figure 1: Draw List Model Diagram**_

The follow examples shows OpenGL code to render a draw list. glyb does
not call any graphic APIs directly making it easy to integrate with any
graphics API. There is _**zero**_ graphics library API code in glyb, as
the interface is exclusively via the draw list. This makes it trivial to
integrate with Vulkan for example.  Note: in this example, `cmd_shader_gl`
and `cmd_mode_gl` functions map the draw list shader and primitive types
defined in `draw.h` to graphics library shader program and batch types.

```
static void display()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
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
        glUseProgram(cmd_shader_gl(cmd.shader)->pid);
        glBindTexture(GL_TEXTURE_2D, tex_map[cmd.iid]);
        glDrawElements(cmd_mode_gl(cmd.mode), cmd.count, GL_UNSIGNED_INT,
            (void*)(cmd.offset * sizeof(uint)));
    }

    glfwSwapBuffers(window);
}
```

## Examples

glyb contains several examples programs showing how to use its API:

- `fontdb` - _example demonstrates scanning font metadata_
- `ftrender` - _example renders glyphs to the console_
- `genatlas` - _example MSDF font atlas batch generator_
- `glbinpack` - _visualization of the bin packing algorithm_
- `glcanvas` - _example GPU-acceralated Bézier font rendering_
- `glemoji` - _example that renders color emoji with alpha transparency_
- `glfont` - _example that displays sample text at multiple sizes_
- `glgraph` - _example showing the UI9 graphical vector toolkit_
- `gllayout` - _example showing text layout with line breaks_
- `glsimple` - _simplest possible example for OpenGL_
- `glyb` - _scalability test for regular and MSDF font atlases_

### glfont

shows sample text font size variations.

![glfont](/images/glfont.png)

### glgraph

shows graphical toolkit using the canvas.

![glgraph](/images/glgraph.png)

### glcanvas

shows usage of the GPU accelerated canvas.

![glcanvas-1](/images/glcanvas-1.png)
![glcanvas-2](/images/glcanvas-2.png)

### glglyph

shows text selection and line editing.

![glglyph](/images/glglyph.png)

### glemoji

shows color emoji glyph rendering.

![glemoji](/images/glemoji.png)

## Building

glyb requires cmake to build. The remaining dependencies for
the library and examples are included as submodules.

_**Source code**_

```
git clone --recursive https://github.com/michaeljclark/glyb.git
cd glyb
```

_**Windows**_

To create Visual Studio 2019 project, open _Visual Studio 2019 x64
Native Tools command prompt_, and run:

```
cmake -G "Visual Studio 16 2019" -A x64 -B build
cmake --build build --config RelWithDebInfo -j
```

_**Linux**_

The Linux cmake build is set up by default to locate FreeType,
HarfBuzz and GLFW3 system packages. The following commands will
install dependencies for Ubuntu 18.04, and build the project:

```
sudo apt-get install libfreetype6-dev libharfbuzz-dev libglfw3-dev
cmake -B build
cmake --build build --config RelWithDebInfo -j
```

_**macOS**_

The macOS cmake rules will build the included FreeType, HarfBuzz
and GLFW3 depdencies. The following commands will build the project:

```
cmake -G Xcode -B build
cmake --build build --config RelWithDebInfo
```

_**Ninja**_

The project can be built using the ninja build tool on Windows, Linux
and macOS, by specifying the `Ninja` generator to cmake:

```
cmake -G Ninja -B build
cmake --build build -- --verbose
```
