#include "callstack.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>

#include "std/logging.h"

#pragma comment(lib, "DbgHelp")

void csInit() {
    HANDLE process = GetCurrentProcess();
    if (!SymInitialize(process, NULL, TRUE)) {
        err("call to SymInitialize failed: %u", GetLastError());
    }
}

void csCleanup() {
    HANDLE process = GetCurrentProcess();
    if (!SymCleanup(process)) {
        err("call to SymCleanup failed: %u", GetLastError());
    }
}

void printCallStack() {
    HANDLE process = GetCurrentProcess();
    void *stack[1024] = {};
    WORD frame_count = RtlCaptureStackBackTrace(0, 1024, stack, nullptr);

    char buf[sizeof(SYMBOL_INFO) + 1024 * sizeof(TCHAR)];
    SYMBOL_INFO *symbol = (SYMBOL_INFO *)buf;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 1024;

    DWORD displacement;
    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    for (WORD i = 0; i < frame_count; ++i) {
        DWORD64 address = (DWORD64)stack[i];
        if (!SymFromAddr(process, address, nullptr, symbol)) {
            break;
        }

        if (!SymGetLineFromAddr64(process, address, &displacement, &line)) {
            break;
        }

        info("at %s in %s:%lu", symbol->Name, line.FileName, line.LineNumber);
    }
}
