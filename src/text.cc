// See LICENSE for license details.

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <cctype>
#include <cassert>
#include <cmath>

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <tuple>
#include <algorithm>
#include <atomic>
#include <mutex>

#include "utf8.h"
#include "color.h"
#include "binpack.h"
#include "font.h"
#include "image.h"
#include "draw.h"
#include "glyph.h"
#include "text.h"

/*
 * Text Part
 */

text_part::text_part(std::string s) : text(s) {}

text_part::text_part(std::string s, tag_map_t t) : text(s), tags(t) {}

text_part::text_part(std::string s, tags_initializer_t l) : text(s)
{
    for (auto li = l.begin(); li != l.end(); li++) {
        tags.insert(tags.end(), *li);
    }
}

std::string text_part::to_string()
{
    std::string s;
    for (auto i = tags.begin(); i != tags.end(); i++) {
        s.append("{");
        s.append(i->first);
        s.append("=");
        s.append(i->second);
        s.append(" ");
    }
    s.append(": \"");
    s.append(text);
    s.append("\"");
    for (auto i = tags.begin(); i != tags.end(); i++) {
        s.append("}");
    }
    return s;
}

/*
 * Text Container
 */

text_container::text_container(std::string s) : parts({ text_part(s) }) {}

text_container::text_container(std::string s,
    tag_map_t t) : parts({ text_part(s, t) }) {}

text_container::text_container(std::string s,
    tags_initializer_t l) : parts({ text_part(s, l) }) {}

text_container::text_container(text_part c) : parts({ c }) {}

void text_container::erase(size_t offset, size_t count)
{
    size_t poff = 0;
    for (auto i = parts.begin(); i != parts.end();) {
        size_t plen = i->text.size();
        size_t pbeg = std::min(plen, std::max(poff, offset) - poff);
        size_t pcnt = std::min(plen - pbeg, (size_t)std::max
            (ptrdiff_t(offset + count - poff - pbeg), ptrdiff_t(0)));
        if (offset < poff + plen) {
            if (pbeg == 0 && pcnt == plen) {
                i = parts.erase(i);
            } else {
                if (pcnt > 0) {
                    i->text.erase(pbeg, pcnt);
                }
                i++;
            }
        } else {
            i++;
        }
        poff += plen;
    }
    coalesce();
}

void text_container::insert(size_t offset, std::string s)
{
    if (parts.size() == 0) {
        parts.insert(parts.end(),std::string());
    }
    size_t poff = 0;
    for (auto i = parts.begin(); i != parts.end(); i++) {
        size_t plen = i->text.size();
        size_t pbeg = std::min(plen, std::max(poff, offset) - poff);
        if (offset >= poff && offset <= poff + plen) {
            i->text.insert(pbeg, s);
            break;
        }
        poff += plen;
    }
    coalesce();
}

void text_container::insert(size_t offset, text_part c)
{
    if (parts.size() == 0) {
        parts.insert(parts.end(),std::string());
    }
    size_t poff = 0;
    for (auto i = parts.begin(); i != parts.end(); i++) {
        size_t plen = i->text.size();
        size_t pbeg = std::min(plen, std::max(poff, offset) - poff);
        if (offset >= poff && offset <= poff + plen) {
            if (i->tags == c.tags) {
                i->text.insert(pbeg, c.text);
                break;
            } else {
                if (pbeg < plen) {
                    if (plen - pbeg > 0) {
                        std::string s1 = i->text.substr(0, pbeg);
                        std::string s2 = i->text.substr(pbeg, plen - pbeg);
                        i->text = s1;
                        parts.insert(std::next(parts.insert(std::next(i), c)),
                            text_part(s2, i->tags));
                    } else {
                        parts.insert(i, c);
                    }
                } else {
                    parts.insert(std::next(i), c);
                }
            }
            break;
        }
        poff += plen;
    }
    coalesce();
}

void text_container::append(std::string s)
{
    parts.insert(parts.end(), { s });
    coalesce();
}

void text_container::append(text_part c)
{
    parts.insert(parts.end(), c);
    coalesce();
}

