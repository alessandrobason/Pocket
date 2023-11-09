#include "stream.h"

#include <ctype.h>
#include <math.h> // HUGE_VALF
#include <stdio.h>

#include "logging.h"

/* == INPUT STREAM ============================================ */

InStream::InStream(const char *cstr) {
    if (cstr) {
        init(cstr, strlen(cstr));
    }
}

InStream::InStream(StrView view) {
    init(view.buf, view.len);
}

InStream::InStream(const char *str, usize len) {
    init(str, len);
}

void InStream::init(const char *str, usize len) {
    start = cur = str;
    size = len;
}

char InStream::get() {
    if (isFinished()) return '\0';
    return *cur++;
}

char InStream::peek() const {
    if (isFinished()) return '\0';
    return *cur;
}

void InStream::ignore(char delim) {
    if (isFinished()) return;
    for(usize i = cur - start; 
        i < size && *cur != delim; 
        ++i, ++cur);
}

void InStream::ignore(StrView view) {
    const char *end = start + size;
    while (
        ((cur + view.len) < end) && 
        view != StrView(cur, view.len)
    ) {
        ++cur;
        // search the first character
        ignore(view.buf[0]);
    }
}

void InStream::ignoreAndSkip(char delim) {
    ignore(delim);
    skip(1);
}

void InStream::ignoreAndSkip(StrView view) {
    ignore(view);
    skip(view.len);
}

void InStream::skip(usize n) {
    if (isFinished() || n > remaining()) return;
    cur += n;
}

void InStream::skipWhitespace() {
    for (const char *end = start + size; 
         cur < end && isspace(*cur); 
         ++cur); 
}

bool InStream::expect(char c) {
    if (isFinished()) return false;
    if (*cur == c) {
        cur++;
        return true;
    }
    return false;
}

bool InStream::expect(StrView v) {
    const char *end = start + size;
    if ((cur + v.len) >= end) {
        return false;
    }
    if (v != StrView(cur, v.len)) {
        return false;
    }
    cur += v.len;
    return true;
}

usize InStream::read(char *buf, usize len) {
    usize rem = remaining();
    if (len > rem) len = rem;
    memcpy(buf, cur, len);
    cur += len;
    return len;
}

void InStream::rewind() {
    cur = start;
}

void InStream::rewind(usize amount) {
    usize len = cur - start;
    if (amount > len) amount = len;
    cur -= len;
}

usize InStream::tell() const {
    return cur - start;
}

usize InStream::remaining() const {
    return (start + size) - cur;
}

bool InStream::isFinished() const {
    return tell() >= size;
}

bool InStream::get(bool &val) {
    usize rem = remaining();
    StrView v = { cur, remaining() };
    if(v == "true") {
        val = true;
        return true;
    }
    if(v == "false") {
        val = false;
        return true;
    }
    return false;
}

template<typename T>
constexpr bool in__uget(InStream &s, T &v) {
    constexpr u64 max_v = (~0ull) >> (64 - sizeof(T) * 8);

    if (s.isFinished()) return false;

    char *end = nullptr;
    // TODO unix doesn't have this i think
    u64 res = strtoull(s.cur, &end, 0);

    if (s.cur == end) {
        warn("no valid conversion could be performed");
        return false;
    }
    if (res == ULLONG_MAX || res >= max_v) {
        warn("value read is out of range of representable values");
        return false;
    }

    s.cur = end;
    v = (T)res;
    return true;
}

template<typename T>
constexpr bool in__iget(InStream &s, T &v) {
    constexpr i64 max_v = (1ull << (sizeof(T) * 8 - 1)) - 1;
    constexpr i64 min_v = ~max_v;

    char *end = nullptr;
    i64 res = strtoll(s.cur, &end, 0);

    if (s.cur == end) {
        warn("no valid conversion could be performed");
        return false;
    }
    if (res <= min_v || res >= max_v) {
        warn("value read is out of range of representable values");
        return false;
    }

    s.cur = end;
    v = (T)res;
    return true;
}

bool InStream::get(u8 &val) {
    return in__uget(*this, val);
}

bool InStream::get(u16 &val) {
    return in__uget(*this, val);
}

