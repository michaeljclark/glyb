// See LICENSE for license details.

#pragma once

struct logger
{
    static bool debug;
    
    static void log(const char* msg);
    static void logDebug(const char* fmt, ...);
    static void logError(const char* fmt, ...);
};

#define Debug(fmt, ...) \
if (logger::debug) { logger::logDebug(fmt, __VA_ARGS__); }

#define Error(fmt, ...) \
logger::logError(fmt, __VA_ARGS__);

#define Warn(fmt, ...) \
logger::logWarn(fmt, __VA_ARGS__);
