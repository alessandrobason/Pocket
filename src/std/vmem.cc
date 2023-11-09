#include "vmem.h"

#include "logging.h"

static usize page_size = 0;

static void vmem__update_page_size();

// platform generic functions

namespace vmem {
    usize getPageSize(void) {
        if (page_size == 0) {
            vmem__update_page_size();
        }
        return page_size;
    }

    usize padToPage(usize byte_count) {
        if (page_size == 0) {
            vmem__update_page_size();
        }

        if (byte_count == 0) {
            return page_size;
        }
        
        usize padding = page_size - (byte_count % page_size);
        
        if (padding == page_size) {
            padding = 0;
        }
        
        return byte_count + padding;
    }
} // namespace vmem

// platform specific functions

#if PK_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace vmem {
    void *init(usize size, usize *out_padded_size) {
        usize alloc_size = padToPage(size);

        void *ptr = VirtualAlloc(nullptr, alloc_size, MEM_RESERVE, PAGE_NOACCESS);
        
        if (!ptr) {
            err("couldn't virtual alloc: %u", GetLastError());
            return NULL;
        }

        if (out_padded_size) {
            *out_padded_size = alloc_size;
        }

        return ptr;
    }

    bool release(void *base_ptr) {
        return VirtualFree(base_ptr, 0, MEM_RELEASE);
    }

    bool commit(void *ptr, usize num_of_pages) {
        if (page_size == 0) {
            warn("commiting memory but page size wasn't initialised");
            return false;
        }

        void *new_ptr = VirtualAlloc(ptr, num_of_pages * page_size, MEM_COMMIT, PAGE_READWRITE);

        if (!new_ptr) {
            err("failed to commit memory: %u", GetLastError());
            return false;
        }

        return true;
    }
} // namespace vmem

static void vmem__update_page_size(void) {
    SYSTEM_INFO info = {0};
    GetSystemInfo(&info);
    page_size = info.dwPageSize;
}

#endif

#if PK_LINUX

#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

struct vmem__header {
    usize len;
};

namespace vmem {
    void *init(usize size, usize *padded_size) {
        size += sizeof(vmem__header);
        usize alloc_size = padToPage(size);

        vmem__header *header = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (header == MAP_FAILED) {
            fatal("could not map %zu memory: %s", size, strerror(errno));
        }

        if (padded_size) {
            *padded_size = alloc_size;
        }

        header->len = alloc_size;

        return header + 1;
    }

    bool release(void *base_ptr) {
        if (!base_ptr) return false;
        vmem__header *header = (vmem__header *)base_ptr - 1;

        int res = munmap(header, header->len);
        if (res == -1) {
            err("munmap failed: %s", strerror(errno));
        }
        return res != -1;
    }

    bool commit(void *ptr, usize num_of_pages) {
        // mmap doesn't need a commit step
        pk_unused(ptr);
        pk_unused(num_of_pages);
        return true;
    }
} // namespace vmem

static void vmem__update_page_size(void) {
    long lin_page_size = sysconf(_SC_PAGE_SIZE);

    if (lin_page_size < 0) {
        fatal("could not get page size: %s", strerror(errno));
    }

    page_size = (usize)lin_page_size;
}
#endif