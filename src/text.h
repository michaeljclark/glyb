#pragma once

/*
 * Text Attributes
 */

typedef enum : intptr_t {
    text_attr_font_family,
    text_attr_font_style,
    text_attr_font_size,
    text_attr_color,
    text_attr_underline,
    text_attr_strike,
} text_attr;

typedef std::pair<intptr_t,intptr_t> tag_pair_t;
typedef std::map<intptr_t,intptr_t> tag_map_t;
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
    void mark(size_t offset, size_t count, intptr_t attr, intptr_t val);
    void unmark(size_t offset, size_t count, intptr_t attr);
    void coalesce();

    std::string as_plaintext();
	std::string to_string();
};