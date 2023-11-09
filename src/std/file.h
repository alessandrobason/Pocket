#pragma once

#include "common.h"
#include "str.h"

typedef struct arena_t arena_t;

typedef enum {
    FILE_READ  = 1 << 0,
    FILE_WRITE = 1 << 1,
    FILE_CLEAR = 1 << 2,
    FILE_BOTH  = FILE_READ | FILE_WRITE
} filemode_e;

typedef uptr file_t;

typedef struct filebuf_t {
    byte *buf;
    usize len;
} filebuf_t;

bool fileExists(const char *fname);

file_t fileOpen(const char *fname, filemode_e mode);
void fileClose(file_t self);

bool fileIsValid(file_t self);

bool filePutc(file_t self, char c);
bool filePuts(file_t self, const char *str);
bool filePutStr(file_t self, const str_t *str);
bool filePutView(file_t self, strview_t view);

usize fileRead(file_t self, void *buf, usize len);
usize fileWrite(file_t self, const void *buf, usize len);

bool fileSeekEnd(file_t self);
bool fileRewind(file_t self);

uint64 fileTell(file_t self);
usize fileGetSize(file_t self);

filebuf_t fileReadWhole(arena_t *arena, const char *fname);
filebuf_t fileReadWholeFP(arena_t *arena, file_t self);

str_t fileReadWholeText(arena_t *arena, const char *fname);
str_t fileReadWholeTextFP(arena_t *arena, file_t self);

bool fileWriteWhole(const char *fname, const byte *data, usize len);
bool fileWriteWholeFP(file_t self, const byte *data, usize len);

bool fileWriteWholeText(const char *fname, strview_t string);
bool fileWriteWholeTextFP(file_t self, strview_t string);

uint64 fileGetTime(file_t self);
uint64 fileGetTimePath(const char *path);
