#pragma once

#include "std/common.h"

namespace guid {
    static u32 guid__counter = 0;

    template<typename T>
    u32 type() {
        static u32 id = ++guid__counter;
        return id;
    }     
} // namespace guid
