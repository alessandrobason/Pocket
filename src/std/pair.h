#pragma once

#include "mem.h"

template<typename T, typename Q>
struct pair {
    pair() = default;
    pair(T &&f, Q &&s) : first(mem::move(f)), second(mem::move(s)) {}
    pair(const T &f, Q &&s) : first(f), second(mem::move(s)) {}
    pair(const T &f, const Q &s) : first(f), second(s) {}
    T first;
    Q second;
};