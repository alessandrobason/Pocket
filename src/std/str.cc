#include "str.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "arena.h"
#include "mem.h"
#include "stream.h"

// use left-most bit as bitflag for owning
static constexpr u64 str_not_owned_flags = 1ull << 63;
static constexpr u64 str_size_mask = ~str_not_owned_flags;
static constexpr bool str__is_owned(usize len) {
    return !(len & str_not_owned_flags);
}

Str::Str(const char *cstr) {
    if (cstr) {
        init(cstr, strlen(cstr));
    }
}

Str::~Str() {
    destroy();
}

Str::Str(const char *cstr, usize cstr_len) {
    if (cstr && cstr_len) {
        init(cstr, cstr_len);
    }
}

Str::Str(StrView view) {
    if (!view.empty()) {
        init(view.buf, view.len);
    }
}

Str::Str(Str &&str) {
    *this = mem::move(str);
}

Str::Str(const Str &str) {
    *this = str;
}

Str::Str(Arena &arena, const char *cstr) {
    if (cstr) {
        init(arena, cstr, strlen(cstr));
    }
}

Str::Str(Arena &arena, const char *cstr, usize cstr_len) {
    if (cstr && cstr_len) {
        init(arena, cstr, cstr_len);
    }
}

Str::Str(Arena &arena, StrView view) {
    if (!view.empty()) {
        init(arena, view.buf, view.len);
    }
}

Str Str::fmt(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Str out = Str::fmtv(fmt, args);
    va_end(args);
    return out;
}

Str Str::fmtv(const char *fmt, va_list args) {
    OutStream ostr;
    ostr.printv(fmt, args);
    return ostr.asStr();
}

Str Str::fmt(Arena &arena, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Str out = Str::fmtv(arena, fmt, args);
    va_end(args);
    return out;
}

Str Str::fmtv(Arena &arena, const char *fmt, va_list args) {
    OutStreamArena ostr = arena;
    ostr.printv(fmt, args);
    return ostr.asStr();
}

void Str::init(const char *cstr, usize cstr_len) {
    buf = (char *)pk_malloc(cstr_len + 1);
    pk_assert(buf);
    memcpy(buf, cstr, cstr_len);
    buf[cstr_len] = '\0';
    len = cstr_len;
}

void Str::init(Arena &arena, const char *cstr, usize cstr_len) {
    buf = arena.alloc<char>(cstr_len);
    memcpy(buf, cstr, cstr_len);
    len = cstr_len | str_not_owned_flags;
}

void Str::moveFrom(char *str, usize str_len) {
    buf = str;
    len = str_len;
}

Str Str::dup(Arena &arena) const {
    Str out;
    out.init(arena, buf, size());
    return out;
}

void Str::destroy() {
    if (str__is_owned(len)) {
        pk_free(buf);
    }
    buf = nullptr;
    len = 0;
}

void Str::resize(usize new_len) {
    usize sz = size();
    if (new_len == sz) return;
    char *newbuf = (char *)pk_realloc(buf, new_len + 1);
    pk_assert(newbuf);
    buf = newbuf;
    bool owned = isOwned();
    // set the rest of the string to zero from the previos null terminator
    memset(buf + len + 1, 0, new_len - len);
    len = new_len;
    if (owned) len |= str_not_owned_flags;
}

void Str::resize(Arena &arena, usize new_len) {
    usize sz = size();
    if (new_len == sz) return;
    char *newbuf = arena.alloc<char>(new_len + 1);
    pk_assert(newbuf);
    memcpy(newbuf, buf, sz);
    bool owned = isOwned();
    len = new_len;
    if (owned) len |= str_not_owned_flags;
}

void Str::replace(char from, char to) {
    for (char &c : *this) {
        if (c == from) {
            c = to;
        }
    }
}

bool Str::empty() const {
    return size() == 0;
}

StrView Str::sub(usize from, usize to) {
    usize max = size();
    if (to > max) to = max;
    if (from > to) from = to;
    return StrView(buf + from, to - from);
}

