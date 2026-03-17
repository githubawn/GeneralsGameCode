#pragma once
#include "WebWin32.h"

// ATL stubs
// GUID, IUnknown, HRESULT types are now provided by WebWin32.h
#define REFCLSID const GUID &

template <class T>
class CComPtr {
public:
    T* p;
    CComPtr() : p(NULL) {}
    CComPtr(T* lp) : p(lp) { if (p) p->AddRef(); }
    ~CComPtr() { if (p) p->Release(); }
    T* operator->() const { return p; }
    operator T*() const { return p; }
    T** operator&() { return &p; }
};

template <class T>
class CComObject : public T {
public:
    static HRESULT CreateInstance(CComObject<T>** pp) {
        *pp = new CComObject<T>();
        return 0;
    }
};

struct IBrowserDispatch : public IUnknown {
    STDMETHOD(TestMethod)(Int num1) = 0;
};

// IID stubs
extern "C" const IID IID_IBrowserDispatch;