bool InStream::get(u32 &val) {
    return in__uget(*this, val);
}

bool InStream::get(u64 &val) {
    return in__uget(*this, val);
}

bool InStream::get(i8 &val) {
    return in__iget(*this, val);
}

bool InStream::get(i16 &val) {
    return in__iget(*this, val);
}

bool InStream::get(i32 &val) {
    return in__iget(*this, val);
}

bool InStream::get(i64 &val) {
    return in__iget(*this, val);
}

bool InStream::get(float &val) {
    char *end = nullptr;
    val = strtof(cur, &end);
    
    if(cur == end) {
        warn("no valid conversion could be performed");
        return false;
    }
    else if(val == HUGE_VALF || val == -HUGE_VALF) {
        warn("value read is out of the range of representable values");
        return false;
    }

    cur = end;
    return true;
}

bool InStream::get(double &val) {
    char *end = nullptr;
    val = strtod(cur, &end);
    
    if(cur == end) {
        warn("no valid conversion could be performed");
        return false;
    }
    else if(val == HUGE_VAL || val == -HUGE_VAL) {
        warn("value read is out of the range of representable values");
        return false;
    }

    cur = end;
    return true;
}

Str InStream::getStr(char delim) {
    const char *from = cur;
    ignore(delim);
    usize len = cur - from;
    return { from, len };
}

Str InStream::getStr(Arena &arena, char delim) {
    const char *from = cur;
    ignore(delim);
    usize len = cur - from;
    return { arena, from, len };
}

StrView InStream::getView(char delim) {
    const char *from = cur;
    ignore(delim);
    usize len = cur - from;
    return { from, len };
}

StrView InStream::getViewEither(StrView view) {
    const char *from = cur;
    const char *end = start + size;
    for(; cur < end; ++cur) {
        if (view.contains(*cur)) {
            break;
        }
    }
    usize len = cur - from;
    return { from, len };
}

/* == OUTPUT STREAM =========================================== */

constexpr usize ostr_append_buf_len = 20;

template<typename T>
static void ostr__push(OutStream &o, T v, const char *fmt) {
    char buf[ostr_append_buf_len];
    int len = snprintf(buf, sizeof(buf), fmt, v);
    if (len <= 0) {
        err("couldn't write");
        return;
    }
    o.push(StrView(buf, len));
}

bool OutStream::isNullTerminated() const {
    return data.empty() ? false : data.back() == '\0';
}

void OutStream::finish() {
    if (!isNullTerminated()) {
        data.push('\0');
    }
}

Str OutStream::asStr() {
    finish();
    Str out;
    out.moveFrom(data.buf, data.len - 1);
    data.release();
    return out;
}

StrView OutStream::asView() const {
    return { data.buf, data.len - isNullTerminated() };
}

void OutStream::replace(char from, char to) {
    for (char &c : data) {
        if (c == from) {
            c = to;
        }
    }
}

void OutStream::print(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printv(fmt, args);
    va_end(args);
}

void OutStream::printv(const char *fmt, va_list args) {
    va_list vtemp;
    int len;

    va_copy(vtemp, args);
    len = vsnprintf(nullptr, 0, fmt, vtemp);
    va_end(vtemp);

    if (len < 0) {
        err("could not format string \"%s\"", fmt);
        return;
    }

    data.reserve(data.cap + len + 1);
    len = vsnprintf(data.end(), len + 1, fmt, args);
    data.len += len + 1;
}

void OutStream::putc(char c) {
    data.push(c);
}

void OutStream::puts(const char *cstr) {
    push(StrView(cstr));
}

void OutStream::push(bool val) {
    push(StrView(val ? "true" : "false"));
}

void OutStream::push(u8 val) {
    ostr__push(*this , val, "%hhu");
}

void OutStream::push(u16 val) {
    ostr__push(*this , val, "%hu");
}

void OutStream::push(u32 val) {
    ostr__push(*this , val, "%u");
}

void OutStream::push(u64 val) {
    ostr__push(*this , val, "%zu");
}

void OutStream::push(i8 val) {
    ostr__push(*this , val, "%hhi");
}

void OutStream::push(i16 val) {
    ostr__push(*this , val, "%hi");
}

