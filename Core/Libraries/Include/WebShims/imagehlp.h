#pragma once
// Stub imagehlp.h for web build
#include "WebWin32.h"

typedef struct _IMAGEHLP_SYMBOL {
    DWORD SizeOfStruct;
    DWORD Address;
    DWORD Size;
    DWORD Flags;
    DWORD MaxNameLength;
    char Name[1];
} IMAGEHLP_SYMBOL, *PIMAGEHLP_SYMBOL;

typedef struct _IMAGEHLP_LINE {
    DWORD SizeOfStruct;
    void* Key;
    DWORD LineNumber;
    char* FileName;
    DWORD Address;
} IMAGEHLP_LINE, *PIMAGEHLP_LINE;

typedef enum _ADDRESS_MODE {
    AddrMode1616,
    AddrMode1632,
    AddrModeReal,
    AddrModeFlat
} ADDRESS_MODE;

typedef struct _ADDRESS {
    DWORD          Offset;
    WORD           Segment;
    ADDRESS_MODE   Mode;
} ADDRESS, *LPADDRESS;

typedef struct _STACKFRAME {
    ADDRESS AddrPC;
    ADDRESS AddrReturn;
    ADDRESS AddrFrame;
    ADDRESS AddrStack;
    ADDRESS AddrBStore;
    PVOID   FuncTableEntry;
    DWORD   Params[4];
    BOOL    Far;
    BOOL    Virtual;
    DWORD   Reserved[3];
    PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine;
    PFUNCTION_TABLE_ACCESS_ROUTINE FunctionTableAccessRoutine;
    PGET_MODULE_BASE_ROUTINE GetModuleBaseRoutine;
    PTRANSLATE_ADDRESS_ROUTINE TranslateAddressRoutine;
} STACKFRAME, *LPSTACKFRAME;

inline BOOL StackWalk(DWORD MachineType, HANDLE hProcess, HANDLE hThread, LPSTACKFRAME StackFrame, PVOID ContextRecord, PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine, PFUNCTION_TABLE_ACCESS_ROUTINE FunctionTableAccessRoutine, PGET_MODULE_BASE_ROUTINE GetModuleBaseRoutine, PTRANSLATE_ADDRESS_ROUTINE TranslateAddressRoutine) { return FALSE; }
inline PVOID SymFunctionTableAccess(HANDLE hProcess, DWORD AddrBase) { return NULL; }
inline DWORD SymGetModuleBase(HANDLE hProcess, DWORD dwAddr) { return 0; }
inline BOOL SymGetSymFromAddr(HANDLE hProcess, DWORD dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_SYMBOL Symbol) { return FALSE; }
inline BOOL SymGetLineFromAddr(HANDLE hProcess, DWORD dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE Line) { return FALSE; }
inline BOOL SymInitialize(HANDLE hProcess, LPCSTR UserSearchPath, BOOL fInvadeProcess) { return TRUE; }
inline BOOL SymCleanup(HANDLE hProcess) { return TRUE; }
inline DWORD SymSetOptions(DWORD SymOptions) { return 0; }
inline DWORD SymGetOptions() { return 0; }

#define SYMOPT_LOAD_LINES        0x00000010
#define SYMOPT_UNDNAME           0x00000002

typedef BOOL (CALLBACK *PREAD_PROCESS_MEMORY_ROUTINE)(HANDLE, DWORD, LPVOID, DWORD, LPDWORD);
typedef LPVOID (CALLBACK *PFUNCTION_TABLE_ACCESS_ROUTINE)(HANDLE, DWORD);
typedef DWORD (CALLBACK *PGET_MODULE_BASE_ROUTINE)(HANDLE, DWORD);
typedef DWORD (CALLBACK *PTRANSLATE_ADDRESS_ROUTINE)(HANDLE, HANDLE, LPVOID);

typedef enum _MINIDUMP_TYPE {
    MiniDumpNormal = 0x00000000,
} MINIDUMP_TYPE;

typedef struct _MINIDUMP_EXCEPTION_INFORMATION {
    DWORD ThreadId;
    PVOID ExceptionPointers;
    BOOL ClientPointers;
} MINIDUMP_EXCEPTION_INFORMATION, *PMINIDUMP_EXCEPTION_INFORMATION;

typedef struct _MINIDUMP_USER_STREAM_INFORMATION {
    ULONG UserStreamCount;
    PVOID UserStreamArray;
} MINIDUMP_USER_STREAM_INFORMATION, *PMINIDUMP_USER_STREAM_INFORMATION;

typedef struct _MINIDUMP_CALLBACK_INFORMATION {
    PVOID CallbackRoutine;
    PVOID CallbackParam;
} MINIDUMP_CALLBACK_INFORMATION, * PMINIDUMP_CALLBACK_INFORMATION;

typedef struct _IMAGEHLP_MODULE {
    DWORD SizeOfStruct;
    DWORD BaseOfImage;
    DWORD ImageSize;
    DWORD TimeDateStamp;
    DWORD CheckSum;
    DWORD NumSyms;
    int SymType;
    char ModuleName[32];
    char ImageName[256];
    char LoadedImageName[256];
} IMAGEHLP_MODULE, *PIMAGEHLP_MODULE;

#ifndef LPDWORD
typedef DWORD* LPDWORD;
#endif
#ifndef PDWORD
typedef DWORD* PDWORD;
#endif
