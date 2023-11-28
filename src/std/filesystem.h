#pragma once

#include "str.h"

namespace fs {
    constexpr usize max_path_len = 256;
    using Path = StaticStr<max_path_len>;

    void setBaseFolder(StrView folder);
    Path getPath(StrView relative);
} // namespace fs