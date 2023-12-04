#include "asio.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "filesystem.h"

namespace asio {
    File::File(StrView filename) {
        init(filename);
    }

    File::~File() {
        HANDLE fp = (HANDLE)handle;
        OVERLAPPED *ov = (OVERLAPPED *)internal;
        if (fp && fp != INVALID_HANDLE_VALUE) {
            CloseHandle(fp);
        }
        if (ov && ov->hEvent != INVALID_HANDLE_VALUE) {
            CloseHandle(ov->hEvent);
        }

        pk_free(internal);
        handle = 0;
        internal = nullptr;
        data.clear();
    }

    bool File::init(StrView filename) {
        fs::Path path = fs::getPath(filename);
			
        HANDLE fp = CreateFile(
            path.cstr(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            nullptr
        );

        if (fp == INVALID_HANDLE_VALUE) {
            err("could not open file %.*s, error: %u", filename.len, filename.buf, GetLastError());
            return false;
        }

        HANDLE event = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        if (!event) {
            err("could not create event: %u", GetLastError());
            CloseHandle(fp);
            return false;
        }

        internal = pk_calloc(sizeof(OVERLAPPED), 1);

        OVERLAPPED *ov = (OVERLAPPED *)internal;
        ov->hEvent = event;

        // this means it caps file size at UINT_MAX, buf ReadFile also does that and we want
        // to read the file all at once. 
        // TODO: find a way to use ReadFile in multiple calls
        DWORD file_size = GetFileSize(fp, nullptr);

        if (file_size == INVALID_FILE_SIZE) {
            err("could not get file size: %u", GetLastError());
            CloseHandle(fp);
            CloseHandle(event);
            return false;
        }

        // grow doesn't call the constructor on the values, which we don't need now
        data.grow((usize)file_size);

        BOOL result = ReadFile(
            fp,
            data.buf,
            (DWORD)data.len,
            nullptr,
            ov
        );

        if (result) {
            info("read %.*s asynchronously", filename.len, filename.buf);
            return true;
        }

        handle = (uptr)fp;

        info("Last error (should be %u) = %u", ERROR_IO_PENDING, GetLastError());

        return true;
    }

    bool File::isValid() const {
        return handle && (HANDLE)handle != INVALID_HANDLE_VALUE && internal;
    }

    bool File::poll() {
        if (!isValid()) return true;
        DWORD bytes_read = 0;
        BOOL result = GetOverlappedResult((HANDLE)handle, (OVERLAPPED *)internal, &bytes_read, FALSE);
        return result;
    }

    arr<byte> &&File::getData() {
        return mem::move(data);
    }
} // namespace asio