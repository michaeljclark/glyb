// See LICENSE for license details.

#pragma once

#include <cstdint>
#include <cstdlib>

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