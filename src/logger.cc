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

void logger::log(const char* msgbuf)
{
#if defined (_WIN32) && !defined (_CONSOLE)
    OutputDebugStringA(msgbuf);
#else
    printf("%s", msgbuf);
#endif
}

void logger::logDebug(const char* fmt, ...)
{
    char msgbuf[2048];
    va_list ap;
    
    va_start(ap, fmt);
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
    va_end(ap);
    
    log(msgbuf);
}

void logger::logError(const char* fmt, ...)
{
    char msgbuf[2048];
    va_list ap;
    
    va_start(ap, fmt);
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
    va_end(ap);
    
    log(msgbuf);
}
