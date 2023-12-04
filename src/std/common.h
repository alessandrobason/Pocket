#pragma once

// small stdint/stddef alternative
using i8    = signed char;
using i16   = short;
using i32   = int;
using i64   = long long;
using u8    = unsigned char;
using u16   = unsigned short;
using u32   = unsigned int;
using u64   = unsigned long long;
using isize = i64;
using usize = u64;
using uptr  = usize;
using iptr  = isize;
using byte  = u8;

using uchar  = unsigned char;
using ushort = unsigned short;
using uint   = unsigned int;
using ulong  = unsigned long;

#ifdef UNICODE
#define PK_UNICODE 1
#define PK_TEXT(msg) L##msg
using TCHAR = wchar_t;
#else
#define PK_UNICODE 0
#define PK_TEXT(msg) msg
using TCHAR = char;
#endif

#define pk_unused(x)   ((void)(x))
#define pk_arrlen(arr) (sizeof(arr)/sizeof(*(arr)))

#ifdef _WIN32
#define PK_WINDOWS 1
#define PK_POSIX   0
#else
#define PK_WINDOWS 0
#define PK_POSIX   1
#endif

#ifdef _DEBUG
#define PK_DEBUG   1
#define PK_RELEASE 0
#else
#define PK_DEBUG   0
#define PK_RELEASE 1
#endif

#define pk_malloc(sz)        malloc(sz)
#define pk_calloc(sz, count) calloc(sz, count)
#define pk_realloc(ptr, sz)  realloc(ptr, sz)
#define pk_free(ptr)         free(ptr)

#define pk_assert(cond)      assert(cond)

#if PK_WINDOWS
#define pk_cdecl __cdecl
#else
#define pk_stdcall __attribute__((stdcall))
#endif