void text_container::mark(size_t offset, size_t count, std::string attr,
    std::string val)
{
    if (parts.size() == 0) {
        parts.insert(parts.end(),std::string());
    }
    size_t poff = 0;
    for (auto i = parts.begin(); i != parts.end(); i++) {
        size_t plen = i->text.size();
        size_t pbeg = std::min(plen, std::max(poff, offset) - poff);
        size_t pcnt = std::min(plen - pbeg, (size_t)std::max
            (ptrdiff_t(offset + count - poff - pbeg), ptrdiff_t(0)));
        if (pcnt == 0) {
            poff += plen;
            continue;
        }
        if (pbeg > 0) {
            if (pbeg + pcnt < plen) {
                // 3-way split (begin <S1> pbeg <S2> pbeg+pcnt <S3> end)
                std::string s1 = i->text.substr(0, pbeg);
                std::string s2 = i->text.substr(pbeg, pcnt); // <-- tags
                std::string s3 = i->text.substr(pbeg+pcnt);
                auto savetags1 = i->tags;
                auto savetags2 = i->tags;
                i->text = s1;
                savetags1[attr] = val;
                i = parts.insert(std::next(i), text_part(s2, savetags1));
                i = parts.insert(std::next(i), text_part(s3, savetags2));
            } else {
                // 2-way split (begin <S1> pbeg <S2> pbeg+pcnt/end)
                std::string s1 = i->text.substr(0, pbeg);
                std::string s2 = i->text.substr(pbeg); // <-- tags
                auto savetags = i->tags;
                i->text = s1;
                savetags[attr] = val;
                i = parts.insert(std::next(i), text_part(s2, savetags));
            }
        } else {
            if (pcnt < plen) {
                // 2-way split (begin/pbeg <S1> pbeg+pcnt <S2> end)
                std::string s1 = i->text.substr(0, pcnt); // <-- tags
                std::string s2 = i->text.substr(pcnt);
                auto savetags = i->tags;
                i->text = s1;
                i->tags[attr] = val;
                i = parts.insert(std::next(i), text_part(s2, savetags));
            } else {
                // whole string
                i->tags[attr] = val;
            }
        }
        poff += plen;
    }
    coalesce();
}

void text_container::unmark(size_t offset, size_t count, std::string attr)
{
    if (parts.size() == 0) {
        parts.insert(parts.end(),std::string());
    }
    size_t poff = 0;
    for (auto i = parts.begin(); i != parts.end(); i++) {
        size_t plen = i->text.size();
        size_t pbeg = std::min(plen, std::max(poff, offset) - poff);
        size_t pcnt = std::min(plen - pbeg, (size_t)std::max
            (ptrdiff_t(offset + count - poff - pbeg), ptrdiff_t(0)));
        if (pcnt == 0) {
            poff += plen;
            continue;
        }
        if (pbeg > 0) {
            if (pbeg + pcnt < plen) {
                // 3-way split (begin <S1> pbeg <S2> pbeg+pcnt <S3> end)
                std::string s1 = i->text.substr(0, pbeg);
                std::string s2 = i->text.substr(pbeg, pcnt); // <-- tags
                std::string s3 = i->text.substr(pbeg+pcnt);
                auto savetags1 = i->tags;
                auto savetags2 = i->tags;
                i->text = s1;
                savetags1.erase(attr);
                i = parts.insert(std::next(i), text_part(s2, savetags1));
                i = parts.insert(std::next(i), text_part(s3, savetags2));
            } else {
                // 2-way split (begin <S1> pbeg <S2> pbeg+pcnt/end)
                std::string s1 = i->text.substr(0, pbeg);
                std::string s2 = i->text.substr(pbeg); // <-- tags
                auto savetags = i->tags;
                i->text = s1;
                savetags.erase(attr);
                i = parts.insert(std::next(i), text_part(s2, savetags));
            }
        } else {
            if (pcnt < plen) {
                // 2-way split (begin/pbeg <S1> pbeg+pcnt <S2> end)
                std::string s1 = i->text.substr(0, pcnt); // <-- tags
                std::string s2 = i->text.substr(pcnt);
                auto savetags = i->tags;
                i->text = s1;
                i->tags.erase(attr);
                i = parts.insert(std::next(i), text_part(s2, savetags));
            } else {
                // whole string
                i->tags.erase(attr);
            }
        }
        poff += plen;
    }
    coalesce();
}

void text_container::coalesce()
{
    for (auto i = parts.rbegin(); i != parts.rend();) {
        if (std::next(i) != parts.rend() && std::next(i)->tags == i->tags) {
            std::next(i)->text.append(i->text);
            std::advance(i, 1);
            parts.erase(i.base());
        } else {
            std::advance(i, 1);
        }
    }
}

std::string text_container::as_plaintext()
{
    std::string s;
    for (const auto &piece : parts) {
        s += piece.text;
    }
    return s;
}

std::string text_container::to_string()
{
    std::string s;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i != 0) {
            s.append(" ");
        }
        s.append(parts[i].to_string());
    }
    return s;
}

/*
 * Text Layout
 */

static const char* language_default = "en";
 
inline bool compare(std::string s1, std::string s2)
{
    return ((s1.size() == s2.size()) &&
        std::equal(s1.begin(), s1.end(), s2.begin(), [](char & c1, char & c2) {
            return (c1 == c2 || std::toupper(c1) == std::toupper(c2));
    }));
}

inline std::string lookupString(tag_map_t &map, std::string key, std::string val = "")
{
    auto i = map.find(key);
    return (i == map.end()) ? val : i->second;
}

inline int lookupInteger(tag_map_t &map, std::string key, int val = 0)
{
    auto i = map.find(key);
    return (i == map.end()) ? val : atoi(i->second.c_str());
}

inline float lookupFloat(tag_map_t &map, std::string key, float val = 0)
{
    auto i = map.find(key);
    return (i == map.end()) ? val : (float)atof(i->second.c_str());
}

