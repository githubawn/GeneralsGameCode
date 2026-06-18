// TheSuperHackers @build bobtista 13/06/2026 Minimal <tchar.h> shim. The engine
// builds ANSI (the wide/TCHAR paths are Windows-only), so TCHAR maps to char and
// the _t* routines map to the narrow CRT functions.
#pragma once

#include <string.h>
#include <stdio.h>
#include <windows.h>

#ifndef _TCHAR_DEFINED
#define _TCHAR_DEFINED
typedef char TCHAR;
typedef char _TCHAR;
typedef char *LPTSTR;
typedef const char *LPCTSTR;
typedef char *PTSTR;
typedef const char *PCTSTR;
#endif

#ifndef TEXT
#define TEXT(quote)  quote
#endif
#ifndef _T
#define _T(quote)    quote
#endif
#ifndef _TEXT
#define _TEXT(quote) quote
#endif

#define _tcslen   strlen
#define _tcscpy   strcpy
#define _tcsncpy  strncpy
#define _tcscat   strcat
#define _tcscmp   strcmp
#define _tcsicmp  strcasecmp
#define _tcschr   strchr
#define _tcsrchr  strrchr
#define _tcsstr   strstr
#define _stprintf sprintf
#define _sntprintf snprintf
#define _tprintf  printf
#define _tfopen   fopen
