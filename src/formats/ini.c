#include "ini.h"

#include "std/arena.h"
#include "std/stream.h"
#include "std/file.h"

#include <stdio.h>

// == INI READER ========================================================================

static const IniOpts default_opts = {
    .key_value_divider = '='
};

static IniOpts ini__getDefaultOptions(const IniOpts *options);
static initable_t *ini__findTable(initable_t *table, strview_t name);
static inivalue_t *ini__findValue(inivalue_t *value, strview_t key);
static void ini__addTable(arena_t *arena, ini_t *self, instream_t *in, const IniOpts *options);
static void ini__addValue(arena_t *arena, initable_t *table, instream_t *in, const IniOpts *options);

void ini__parseInternal(arena_t *arena, ini_t *ini, const IniOpts *options) {
    // add root table
    ini->root = new(arena, initable_t);
    ini->root->name = strv("root");
    ini->tail = ini->root;
    instream_t in = istrLen(ini->text.buf, ini->text.len);
    
    istrSkipWhitespace(&in);
    while (!istrIsFinished(&in)) {
        switch(istrPeek(&in)) {
        case '[':
            ini__addTable(arena, ini, &in, options);
            break;
        case '#': case ';':
            istrIgnore(&in, '\n');
            break;
        default:
            ini__addValue(arena, ini->root, &in, options);
            break;
        }
        istrSkipWhitespace(&in);
    }
}

ini_t iniParse(arena_t *arena, const char *filename, const IniOpts *options) {
    ini_t ini = {
        .text = strvFromStr(fileReadWholeText(arena, filename))
    };
    if (strvIsEmpty(ini.text)) {
        return ini;
    }
    IniOpts opts = ini__getDefaultOptions(options);
    ini__parseInternal(arena, &ini, &opts);
    return ini;
}

ini_t iniParseString(arena_t *arena, const char *inistr, const IniOpts *options) {
    ini_t ini = { .text = strv(inistr) };
    if (!options) options = &default_opts;
    ini__parseInternal(arena, &ini, options);
    return ini;
}

initable_t *iniGetTable(ini_t *self, const char *name) {
    if (!self || !self->root) return NULL;

    if (!name) {
        return self->root;
    }
    else {
        return ini__findTable(self->root->next, strv(name));
    }
}

inivalue_t *iniGet(initable_t *self, const char *key) {
    return self ? ini__findValue(self->head, strv(key)) : NULL;
}

#if 0
vec(strview_t) iniAsArray(const inivalue_t *value, char delim) {
    if (!value) return NULL;
    if (!delim) delim = ' ';

    vec(strview_t) out = NULL;
    strview_t v = value->value;

    usize start = 0;
    for (usize i = 0; i < v.len; ++i) {
        if (v.buf[i] == delim) {
            strview_t arr_val = strvTrim(strvSub(v, start, i));
            if (!strvIsEmpty(arr_val)) vecAppend(out, arr_val);
            start = i + 1;
        }
    }
    strview_t last = strvTrim(strvSub(v, start, SIZE_MAX));
    if (!strvIsEmpty(last)) vecAppend(out, last);
    return out;
}

vec(strview_t) iniAsArrayU8(const inivalue_t *value, const char *delim) {
    if (!value || !delim) return NULL;

    rune cpdelim = utf8Decode(&delim);
    vec(strview_t) out = NULL;
    strview_t v = value->value;

    const char *start = v.buf;
    const char *buf = v.buf;
    const char *prevbuf = buf;

    for(rune cp = utf8Decode(&buf); 
        buf != (v.buf + v.len); 
        cp = utf8Decode(&buf)
    ) {
        if (cp == cpdelim) {
            usize start_pos = start - v.buf;
            usize end_pos = prevbuf - v.buf;
            strview_t arr_val = strvTrim(strvSub(v, start_pos, end_pos));
            if (!strvIsEmpty(arr_val)) vecAppend(out, arr_val);
            // buf has already gone to the next codepoint, skipping the delimiter
            start = buf;
        }
        prevbuf = buf;
    }

    strview_t last = strvTrim(strvSub(v, start - v.buf, SIZE_MAX));
    if (!strvIsEmpty(last)) vecAppend(out, last);
    return out;
}
#endif

