#pragma once

#ifdef __EMSCRIPTEN__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include <SDL2/SDL.h>
#ifndef __min
#define __min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef __max
#define __max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#define ZeroMemory(Destination,Length) memset((Destination),0,(Length))
#define CopyMemory(Destination,Source,Length) memcpy((Destination),(Source),(Length))

// MSVC specific keywords
#define __forceinline inline __attribute__((always_inline))
#define __cdecl
#define __stdcall
#define __fastcall

// String functions
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _wcsicmp wcscasecmp
#define _wcsnicmp wcsncasecmp
#define _strdup strdup

// Basic Win32 Types
#ifndef _WIN32_TYPES_
#define _WIN32_TYPES_
typedef unsigned long DWORD; 
typedef uint16_t WORD;
typedef uint8_t BYTE;
typedef BYTE* LPBYTE;
typedef int16_t SHORT;
typedef long LONG;
#define _LONG_DEFINED_
typedef uint32_t UINT;
typedef int BOOL;
#ifndef _HRESULT_DEFINED
#define _HRESULT_DEFINED
typedef long HRESULT;
#endif
typedef char CHAR;
typedef unsigned char UCHAR;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef void* PVOID;
typedef void* HANDLE;
typedef HANDLE HWND;

typedef struct _GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID;

typedef GUID CLSID;
typedef GUID IID;

#define TRUE 1
#define FALSE 0
#define CALLBACK
#define WINAPI
#define APIENTRY WINAPI
#ifdef __cplusplus
#define STDMETHOD(m) virtual HRESULT STDMETHODCALLTYPE m
#define STDMETHOD_(t, m) virtual t STDMETHODCALLTYPE m
#define PURE = 0
#define IUNKNOWN_NOEXCEPT noexcept
#else
#define STDMETHOD(m) HRESULT m
#define STDMETHOD_(t, m) t m
#define PURE
#define IUNKNOWN_NOEXCEPT
#endif
#define STDMETHODCALLTYPE

#define VK_LBUTTON        0x01
#define VK_RBUTTON        0x02
#define VK_CANCEL         0x03
#define VK_MBUTTON        0x04
#define VK_BACK           0x08
#define VK_TAB            0x09
#define VK_CLEAR          0x0C
#define VK_RETURN         0x0D
#define VK_SHIFT          0x10
#define VK_CONTROL        0x11
#define VK_MENU           0x12
#define VK_PAUSE          0x13
#define VK_CAPITAL        0x14
#define VK_ESCAPE         0x1B
#define VK_SPACE          0x20
#define VK_PRIOR          0x21
#define VK_NEXT           0x22
#define VK_END            0x23
#define VK_HOME           0x24
#define VK_LEFT           0x25
#define VK_UP             0x26
#define VK_RIGHT          0x27
#define VK_DOWN           0x28
#define VK_SELECT         0x29
#define VK_PRINT          0x2A
#define VK_EXECUTE        0x2B
#define VK_SNAPSHOT       0x2C
#define VK_INSERT         0x2D
#define VK_DELETE         0x2E
#define VK_HELP           0x2F
#define VK_0              0x30
#define VK_1              0x31
#define VK_2              0x32
#define VK_3              0x33
#define VK_4              0x34
#define VK_5              0x35
#define VK_6              0x36
#define VK_7              0x37
#define VK_8              0x38
#define VK_9              0x39
#define VK_A              0x41
#define VK_B              0x42
#define VK_C              0x43
#define VK_D              0x44
#define VK_E              0x45
#define VK_F              0x46
#define VK_G              0x47
#define VK_H              0x48
#define VK_I              0x49
#define VK_J              0x4A
#define VK_K              0x4B
#define VK_L              0x4C
#define VK_M              0x4D
#define VK_N              0x4E
#define VK_O              0x4F
#define VK_P              0x50
#define VK_Q              0x51
#define VK_R              0x52
#define VK_S              0x53
#define VK_T              0x54
#define VK_U              0x55
#define VK_V              0x56
#define VK_W              0x57
#define VK_X              0x58
#define VK_Y              0x59
#define VK_Z              0x5A
#define VK_LWIN           0x5B
#define VK_RWIN           0x5C
#define VK_APPS           0x5D
#define VK_SLEEP          0x5F
#define VK_NUMPAD0        0x60
#define VK_NUMPAD1        0x61
#define VK_NUMPAD2        0x62
#define VK_NUMPAD3        0x63
#define VK_NUMPAD4        0x64
#define VK_NUMPAD5        0x65
#define VK_NUMPAD6        0x66
#define VK_NUMPAD7        0x67
#define VK_NUMPAD8        0x68
#define VK_NUMPAD9        0x69
#define VK_MULTIPLY       0x6A
#define VK_ADD            0x6B
#define VK_SEPARATOR      0x6C
#define VK_SUBTRACT       0x6D
#define VK_DECIMAL        0x6E
#define VK_DIVIDE         0x6F
#define VK_F1             0x70
#define VK_F2             0x71
#define VK_F3             0x72
#define VK_F4             0x73
#define VK_F5             0x74
#define VK_F6             0x75
#define VK_F7             0x76
#define VK_F8             0x77
#define VK_F9             0x78
#define VK_F10            0x79
#define VK_F11            0x7A
#define VK_F12            0x7B
#define VK_NUMLOCK        0x90
#define VK_SCROLL         0x91
#define VK_LSHIFT         0xA0
#define VK_RSHIFT         0xA1
#define VK_LCONTROL       0xA2
#define VK_RCONTROL       0xA3
#define VK_LMENU          0xA4
#define VK_RMENU          0xA5

