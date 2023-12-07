#include "threads.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "mem.h"
#include "logging.h"

// THREADS ////////////////////////////////////////////////////////////////////////////////////////////////////////

Thread::Thread(Func fn, void *userdata) {
    init(fn, userdata);
}

Thread::~Thread() {
    close();
}

Thread::Thread(Thread &&t) {
    *this = mem::move(t);
}

Thread Thread::create(Func fn, void *userdata) {
    return Thread(fn, userdata);
}

Thread Thread::current() {
    Thread out;
    out.handle = (uptr)GetCurrentThread();
    return out;
}

uptr Thread::currentId() {
    return (uptr)GetCurrentThreadId();
}

void Thread::exit(int code) {
    ExitThread((DWORD)code);
}

bool Thread::joinAll(Slice<Thread> threads) {
#if PK_DEBUG
    // check that all threads are valid
    for (const Thread &t : threads) {
        if (!t.isValid()) {
            err("not all threads are valid");
            return false;
        }
    }
#endif

    DWORD result = WaitForMultipleObjects((DWORD)threads.len, (HANDLE *)threads.buf, TRUE, INFINITE);
    return result < (DWORD)threads.len;
}

bool Thread::areAllFinished(Slice<Thread> threads) {
    DWORD result = WaitForMultipleObjects((DWORD)threads.len, (HANDLE *)threads.buf, TRUE, 0);
    return result < (DWORD)threads.len;
}

void Thread::init(Func fn, void *userdata) {
    handle = (uptr)CreateThread(
        nullptr,
        0,
        (LPTHREAD_START_ROUTINE)fn,
        userdata,
        0,
        nullptr
    );
}

bool Thread::isValid() const {
    return handle != 0 && handle != (uptr)INVALID_HANDLE_VALUE;
}

bool Thread::close() {
    bool closed = CloseHandle((HANDLE)handle);
    handle = 0;
    return closed;
}

bool Thread::detach() {
    if (!isValid()) return false;
    return close();
}

bool Thread::join(int *out_code) {
    if (!isValid()) return false;
    WaitForSingleObject((HANDLE)handle, INFINITE);
    DWORD exit_code = 0;
    if (out_code) {
        GetExitCodeThread((HANDLE)handle, &exit_code);
        *out_code = (int)exit_code;
    }
    return close();
}

uptr Thread::getId() const {
    if (!isValid()) return SIZE_MAX;
    return GetThreadId((HANDLE)handle);
}

Thread &Thread::operator=(Thread &&t) {
    if (this != &t) {
        mem::swap(handle, t.handle);
    }
    return *this;
}

// MUTEXES ////////////////////////////////////////////////////////////////////////////////////////////////////////

Mutex::Mutex() {
    init();
}

Mutex::~Mutex() {
    cleanup();
}

Mutex::Mutex(Mutex &&m) {
    *this = mem::move(m);
}

void Mutex::init() {
    CRITICAL_SECTION *crit_sect = (CRITICAL_SECTION *)pk_malloc(sizeof(CRITICAL_SECTION));
    if (crit_sect) {
        InitializeCriticalSection(crit_sect);
    }
    else {
        fatal("crit_sect is null");
    }
    handle = (uptr)crit_sect;
}

void Mutex::cleanup() {
    CRITICAL_SECTION *crit_sect = (CRITICAL_SECTION *)handle;
    DeleteCriticalSection(crit_sect);
    pk_free(crit_sect);
    handle = 0;
}

bool Mutex::isValid() const {
    return handle;
}

bool Mutex::lock() {
     if (!isValid()) { warn("mutex::lock: mutex is not valid"); return false; }
     EnterCriticalSection((CRITICAL_SECTION *)handle);
    return true;
}

bool Mutex::tryLock() {
    if (!isValid()) { warn("mutex::tryLock: mutex is not valid"); return false; }
    return TryEnterCriticalSection((CRITICAL_SECTION *)handle);
}

bool Mutex::unlock() {
    if (!isValid()) { warn("mutex::unlock: mutex is not valid"); return false; }
    LeaveCriticalSection((CRITICAL_SECTION *)handle);
    return true;
}

Mutex &Mutex::operator=(Mutex &&m) {
    if (this != &m) {
        mem::swap(handle, m.handle);
    }
    return *this;
}

// CONDITIONAL VARIABLES //////////////////////////////////////////////////////////////////////////////////////////

CondVar::CondVar() {
    init();
}

void CondVar::init() {
    static_assert(sizeof(CONDITION_VARIABLE) <= sizeof(handle));
    CONDITION_VARIABLE cond = CONDITION_VARIABLE_INIT;
    memcpy(&handle, &cond, sizeof(cond));
}

void CondVar::wake() {
    WakeConditionVariable((CONDITION_VARIABLE *)&handle);
}

void CondVar::wakeAll() {
    WakeAllConditionVariable((CONDITION_VARIABLE *)&handle);
}

void CondVar::wait(const Mutex &mtx) {
    SleepConditionVariableCS((CONDITION_VARIABLE *)&handle, (CRITICAL_SECTION *)mtx.handle, INFINITE);
}

void CondVar::waitTimed(const Mutex &mtx, uint ms) {
    SleepConditionVariableCS((CONDITION_VARIABLE *)&handle, (CRITICAL_SECTION *)mtx.handle, ms);
}
