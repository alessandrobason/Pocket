#pragma once

#include "common.h"
#include "slice.h"

struct Thread {
    using Func = int(void*);

    Thread() = default;
    Thread(Func fn, void *userdata);
    ~Thread();
    Thread(Thread &&t);

    static Thread create(Func fn, void *userdata = nullptr);
    static Thread current();
    static uptr currentId();
    static void exit(int code = 0);
    static bool joinAll(Slice<Thread> threads);
    static bool areAllFinished(Slice<Thread> threads);

    void init(Func fn, void *userdata = nullptr);

    bool isValid() const;
    bool close();
    bool detach();
    bool join(int *out_code = nullptr);
    uptr getId() const;

    Thread &operator=(Thread &&t);

    uptr handle = 0;
};

struct Mutex {
    Mutex();
    ~Mutex();
    Mutex(Mutex &&m);

    void init();
    void cleanup();

    bool isValid() const;

    bool lock();
    bool tryLock();
    bool unlock();

    Mutex &operator=(Mutex &&m);

    uptr handle = 0;
};

struct CondVar {
    CondVar();

    void init();

    void wake();
    void wakeAll();

    void wait(const Mutex &mtx);
    void waitTimed(const Mutex &mtx, uint ms);

    uptr handle = 0;
};

struct ReadWriteLock {

    
    Mutex write_mtx;
};