typedef HANDLE HINSTANCE;
typedef HANDLE HMODULE;
typedef const char* LPCSTR;
typedef char* LPSTR;
typedef DWORD* LPDWORD;
typedef DWORD* PDWORD;
typedef unsigned long ULONG;
typedef ULONG* PULONG;

#ifdef __cplusplus
typedef const GUID& REFGUID;
typedef const IID& REFIID;

struct IUnknown {
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
    virtual ULONG STDMETHODCALLTYPE Release() = 0;
};

struct IDispatch : public IUnknown {
};
#endif

typedef struct tagRECT {
    long left;
    long top;
    long right;
    long bottom;
} RECT, *PRECT, *LPRECT;

typedef struct tagPOINT {
    long x;
    long y;
} POINT, *PPOINT, *LPPOINT;

#define MAX_PATH 260
#define CONST const

typedef struct _SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

#define _S_IFDIR 0040000

inline UINT GetSystemDirectoryA(LPSTR lpBuffer, UINT uSize) { return 0; }
inline HMODULE LoadLibraryA(LPCSTR lpLibFileName) { return (HMODULE)0; }
inline LPVOID GetProcAddress(HMODULE hModule, LPCSTR lpProcName) { return (LPVOID)0; }
inline BOOL FreeLibrary(HMODULE hModule) { return TRUE; }
inline LPCSTR GetCommandLineA() { return ""; }
inline DWORD GetModuleFileNameA(HANDLE hModule, LPSTR lpFilename, DWORD nSize) { if (nSize > 0) lpFilename[0] = 0; return 0; }
#define GetModuleFileName GetModuleFileNameA
inline char* lstrcpynA(char* d, const char* s, int n) { if (n > 0) { strncpy(d, s, n); d[n - 1] = 0; } return d; }
#define lstrcpyn lstrcpynA
typedef HANDLE HDC;
typedef HANDLE HFONT;
typedef HANDLE HBITMAP;
typedef HANDLE HGDIOBJ;
typedef const wchar_t* LPCWSTR;
typedef wchar_t* LPWSTR;
typedef uint32_t COLORREF;
typedef float FLOAT;
typedef int32_t INT;
typedef uint32_t UnsignedInt;
typedef int32_t Int;
typedef uint16_t UnsignedShort;
typedef int16_t Short;
typedef uint8_t UnsignedByte;
typedef char Byte; // Match BaseTypeCore.h
typedef uintptr_t UINT_PTR;
typedef intptr_t INT_PTR;

// HRESULT already defined above
#endif

// HRESULT macros
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define MAKE_HRESULT(sev,fac,code) \
    ((HRESULT) (((unsigned long)(sev)<<31) | ((unsigned long)(fac)<<16) | ((unsigned long)(code))) )
#define SEVERITY_ERROR      1
#define FACILITY_ITF        4

// 64-bit types
#define __int64 long long
#define _int64 long long
#define unsigned__int64 unsigned long long
#define unsigned___int64 unsigned long long



#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif

#ifndef DEBUG_ASSERTCRASH
#define DEBUG_ASSERTCRASH(x, y)
#endif
#ifndef DEBUG_LOG
#define DEBUG_LOG(x)
#endif
#ifndef timeGetTime
#define _TIMEGETTIME_DEFINED_
inline unsigned int timeGetTime() {
    return (unsigned int)SDL_GetTicks();
}
#endif

