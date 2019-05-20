#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <climits>

#include <memory>
#include <vector>
#include <map>
#include <tuple>
#include <chrono>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include <hb.h>
#include <hb-ft.h>

#include "binpack.h"
#include "utf8.h"
#include "glyph.h"

using namespace std::chrono;

const char* test_str_1 = "the quick brown fox jumps over the lazy dog";
size_t iterations = 100000;

int main()
{
    font_manager manager;
    font_atlas atlas;
    font_face *face = manager.lookup_font("fonts/Roboto-Regular.ttf");

    std::vector<text_vertex> vertices;
    std::vector<uint32_t> indices;

    text_renderer renderer(&manager, &atlas);
    text_segment segment(test_str_1, face, 48 * 64, 0, 0, 0xffffffff);

    /* render (cold) */
    const auto t1 = high_resolution_clock::now();
    renderer.render(vertices, indices, &segment);
    const auto t2 = high_resolution_clock::now();

    indices.clear();
    vertices.clear();

    /* render (hot) */
    const auto t3 = high_resolution_clock::now();
    renderer.render(vertices, indices, &segment);
    const auto t4 = high_resolution_clock::now();

    /* render (loop) */
    const auto t5 = high_resolution_clock::now();
    for (size_t i = 0; i < iterations; i++) {
        renderer.render(vertices, indices, &segment);
    }
    const auto t6 = high_resolution_clock::now();

    float r1 = (float)duration_cast<nanoseconds>(t2 - t1).count() / 1e3;
    float r2 = (float)duration_cast<nanoseconds>(t4 - t3).count() / 1e3;
    float r3 = (float)duration_cast<nanoseconds>(t6 - t5).count() / 1e3;

    printf("runtime (cold) = %12.3f microseconds\n", r1);
    printf("runtime (hot)  = %12.3f microseconds\n", r2);
    printf("glyph time     = %12.3f microseconds\n", r3/(iterations*strlen(test_str_1)));
}