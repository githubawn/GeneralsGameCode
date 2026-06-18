// TheSuperHackers @build bobtista 13/06/2026 <winreg.h> shim. The Windows
// registry has no equivalent on Android/Unix. Rather than fail to compile, the
// Reg* calls are provided as inline stubs: reads report "not found" so callers
// fall back to their built-in defaults, and writes are accepted as no-ops. This
// keeps WWDownload/registry.cpp and other registry users building and linking.
// A real virtual-registry backend can replace these stubs later.
#pragma once

#include <windows.h>
#include <winerror.h>

#ifndef PHKEY
typedef HKEY *PHKEY;
#endif
#ifndef REGSAM
typedef DWORD REGSAM;
#endif

// Predefined root keys (values mirror the Win32 constants).
#define HKEY_CLASSES_ROOT       ((HKEY)(uintptr_t)0x80000000)
#define HKEY_CURRENT_USER       ((HKEY)(uintptr_t)0x80000001)
#define HKEY_LOCAL_MACHINE      ((HKEY)(uintptr_t)0x80000002)
#define HKEY_USERS              ((HKEY)(uintptr_t)0x80000003)

// Access rights.
#define KEY_QUERY_VALUE         0x0001
#define KEY_SET_VALUE           0x0002
#define KEY_READ                0x20019
#define KEY_WRITE               0x20006
#define KEY_ALL_ACCESS          0xF003F

// Value types.
#define REG_NONE                0
#define REG_SZ                  1
#define REG_EXPAND_SZ           2
#define REG_BINARY              3
#define REG_DWORD               4

// Create options.
#define REG_OPTION_NON_VOLATILE 0x00000000
#define REG_OPTION_VOLATILE     0x00000001
#define REG_CREATED_NEW_KEY     0x00000001
#define REG_OPENED_EXISTING_KEY 0x00000002

#ifdef __cplusplus
extern "C" {
#endif

static inline LONG RegOpenKeyExA(HKEY, LPCSTR, DWORD, REGSAM, PHKEY result)
{
    if (result) *result = (HKEY)0;
    return ERROR_FILE_NOT_FOUND;
}

static inline LONG RegCreateKeyExA(HKEY, LPCSTR, DWORD, LPSTR, DWORD, REGSAM,
                                   void *, PHKEY result, LPDWORD disposition)
{
    if (result) *result = (HKEY)0;
    if (disposition) *disposition = REG_OPENED_EXISTING_KEY;
    return ERROR_FILE_NOT_FOUND;
}

static inline LONG RegQueryValueExA(HKEY, LPCSTR, LPDWORD, LPDWORD,
                                    LPBYTE, LPDWORD)
{
    return ERROR_FILE_NOT_FOUND;
}

static inline LONG RegSetValueExA(HKEY, LPCSTR, DWORD, DWORD,
                                  const BYTE *, DWORD)
{
    return ERROR_SUCCESS; // accept writes as no-ops
}

static inline LONG RegDeleteValueA(HKEY, LPCSTR) { return ERROR_SUCCESS; }
static inline LONG RegDeleteKeyA(HKEY, LPCSTR)   { return ERROR_SUCCESS; }
static inline LONG RegCloseKey(HKEY)             { return ERROR_SUCCESS; }

#ifdef __cplusplus
}
#endif

// ANSI aliases (the engine builds without UNICODE).
#define RegOpenKeyEx    RegOpenKeyExA
#define RegCreateKeyEx  RegCreateKeyExA
#define RegQueryValueEx RegQueryValueExA
#define RegSetValueEx   RegSetValueExA
#define RegDeleteValue  RegDeleteValueA
#define RegDeleteKey    RegDeleteKeyA
