// See LICENSE for license details.

#include "utf8.h"

size_t utf8_codelen(const char* s)
{
    if (!s) {
        return 0;
    } else if ((s[0]&0x80) == 0x0) {
        return 1;
    } else if ((s[0]&0xF8) == 0xF8) {
        return 5;
    } else if ((s[0]&0xF0) == 0xF0) {
        return 4;
    } else if ((s[0]&0xE0) == 0xE0) {
        return 3;
    } else if ((s[0]&0xC0) == 0xC0) {
        return 2;
    } else {
        return 1;
    }
}

size_t utf8_strlen(const char *s)
{
    size_t count = 0;
    while (*s) {
        size_t c = *s;
        s++;
        if ((c&0xC0) == 0xC0) {
            s++;
            if ((c&0xE0) == 0xE0) {
                s++;
                if ((c&0xF0) == 0xF0) {
                    s++;
                    if ((c&0xF8) == 0xF8) {
                       s++;
                    }
                }
            }
        }
        count++;
    }
    return count;
}

enum {
    m = 0x3f, n = 0x1f, o = 0xf, p = 0x7,
    q = 0xc0, r = 0xe0, t = 0xf0, u = 0xf8, v = 0x80
};

uint32_t utf8_to_utf32(const char *s)
{
    if (!s) {
        return -1;
    } else if ((s[0]&v) == 0x0) {
        return s[0];
    } else if ((s[0]&u) == u) {
        return ((s[0]&p)<<24)|((s[1]&m)<<18)|((s[2]&m)<<12)|((s[3]&m)<<6)|(s[4]&m);
    } else if ((s[0]&t) == t) {
        return ((s[0]&o)<<18)|((s[1]&m)<<12)|((s[2]&m)<<6)|(s[3]&m);
    } else if ((s[0]&r) == r) {
        return ((s[0]&n)<<12)|((s[1]&m)<<6)|(s[2]&m);
    } else if ((s[0]&q) == q) {
        return ((s[0]&m)<<6)|(s[1]&m);
    } else {
        return -1;
    }
}

utf32_code utf8_to_utf32_code(const char *s)
{
    if (!s) {
        return { -1, 0 };
    } else if ((s[0]&v) == 0x0) {
        return { s[0], 1 };
    } else if ((s[0]&u) == u) {
        return { ((s[0]&p)<<24)|((s[1]&m)<<18)|((s[2]&m)<<12)|((s[3]&m)<<6)|(s[4]&m), 5 };
    } else if ((s[0]&t) == t) {
        return { ((s[0]&o)<<18)|((s[1]&m)<<12)|((s[2]&m)<<6)|(s[3]&m), 4 };
    } else if ((s[0]&r) == r) {
        return { ((s[0]&n)<<12)|((s[1]&m)<<6)|(s[2]&m), 3 };
    } else if ((s[0]&q) == q) {
        return { ((s[0]&m)<<6)|(s[1]&m), 2 };
    } else {
        return { -1, 1 };
    }
}

int utf32_to_utf8(char *s, size_t len, uint32_t c)
{
    if (c < 0x80 && len >= 2) {
        s[0] = c;
        s[1] = 0;
        return 1;
    } else if (c < 0x800 && len >= 3) {
        s[0] = q|((c>>6)&0b11111);
        s[1] = v|(c&0b111111);
        s[2] = 0;
        return 2;
    } else if (c < 0x10000 && len >= 4) {
        s[0] = r|((c>>12)&0b1111);
        s[1] = v|((c>>6)&0b111111);
        s[2] = v|(c&0b111111);
        s[3] = 0;
        return 3;
    } else if (c < 0x110000 && len >= 5) {
        s[0] = t|((c>>18)&0b111);
        s[1] = v|((c>>12)&0b111111);
        s[2] = v|((c>>6)&0b111111);
        s[3] = v|(c&0b111111);
        s[4] = 0;
        return 4;
   } else if (c < 0x110000 && len >= 6) {
        s[0] = u|((c>>24)&0b11);
        s[1] = v|((c>>18)&0b111111);
        s[2] = v|((c>>12)&0b111111);
        s[3] = v|((c>>6)&0b111111);
        s[4] = v|(c&0b111111);
        s[5] = 0;
        return 5;
    }
    return -1;
}

/*
 * utf8_ranges_from_text
 *
 * scan text and return ranges of characters matching unicode blocks
 *
 * @param text to scan
 * @param length of text to scan
 * @param code to match on and split
 * @param mask to match on and split
 * @param flag to add to ranges that include the code
 */

std::vector<utf8_range> utf8_ranges_from_text(const char* text, size_t length,
    uint32_t code, uint32_t mask, uint32_t flag)
{
    std::vector<utf8_range> vec;

    size_t i = 0, j = 0;
    bool last = false;
    while (i < length)
    {
        utf32_code cp = utf8_to_utf32_code(text + i);
        bool match = (cp.code & mask) == code;
        if (i != 0 && last != match || i == UINT_MAX ) {
            vec.push_back({ j, uint(i - j), flag&-(uint32_t)last });
            j = i;
        }
        last = match;
        i += cp.len;
    }
    if (i - j > 0) {
        vec.push_back({ j, uint(i - j), flag&-(uint32_t)last });
    }

    return vec;
}
