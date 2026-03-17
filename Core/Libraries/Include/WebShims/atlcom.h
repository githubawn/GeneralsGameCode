#pragma once
#include "atlbase.h"

template <class T>
class CComSingleThreadModel {};

template <class T>
class CComObjectRootEx {
public:
    void AddRef() {}
    void Release() {}
};

template <class T>
class CComCoClass {};

#define BEGIN_COM_MAP(x)
#define COM_INTERFACE_ENTRY(x)
#define COM_INTERFACE_ENTRY_AGGREGATE(x, y)
#define END_COM_MAP()

class CComModule {
public:
    HRESULT Init(void* p, HINSTANCE h, void* p2 = NULL) { return 0; }
    void Term() {}
};
extern CComModule _Module;
