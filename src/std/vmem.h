#pragma once

#include "common.h"

namespace vmem {
    // reserve virtual memory, this doesn't actually commit the memory in win32
    void *init(usize size, usize *out_padded_size = nullptr);
    // free the virtual memory
    bool release(void *base_ptr);
    // commit memory, ptr needs to be aligned to page size
    bool commit(void *ptr, usize num_of_pages);
    // get the size of a page
    usize getPageSize(void);
    // pads the byte count to a page size
    usize padToPage(usize byte_count);
} // namespace vmem
