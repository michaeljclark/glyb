# glyphic

glyphic is a high performance graphics API agnostic text rendering
and layout library. glyphic contains high level interfaces for text
rendering. glyphic outputs a font atlas bitmap, vertex arrays
and index arrays which can be used with OpenGL, Vulkan and DirectX.

glyphic includes a font-atlas based text renderer using HarfBuzz
and FreeType. Render time is less than 1 microsecond per glyph.
glyphic contains a 2D bin packer implementing the _**MAXRECTS-BSSF**_
algorithm as outlined in _"A Thousand Ways to Pack the Bin - A Practical
Approach to Two-Dimensional Rectangle Bin Packing, Jukka Jylänki_.

glyphic includes an MSDF (multi-channel signed distance field) glyph
renderer that uses the [msdfgen](https://github.com/Chlumsky/msdfgen)
library to create _variable-size_ MSDF font atlases. MSDF font atlases
are CPU-intensive to produce so an offline tool `genatlas` is included
to pregenerate MSDF font atlases. The advantage of MSDF font atlases is
that glyphs only need to be rendered for one size, and after the atlas
has been generated, text renderering becomes extremely fast. 

glyphic includes an online multi-threaded MSDF renderer. This allows
online MSDF atlas generation with any truetrype font. Rendering signed
distance field font atlases from truetype contours online is typically
prohibitive due to CPU requirements, however, when spread over 8 to 16
cores, the latency becomes acceptable for real-time use.

The project also contains several OpenGL examples using the library.

## Project Structure

- `src`
  - `binpack` - _bin packing algorithm_
  - `font` - _font manager, font face and font attributes_
  - `glyph` - _font atlas, text shaper and text renderer_
  - `text` - _text container, text layout and text part_
  - `utf8` - _UTF-8 <-> UTF-32 conversion_
  - `util` - _directory listing for POSIX and Win32_
- `examples`
  - `gllayout` - _example showing text layout with line breaks_
  - `glfont` - _example that displays sample text at multiple sizes_
  - `glsimple` - _simplest possible example for OpenGL_
  - `glbinpack` - _visualization of the bin packing algorithm_
- `third_party`
  - `freetype` - _font rendering engine_
  - `harfbuzz` - _text shaping engine_
  - `msdfgen` - _multi-channel signed distance field generator_
  - `zlib` - _massively spiffy yet delicately unobtrusive compression library_
  - `glad` - _OpenGL extension loader used by examples_
  - `glfw` - _OpenGL window library used by examples_

### Dependencies

glyphic links to: 
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

## Building

glyphic requires cmake to build. The remaining dependencies for
the library and examples are included as submodules.

_**Windows:**_

To create Visual Studio 2017 project, open _Visual Studio 2017 x64
Native Tools command prompt_, and run:

```
git clone --recursive https://github.com/michaeljclark/glyphic.git
cd glyphic
mkdir build
cd build
cmake -G "Visual Studio 15 2017 Win64" ..
```

_**Linux:**_

The Linux cmake build is set up by default to locate FreeType,
HarfBuzz and GLFW3 system packages. To install these dependencies
for Ubuntu 18.04, run:

```
sudo apt-get install libfreetype6-dev libharfbuzz-dev libglfw3-dev
```

## Example Code

The following code snippet shows glyphic's high level text layout interface:

```
{
    std::vector<text_segment> segments;
    std::vector<glyph_shape> shapes;
    std::vector<text_vertex> vertices;
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

_Note: graphics API specific code is necessary to update the atlas texture._

## Text Attributes

The following attributes are supported by `text_layout`.

- `font-family`
  - string e.g. Helvetica, Arial, Times, Times New Roman, etc.
- `font-style`
  - string e.g. Regular, Bold, Bold Italic, Italic, etc.
- `font-weight`
  - 100 = thin
  - 200 = extra light
  - 200 = light
  - 300 = light
  - 350 = semi light
  - 350 = book
  - 400 = normal
  - 400 = regular
  - 500 = medium
  - 600 = demibold
  - 600 = semibold
  - 700 = bold
  - 800 = extra bold
  - 800 = ultra bold
  - 900 = black
  - 900 = heavy
  - 950 = extra black
  - 950 = ultra black
- `font-slope`
  - 0 = none
  - 1 = oblique
  - 1 = italic
- `font-stretch`
  - 1 = ultra_condensed,
  - 2 = extra_condensed,
  - 3 = condensed,
  - 4 = semi_condensed,
  - 5 = medium,
  - 6 = semi_expanded,
  - 7 = expanded,
  - 8 = extra_expanded,
  - 9 = ultra_expanded,
- `font-spacing`
  - 0 = normal,
  - 1 = monospaced,
- `font-size`
  - integer points
- `baseline-shift`
  - integer pixels, y-shift, positive value raises
- `tracking`
  - integer pixels, y-spacing, added to character advance
- `line-spacing`
  - integer pixels, y-height, added for new lines
- `color`
  - HTML RGBA little-endian e.g. #ff000000 is black
- `language`
  - defaults to `en`, passed to HarfBuzz
