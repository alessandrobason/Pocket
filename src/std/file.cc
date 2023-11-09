#include "file.h"

#include "std/arena.h"

#if PLATFORM_WIN

#include "os/win32.h"

static ulong file__toWin32Access(filemode_e mode) {
    ulong access = 0;
    if(mode & FILE_READ)  access |= GENERIC_READ;
    if(mode & FILE_WRITE) access |= GENERIC_WRITE;
    return access;
}

static ulong file__toWin32Creation(filemode_e mode) {
    if(mode & FILE_READ)                  return OPEN_EXISTING;
    if(mode == (FILE_WRITE | FILE_CLEAR)) return CREATE_ALWAYS;
    if(mode & FILE_WRITE)                 return OPEN_ALWAYS;
    if(mode & FILE_BOTH)                  return OPEN_ALWAYS;
    err("unrecognized creation mode: %d", mode);
    return 0;
}

bool fileExists(const char *fname) {
    if (!fname) return false;
    return GetFileAttributesA(fname) != INVALID_FILE_ATTRIBUTES;
}

file_t fileOpen(const char *fname, filemode_e mode) {
    if (!fname) return (file_t){0};

    return (file_t)CreateFileA(
        fname, 
        file__toWin32Access(mode), 
        0, 
        NULL, 
        file__toWin32Creation(mode), 
        FILE_ATTRIBUTE_NORMAL, 
        NULL
    );
}

void fileClose(file_t self) {
    if (self) {
        CloseHandle((HANDLE)self);
    }
}

bool fileIsValid(file_t self) {
    return self && (HANDLE)self != INVALID_HANDLE_VALUE;
}

usize fileRead(file_t self, void *buf, usize len) {
    if (!fileIsValid(self)) {
        return 0;
    }
    ulong bytes_read = 0;
    bool32 result = ReadFile((HANDLE)self, buf, (ulong)len, &bytes_read, NULL);
    return result == TRUE ? (usize)bytes_read : 0;
}

usize fileWrite(file_t self, const void *buf, usize len) {
    if (!fileIsValid(self)) {
        return 0;
    }
    ulong bytes_read = 0;
    bool32 result = WriteFile((HANDLE)self, buf, (ulong)len, &bytes_read, NULL);
    return result == TRUE ? (usize)bytes_read : 0;
}

bool fileSeekEnd(file_t self) {
    if (!fileIsValid(self)) {
        return false;
    }
    return SetFilePointerEx((HANDLE)self, (LARGE_INTEGER){0}, NULL, FILE_END) == TRUE;
}

bool fileRewind(file_t self) {
    if (!fileIsValid(self)) {
        return false;
    }
    return SetFilePointerEx((HANDLE)self, (LARGE_INTEGER){0}, NULL, FILE_BEGIN) == TRUE;
}

uint64 fileTell(file_t self) {
    if (!fileIsValid(self)) {
        return 0;
    }
    LARGE_INTEGER tell;
    bool32 result = SetFilePointerEx((HANDLE)self, (LARGE_INTEGER){0}, &tell, FILE_CURRENT);
    return result == TRUE ? (uint64)tell.QuadPart : 0;
}

usize fileGetSize(file_t self) {
    if (!fileIsValid(self)) {
        return 0;
    }
    ulong high_order = 0;
    ulong low_order = GetFileSize((HANDLE)self, &high_order);
    if (low_order == INVALID_FILE_SIZE) {
        err("Invalid file size: %d", GetLastError());
        return 0;
    }
    uint64 size = (high_order << sizeof(ulong)) | low_order;
    return size;
}

uint64 fileGetTime(file_t self) {
    if (!fileIsValid(self)) {
        return 0;
    }
    uint64 fp_time = 0;
    GetFileTime((HANDLE)self, NULL, NULL, (FILETIME *)&fp_time);
    return fp_time;
}

uint64 fileGetTimePath(const char *path) {
    file_t fp = fileOpen(path, FILE_READ);
    uint64 time = fileGetTime(fp);
    fileClose(fp);
    return time;
}

#endif

#if PLATFORM_LINUX

// TODO use linux specific stuff?
#include <stdio.h>

bool fileExists(const char *fname) {
    FILE *fp = fopen(fname, "rb");
    if (fp) fclose(fp);
    return fp != NULL;
}

file_t fileOpen(const char *fname, filemode_e mode) {
    const char *m = "rb";
    switch (mode) {
        case FILE_READ:  m = "rb"; break;
        case FILE_WRITE: m = "wb"; break;
        case FILE_CLEAR: m = "wb"; break;
        case FILE_BOTH:  m = "rb+"; break;
    }

    return (file_t)fopen(fname, m);
}

