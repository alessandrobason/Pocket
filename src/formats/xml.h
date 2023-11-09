#pragma once

#include "common.h"
#include "std/str.h"

typedef struct arena_t arena_t;
typedef struct xmlattr_t xmlattr_t;
typedef struct xmlelem_t xmlelem_t;

typedef struct xmlattr_t {
    strview_t key;
    strview_t value;
    xmlattr_t *next;
} xmlattr_t;

typedef struct xmlelem_t {
    strview_t tag;
    strview_t body;
    xmlattr_t *attributes;
    xmlattr_t *attr_tail;
    xmlelem_t *next;
    xmlelem_t *children;
    xmlelem_t *child_tail;
} xmlelem_t;

typedef struct xml_t {
    xmlelem_t *root;
    str_t text;
} xml_t;

typedef enum xmlflags_e {
    XML_NONE           = 0,
    XML_NO_OPENING_TAG = 1 << 0,
    XML_COPY_STR       = 1 << 1,
    XML_HTML           = 1 << 2 | XML_NO_OPENING_TAG,
} xmlflags_e;

xml_t xmlParse(arena_t *arena, const char *filename, xmlflags_e flags);
xml_t xmlParseStr(arena_t *arena, str_t str, xmlflags_e flags);

xmlelem_t *xmlGet(const xmlelem_t *self, strview_t tag);
xmlattr_t *xmlGetAttr(const xmlelem_t *self, strview_t key);