uint64 iniAsUInt(const inivalue_t *value) {
    if (!value) return 0;
    instream_t in = istrLen(value->value.buf, value->value.len);
    uint64 val = 0;
    if (!istrGetUint64(&in, &val)) val = 0;
    return val;
}

int64 iniAsInt(const inivalue_t *value) {
    if (!value) return 0;
    instream_t in = istrLen(value->value.buf, value->value.len);
    int64 val = 0;
    if (!istrGetInt64(&in, &val)) val = 0;
    return val;
}

double iniAsNum(const inivalue_t *value) {
    if (!value) return 0.f;
    instream_t in = istrLen(value->value.buf, value->value.len);
    double val = 0;
    if (!istrGetDouble(&in, &val)) val = 0;
    return val;
}

bool iniAsBool(const inivalue_t *value) {
    if (!value) return false;
    return strvEqual(value->value, strv("true"));
}

strview_t iniAsView(const inivalue_t *value) {
    if (!value) return (strview_t){0};
    return value->value;
}

str_t iniAsStr(arena_t *arena, const inivalue_t *value) {
    if (!value) return (str_t){0};
    return strvDup(arena, value->value);
}

// == PRIVATE FUNCTIONS ====================================================================================

static IniOpts ini__getDefaultOptions(const IniOpts *options) {
    if (!options) return default_opts;
    
    IniOpts opts = default_opts;
    
    if (options->merge_duplicate_keys) 
        opts.merge_duplicate_keys = options->merge_duplicate_keys;
    
    if (options->merge_duplicate_tables) 
        opts.merge_duplicate_tables = options->merge_duplicate_tables;
    
    if (options->key_value_divider) 
        opts.key_value_divider = options->key_value_divider;
    
    return opts;
}

static initable_t *ini__findTable(initable_t *table, strview_t name) {
    if (strvIsEmpty(name)) return NULL;
    while (table) {
        if (strvEqual(table->name, name)) {
            break;
        }
        table = table->next;
    }
    return table;
}

static inivalue_t *ini__findValue(inivalue_t *value, strview_t key) {
    if (strvIsEmpty(key)) return NULL;
    while (value) {
        if (strvEqual(value->key, key)) {
            break;
        }
        value = value->next;
    }
    return value;
}

static void ini__addTable(arena_t *arena, ini_t *self, instream_t *in, const IniOpts *options) {
    istrSkip(in, 1); // skip [
    strview_t name = istrGetView(in, ']');
    istrSkip(in, 1); // skip ]
    initable_t *table = options->merge_duplicate_tables 
                            ? ini__findTable(self->root->next, name) 
                            : NULL;

    if (!table) {
        table = new(arena, initable_t);
        table->name = name;
        self->tail->next = table;
        self->tail = table;
    }
    
    istrIgnore(in, '\n'); istrSkip(in, 1);
    while (!istrIsFinished(in)) {
        switch (istrPeek(in)) {
        case '\n': case '\r':
            return;
        case '#': case ';':
            istrIgnore(in, '\n');
            break;
        default:
            ini__addValue(arena, table, in, options);
            break;
        }
    }
}

static void ini__addValue(arena_t *arena, initable_t *table, instream_t *in, const IniOpts *options) {
    if (!table) {
        panic("Table is null");
        abort();
    }
    
    strview_t key = strvTrim(istrGetView(in, options->key_value_divider));
    istrSkip(in, 1);
    strview_t value = strvTrim(istrGetView(in, '\n'));
    // value might be until EOF, in that case no use in skipping
    if (!istrIsFinished(in)) istrSkip(in, 1); // skip newline
    
    inivalue_t *new_value = options->merge_duplicate_keys ? ini__findValue(table->head, key) : NULL;
    
    if (!new_value) {
        new_value = new(arena, inivalue_t);
        new_value->key = key;
        new_value->value = value;
        if (table->head) {
            // find tail
            inivalue_t *tail = table->head;
            while (tail->next) {
                tail = tail->next;
            }
            tail->next = new_value;
        }
        else {
            table->head = new_value;
        }
    }
    else {
        new_value->value = value;
    }
}
