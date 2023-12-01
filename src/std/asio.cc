#include "asio.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "filesystem.h"

namespace asio {
    File::File(StrView filename) {
        init(filename);
    }

    bool File::init(StrView filename) {
        fs::Path path = fs::getPath(filename);

        // ulong file_size = GetFileSize()
    }

    bool File::poll() {

    }

    arr<byte> &&File::getData() {

    }
} // namespace asio