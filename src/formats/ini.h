#pragma once

#include "std/common.h"
#include "std/str.h"
#include "std/hashmap.h"
#include "std/arr.h"
#include "std/slice.h"
#include "std/stream.h"

struct Arena;

struct Ini {
    struct Value {
        u64 asUint(u64 default_value = 0) const;
        i64 asInt(i64 default_value = 0) const;
        double asNum(double default_value = 0) const;
        bool asBool(bool default_value = false) const;
        Str asStr(const Str &default_value = {}) const;
        Str asStr(Arena &arena, const Str &default_value = {}) const;
        arr<StrView> asArr(char delim = ',', const Slice<StrView> &default_value = {}) const;
        template<typename T>
        arr<T> asArr(char delim = ',', const Slice<T> &default_value = {}) const {
            if (value.empty()) return default_value.dup();
            if (!delim) delim = ',';

            arr<T> out = {};
            InStream in = value;

            while (!in.isFinished()) {
                T val;
                if (in.get(val)) {
                    out.push(mem::move(val));
                }
                in.ignoreAndSkip(delim);
            }

            return out;
        }

        operator u64() const;
        operator i64() const;
        operator double() const;
        operator bool() const;
        operator Str() const;
        operator StrView() const;
        operator arr<StrView>() const;
        template<typename T>
        operator arr<T>() const {
            return asArr<T>();
        }

        StrView value;
    };

    struct Table {
        Value operator[](StrView key);

        HashMap<StrView, Value> values;
    };

    struct Options {
        bool merge_duplicate_tables = false;
        bool merge_duplicate_keys = false;
        char key_value_divider = '=';
    };

    static Ini parse(const char *filename, const Options &options = {});
    static Ini parseStr(const char *inistr, const Options &options = {});

    Table &operator[](StrView name);

    Str text;
    HashMap<StrView, Table> tables;
    Table root;
};