#ifndef OutputDebugString
inline void OutputDebugString(LPCSTR lpOutputString) {
    if (lpOutputString) printf("%s", lpOutputString);
}
#endif

// CRT stubs
#ifndef MulDiv
inline int MulDiv(int nNumber, int nNumerator, int nDenominator) {
    if (nDenominator == 0) return -1;
    return (int)(((int64_t)nNumber * nNumerator) / nDenominator);
}
#endif

#ifndef _splitpath
inline void _splitpath(const char* path, char* drive, char* dir, char* fname, char* ext) {
    // Very basic stub
    if (drive) drive[0] = '\0';
    if (dir) dir[0] = '\0';
    if (fname) fname[0] = '\0';
    if (ext) ext[0] = '\0';
}
#endif

#include <sys/stat.h>
#ifndef _stat
#define _stat stat
#endif

#ifndef _S_IWRITE
#define _S_IWRITE S_IWUSR
#endif
#ifndef _S_IREAD
#define _S_IREAD S_IRUSR
#endif
#define _chmod(f, m) chmod(f, m)

#define FILE_ATTRIBUTE_DIRECTORY 0x00000010

#include <stdarg.h>
inline int _vsnprintf(char* buffer, size_t count, const char* format, va_list argptr) { return vsnprintf(buffer, count, format, argptr); }
inline int _vsnwprintf(wchar_t* buffer, size_t count, const wchar_t* format, va_list argptr) { return vswprintf(buffer, count, format, argptr); }
inline wchar_t* _wsetlocale(int category, const wchar_t* locale) { return NULL; }

// Memory
#define GMEM_FIXED 0
inline LPVOID GlobalAlloc(UINT uFlags, size_t dwBytes) {
    void* p = malloc(dwBytes + sizeof(size_t));
    if (p) {
        *(size_t*)p = dwBytes;
        return (char*)p + sizeof(size_t);
    }
    return NULL;
}

inline size_t GlobalSize(LPVOID hMem) {
    if (hMem) {
        return *(size_t*)((char*)hMem - sizeof(size_t));
    }
    return 0;
}

inline void GlobalFree(LPVOID hMem) {
    if (hMem) {
        free((char*)hMem - sizeof(size_t));
    }
}

// Critical Sections
typedef struct {
} CRITICAL_SECTION;

inline void InitializeCriticalSection(CRITICAL_SECTION* lpCriticalSection) {}
inline void EnterCriticalSection(CRITICAL_SECTION* lpCriticalSection) {}
inline void LeaveCriticalSection(CRITICAL_SECTION* lpCriticalSection) {}
inline void DeleteCriticalSection(CRITICAL_SECTION* lpCriticalSection) {}

// Registry Stubs
typedef void* HKEY;
#define HKEY_LOCAL_MACHINE ((HKEY)(uintptr_t)0x80000002)
#define HKEY_CURRENT_USER  ((HKEY)(uintptr_t)0x80000001)
#define KEY_READ 0
#define KEY_WRITE 0
#define ERROR_SUCCESS 0
#define REG_OPTION_NON_VOLATILE 0
#define REG_SZ 1
#define REG_DWORD 4

inline LONG RegOpenKeyEx(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, DWORD samDesired, HKEY* phkResult) { return 1; }
inline LONG RegCreateKeyEx(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions, DWORD samDesired, void* lpSecurityAttributes, HKEY* phkResult, DWORD* lpdwDisposition) { return 1; }
inline LONG RegCloseKey(HKEY hKey) { return 0; }
inline LONG RegQueryValueEx(HKEY hKey, LPCSTR lpValueName, DWORD* lpReserved, DWORD* lpType, LPBYTE lpData, DWORD* lpcbData) { return 1; }
inline LONG RegSetValueEx(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData) { return 0; }

// Errors
#define D3D_OK 0
#define S_OK 0
#define E_FAIL (int32_t)0x80004005L

// Sockets / Networking
typedef int SOCKET;
#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)

#ifndef closesocket
#define closesocket close
#endif

#ifndef ioctlsocket
#define ioctlsocket ioctl
#endif

#define FIONBIO 0x8004667e // Standard value

#include <errno.h>

#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAEINVAL      EINVAL
#define WSAEALREADY    EALREADY
#define WSAEISCONN     EISCONN
#define WSAECONNRESET  ECONNRESET
#define WSAENOTCONN    ENOTCONN

inline int WSAGetLastError() { return errno; }

