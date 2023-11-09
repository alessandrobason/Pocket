#pragma once

#include <stdarg.h>

#include "common.h"

struct Arena;
struct StrView;

struct Str {
    Str() = default;
    Str(const char *cstr);
    Str(const char *cstr, usize cstr_len);
    Str(StrView view);
    Str(Str &&str);
    Str(const Str &str);
    ~Str();
    
    Str(Arena &arena, const char *cstr);
    Str(Arena &arena, const char *cstr, usize cstr_len);
    Str(Arena &arena, StrView view);

    static Str fmt(const char *fmt, ...);
    static Str fmtv(const char *fmt, va_list args);
    static Str fmt(Arena &arena, const char *fmt, ...);
    static Str fmtv(Arena &arena, const char *fmt, va_list args);

    void init(const char *cstr, usize cstr_len);
    void init(Arena &arena, const char *cstr, usize cstr_len);
    void moveFrom(char *str, usize str_len);
    Str dup(Arena &arena) const;
    void destroy();
    void resize(usize new_len);
    void resize(Arena &arena, usize new_len);

    void replace(char from, char to);
    bool empty() const;
    StrView sub(usize from = 0, usize to = -1);
    void lower();
    void upper();
    Str toLower() const;
    Str toUpper() const;
    Str toLower(Arena &arena) const;
    Str toUpper(Arena &arena) const;

    char *data();
    const char *data() const;
    const char *c_str() const;
    usize size() const;
    bool isOwned() const;

    char *begin();
    char *end();
    char &back();
    char &front();
    char &operator[](usize index);

    const char *begin() const;
    const char *end() const;
    const char &back() const;
    const char &front() const;
    const char &operator[](usize index) const;

    Str &operator=(Str &&str);
    Str &operator=(const Str &str);

    bool operator==(StrView v) const;
    bool operator!=(StrView v) const;

private:
    // we're doing some funky stuff to support both owned and
    // not owned strings, so don't access these directly

    char *buf = nullptr;
    usize len = 0;
};

struct StrView {
    StrView() = default;
    StrView(const char *cstr);
    StrView(const char *cstr, usize cstr_len);
    StrView(const Str &str);

    void init(const char *cstr, usize cstr_len);

    StrView removePrefix(usize amount) const;
    StrView removeSuffix(usize amount) const;
    StrView trim() const;
    StrView trimLeft() const;
    StrView trimRight() const;
    StrView sub(usize from = 0, usize to = -1);

    Str dup(Arena &arena) const;

    bool empty() const;
    bool startsWith(char c);
    bool startsWith(StrView view);
    bool endsWith(char c);
    bool endsWith(StrView view);
    bool contains(char c);
    bool contains(StrView view);
    usize find(char c, usize from = 0);
    usize find(StrView view, usize from = 0);
    usize rfind(char c, isize from_end = -1);
    usize rfind(StrView view, isize from_end = -1);
    usize findFirstOf(StrView view, usize from = 0);
    usize findLastOf(StrView view, isize from_end = 0);
    usize findFirstNot(char c, usize from = 0);
    usize findFirstNot(StrView view, usize from = 0);
    usize findLastNot(char c, isize from_end = -1);
    usize findLastNotOf(StrView view, isize from_end = -1);

    const char *data();
    const char *c_str() const;
    usize size() const;

    const char *begin() const;
    const char *end() const;
    const char &back() const;
    const char &front() const;
    const char &operator[](usize index) const;

    bool operator==(StrView v) const;
    bool operator!=(StrView v) const;

    const char *buf = nullptr;
    usize len = 0;
};
