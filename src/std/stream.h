#pragma once

#include "common.h"
#include "str.h"
#include "arr.h"
#include "arena.h"

struct InStream {
    InStream() = default;
    // initialise with null terminated string
    InStream(const char *cstr);
    InStream(StrView view);
    // initialise with buffer (does not need to be null terminated)
    InStream(const char *str, usize len);

    void init(const char *str, usize len);

    // get the current character and advance
    char get();
    // get the current character but don't advance
    char peek() const;
    // ignore characters until the delimiter
    void ignore(char delim);
    // ignore characters until the view
    void ignore(StrView view);
    // ignore characters until the delimiter and skip it
    void ignoreAndSkip(char delim);
    // ignore characters until the view and skip it
    void ignoreAndSkip(StrView view);
    // skip n characters
    void skip(usize n = 1);
    // skips whitespace (' ', '\n', '\t', '\r')
    void skipWhitespace();
    // if the next character is c, advances and returns true, otherwise returns false
    bool expect(char c);
    // if the next characters are equal to v, advances and returns true, otherwise returns false
    bool expect(StrView v);
    // read len bytes into buffer, the buffer will not be null terminated
    // returns the number of bytes read
    usize read(char *buf, usize len);
    // returns to the beginning of the stream
    void rewind();
    // returns back <amount> characters
    void rewind(usize amount);
    // returns the number of bytes read from beginning of stream
    usize tell() const;
    // returns the number of bytes left to read in the stream
    usize remaining() const; 
    // return true if the stream doesn't have any new bytes to read
    bool isFinished() const;

    // parse boolean
    bool get(bool &val);
    // parse 8 bit unsigned integer
    bool get(u8 &val);
    // parse 16 bit unsigned integer
    bool get(u16 &val);
    // parse 32 bit unsigned integer
    bool get(u32 &val);
    // parse 64 bit unsigned integer
    bool get(u64 &val);
    // parse 8 bit integer
    bool get(i8 &val);
    // parse 16 bit integer
    bool get(i16 &val);
    // parse 32 bit integer
    bool get(i32 &val);
    // parse 64 bit integer
    bool get(i64 &val);
    // parse float
    bool get(float &val);
    // parse double
    bool get(double &val);

    // get a string until a delimiter
    Str getStr(char delim);
    // get a string until a delimiter
    Str getStr(Arena &arena, char delim);
    // get a view until delimiter
    StrView getView(char delim);
    // get a view until any delimiter inside view
    StrView getViewEither(StrView view);

    const char *start = nullptr;
    const char *cur = nullptr;
    usize size = 0;
};

struct OutStream {
    OutStream() = default;

    bool isNullTerminated() const;
    void finish();

    Str asStr();
    StrView asView() const;

    void replace(char from, char to);
    void print(const char *fmt, ...);
    void printv(const char *fmt, va_list args);
    void putc(char c);
    void puts(const char *cstr);

    void push(bool val);
    void push(u8 val);
    void push(u16 val);
    void push(u32 val);
    void push(u64 val);
    void push(i8 val);
    void push(i16 val);
    void push(i32 val);
    void push(i64 val);
    void push(float val);
    void push(double val);
    void push(StrView view);

    const char &back() const;

    arr<char> data;
};

struct OutStreamArena {
    OutStreamArena(Arena &exclusive_arena);

    bool isNullTerminated() const;
    void finish();

    Str asStr();
    StrView asView() const;

    void replace(char from, char to);
    void print(const char *fmt, ...);
    void printv(const char *fmt, va_list args);
    void putc(char c);
    void puts(const char *cstr);

    void push(bool val);
    void push(u8 val);
    void push(u16 val);
    void push(u32 val);
    void push(u64 val);
    void push(i8 val);
    void push(i16 val);
    void push(i32 val);
    void push(i64 val);
    void push(float val);
    void push(double val);
    void push(StrView view);

    const char &back() const;

    char *buf = nullptr;
    usize len = 0;
    Arena &arena;
};
