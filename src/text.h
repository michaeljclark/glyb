// See LICENSE for license details.

#pragma once

/*
 * Text Attributes
 */

typedef enum : intptr_t {
    text_attr_none,
    text_attr_font_name,
    text_attr_font_family,
    text_attr_font_style,
    text_attr_font_weight,
    text_attr_font_slope,
    text_attr_font_stretch,
    text_attr_font_spacing,
    text_attr_font_size,
    text_attr_color,
    text_attr_underline,
    text_attr_strike,
} text_attr;

typedef std::pair<std::string,std::string> tag_pair_t;
typedef std::map<std::string,std::string> tag_map_t;
typedef std::initializer_list<tag_pair_t> tags_initializer_t;

/*
 * Text Part
 */

struct text_part
{
    std::string text;
    tag_map_t tags;

    text_part() = default;
    text_part(std::string s);
    text_part(std::string s, tag_map_t t);
    text_part(std::string s, tags_initializer_t l);

    std::string to_string();
};

/*
 * Text Container
 */

struct text_container
{
    std::vector<text_part> parts;

    text_container() = default;
    text_container(std::string s);
    text_container(std::string s, tag_map_t t);
    text_container(std::string s, tags_initializer_t l);
    text_container(text_part c);

    void erase(size_t offset, size_t count);
    void insert(size_t offset, std::string s);
    void insert(size_t offset, text_part c);
    void append(std::string s);
    void append(text_part c);
    void mark(size_t offset, size_t count, std::string attr, std::string val);
    void unmark(size_t offset, size_t count, std::string attr);
    void coalesce();

    std::string as_plaintext();
    std::string to_string();
};

/*
 * Text Layout
 */

struct text_layout
{
    font_manager_ft* manager;
    text_shaper* shaper;
    text_renderer_ft* renderer;

    const float font_size_default = 12.0f;
    const uint32_t color_default = 0xff000000;
    const float baseline_shift_default = 0;
    const float tracking_default = 0;

    text_layout(font_manager_ft* manager,
        text_shaper* shaper, text_renderer_ft* renderer);

    void style(text_segment *segment, text_part *part);
    void layout(std::vector<text_segment> &segments,
        text_container *container, int x, int y, int width, int height);
};

inline text_layout::text_layout(font_manager_ft* manager, text_shaper* shaper,
    text_renderer_ft* renderer) :
    manager(manager), shaper(shaper), renderer(renderer) {}
