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

#ifdef __cplusplus
}
#endif