void OutStream::push(i32 val) {
    ostr__push(*this , val, "%i");
}

void OutStream::push(i64 val) {
    ostr__push(*this , val, "%zi");
}

void OutStream::push(float val) {
    ostr__push(*this , val, "%g");
}

void OutStream::push(double val) {
    ostr__push(*this , val, "%g");
}

void OutStream::push(StrView view) {
    data.reserve(data.cap + view.len);
    memcpy(data.end(), view.buf, view.len);
    data.len += view.len;
}

const char &OutStream::back() const {
    return data.back();
}

/* == ARENA OUTPUT STREAM ===================================== */

static char *ostra__push(OutStreamArena &o, usize len) {
    if (len == 0) return nullptr;
    bool nil_term = o.isNullTerminated();

    char *newbuf = o.arena.alloc<char>(len);
    o.len += len;

    if (!o.buf) o.buf = newbuf;

    return newbuf - nil_term;
}

template<typename T>
static void ostra__pushval(OutStreamArena &o, T v, const char *fmt) {
    char buf[ostr_append_buf_len];
    int len = snprintf(buf, sizeof(buf), fmt, v);
    if (len <= 0) {
        err("couldn't write");
        return;
    }
    o.push(StrView(buf, len));
}

OutStreamArena::OutStreamArena(Arena &arena) 
    : arena(arena) 
{
}

bool OutStreamArena::isNullTerminated() const {
    return len ? buf[len - 1] == '\0' : false;
}

void OutStreamArena::finish() {
    if (!isNullTerminated()) {
        putc('\0');
    }
}

Str OutStreamArena::asStr() {
    finish();
    Str out;
    out.moveFrom(buf, len);
    buf = nullptr;
    len = 0;
    return out;
}

StrView OutStreamArena::asView() const {
    return { buf, len };
}

void OutStreamArena::replace(char from, char to) {
    for (usize i = 0; i < len; ++i) {
        if (buf[i] == from) {
            buf[i] = to;
        }
    }
}

void OutStreamArena::print(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printv(fmt, args);
    va_end(args);
}

void OutStreamArena::printv(const char *fmt, va_list args) {
    va_list vtemp;
    int len;
    
    va_copy(vtemp, args);
    len = vsnprintf(nullptr, 0, fmt, vtemp);
    va_end(vtemp);
    if(len < 0) {
        err("couldn't format string \"%s\"", fmt);
        return;
    }

    // because we're the only ones using this arena,
    // creating a new buffer appends to the previous one
    
    char *buf = ostra__push(*this, len + 1);
    len = vsnprintf(buf, len + 1, fmt, args);
}

void OutStreamArena::putc(char c) {
    *ostra__push(*this, 1) = c;
}

void OutStreamArena::puts(const char *cstr) {
    push(StrView(cstr));
}

void OutStreamArena::push(bool val) {
    push(StrView(val ? "true" : "false"));
}

void OutStreamArena::push(u8 val) {
    ostra__pushval(*this, val, "%hhu");
}

void OutStreamArena::push(u16 val) {
    ostra__pushval(*this, val, "%hu");
}

void OutStreamArena::push(u32 val) {
    ostra__pushval(*this, val, "%u");
}

void OutStreamArena::push(u64 val) {
    ostra__pushval(*this, val, "%zu");
}

void OutStreamArena::push(i8 val) {
    ostra__pushval(*this, val, "%hhi");
}

void OutStreamArena::push(i16 val) {
    ostra__pushval(*this, val, "%hi");
}

void OutStreamArena::push(i32 val) {
    ostra__pushval(*this, val, "%i");
}

void OutStreamArena::push(i64 val) {
    ostra__pushval(*this, val, "%zi");
}

void OutStreamArena::push(float val) {
    ostra__pushval(*this, val, "%g");
}

void OutStreamArena::push(double val) {
    ostra__pushval(*this, val, "%g");
}

void OutStreamArena::push(StrView view) {
    char *buf = ostra__push(*this, view.len);
    memcpy(buf, view.buf, view.len);
}

const char &OutStreamArena::back() const {
    pk_assert(buf && len);
    return buf[len - 1];
}
