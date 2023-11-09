#include "ini.h"

#include "std/stream.h"
#include "std/arena.h"
#include "std/file.h"

// == VALUE ==============================================================================================

u64 Ini::Value::asUint(u64 default_value) const {
    InStream in = value;
    u64 val = default_value;
    in.get(val);
    return val;
}

i64 Ini::Value::asInt(i64 default_value) const {
    InStream in = value;
    i64 val = default_value;
    in.get(val);
    return val;
}

double Ini::Value::asNum(double default_value) const {
    InStream in = value;
    double val = default_value;
    in.get(val);
    return val;
}

bool Ini::Value::asBool(bool default_value) const {
    InStream in = value;
    bool val = default_value;
    in.get(val);
    return val;
}

Str Ini::Value::asStr(const Str &default_value) const {
    if (value.empty()) return default_value;
    InStream in = value;
    return in.getStr('\0');
}

Str Ini::Value::asStr(Arena &arena, const Str &default_value) const {
    if (value.empty()) return default_value;
    InStream in = value;
    return in.getStr(arena, '\0');
}
arr<StrView> Ini::Value::asArr(char delim, const Slice<StrView> &default_value) const {
    if (value.empty()) return default_value.dup();
    if (!delim) delim = ',';
    
    arr<StrView> out = {};
    InStream in = value;

    while (!in.isFinished()) {
        out.push(in.getView(delim).trim());
        in.ignoreAndSkip(delim);
    }

    return out;
}

Ini::Value::operator u64() const {
    return asUint();
}

Ini::Value::operator i64() const {
    return asInt();
}

Ini::Value::operator double() const {
    return asNum();
}

Ini::Value::operator bool() const {
    return asBool();
}

Ini::Value::operator Str() const {
    return asStr();
}

Ini::Value::operator StrView() const {
    return value;
}

Ini::Value::operator arr<StrView>() const {
    return asArr();
}

// == TABLE ==============================================================================================

Ini::Value Ini::Table::operator[](StrView key) {
    if (Value *v = values.get(key)) {
        return *v;
    }
    return Value();
}

// == DOC ================================================================================================

void ini__add_value(Ini::Table &table, InStream &in, const Ini::Options &options) {
    StrView key = in.getView(options.key_value_divider).trim();
    in.skip(); // skip divider
    StrView value = in.getView('\n').trim();
    // value might be until EOF, in that case no use in skipping
    if (!in.isFinished()) {
        // skip newline
        in.skip();
    }

    Ini::Value *val = nullptr;

    if (options.merge_duplicate_keys) {
        val = table.values.get(key);
    }

    if (!val) {
        val = table.values.push(key);
    }

    val->value = value;
}

void ini__add_table(Ini &ini, InStream &in, const Ini::Options &options) {
    in.skip(); // skip [
    StrView name = in.getView(']');
    in.skip(); // skip ]

    Ini::Table *tab = nullptr;

    if (options.merge_duplicate_tables) {
        tab = ini.tables.get(name);
    }

    if (!tab) {
        tab = ini.tables.push(name);
    }

    in.ignoreAndSkip('\n');
    while (!in.isFinished()) {
        switch (in.peek()) {
            case '\r': // fallthrough
            case '\n':
                return;
            case '#': // fallthrough
            case ';':
                in.ignoreAndSkip('\n');
                break;
            default:
                ini__add_value(*tab, in, options);
                break;
        }
    }
}

void ini__parse(Ini &ini, const Ini::Options &options) {
    InStream in = StrView(ini.text);

    in.skipWhitespace();
    while (!in.isFinished()) {
        switch (in.peek()) {
            case '[':
                ini__add_table(ini, in, options);
                break;
            case '#': // fallthrough
            case ';':
                in.ignoreAndSkip('\n');
                break;
            default:
                ini__add_value(ini.root, in, options);
                break;
        }
        in.skipWhitespace();
    }
}

Ini Ini::parse(const char *filename, const Options &options) {
    Ini ini = { .text = File::readWholeText(filename) };
    ini__parse(ini, options);
    return ini;
}

Ini Ini::parseStr(const char *inistr, const Options &options) {
    Ini ini = { .text = inistr };
    ini__parse(ini, options);
    return ini;
}

Ini::Table &Ini::operator[](StrView name) {
    static Table empty_table = {};
    Table *t = tables.get(name);
    return t ? *t : empty_table;
}