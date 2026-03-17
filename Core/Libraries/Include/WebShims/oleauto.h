#pragma once
#include "WebWin32.h"

struct ITypeLib : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoOfGuid(REFGUID guid, struct ITypeInfo** ppTInfo) = 0;
};

struct ITypeInfo : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetTypeAttr(void** ppTypeAttr) = 0;
    virtual void STDMETHODCALLTYPE ReleaseTypeAttr(void* pTypeAttr) = 0;
};

typedef struct ITypeLib* LPTYPELIB;
typedef struct ITypeInfo* LPTYPEINFO;

#ifdef __cplusplus
extern "C" {
#endif
HRESULT LoadTypeLib(LPCWSTR szFile, ITypeLib** pptlib);
#ifdef __cplusplus
}
#endif

inline HRESULT LoadTypeLib(LPCWSTR szFile, ITypeLib** pptlib) {
    if (pptlib) *pptlib = NULL;
    return 0;
}

inline HRESULT CreateStdDispatch(IUnknown* pUnkOuter, void* pv, ITypeInfo* ptinfo, IDispatch** ppunkStdDisp) {
    if (ppunkStdDisp) *ppunkStdDisp = NULL;
    return 0;
}