inline int lookupEnumInteger(tag_map_t &map, std::string key,
    const char* names[], const int values[], size_t count, int val = 0)
{
    auto i = map.find(key);
    if (i == map.end()) return val;
    for (size_t j = 0; j < count; j++) {
        if (compare(i->second, names[j])) return (int)j;
    }
    return val;
}

inline int lookupEnumFloat(tag_map_t &map, std::string key, 
    const char* names[], const float values[], size_t count, int val = 0)
{
    auto i = map.find(key);
    if (i == map.end()) return val;
    for (size_t j = 0; j < count; j++) {
        if (compare(i->second, names[j])) return (int)j;
    }
    return val;
}

void text_layout::style(text_segment *segment, text_part *part)
{
    /* converts text part attributes into a font face and size */

    font_data fontData;
    std::string colorStr;
    std::string languageStr;

    fontData.familyName =
        lookupString(part->tags, "font-family", font_family_any);
    fontData.styleName =
        lookupString(part->tags, "font-style", font_style_any);
    fontData.fontWeight =
        (font_weight)lookupEnumInteger(part->tags, "font-weight",
            font_manager::weightName, font_manager::weightTable,
            font_weight_count, font_weight_any);
    fontData.fontSlope =
        (font_slope)lookupEnumInteger(part->tags, "font-slope",
            font_manager::slopeName, font_manager::slopeTable,
            font_slope_count, font_slope_any);
    fontData.fontStretch =
        (font_stretch)lookupEnumFloat(part->tags, "font-stretch",
            font_manager::stretchName, font_manager::stretchPercentTable,
            font_stretch_count, font_stretch_any);
    fontData.fontSpacing =
        (font_spacing)lookupEnumInteger(part->tags, "font-spacing", 
            font_manager::spacingName, font_manager::spacingTable,
            font_spacing_count, font_spacing_any);

    segment->face = manager->findFontByData(fontData);
    segment->font_size = (int)ceilf(
        lookupFloat(part->tags, "font-size", font_size_default) * 64.0f);
    segment->baseline_shift = lookupFloat(part->tags, "baseline-shift",
        baseline_shift_default);
    segment->tracking = lookupFloat(part->tags, "tracking",
        tracking_default);
    segment->line_spacing = lookupFloat(part->tags, "line-spacing");

    if (segment->line_spacing == 0) {
        segment->line_spacing = roundf((float)static_cast<font_face_ft*>
            (segment->face)->get_height(segment->font_size) / 64.0f);
    }

    colorStr = lookupString(part->tags, "color");
    if (colorStr.size() == 0) {
        segment->color = color_default;
    } else {
        segment->color = color(colorStr).rgba32();
    }

    languageStr = lookupString(part->tags, "language");
    if (colorStr.size() == 0) {
        segment->language = language_default;
    } else {
        segment->language = languageStr;
    }
}

void text_layout::layout(std::vector<text_segment> &segments,
    text_container *container, int x, int y, int width, int height)
{
    std::vector<glyph_shape> shapes;

    /* layout the text in the container into styled text segments */
    int dx = x, dy = y;
    for (size_t i = 0; i < container->parts.size(); i++)
    {
        text_part *part = &container->parts[i];

        /* make a text segment */
        text_segment segment(part->text, language_default);

        /* get font, font size, tracking, line_height, color, etc. */
        style(&segment, part);

        /* set segment position */
        segment.x = (float)dx;
        segment.y = (float)dy + segment.line_spacing;

        /* measure text, splitting over multiple lines */
        float segment_width = 0;
        unsigned break_cluster = 0, space_cluster = 0;
        for (;;) {
            break_cluster = 0;
            segment_width = 0;

            /* find fitting segment width */
            shapes.clear();
            shaper->shape(shapes, segment);
            for (auto &s : shapes) {
                segment_width += s.x_advance/64.0f + segment.tracking;
                if (segment.text[s.cluster] == ' ') {
                    space_cluster = s.cluster;
                }
                if (dx + segment_width > x + width) {
                    break_cluster = s.cluster;
                    break;
                }
            }

            /* exit inner loop if segment fits width */
            if (break_cluster == 0) {
                break;
            }

            /* break segment at space character if seen */
            if (space_cluster > 0) {
                break_cluster = space_cluster + 1;
            }

            /* perform segment split */
            std::string s1 = segment.text.substr(0, break_cluster - 1);
            std::string s2 = segment.text.substr(break_cluster);
            segment.text = s1;
            segments.push_back(segment);

            /* advance to next line */
            dx = x;
            dy += (int)segment.line_spacing;

            /* loop with remaining text */
            segment.text = s2;
            segment.x = (float)dx;
            segment.y = (float)dy + segment.line_spacing;
        }

        /* add segment to the list */
        segments.push_back(segment);

        /* increment position, advancing to next line if required */
        dx += (int)ceilf(segment_width);
        if (dx > width) {
            dx = x;
            dy += (int)segment.line_spacing;
        }
        if (dy > y + height) {
            break;
        }
    }
}
