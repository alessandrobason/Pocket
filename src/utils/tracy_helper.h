#pragma once

#include "std/arr.h"

struct Tracy {
    void init();
    void *getCtx(u32 frame);
    void cleanup();

    arr<void *> ctxs;
};