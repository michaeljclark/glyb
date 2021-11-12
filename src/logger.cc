// See LICENSE for license details.

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstddef>
#include <cassert>

#include <string>
#include <vector>

#include "logger.h"

#ifdef _WIN32
#include <Windows.h>
#endif

logger::L logger::level = logger::L::Linfo;

const char* logger::level_names[6] = { "trace", "debug", "info", "warn", "error", "panic" };

void logger::output(const char *prefix, const char* fmt, va_list args1)
{
    std::vector<char> buf;
    va_list args2;
    size_t plen, pout, len, ret;

    va_copy(args2, args1);

    plen = strlen(prefix);
    len = (size_t)vsnprintf(NULL, 0, fmt, args1);
    assert(len >= 0);
    buf.resize(plen + len + 4); // prefix + ": " + message + CR + zero-terminator
    pout = (size_t)snprintf(buf.data(), buf.capacity(), "%s: ", prefix);
    ret = (size_t)vsnprintf(buf.data() + pout, buf.capacity(), fmt, args2);
    assert(len == ret);
    if (buf[buf.size()-3] != '\n') buf[buf.size()-2] = '\n';

#if defined (_WIN32) && !defined (_CONSOLE)
    OutputDebugStringA(buf.data());
#else
    printf("%s", buf.data());
#endif
}

void logger::log(L level, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (level >= logger::level) {
        output(logger::level_names[level], fmt, ap);
    }
    va_end(ap);
}

void logger::set_level(L level)
{
    logger::level = level;
}