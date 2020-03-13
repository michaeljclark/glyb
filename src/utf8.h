// See LICENSE for license details.

#pragma once

#include <cstdint>
#include <cstdlib>
#include <climits>

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

size_t utf8_codelen(const char* s);
size_t utf8_strlen(const char *s);
uint32_t utf8_to_utf32(const char *s);
int utf32_to_utf8(char *s, size_t len, uint32_t c);

struct utf32_code { intptr_t code; intptr_t len; };
utf32_code utf8_to_utf32_code(const char *s);

#ifdef __cplusplus
}
#endif

enum {
	emoji_block = 0x1F000,
	emoji_mask = ~0x00fff,
	emoji_flag = 0x1,
};

#ifdef __cplusplus

struct utf8_range { uint64_t off; uint32_t len; uint32_t flags; };

std::vector<utf8_range> utf8_ranges_from_text(const char* text, size_t length,
    uint32_t code, uint32_t mask, uint32_t flag);

#endif