#pragma once

#include <stdarg.h>

struct Arena;

#define info(...)  trace::print(trace::Level::Info,  __VA_ARGS__)
#define warn(...)  trace::print(trace::Level::Warn,  __VA_ARGS__)
#define err(...)   trace::print(trace::Level::Error, __VA_ARGS__)
#define fatal(...) trace::print(trace::Level::Fatal, __VA_ARGS__)

namespace trace {
    enum class Level {
        Info, 
        Warn, 
        Error, 
        Fatal,
    };

    void init(Arena &arena);
    void print(Level level, const char *fmt, ...);
    void printv(Level level, const char *fmt, va_list args);
} // namespace trace