void fileClose(file_t self) {
    if (self) fclose((FILE *)self);
}

bool fileIsValid(file_t self) {
    return (FILE *)self;
}

usize fileRead(file_t self, void *buf, usize len) {
    FILE *fp = (FILE *)self;
    if (!fp) return 0;
    return fread(buf, 1, len, fp);
}

usize fileWrite(file_t self, const void *buf, usize len) {
    FILE *fp = (FILE *)self;
    if (!fp) return 0;
    return fwrite(buf, 1, len, fp);
}

bool fileSeekEnd(file_t self) {
    FILE *fp = (FILE *)self;
    if (!fp) return false;
    return fseek(fp, 0, SEEK_END) == 0;
}

bool fileRewind(file_t self) {
    FILE *fp = (FILE *)self;
    if (!fp) return false;
    return fseek(fp, 0, SEEK_SET) == 0;
}

uint64 fileTell(file_t self) {
    FILE *fp = (FILE *)self;
    if (!fp) return 0;
    return (uint64)ftell(fp);
}

usize fileGetSize(file_t self) {
    FILE *fp = (FILE *)self;
    if (!fp) return 0;
    fileSeekEnd(self);
    usize len = fileTell(self);
    fileRewind(self);
    return len;
}

uint64 fileGetTime(file_t self) {
    FILE *fp = (FILE *)self;
    if (!fp) return 0;
    // TODO linux
    assert(false);
}

uint64 fileGetTimePath(const char *path) {
    if (!path) return 0;
    // TODO linux
    assert(false);
}

#endif

bool filePutc(file_t self, char c) {
    return fileWrite(self, &c, 1) == 1;
}

bool filePuts(file_t self, const char *str) {
    if (!str) return false;
    usize len = strlen(str);
    return fileWrite(self, str, len) == len;
}

bool filePutStr(file_t self, const str_t *data) {
    return fileWrite(self, data->buf, data->len) == data->len;
}

bool filePutView(file_t self, strview_t view) {
    return fileWrite(self, view.buf, view.len) == view.len;
}

filebuf_t fileReadWhole(arena_t *arena, const char *fname) {
    file_t fp = fileOpen(fname, FILE_READ);
    filebuf_t buf = fileReadWholeFP(arena, fp);
    fileClose(fp);
    return buf;
}

filebuf_t fileReadWholeFP(arena_t *arena, file_t self) {
    if (!fileIsValid(self)) {
        return (filebuf_t){0};
    }
    filebuf_t out = {0};
    out.len = fileGetSize(self);
    out.buf = new(arena, byte, out.len);
    bool success = fileRead(self, out.buf, out.len) == out.len;
    if (!success) {
        arenaRewind(arena, arenaTell(arena) - out.len);
        return (filebuf_t){0};
    }
    return out;
}

str_t fileReadWholeText(arena_t *arena, const char *fname) {
    file_t fp = fileOpen(fname, FILE_READ);
    str_t buf = fileReadWholeTextFP(arena, fp);
    fileClose(fp);
    return buf;
}

str_t fileReadWholeTextFP(arena_t *arena, file_t self) {
    if (!fileIsValid(self)) {
        return (str_t){0};
    }
    str_t out = {0};
    out.len = fileGetSize(self);
    out.buf = new(arena, char, out.len + 1);
    bool success = fileRead(self, out.buf, out.len) == out.len;
    if (!success) {
        arenaRewind(arena, arenaTell(arena) - (out.len + 1));
        return (str_t){0};
    }
    return out; 
}

bool fileWriteWhole(const char *fname, const byte *data, usize len) {
    file_t fp = fileOpen(fname, FILE_WRITE);
    bool success = fileWriteWholeFP(fp, data, len);
    fileClose(fp);
    return success;
}

bool fileWriteWholeFP(file_t self, const byte *data, usize len) {
    if (!fileIsValid(self)) {
        return false;
    }
    return fileWrite(self, data, len) == len;
}

bool fileWriteWholeText(const char *fname, strview_t string) {
    file_t fp = fileOpen(fname, FILE_WRITE);
    bool success = fileWriteWholeTextFP(fp, string);
    fileClose(fp);
    return success;
}

bool fileWriteWholeTextFP(file_t self, strview_t string) {
    if (!fileIsValid(self)) {
        return false;
    }
    return fileWrite(self, string.buf, string.len) == string.len;
}