void Str::lower() {
    for (char &c : *this) {
        c = tolower(c);
    }
}

void Str::upper() {
    for (char &c : *this) {
        c = toupper(c);
    }
}

Str Str::toLower() const {
    Str out = *this;
    out.lower();
    return out;
}

Str Str::toUpper() const {
    Str out = *this;
    out.upper();
    return out;
}

Str Str::toLower(Arena &arena) const {
    Str out = dup(arena);
    out.lower();
    return out;
}

Str Str::toUpper(Arena &arena) const {
    Str out = dup(arena);
    out.upper();
    return out;
}

char *Str::data() {
    return buf;
}

const char *Str::data() const {
    return buf;
}

const char *Str::cstr() const {
    return buf;
}

usize Str::size() const {
    return len & str_size_mask;
}

bool Str::isOwned() const {
    return str__is_owned(len);
}

char *Str::begin() {
    return buf;
}

char *Str::end() {
    return buf + size();
}

char &Str::back() {
    pk_assert(!empty());
    return buf[size() - 1];
}

char &Str::front() {
    pk_assert(!empty());
    return buf[0];
}

char &Str::operator[](usize index) {
    pk_assert(index < size());
    return buf[index];
}

const char *Str::begin() const {
    return buf;
}

const char *Str::end() const {
    return buf + size();
}

const char &Str::back() const {
    pk_assert(!empty());
    return buf[size() - 1];
}

const char &Str::front() const {
    pk_assert(!empty());
    return buf[0];
}

const char &Str::operator[](usize index) const {
    pk_assert(index < size());
    return buf[index];
}

Str &Str::operator=(Str &&str) {
    if (this != &str) {
        mem::swap(buf, str.buf);
        mem::swap(len, str.len);
    }
    return *this;
}

Str &Str::operator=(const Str &str) {
    if (this != &str) {
        init(str.buf, str.len);
    }
    return *this;
}

bool Str::operator==(StrView v) const {
    if (v.size() != size()) return false;
    return memcmp(buf, v.buf, size()) == 0;
}

bool Str::operator!=(StrView v) const {
    return !(*this == v);
}

StrView::StrView(const char *cstr) {
    if (cstr) {
        init(cstr, strlen(cstr));
    }
}

StrView::StrView(const char *cstr, usize cstr_len) {
    init(cstr, cstr_len);
}

StrView::StrView(const Str &str) {
    init(str.data(), str.size());
}

void StrView::init(const char *cstr, usize cstr_len) {
    buf = cstr;
    len = cstr_len;
}

StrView StrView::removePrefix(usize amount) const {
    if (amount > len) amount = len;
    return { buf + amount, len - amount };
}

StrView StrView::removeSuffix(usize amount) const {
    if (amount > len) amount = len;
    return { buf, len - amount };
}

StrView StrView::trim() const {
    return trimLeft().trimRight();
}

StrView StrView::trimLeft() const {
    StrView out = *this;
    for (usize i = 0; i < len && isspace(buf[i]); ++i) {
        ++out.buf;
        --out.len;
    }
    return out;
}

StrView StrView::trimRight() const {
    StrView out = *this;
    for (isize i = len - 1; i >= 0 && isspace(buf[i]); --i) {
        --out.len;
    }
    return out;
}

StrView StrView::sub(usize from, usize to) {
    if (to > len) to = len;
    if (from > to) from = to;
    return { buf + from, to - from };
}

Str StrView::dup(Arena &arena) const {
    Str out;
    out.init(arena, buf, len);
    return out;
}

bool StrView::empty() const {
    return len == 0;
}

bool StrView::startsWith(char c) {
    return len ? buf[0] == c : false;
}

bool StrView::startsWith(StrView view) {
    if (len < view.len) return false;
    return memcmp(buf, view.buf, view.len) == 0;
}

bool StrView::endsWith(char c) {
    return len ? buf[len - 1] == c : false;
}

bool StrView::endsWith(StrView view) {
    if(len < view.len) return false;
    return memcmp(buf + len - view.len, view.buf, view.len) == 0;
}

