#pragma once

#include "str.h"

namespace fs {
    constexpr usize max_path_len = 256;
#if PK_UNICODE
    using Path = StaticWStr<max_path_len>;
#else
    using Path = StaticStr<max_path_len>;
#endif

    void setBaseFolder(StrView folder);
    Path getPath(StrView relative);
} // namespace fs