// See LICENSE for license details.

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstddef>

#include <string>

#include "logger.h"

#ifdef _WIN32
#include <Windows.h>
#endif

bool logger::debug = true;

void logger::log(const char* fmt, va_list ap)
{
    char msgbuf[2048];
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
#if defined (_WIN32) && !defined (_CONSOLE)
    OutputDebugStringA(msgbuf);
#else
    printf("%s", msgbuf);
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