bool StrView::contains(char c) {
    for(usize i = 0; i < len; ++i) {
        if(buf[i] == c) return true;
    }
    return false;
}

bool StrView::contains(StrView view) {
    if(len < view.len) return false;
    if (len == view.len) return *this == view;
    usize end = len - view.len + 1;
    for(usize i = 0; i < end; ++i) {
        if(memcmp(buf + i, view.buf, view.len) == 0) return true;
    }
    return false;
}

usize StrView::find(char c, usize from) {
    for(usize i = from; i < len; ++i) {
        if(buf[i] == c) return i;
    }
    return SIZE_MAX;
}

usize StrView::find(StrView view, usize from) {
    if(len < view.len) return SIZE_MAX;
    usize end = len - view.len;
    for(usize i = from; i < end; ++i) {
        if(memcmp(buf + i, view.buf, view.len) == 0) return i;
    }
    return SIZE_MAX;
}

usize StrView::rfind(char c, isize from_end) {
    const char *end = buf + len - from_end;
    if (end > (buf + len)) return SIZE_MAX;

    for(; end >= buf; --end) {
        if(*end == c) return (end - buf);
    }

    return SIZE_MAX;
}

usize StrView::rfind(StrView view, isize from_end) {
    if(view.len > len) {
        return SIZE_MAX;
    }

    from_end -= view.len;
    const char *end = buf + len - from_end;
    if (end > (buf + len)) return SIZE_MAX;

    for(; end >= buf; --end) {
        if(memcmp(end, view.buf, view.len) == 0) return (end - buf);
    }
    return SIZE_MAX;
}

usize StrView::findFirstOf(StrView view, usize from) {
    if(len < view.len) return SIZE_MAX;
    for(usize i = from; i < len; ++i) {
        for(usize j = 0; j < view.len; ++j) {
            if(buf[i] == view.buf[j]) return i;
        }
    }
    return SIZE_MAX;
}

usize StrView::findLastOf(StrView view, isize from_end) {
    const char *end = buf + len - from_end;
    if (end > (buf + len)) return SIZE_MAX;

    for(; end >= buf; --end) {
        for(usize j = 0; j < view.len; ++j) {
            if(*end == view.buf[j]) return (end - buf);
        }
    }

    return SIZE_MAX;
}

usize StrView::findFirstNot(char c, usize from) {
    usize end = len - 1;
    for(usize i = from; i < end; ++i) {
        if(buf[i] != c) return i;
    }
    return SIZE_MAX;
}

usize StrView::findFirstNot(StrView view, usize from) {
    for(usize i = from; i < len; ++i) {
        if(!view.contains(buf[i])) {
            return i;
        }
    }
    return SIZE_MAX;
}

usize StrView::findLastNot(char c, isize from_end) {
    const char *end = buf + len - from_end;
    if (end > (buf + len)) return SIZE_MAX;

    for(; end >= buf; --end) {
        if(*end != c) {
            return end - buf;
        }
    }

    return SIZE_MAX;
}

usize StrView::findLastNotOf(StrView view, isize from_end) {
    const char *end = buf + len - from_end;
    if (end > (buf + len)) return SIZE_MAX;

    for(; end >= buf; --end) {
        if(!view.contains(*end)) {
            return end - buf;
        }
    }

    return SIZE_MAX;
}

const char *StrView::data() {
    return buf;
}

const char *StrView::c_str() const {
    return buf;
}

usize StrView::size() const {
    return len;
}

const char *StrView::begin() const {
    return buf;
}

const char *StrView::end() const {
    return buf + len;
}

const char &StrView::back() const {
    pk_assert(!empty());
    return buf[len - 1];
}

const char &StrView::front() const {
    pk_assert(!empty());
    return buf[0];
}

const char &StrView::operator[](usize index) const {
    pk_assert(index < len);
    return buf[index];
}

bool StrView::operator==(StrView v) const {
    if (v.len != len) return false;
    return memcmp(buf, v.buf, len) == 0;
}

bool StrView::operator!=(StrView v) const {
    return !(*this == v);
}