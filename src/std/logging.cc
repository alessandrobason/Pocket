#include "logging.h"

#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "arena.h"

static Arena *log_arena = nullptr;
static const char *level_str[] = { "INFO", "WARN", "ERROR", "FATAL" };

static void trace__set_level_colour(trace::Level level);

static void trace__init_small_buf(void) {
    static byte small_buf[512];
    static Arena buf_arena;
    buf_arena = Arena::makeStatic(small_buf);
    trace::init(buf_arena);
}

namespace trace {
    void init(Arena &arena) {
        log_arena = &arena;
    }

    void print(Level level, const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        printv(level, fmt, args);
        va_end(args);
    }

    void printv(Level level, const char *fmt, va_list args) {
        if (!log_arena) {
            puts("No arena provided to the logger, using instead small buffer");
            trace__init_small_buf();
        }

        Arena scratch = *log_arena;

        va_list va_tmp;
        va_copy(va_tmp, args);
        int len = vsnprintf(NULL, 0, fmt, va_tmp);
        va_end(va_tmp);

        if (len < 0) {
            printf("[FATAL]: vsnprintf return less than 0!: %d\n", len);
            abort();
        }

        char *buf = scratch.alloc<char>(len + 1);
        len = vsnprintf(buf, len + 1, fmt, args);

        trace__set_level_colour(level);
        printf("[%s]: ", level_str[(int)level]);
        // reset level colour
        trace__set_level_colour((Level)-1);
        printf("%s\n", buf);

        if (level == Level::Fatal) {
            abort();
        }
    }
} // namespace trace

#if PK_WINDOWS
// forward declare so we don't include windows.h
#define win32(T) extern "C" T
constexpr uint STD_OUTPUT_HANDLE = -11;
win32(i32) SetConsoleTextAttribute(void *console_handle, ushort attributes);
win32(void*) GetStdHandle(uint std_handle);

static void trace__set_level_colour(trace::Level level) {
    ushort attribute = 15;
    switch (level) {
        case trace::Level::Info:  attribute = 2; break;
        case trace::Level::Warn:  attribute = 6; break;
        case trace::Level::Error: attribute = 4; break;
        case trace::Level::Fatal: attribute = 12; break;
        default:                  attribute = 15; break;
    }

    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), attribute);
}

#endif

#if PK_LINUX

static void trace__set_level_colour(trace::Level level) {
    static const char *level_colours[(int)trace::Level::Fatal + 1] = {
        "\033[32m", // LOG_INFO
        "\033[33m", // LOG_WARN
        "\033[31m", // LOG_ERROR
        "\033[31m", // LOG_PANIC
    };
    // reset
    if (level < 0 || level > trace::Level::Fatal) {
        printf("\033[0m");
        return;
    }
    printf("%s", level_colours[(int)level]);
}

#endif