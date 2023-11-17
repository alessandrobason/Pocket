#include "file.h"

#include "logging.h"

File::File(const char *filename, Mode mode) {
    open(filename, mode);
}

File::~File() {
    close();
}

arr<byte> File::readWhole(const char *fname) {
    File fp = File(fname, File::Read);
    if (!fp.isValid()) {
        err("could not open file %s", fname);
        return {};
    }

    arr<byte> out;
    out.resize(fp.getSize());

    bool success = fp.read(out.buf, out.len);
    if (!success) {
        err("could not read data from file %s", fname);
        return {};
    }

    return out;
}

Str File::readWholeText(const char *fname) {
    File fp = File(fname, File::Read);
    if (!fp.isValid()) {
        err("could not open file %s", fname);
        return {};
    }

    Str out;
    out.resize(fp.getSize());

    bool success = fp.read(out.data(), out.size());
    if (!success) {
        err("could not read data from file %s", fname);
        return {};
    }

    return out;
}

Str File::readWholeText(Arena &arena, const char *fname) {
    File fp = File(fname, File::Read);
    if (!fp.isValid()) {
        err("could not open file %s", fname);
        return {};
    }

    Str out;
    out.resize(arena, fp.getSize());

    bool success = fp.read(out.data(), out.size());
    if (!success) {
        err("could not read data from file %s", fname);
        return {};
    }

    return out;
}

bool File::writeWhole(const char *fname, Slice<byte> data) {
    File fp = File(fname, File::Write);
    if (!fp.isValid()) {
        err("could not open file %s", fname);
        return false;
    }

    return fp.write(data.buf, data.len);
}

bool File::writeWhole(const char *fname, StrView string) {
    return writeWhole(fname, Slice<char>(string.buf, string.len));
}

bool File::putc(char c) {
    return write(&c);
}

bool File::puts(StrView view) {
    return write(view.buf, view.len);
}

#if PK_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static uint file__to_win32_access(File::Mode mode) {
    uint access = 0;
    if (mode & File::Read)  access |= GENERIC_READ;
    if (mode & File::Write) access |= GENERIC_WRITE;
    return access;
}

static uint file__to_win32_creation(File::Mode mode) {
    if (mode & File::Read)                   return OPEN_EXISTING;
    if (mode == (File::Write | File::Clear)) return CREATE_ALWAYS;
    if (mode & File::Write)                  return OPEN_ALWAYS;
    if (mode & File::Both)                   return OPEN_ALWAYS;
    err("unrecognized creation mode: %u", mode);
    return 0;
}

u64 File::getTime(const char *path) {
    u64 time = File(path, File::Read).getTime();
    return time;
}

bool File::exists(const char *fname) {
    if (!fname) return false;
    return GetFileAttributesA(fname) != INVALID_FILE_ATTRIBUTES;
}

bool File::open(const char *fname, Mode mode) {
    if (!fname) {
        err("null filename passed to File::open");
        return false;
    }

    file_ptr = (uptr)CreateFileA(
        fname,
        file__to_win32_access(mode),
        0,
        nullptr,
        file__to_win32_creation(mode),
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    return isValid();
}

void File::close() {
    if (file_ptr) {
        CloseHandle((HANDLE)file_ptr);
    }
}

bool File::isValid() const {
    return file_ptr && (HANDLE)file_ptr != INVALID_HANDLE_VALUE;
}

bool File::read(void *buf, usize len) {
    if (!isValid()) return false;
    
    ulong bytes_read = 0;
    i32 result = ReadFile((HANDLE)file_ptr, buf, (uint)len, &bytes_read, nullptr);
    return result == TRUE;
}

bool File::write(const void *buf, usize len) {
    if (!isValid()) return false;
    ulong bytes_read = 0;
    i32 result = WriteFile((HANDLE)file_ptr, buf, (ulong)len, &bytes_read, nullptr);
    return result == TRUE;
}

bool File::seekEnd() {
    if (!isValid()) return false;
    return SetFilePointerEx((HANDLE)file_ptr, {}, nullptr, FILE_END) == TRUE;
}

bool File::rewind() {
    if (!isValid()) return false;
    return SetFilePointerEx((HANDLE)file_ptr, {}, nullptr, FILE_BEGIN) == TRUE;
}

u64 File::tell() {
    if (!isValid()) return 0;
    LARGE_INTEGER tell;
    i32 result = SetFilePointerEx((HANDLE)file_ptr, {}, &tell, FILE_CURRENT);
    return result == TRUE ? (u64)tell.QuadPart : 0;
}

usize File::getSize() {
    if (!isValid()) return 0;
    LARGE_INTEGER file_size;
    i32 result = GetFileSizeEx((HANDLE)file_ptr, &file_size);
    return result == TRUE ? (u64)file_size.QuadPart : 0;
}

u64 File::getTime() {
    if (!isValid()) return 0;
    u64 fp_time = 0;
    GetFileTime((HANDLE)file_ptr, nullptr, nullptr, (FILETIME *)&fp_time);
    return fp_time;
}

#endif


#if PK_POSIX

// TODO use linux specific stuff?
#include <stdio.h>

u64 File::getTime(const char *path) {
    if (!path) return 0;
    // TODO linux
    pk_assert(false);
}

bool File::exists(const char *fname) {
    FILE *fp = fopen(fname, "rb");
    if (fp) fclose(fp);
    return fp != nullptr;
}

bool File::open(const char *fname, Mode mode) {
    const char *m = "rb";
    switch (mode) {
        case File::Read:  m = "rb"; break;
        case File::Write: m = "wb"; break;
        case File::Clear: m = "wb"; break;
        case File::Both:  m = "rb+"; break;
    }

    file_ptr = (uptr)fopen(fname, m);
    return (FILE *)file_ptr != nullptr;
}

void File::close() {
    if (file_ptr) {
        fclose((FILE *)file_ptr);
    }
}

bool File::isValid() const {
    return (FILE *)file_ptr != nullptr;
}

bool File::read(void *buf, usize len) {
    if (!isValid()) return false;
    return fread(buf, 1, len, (FILE *)file_ptr) == len;
}

bool File::write(const void *buf, usize len) {
    if (!isValid()) return false;
    return fwrite(buf, 1, len, (FILE *)file_ptr) == len;
}

bool File::seekEnd() {
    if (!isValid()) return false;
    return fseek((FILE *)file_ptr, 0, SEEK_END) == 0;
}

bool File::rewind() {
    if (!isValid()) return false;
    return fseek((FILE *)file_ptr, 0, SEEK_SET) == 0;
}

u64 File::tell() {
    if (!isValid()) return 0;
    return (u64)ftell((FILE *)file_ptr);
}

usize File::getSize() {
    if (!isValid()) return 0;
    seekEnd();
    usize len = tell();
    rewind();
    return len;
}

u64 File::getTime() {
    if (!isValid()) return 0;
    // TODO linux
    pk_assert(false);
}

#endif
