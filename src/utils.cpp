#include "utils.h"

#include <stdio.h>
#include <stdlib.h>

static const char *level_str[(int)LogLevel::Count] = {
    "NONE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
};

#define WIN_LEAN_AND_MEAN
#include <Windows.h>

static void setLevelColour(LogLevel level) {
    WORD attribute = 15;
    switch (level) {
        case LogLevel::Debug: attribute = 1; break;
        case LogLevel::Info:  attribute = 2; break;
        case LogLevel::Warn:  attribute = 6; break;
        case LogLevel::Error: attribute = 4; break;
        case LogLevel::Fatal: attribute = 12; break;
        default:              attribute = 15; break;
    }

    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), attribute);
}

void logMessage(LogLevel level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logMessageV(level, fmt, args);
    va_end(args);
}

void logMessageV(LogLevel level, const char *fmt, va_list args) {
    if (level > LogLevel::Fatal) level = LogLevel::Fatal;

    va_list va_tmp;
    va_copy(va_tmp, args);
    int len = vsnprintf(NULL, 0, fmt, va_tmp);
    va_end(va_tmp);

    if (len < 0) {
        printf("[FATAL]: vsnprintf return less than 0!: %d\n", len);
        abort();
    }

    static char buf[1024];

    len = vsnprintf(buf, sizeof(buf), fmt, args);

    setLevelColour(level);
    printf("[%s]: ", level_str[(int)level]);
    // reset level colour
    if (level < LogLevel::Fatal) setLevelColour(LogLevel::None);
    printf("%s\n", buf);

    if (level == LogLevel::Fatal) {
        setLevelColour(LogLevel::None);
        abort();
    }
}