#pragma once

#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>

static std::string format(const char* fmt, ...)
{
    std::vector<char> buf;
    int len;
    va_list ap;

    va_start(ap, fmt);
    len = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);

    buf.resize(len+1);
    va_start(ap, fmt);
    vsnprintf(buf.data(), len+1, fmt, ap);
    va_end(ap);

    return std::string(buf.data(), len);
}
