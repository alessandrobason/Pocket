#include "arena.h"

#include <assert.h>
#include <stdlib.h> // malloc etc
#include <string.h> // memset

#include "vmem.h"
#include "mem.h"
#include "logging.h"

static constexpr usize arena__calc_padding(usize amount, usize align) {
#pragma warning(suppress : 4146)
    return -(usize)amount & (align - 1);
}

static Arena arena__make_virtual(usize initial_allocation);
static Arena arena__make_malloc(usize initial_allocation);
static Arena arena__make_static(byte *data, usize len);

static void arena__free_virtual(Arena &self);
static void arena__free_malloc(Arena &self);

static void *arena__alloc_virtual(Arena &self, usize size, usize align, usize count, Arena::Flags flags);
static void *arena__alloc_simple(Arena &self, usize size, usize align, usize count, Arena::Flags flags);

Arena Arena::make(usize initial_allocation, Type type) {
    switch (type & __TypeMask) {
        case Virtual: return arena__make_virtual(initial_allocation);
        case Malloc:  return arena__make_malloc(initial_allocation);
        case Static:  err("Can't initialise static arena using Arena::make, call Arena::makeStatic with your buffer instead"); break;
        default:      err("Invalid arena type provided: %u", type & __TypeMask); break;
    }

    return {};
}

Arena Arena::makeStatic(byte *data, usize len) {
    return arena__make_static(data, len);
}

Arena::Arena(const Arena &arena) {
    *this = arena;
}

Arena::Arena(Arena &&arena) {
    *this = mem::move(arena);
}

Arena::~Arena() {
    cleanup();
}

void Arena::cleanup() {
    if (type & __NotOwned) {
        return;
    }

    switch (type & __TypeMask) {
        case Virtual: return arena__free_virtual(*this);
        case Malloc:  return arena__free_malloc(*this);
        default:      err("Invalid arena type provided: %u", type & __TypeMask); break;
    }
}

void *Arena::alloc(usize size, usize count, usize align, Flags flags) {
    switch (type & __TypeMask) {
        case Virtual: return arena__alloc_virtual(*this, size, align, count, flags);
        case Malloc: // fallthrough
        case Static:  return arena__alloc_simple(*this, size, align, count, flags);
    }

    err("(POSSIBLE CORRUPTION) -> unknown arena type: %u", type & __TypeMask);
    return nullptr;
}

usize Arena::tell() const {
    return current - start;
}

void Arena::rewind(usize from_start) {
    usize position = tell();
    if (position < from_start) {
        warn("trying to rewind arena to %zu, but position is %zu", from_start, position);
        return;
    }
    current = start + from_start;
}

void Arena::pop(usize amount) {
    rewind(tell() - amount);
}

Arena &Arena::operator=(const Arena &other) {
    if (this != &other) {
        memcpy(this, &other, sizeof(other));
        type = (Type)(type | __NotOwned);
    }
    return *this;
}

Arena &Arena::operator=(Arena &&other) {
    if (this != &other) {
        mem::swap(start, other.start);
        mem::swap(current, other.current);
        mem::swap(end, other.end);
        mem::swap(type, other.type);
    }
    return *this;
}

// == VIRTUAL ARENA ====================================================================================================

static Arena arena__make_virtual(usize initial_allocation) {
    usize alloc_size = 0;
    byte *ptr = (byte *)vmem::init(initial_allocation, &alloc_size);
    if (!vmem::commit(ptr, 1)) {
        vmem::release(ptr);
        ptr = nullptr;
    }

    Arena out;
    out.start = ptr;
    out.current = ptr;
    out.end = ptr ? ptr + alloc_size : nullptr;
    out.type = Arena::Virtual;
    return out;
}

static void arena__free_virtual(Arena &self) {
    if (!vmem::release(self.start)) {
        err("failed to free virtual memory");
    }
}

static void *arena__alloc_virtual(Arena &self, usize size, usize align, usize count, Arena::Flags flags) {
    isize total = size * count;
    
    usize allocated = self.tell();
    usize page_end = vmem::padToPage(allocated);

    if (total > (isize)(page_end - allocated)) {
        usize page_size = vmem::getPageSize();
        usize new_memory_end = page_end + vmem::padToPage(total);
        usize num_of_pages = new_memory_end / page_size;

        pk_assert(num_of_pages > 0);

        if (!vmem::commit(self.start, num_of_pages)) {
            err("arena could not commit %zi pages", num_of_pages);
            if (flags & Arena::SoftFail) {
                return nullptr;
            }
            fatal("Virtual arena allocation fail");
        }
    }

    usize padding = arena__calc_padding((usize)self.current, align);
    byte *ptr = self.current + padding;
    self.current += padding + total;
    return flags & Arena::NoZero ? ptr : memset(ptr, 0, total);
}

// == MALLOC ARENA =====================================================================================================

static Arena arena__make_malloc(usize initial_allocation) {
    byte *ptr = (byte *)pk_malloc(initial_allocation);
    if (!ptr) {
        fatal("Could not malloc %zu bytes", initial_allocation);
    }
    Arena out;
    out.start = ptr;
    out.current = ptr;
    out.end = ptr + initial_allocation;
    out.type = Arena::Malloc;
    return out;
}

static void arena__free_malloc(Arena &self) {
    pk_free(self.start);
}

// == STATIC ARENA =====================================================================================================

static Arena arena__make_static(byte *data, usize len) {
    Arena out;
    out.start = data;
    out.current = data;
    out.end = data + len;
    out.type = (Arena::Type)(Arena::Static | Arena::__NotOwned);
    return out;
}

static void *arena__alloc_simple(Arena &self, usize size, usize align, usize count, Arena::Flags flags) {
    isize total = size * count;
    isize remaining = self.end - self.current;

    if (total > remaining) {
        if (flags & Arena::SoftFail) {
            return nullptr;
        }
        fatal("OUT OF MEMORY");
    }
    
    isize padding = arena__calc_padding((usize)self.current, align);
    byte *ptr = self.current + padding;
    self.current = ptr + total;
    return flags & Arena::NoZero ? ptr : memset(ptr, 0, total);
}

