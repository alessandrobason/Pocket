#pragma once

#include "common.h"
#include "str.h"
#include "arr.h"

namespace asio {
    struct File {
        File() = default;
        File(StrView filename);
        ~File();

        bool init(StrView filename);
        bool isValid() const;
        // returns true when finished
        bool poll();
        arr<byte> &&getData();

    private:
        uptr handle = 0;
        void *internal = nullptr;
        arr<byte> data;
    };
} // namespace asio