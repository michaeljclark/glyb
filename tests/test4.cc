#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <climits>
#include <cassert>
#include <cmath>
#include <ctime>

#include <vector>
#include <map>
#include <memory>
#include <tuple>
#include <atomic>
#include <mutex>
#include <chrono>

#include "binpack.h"
#include "utf8.h"
#include "draw.h"
#include "font.h"
#include "image.h"
#include "glyph.h"
#include "text.h"

using namespace std::chrono;

const char* test_str_1 = "the quick brown fox jumps over the lazy dog";
static const char* text_lang = "en";
size_t iterations = 100000;

int main()
{
    font_manager_ft manager;
    auto face = manager.findFontByPath("fonts/Roboto-Regular.ttf");

    std::vector<glyph_shape> shapes;
    draw_list batch;

    text_shaper_hb shaper;
    text_renderer_ft renderer(&manager);
    text_segment segment(test_str_1, text_lang, face,
        48 * 64, 0, 0, 0xffffffff);

    /* shape (cold) */
    const auto t1 = high_resolution_clock::now();
    shaper.shape(shapes, &segment);
    const auto t2 = high_resolution_clock::now();

    shapes.clear();

    /* shape (hot) */
    const auto t3 = high_resolution_clock::now();
    shaper.shape(shapes, &segment);
    const auto t4 = high_resolution_clock::now();

    /* render (cold) */
    const auto t5 = high_resolution_clock::now();
    renderer.render(batch, shapes, &segment);
    const auto t6 = high_resolution_clock::now();

    draw_list_clear(batch);

    /* render (hot) */
    const auto t7 = high_resolution_clock::now();
    renderer.render(batch, shapes, &segment);
    const auto t8 = high_resolution_clock::now();

    /* shape (loop) */
    const auto t9 = high_resolution_clock::now();
    for (size_t i = 0; i < iterations; i++) {
        shapes.clear();
        shaper.shape(shapes, &segment);
    }
    const auto t10 = high_resolution_clock::now();

    /* render (loop) */
    const auto t11 = high_resolution_clock::now();
    for (size_t i = 0; i < iterations; i++) {
        draw_list_clear(batch);
        renderer.render(batch, shapes, &segment);
    }
    const auto t12 = high_resolution_clock::now();

    float r1 = (float)duration_cast<nanoseconds>(t2 - t1).count() / 1e3;
    float r2 = (float)duration_cast<nanoseconds>(t4 - t3).count() / 1e3;
    float r3 = (float)duration_cast<nanoseconds>(t6 - t5).count() / 1e3;
    float r4 = (float)duration_cast<nanoseconds>(t8 - t7).count() / 1e3;
    float r5 = (float)duration_cast<nanoseconds>(t10 - t9).count() / 1e3;
    float r6 = (float)duration_cast<nanoseconds>(t12 - t11).count() / 1e3;

    printf("shape (cold)               = %12.3f microseconds\n", r1);
    printf("shape (hot)                = %12.3f microseconds\n", r2);
    printf("render (cold)              = %12.3f microseconds\n", r3);
    printf("render (hot)               = %12.3f microseconds\n", r4);
    printf("shape time (per glyph)     = %12.3f microseconds\n", r5/(iterations*strlen(test_str_1)));
    printf("render time (per glyph)    = %12.3f microseconds\n", r6/(iterations*strlen(test_str_1)));
}