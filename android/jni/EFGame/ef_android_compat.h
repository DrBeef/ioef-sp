/*
===========================================================================
ef_android_compat.h -- force-included (-include) into every Elite-Force-VR TU
when building the Android/Quest game+ui .so with clang/NDK.

The EF/ICARUS C++ was MSVC-only and pulled Win32 types + CRT spellings in
transitively via <windows.h>; that header is gone on Android, so provide the
small set the codebase actually uses (sized correctly for LP64 -- 32-bit DWORD,
NOT unsigned long).  Guarded with ICARUS_COMPAT_TYPES so it agrees with the
matching blocks in icarus/Tokenizer.h + Interface.h regardless of include order
(this header is force-included first, so those blocks no-op).

Desktop MSVC builds never see this file -- it is only added via the Android.mk.
===========================================================================
*/
#ifndef EF_ANDROID_COMPAT_H
#define EF_ANDROID_COMPAT_H

#if defined(__ANDROID__)

#include <strings.h>   /* strcasecmp / strncasecmp */
#include <time.h>      /* clock_gettime (timeGetTime shim) */
#include <stdio.h>     /* FILE* for the Win32 file-API shims */

/* MSVC keywords clang doesn't define on Android. */
#define __cdecl
#define __stdcall
#define __fastcall
#define __forceinline inline
#ifndef __declspec
#define __declspec(x)
#endif

#ifndef ICARUS_COMPAT_TYPES
#define ICARUS_COMPAT_TYPES
typedef const char *   LPCTSTR;
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned int   DWORD;     /* 32-bit on LP64 (Win32 DWORD width) */
typedef unsigned int   UINT;
typedef unsigned int   COLORREF;
typedef void *         HANDLE;
typedef float          FLOAT;
typedef int            INT;
#endif

/* MSVC CRT spellings -> POSIX / engine equivalents. */
#ifndef _stricmp
#define _stricmp  strcasecmp
#endif
#ifndef _strnicmp
#define _strnicmp strncasecmp
#endif
#ifndef stricmp
#define stricmp   strcasecmp
#endif
#ifndef strnicmp
#define strnicmp  strncasecmp
#endif
#ifndef _strcmpi
#define _strcmpi  strcasecmp
#endif
#ifndef strcmpi
#define strcmpi   strcasecmp
#endif
/* _strlwr/_strupr -> the engine's Q_ versions (declared later in q_shared.h;
   these macros only rewrite the call sites). */
#define _strlwr   Q_strlwr
#define _strupr   Q_strupr

/* Win32 RGB macro (COLORREF packing) used by the ICARUS tokenizer. */
#ifndef RGB
#define RGB(r,g,b)  ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#endif

/* Win32 debug/timer shims. */
#define OutputDebugString(s)   ((void)0)
#define OutputDebugStringA(s)  ((void)0)
static inline DWORD ef_timeGetTime(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (DWORD)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#define timeGetTime ef_timeGetTime

/* ---- Win32 file API shims -> stdio, for icarus/Tokenizer.cpp (CParseFile).
   m_fileHandle is HANDLE (void*); we stash a FILE* in it. ------------------- */
#define GENERIC_READ          0
#define FILE_SHARE_READ       0
#define FILE_SHARE_WRITE      0
#define OPEN_EXISTING         0
#define FILE_ATTRIBUTE_NORMAL 0
#define FILE_BEGIN            SEEK_SET
#define FILE_CURRENT          SEEK_CUR
#define FILE_END              SEEK_END
#define INVALID_HANDLE_VALUE  ((HANDLE)-1)

typedef struct { DWORD nLength; void *lpSecurityDescriptor; int bInheritHandle; } SECURITY_ATTRIBUTES;

static inline HANDLE CreateFile(const char *name, DWORD access, DWORD share,
		void *sa, DWORD create, DWORD attr, void *templ) {
	FILE *f = fopen(name, "rb");
	(void)access; (void)share; (void)sa; (void)create; (void)attr; (void)templ;
	return f ? (HANDLE)f : (HANDLE)-1;
}
static inline int CloseHandle(HANDLE h) { return h ? (fclose((FILE *)h) == 0) : 0; }
static inline DWORD SetFilePointer(HANDLE h, long dist, void *distHigh, DWORD method) {
	(void)distHigh;
	fseek((FILE *)h, dist, (int)method);
	return (DWORD)ftell((FILE *)h);
}
static inline int ReadFile(HANDLE h, void *buf, DWORD n, DWORD *read, void *ov) {
	size_t r = fread(buf, 1, n, (FILE *)h);
	(void)ov;
	if (read) *read = (DWORD)r;
	return 1;
}

#endif /* __ANDROID__ */

#endif /* EF_ANDROID_COMPAT_H */