typedef DWORD (WINAPI *LPTHREAD_START_ROUTINE)(LPVOID lpThreadParameter);
inline HANDLE CreateThread(void* lpThreadAttributes, size_t dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, void* lpParameter, DWORD dwCreationFlags, DWORD* lpThreadId) {
    if (lpThreadId) *lpThreadId = 0;
    return (HANDLE)1;
}

inline BOOL CreateDirectory(LPCSTR lpPathName, void* lpSecurityAttributes) { return TRUE; }

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    } DUMMYSTRUCTNAME;
    struct {
        DWORD LowPart;
        LONG HighPart;
    } u;
    long long QuadPart;
} LARGE_INTEGER;

inline BOOL QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount) {
    if (lpPerformanceCount) lpPerformanceCount->QuadPart = (long long)SDL_GetTicks();
    return TRUE;
}

inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency) {
    if (lpFrequency) lpFrequency->QuadPart = 1000;
    return TRUE;
}

inline int MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) { return 0; }
inline int MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) { return 0; }
#ifdef UNICODE
#define MessageBox MessageBoxW
#else
#define MessageBox MessageBoxA
#endif
inline void OutputDebugStringA(LPCSTR lpOutputString) {}
inline void OutputDebugStringW(LPCWSTR lpOutputString) {}
#ifdef UNICODE
#define OutputDebugString OutputDebugStringW
#else
#define OutputDebugString OutputDebugStringA
#endif

// COM stubs
typedef struct _VARIANT {
    void* pDispVal;
} VARIANT;

// Exception codes
#define EXCEPTION_ACCESS_VIOLATION          ((DWORD)0xC0000005L)
#define EXCEPTION_DATATYPE_MISALIGNMENT     ((DWORD)0x80000002L)
#define EXCEPTION_BREAKPOINT                ((DWORD)0x80000003L)
#define EXCEPTION_SINGLE_STEP               ((DWORD)0x80000004L)
#define EXCEPTION_ARRAY_BOUNDS_EXCEEDED     ((DWORD)0xC000008CL)
#define EXCEPTION_FLT_DENORMAL_OPERAND      ((DWORD)0xC000008DL)
#define EXCEPTION_FLT_DIVIDE_BY_ZERO        ((DWORD)0xC000008EL)
#define EXCEPTION_FLT_INEXACT_RESULT        ((DWORD)0xC000008FL)
#define EXCEPTION_FLT_INVALID_OPERATION     ((DWORD)0xC0000090L)
#define EXCEPTION_FLT_OVERFLOW              ((DWORD)0xC0000091L)
#define EXCEPTION_FLT_STACK_CHECK           ((DWORD)0xC0000092L)
#define EXCEPTION_FLT_UNDERFLOW             ((DWORD)0xC0000093L)
#define EXCEPTION_INT_DIVIDE_BY_ZERO        ((DWORD)0xC0000094L)
#define EXCEPTION_INT_OVERFLOW              ((DWORD)0xC0000095L)
#define EXCEPTION_PRIV_INSTRUCTION          ((DWORD)0xC0000096L)
#define EXCEPTION_IN_PAGE_ERROR             ((DWORD)0xC0000006L)
#define EXCEPTION_ILLEGAL_INSTRUCTION       ((DWORD)0xC000001DL)
#define EXCEPTION_NONCONTINUABLE_EXCEPTION  ((DWORD)0xC0000025L)
#define EXCEPTION_STACK_OVERFLOW            ((DWORD)0xC00000FDL)
#define EXCEPTION_INVALID_DISPOSITION       ((DWORD)0xC0000026L)
#define EXCEPTION_GUARD_PAGE                ((DWORD)0x80000001L)
#define EXCEPTION_INVALID_HANDLE            ((DWORD)0xC0000008L)

// Shell folders
#define CSIDL_PERSONAL      0x0005
#define CSIDL_APPDATA       0x001a
#define CSIDL_LOCAL_APPDATA 0x001c
#define SHGFP_TYPE_CURRENT  0

inline HRESULT SHGetFolderPathA(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPSTR pszPath) {
    if (pszPath) strcpy(pszPath, "/");
    return 0;
}

inline BOOL SHGetSpecialFolderPathA(HWND hwnd, LPSTR pszPath, int csidl, BOOL fCreate) {
    if (pszPath) strcpy(pszPath, "/");
    return TRUE;
}
#define SHGetSpecialFolderPath SHGetSpecialFolderPathA

inline UINT GetDoubleClickTime() {
    return 500;
}

#endif
