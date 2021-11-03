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

bool logger::debug = false;

void logger::log(const char* fmt, va_list args1)
{
    std::vector<char> buf;
    va_list args2;
    int len, ret;

    va_copy(args2, args1);

    len = vsnprintf(NULL, 0, fmt, args1);
    assert(len >= 0);
    buf.resize(len + 1); // space for zero
    ret = vsnprintf(buf.data(), buf.capacity(), fmt, args2);
    assert(len == ret);

#if defined (_WIN32) && !defined (_CONSOLE)
    OutputDebugStringA(buf.data());
#else
    printf("%s", buf.data());
#endif
}

void logger::logDebug(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log(fmt, ap);
    va_end(ap);
}

void logger::logError(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log(fmt, ap);
    va_end(ap);
}

void logger::logPanic(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log(fmt, ap);
    va_end(ap);
    exit(9);
}
