// See LICENSE for license details.

#include "utf8.h"

size_t utf8_codelen(const char* s)
{
    if (!s) {
        return 0;
    } else if ((s[0] & 0x80) == 0x0) {
        return 1;
    } else if ((s[0] & 0xF8) == 0xF8) {
        return 5;
    } else if ((s[0] & 0xF0) == 0xF0) {
        return 4;
    } else if ((s[0] & 0xE0) == 0xE0) {
        return 3;
    } else if ((s[0] & 0xC0) == 0xC0) {
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
        if ((c & 0xC0) == 0xC0) {
            s++;
            if ((c & 0xE0) == 0xE0) {
                s++;
                if ((c & 0xF0) == 0xF0) {
                    s++;
                    if ((c & 0xF8) == 0xF8) {
                       s++;
                    }
                }
            }
        }
        count++;
    }
    return count;
}

uint32_t utf8_to_utf32(const char *s)
{
    if (!s) {
        return -1;
    } else if ((s[0] & 0x80) == 0x0) {
        return s[0];
    } else if ((s[0] & 0xF8) == 0xF8) {
        return ((s[0] & 0x07) << 24) | ((s[1] & 0x3F) << 18) |
            ((s[2] & 0x3F) << 12) | ((s[3] & 0x3F) << 6) | (s[4] & 0x3F);
    } else if ((s[0] & 0xF0) == 0xF0) {
        return ((s[0] & 0x0F) << 18) | ((s[1] & 0x3F) << 12) |
            ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    } else if ((s[0] & 0xE0) == 0xE0) {
        return ((s[0] & 0x1F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    } else if ((s[0] & 0xC0) == 0xC0) {
        return ((s[0] & 0x3F) << 6) | (s[1] & 0x3F);
    } else {
        return -1;
    }
}

int utf32_to_utf8(char *s, size_t len, uint32_t c)
{
    if (c < 0x80 && len >= 2) {
        s[0] = c;
        s[1] = 0;
    } else if (c < 0x800 && len >= 3) {
        s[0] = 0xC0 | ((c >> 6) & 0b11111);
        s[1] = 0x80 | (c & 0b111111);
        s[2] = 0;
    } else if (c < 0x10000 && len >= 4) {
        s[0] = 0xE0 | ((c >> 12) & 0b1111);
        s[1] = 0x80 | ((c >> 6) & 0b111111);
        s[2] = 0x80 | (c & 0b111111);
        s[3] = 0;
    } else if (c < 0x110000 && len >= 5) {
        s[0] = 0xF0 | ((c >> 18) & 0b111);
        s[1] = 0x80 | ((c >> 12) & 0b111111);
        s[2] = 0x80 | ((c >> 6) & 0b111111);
        s[3] = 0x80 | (c & 0b111111);
        s[4] = 0;
    } else {
        return -1;
    }
    return 0;
}