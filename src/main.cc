#include "std/logging.h"
#include "std/arena.h"
#include "std/filesystem.h"

#include "gfx/engine.h"

#include <iostream>
#include <windows.h>

Engine *g_engine = nullptr;

#include "std/file.h"
#include <chrono>

using timer = std::chrono::high_resolution_clock;
usize ms(auto t) {
	return std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
}

namespace asio {
	struct win32_overlapped {
		uptr Internal;
		uptr InternalHigh;
		union {
			struct {
				ulong Offset;
				ulong OffsetHigh;
			};
			void *Pointer;
		};

		void *hEvent;
	};

	struct File {
		enum PollResult {
			IoPending = 997,

		};

		File() = default;
		File(StrView filename) {
			static_assert(sizeof(win32_overlapped) == sizeof(OVERLAPPED));
			init(filename);
		}

		~File() {

		}
		
		bool init(StrView filename) {
			fs::Path path = fs::getPath(filename);
			
			HANDLE fp = CreateFileA(
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
				err("could not create event %u", GetLastError());
				CloseHandle(fp);
				return false;
			}

			OVERLAPPED ov = { .hEvent = event };

			LARGE_INTEGER file_size;
			BOOL result = GetFileSizeEx(fp, &file_size);

			if (!result) {
				err("could not get file %.*s size: %u", filename.len, filename.buf, GetLastError());
				CloseHandle(fp);
				CloseHandle(event);
				return false;
			}

			data.grow((usize)file_size.QuadPart);

			DWORD bytes_read = 0;

			result = ReadFile(
				fp,
				data.buf,
				(DWORD)data.len,
				&bytes_read,
				&ov
			);

			if (result) {
				info("finished reading file synchronously!");
				return true;
			}

			last_error = GetLastError();

			handle = (uptr)fp;
			memcpy(&overlapped, &ov, sizeof(ov));
		}

		bool poll() {
			if (last_error != ERROR_IO_PENDING && last_error != ERROR_IO_INCOMPLETE) {
				err("unkown async error: %u");
				return false;
			}

			DWORD bytes_read = 0;

			BOOL result = GetOverlappedResult(
				(HANDLE)handle,
				(OVERLAPPED *)&overlapped,
				&bytes_read,
				FALSE
			);

			last_error = GetLastError();

			return result;
		}

		arr<byte> &&getData() {
			return mem::move(data);
		}

	private:
		arr<byte> data;
		ulong last_error = 0;
		uptr handle = 0;
		win32_overlapped overlapped = {};
	};
} // namespace asio

struct asFile {
	HANDLE handle;
	OVERLAPPED data;
};

