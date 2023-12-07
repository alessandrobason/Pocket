#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "common.h"
#include "arena.h"
#include "callstack.h"
#include "str.h"
#include "threads.h"

static Arena *log_arena = nullptr;
static Mutex log_mtx;
static uptr log_thr_id = 0;
static const char *level_str[] = { "INFO", "WARN", "ERROR", "FATAL" };

static void trace__set_level_colour(trace::Level level);
static void trace__msg_box(const char *msg);

static void trace__init_small_buf(void) {
    static byte small_buf[512];
    static Arena buf_arena;
    buf_arena = Arena::makeStatic(small_buf);
    trace::init(buf_arena);
}

namespace trace {
    void init(Arena &arena) {
        log_arena = &arena;
        log_thr_id = Thread::currentId();
    }

    void print(Level level, const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        printv(level, fmt, args);
        va_end(args);
    }

    void printv(Level level, const char *fmt, va_list args) {
        log_mtx.lock();
        
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

        char *buf = scratch.alloc<char>(len + 1, Arena::SoftFail);
        if (!buf) {
            printf("[ERR]: trying to print string of length %d, which is more than what the arena can handle", len + 1);
            log_mtx.unlock();
            return;
        }
        len = vsnprintf(buf, len + 1, fmt, args);

        trace__set_level_colour(level);
        printf("[%s]: ", level_str[(int)level]);
        if (Thread::currentId() != log_thr_id) {
            trace__set_level_colour(Level::Warn);
            printf("(0x%llx) ", Thread::currentId());
        }
        // reset level colour
        trace__set_level_colour((Level)-1);
        printf("%s\n", buf);

        if (level == Level::Fatal) {
            Str message = Str::fmt("Fatal Error: %s", buf);
            CallStack::print();
            trace__msg_box(message.cstr());
            raise(SIGABRT);
        }

        log_mtx.unlock();
    }
} // namespace trace

#if PK_WINDOWS

// forward declare so we don't include windows.h
#define win32(T) extern "C" T
win32(i32) SetConsoleTextAttribute(void *console_handle, ushort attributes);
win32(void*) GetStdHandle(uint std_handle);
win32(int) MessageBoxA(void *hwnd, const char *text, const char *caption, uint type);
win32(void) DebugBreak();

constexpr uint w32_std_output_handle  = -11;
constexpr uint w32_abort_retry_ignore = 0x00000002L;
constexpr uint w32_icon_error         = 0x00000010L;
constexpr uint w32_retry_button       = 4;

static void trace__set_level_colour(trace::Level level) {
    ushort attribute = 15;
    switch (level) {
        case trace::Level::Info:  attribute = 2; break;
        case trace::Level::Warn:  attribute = 6; break;
        case trace::Level::Error: attribute = 4; break;
        case trace::Level::Fatal: attribute = 12; break;
        default:                  attribute = 15; break;
    }

    SetConsoleTextAttribute(GetStdHandle(w32_std_output_handle), attribute);
}

static void trace__msg_box(const char *msg) {
    int result = MessageBoxA(
        nullptr,
        msg,
        "Pocket: FATAL ERROR",
        w32_abort_retry_ignore | w32_icon_error
    );

    if (result == w32_retry_button) {
        DebugBreak();
    }
}

#endif

#if PK_POSIX

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