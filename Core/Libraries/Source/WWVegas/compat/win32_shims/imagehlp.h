#pragma once

// TheSuperHackers @build bobtista 13/06/2026 — imagehlp.h shim for non-Windows
// platforms (Android, macOS, Linux). The crash-dump / stack-walk APIs are
// Windows-only; DbgHelpLoader gates all real usage on WIN32 at runtime via
// dynamic DLL loading. On non-Windows we just need the type stubs so the
// header compiles; none of the APIs are actually called.

#ifndef _WIN32

#include "windows.h"

// STACKFRAME — used by StackWalk() to walk call stacks.
typedef struct _STACKFRAME {
    // Simplified stub; fields are never written/read on non-Windows.
    void *AddrPC;
    void *AddrReturn;
    void *AddrFrame;
    void *AddrStack;
    void *FuncTableEntry;
    DWORD Params[4];
    BOOL  Far;
    BOOL  Virtual;
    DWORD Reserved[3];
    void *KdHelp;
    WORD  AddrBStore;
} STACKFRAME, *LPSTACKFRAME;

// IMAGEHLP_SYMBOL stub
#ifndef MAX_SYM_NAME
#define MAX_SYM_NAME 2000
#endif
typedef struct _IMAGEHLP_SYMBOL {
    DWORD SizeOfStruct;
    DWORD Address;
    DWORD Size;
    DWORD Flags;
    DWORD MaxNameLength;
    CHAR  Name[MAX_SYM_NAME + 1];
} IMAGEHLP_SYMBOL, *PIMAGEHLP_SYMBOL;

// IMAGEHLP_LINE stub
typedef struct _IMAGEHLP_LINE {
    DWORD SizeOfStruct;
    PVOID Key;
    DWORD LineNumber;
    PSTR  FileName;
    DWORD Address;
} IMAGEHLP_LINE, *PIMAGEHLP_LINE;

// Callback typedefs expected by StackWalk
typedef BOOL (CALLBACK *PREAD_PROCESS_MEMORY_ROUTINE)(HANDLE, LPCVOID, PVOID, DWORD, PDWORD);
typedef PVOID (CALLBACK *PFUNCTION_TABLE_ACCESS_ROUTINE)(HANDLE, DWORD);
typedef DWORD (CALLBACK *PGET_MODULE_BASE_ROUTINE)(HANDLE, DWORD);
typedef DWORD (CALLBACK *PTRANSLATE_ADDRESS_ROUTINE)(HANDLE, HANDLE, void*);

#endif // !_WIN32
