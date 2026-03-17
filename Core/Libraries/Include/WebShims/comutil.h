#pragma once
#include "WebWin32.h"

class _bstr_t {
public:
    _bstr_t() : m_s(NULL) {}
    _bstr_t(const char* s) : m_s(s ? strdup(s) : NULL) {}
    _bstr_t(const wchar_t* s) : m_s(NULL) {} // Stub
    ~_bstr_t() { if (m_s) free(m_s); }
    operator const char*() const { return m_s; }
    operator const wchar_t*() const { return NULL; } // Stub
private:
    char* m_s;
};

#include "oleauto.h"