arr<byte> asTestEnd(HANDLE event, HANDLE fp) {
	OVERLAPPED ov = {};

	ov.hEvent = event;

	DWORD file_size = GetFileSize(fp, nullptr);
	info("file size: %u", file_size);

	arr<byte> out;
	out.grow(file_size);
	//DWORD bytes_read = 0;

	bool is_running = true;
	while (is_running) {
		//char buf[1024];
		//DWORD bytes_read = 0;

		auto start = timer::now();
		//BOOL result = ReadFileEx(
		//	fp,
		//	out.buf,
		//	(DWORD)out.len,
		//	&ov,
		//	[](DWORD error_code, DWORD bytes_read, OVERLAPPED *ov) {
		//		info("error: %u", error_code);
		//		info("read: %u", bytes_read);
		//	}
		//);

		BOOL result = ReadFile(
			fp,
			out.buf,
			(DWORD)out.len,
			nullptr,
			&ov
		);

		auto end = timer::now();
		info("readfile: %zums", ms(end - start));
		//info("readfileex: %zums", ms(end - start));

		info("last error: %u", GetLastError());

		//SleepEx(INFINITE, TRUE);
		//break;

		//BOOL result = ReadFile(
		//	fp,
		//	out.buf,
		//	(DWORD)out.len,
		//	nullptr,
		//	&ov
		//);

		//if (!result) {
		//	err("%u", GetLastError());
		//	return {};
		//}

		info("> %u", GetLastError());

		while (true) {
			DWORD bytes_read;
			//BOOL result = GetOverlappedResultEx(fp, &ov, &bytes_read, 0, TRUE);
			BOOL result = GetOverlappedResult(fp, &ov, &bytes_read, FALSE);
			info("result: %s", result ? "true" : "false");
			info("last error: %u", GetLastError());
			if (result) break;
			//Slice<HANDLE> handles = { fp, ov.hEvent };
			////GetOverlappedResult(fp, &ov, &bytes_read, FALSE);
			////DWORD result = WaitForMultipleObjectsEx((DWORD)handles.len, handles.buf, FALSE, 100, TRUE);
			//DWORD result = WaitForSingleObjectEx(ov.hEvent, 1000, TRUE);
			//info("waiting for read: %u", result);
			//switch (result) {
			//	case WAIT_ABANDONED: info("abandoned"); break;
			//	case WAIT_IO_COMPLETION: info("completed"); break;
			//	case WAIT_OBJECT_0: info("object 0"); break;
			//	case WAIT_TIMEOUT: info("timeout"); break;
			//	case WAIT_FAILED: info("failed"); break;
			//	default: err("unknown"); return {};
			//}
			//if (result != WAIT_TIMEOUT) break;
		}

		break;

		//BOOL result = ReadFile(
		//	fp,
		//	buf,
		//	file_size,
		//	nullptr,
		//	&ov
		//);

		DWORD error = GetLastError();

		if (!result) {
			switch (error) {
				case ERROR_HANDLE_EOF:
					info("readfile returned false and eof condition, async eof not triggered");
					break;
				case ERROR_IO_PENDING:
				{
					info("io pending");

					bool pending = true;

					while (pending) {
						pending = false;

						info ("read file operation is pending");
						// do something else

						DWORD bytes_read = 0;

						result = GetOverlappedResult(
							fp,
							&ov,
							&bytes_read,
							FALSE
						);

						info("read: %u", bytes_read);

						if (!result) {
							error = GetLastError();
							switch (error) {
								case ERROR_HANDLE_EOF:
									info("getoverlappedresult found eof");
									break;
								case ERROR_IO_INCOMPLETE:
									info("getoverlappedresult io incomplete %u", bytes_read);
									pending = true;
									break;
								default:
									err("getoverlappedresult failed %u", error);
									break;
							}
						}
						else {
							info("ReadFile operation completed");
							ResetEvent(ov.hEvent);
							is_running = false;
						}
					}
					break;
				}
				default:
					err("ReadFile unhandled %u", error);
					break;
			}
		}
		else {
			info("readfile completed synchronously");
			break;
		}

		//ov.Offset += bytes_read;
		//if (ov.Offset < file_size) {
		//	is_running = true;
		//}
	}

	warn("finished reading: %zu %zu", ov.Offset, file_size);

	return out;
}

arr<byte> addFile(const TCHAR *fname) {
	HANDLE fp = CreateFile(
		fname,
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		nullptr
	);

	if (fp == INVALID_HANDLE_VALUE) {
		err("could not open file %s: %u", fname, GetLastError());
		return {};
	}

	HANDLE event = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	if (event == nullptr) {
		err("could not create event %u", GetLastError());
		return {};
	}

	auto start = timer::now();
	arr<byte> out = asTestEnd(event, fp);
	auto end = timer::now();

	info("diff: %zums", ms(end - start));

	CloseHandle(fp);
	CloseHandle(event);

	info("checking file");
	bool is_good = true;
	u64 *buf = (u64 *)out.buf;
	usize len = out.len / sizeof(u64);

	for (usize i = 0; i < len; ++i) {
		if (buf[i] != i) {
			warn("incorrect at i %u", i);
			is_good = false;
			break;
		}
	}

	if (is_good) info("all good fam");

	return out;

#if 0
	asFile file;

	file.handle = CreateFile(
		fname, //TEXT("lost_empire-Alpha.png"),
		GENERIC_READ,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		nullptr
	);

	if (file.handle == INVALID_HANDLE_VALUE) {
		err("could not open %s: %u", fname, GetLastError());
		return;
	}

	file.data.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	byte buf[1024];

	if (!ReadFile(file.handle, buf, sizeof(buf), nullptr, &file.data)) {
		ulong error = GetLastError();
		err("could not read file: %u, %s", error, error == ERROR_IO_PENDING ? "io pending" : "other");
	}

	ulong bytes = 0;
	BOOL result = GetOverlappedResult(
		file.handle,
		&file.data,
		&bytes,
		FALSE
	);

	info("result: %s, bytes: %u", result ? "true":"false", bytes);
#endif
}

int main(int argc, char* argv[]) {
	Arena log_arena = Arena::make(gb(1));
	trace::init(log_arena);

	addFile(TEXT("assets/test.bin"));
	
#if 0
	const char *asset_folder = "assets";
	if (argc > 1) {
		asset_folder = argv[1];
	}
	else {
		info("no asset folder provided, using default");
	}

	info("asset folder: (%s)", asset_folder);

	fs::setBaseFolder(asset_folder);

	Engine engine;
	g_engine = &engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();
#endif

	return 0;
}
