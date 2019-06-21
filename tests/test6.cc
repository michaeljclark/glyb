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
#include <string>
#include <algorithm>
#include <atomic>
#include <mutex>

#include "binpack.h"
#include "image.h"
#include "draw.h"
#include "font.h"
#include "glyph.h"
#include "text.h"

std::string font_family_default = font_family_any;
std::string font_style_default = font_style_any;
font_weight font_weight_default = font_weight_regular;
font_slope font_slope_default = font_slope_any;
font_stretch font_stretch_default = font_stretch_any;
font_spacing font_spacing_default = font_spacing_any;
int font_size_default = 12;

const char* text_attributes[] = {
    "none",
    "font-name",
    "font-family",
    "font-style",
    "font-weight",
    "font-slope",
    "font-stretch",
    "font-spacing",
    "font-size",
    "text-color",
    "underline",
    "strike",
};

static font_manager_ft manager;

const char* test_str_1 = "the quick brown fox jumps over the lazy dog";
static const char* text_lang = "en";

void t1()
{
    font_face *f;

    f = manager.findFontByFamily("Roboto", font_style_normal);
    if (f) {
        printf("byFamily: %s\n", f->getFontData().toString().c_str());
    }

    f = manager.findFontByName("Roboto-Light");
    if (f) {
        printf("byName: %s\n", f->getFontData().toString().c_str());
    }

    font_data fontData;
    fontData.familyName = font_family_any;
    fontData.styleName = font_style_any;
    fontData.fontWeight = font_weight_any;
    fontData.fontSlope = font_slope_any;
    fontData.fontStretch = font_stretch_any;
    fontData.fontSpacing = font_spacing_any;
    f = manager.findFontByData(fontData);
    if (f) {
        printf("byData: %s\n", f->getFontData().toString().c_str());
    }
}

void t2()
{
    auto face = manager.findFontByPath("fonts/Roboto-Regular.ttf");

    std::vector<text_segment> segments;
    std::vector<glyph_shape> shapes;
    std::vector<draw_vertex> vertices;
    std::vector<uint32_t> indices;

    text_shaper_hb shaper;
    text_renderer_ft renderer(&manager);
    text_segment segment(test_str_1, text_lang, face,
        48 * 64, 0, 0, 0xffffffff);
    text_layout layout(&manager, &shaper, &renderer);

    text_container c;
    c.append(text_part("regular ", {{ "font-style", "Regular" }}));
    c.append(text_part("italic ", {{ "font-style", "Italic" }} ));
    c.append(text_part("bold ", {{ "font-weight", "bold" }} ));

    layout.layout(segments, &c, 10, 10, 300, 500);

    for (auto &seg : segments) {
        if (seg.face) {
            printf("x: %f, y: %f, font-name: %s, font-size: %d, text: %s\n",
                seg.x, seg.y, seg.face->name.c_str(), seg.font_size,
                seg.text.c_str());
        }
    }
}

int main()
{
    manager.scanFontDir("fonts");
    t1();
    t2();
}