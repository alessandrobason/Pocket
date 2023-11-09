#pragma once

#include "common.h"
#include "std/str.h"

typedef struct arena_t arena_t;

typedef struct inivalue_t inivalue_t;
typedef struct inivalue_t {
    strview_t key;
    strview_t value;
    inivalue_t *next;
} inivalue_t;

typedef struct initable_t initable_t;
typedef struct initable_t {
    strview_t name;
    inivalue_t *head;
    initable_t *next;
} initable_t;

typedef struct {
    // str_t text;
    strview_t text;
    initable_t *root;
    initable_t *tail;
} ini_t;

typedef struct {
    bool merge_duplicate_tables; // default false
    bool merge_duplicate_keys;   // default false
    char key_value_divider;      // default =
} IniOpts;

ini_t iniParse(arena_t *arena, const char *filename, const IniOpts *options);
ini_t iniParseString(arena_t *arena, const char *inistr, const IniOpts *options);

initable_t *iniGetTable(ini_t *ctx, const char *name);
inivalue_t *iniGet(initable_t *ctx, const char *key);

// vec(strview_t) iniAsArray(const inivalue_t *value, char delim);
// // delim is expected to be a single utf8 character
// vec(strview_t) iniAsArrayU8(const inivalue_t *value, const char *delim);
uint64 iniAsUInt(const inivalue_t *value);
int64 iniAsInt(const inivalue_t *value);
double iniAsNum(const inivalue_t *value);
bool iniAsBool(const inivalue_t *value);
strview_t iniAsView(const inivalue_t *value);
str_t iniAsStr(arena_t *arena, const inivalue_t *value);
