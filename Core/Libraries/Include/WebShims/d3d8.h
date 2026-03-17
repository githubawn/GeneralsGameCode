/*
**	Command & Conquer Generals Zero Hour(tm)
**	D3D8 to WebGL Shim
*/

#ifndef _D3D8_SHIM_H_
#define _D3D8_SHIM_H_

#include "d3d8types.h"
#include "d3d8caps.h"

#define D3D_OK 0

#define D3DCLEAR_TARGET         0x00000001L
#define D3DCLEAR_ZBUFFER        0x00000002L
#define D3DCLEAR_STENCIL        0x00000004L


typedef struct _D3DSURFACE_DESC {
    D3DFORMAT Format;
    int Type;
    DWORD Usage;
    D3DPOOL Pool;
    UINT MultiSampleType;
    UINT Width;
    UINT Height;
} D3DSURFACE_DESC;

typedef struct _D3DVIEWPORT8 {
    DWORD X;
    DWORD Y;
    DWORD Width;
    DWORD Height;
    float MinZ;
    float MaxZ;
} D3DVIEWPORT8;

#define D3DDP_MAXTEXCOORD 8

#define D3DUSAGE_WRITEONLY              0x00000008L
#define D3DUSAGE_SOFTWAREPROCESSING     0x00000010L
#define D3DUSAGE_DONOTCLIP              0x00000020L
#define D3DUSAGE_POINTS                 0x00000040L
#define D3DUSAGE_RTPATCHES              0x00000080L
#define D3DUSAGE_NPATCHES               0x00000100L
#define D3DUSAGE_DYNAMIC                0x00000200L

#define D3DDEVCAPS_NPATCHES             0x01000000L
#define D3DPRASTERCAPS_ZBIAS            0x00004000L

typedef struct _D3DLIGHT8 {
    int Type;
    D3DCOLORVALUE Diffuse;
    D3DCOLORVALUE Specular;
    D3DCOLORVALUE Ambient;
    D3DVECTOR Position;
    D3DVECTOR Direction;
    float Range;
    float Falloff;
    float Attenuation0;
    float Attenuation1;
    float Attenuation2;
    float Theta;
    float Phi;
} D3DLIGHT8;

typedef struct _D3DMATERIAL8 {
    D3DCOLORVALUE Diffuse;
    D3DCOLORVALUE Ambient;
    D3DCOLORVALUE Specular;
    D3DCOLORVALUE Emissive;
    float Power;
} D3DMATERIAL8;

struct IDirect3DSwapChain8 {
    virtual HRESULT Release() = 0;
};

// Interface stubs
struct IDirect3DResource8 {
    virtual unsigned long AddRef() = 0;
    virtual unsigned long Release() = 0;
    virtual HRESULT GetDevice(struct IDirect3DDevice8** ppDevice) = 0;
};

struct IDirect3DBaseTexture8 : IDirect3DResource8 {
    virtual void GenerateMipSubLevels() = 0;
    virtual DWORD GetLevelCount() = 0;
};

struct IDirect3DTexture8 : IDirect3DBaseTexture8 {
    virtual HRESULT LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect, const void* pRect, DWORD Flags) = 0;
    virtual HRESULT UnlockRect(UINT Level) = 0;
    virtual HRESULT GetSurfaceLevel(UINT Level, struct IDirect3DSurface8** ppSurfaceLevel) = 0;
};

struct IDirect3DVolumeTexture8 : IDirect3DBaseTexture8 {
    virtual HRESULT GetLevelDesc(UINT Level, void* pDesc) = 0;
};

struct IDirect3DVertexBuffer8 : IDirect3DResource8 {
    virtual HRESULT Lock(UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD Flags) = 0;
    virtual HRESULT Unlock() = 0;
    virtual HRESULT GetDesc(D3DVERTEXBUFFER_DESC* pDesc) = 0;
};

struct IDirect3DIndexBuffer8 : IDirect3DResource8 {
    virtual HRESULT Lock(UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD Flags) = 0;
    virtual HRESULT Unlock() = 0;
    virtual HRESULT GetDesc(D3DINDEXBUFFER_DESC* pDesc) = 0;
};


typedef struct IDirect3DDevice8 *LPDIRECT3DDEVICE8;
typedef struct IDirect3D8 *LPDIRECT3D8;
typedef struct IDirect3DVertexBuffer8 *LPDIRECT3DVERTEXBUFFER8;
typedef struct IDirect3DIndexBuffer8 *LPDIRECT3DINDEXBUFFER8;
typedef struct IDirect3DTexture8 *LPDIRECT3DTEXTURE8;
typedef struct IDirect3DSurface8 *LPDIRECT3DSURFACE8;

struct IDirect3DSurface8 : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetDevice(struct IDirect3DDevice8** ppDevice) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC* pDesc) = 0;
    virtual HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT* pLockedRect, const void* pRect, DWORD Flags) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnlockRect() = 0;
};

struct IDirect3DDevice8 {
    virtual HRESULT Release() = 0;
    virtual HRESULT BeginScene() = 0;
    virtual HRESULT EndScene() = 0;
    virtual HRESULT Clear(DWORD Count, const void* pRects, DWORD Flags, DWORD Color, float Z, DWORD Stencil) = 0;
    virtual HRESULT SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) = 0;
    virtual HRESULT CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer8** ppVertexBuffer) = 0;
    virtual HRESULT CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer8** ppIndexBuffer) = 0;
    virtual HRESULT CreateVertexShader(const DWORD* pDeclaration, const DWORD* pFunction, DWORD* pHandle, DWORD Usage) = 0;
    virtual HRESULT SetVertexShader(DWORD Handle) = 0;
    virtual HRESULT CreatePixelShader(const DWORD* pFunction, DWORD* pHandle) = 0;
    virtual HRESULT SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer8* pStreamData, UINT Stride) = 0;
    virtual HRESULT SetIndices(IDirect3DIndexBuffer8* pIndexData, UINT BaseVertexIndex) = 0;
    virtual HRESULT DrawIndexedPrimitive(int Type, UINT MinIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount) = 0;
    virtual HRESULT SetTexture(DWORD Stage, IDirect3DBaseTexture8* pTexture) = 0;
    virtual HRESULT SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) = 0;
    virtual HRESULT CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture8** ppTexture) = 0;
    virtual HRESULT GetDeviceCaps(D3DCAPS8* pCaps) = 0;
    virtual HRESULT SetViewport(const D3DVIEWPORT8* pViewport) = 0;
    virtual HRESULT SetMaterial(const D3DMATERIAL8* pMaterial) = 0;
    virtual HRESULT SetLight(DWORD Index, const D3DLIGHT8* pLight) = 0;
    virtual HRESULT LightEnable(DWORD Index, BOOL Enable) = 0;
    virtual HRESULT SetPixelShader(unsigned long Handle) = 0;
    virtual HRESULT SetVertexShaderConstant(unsigned long Register, const void* pConstantData, unsigned long ConstantCount) = 0;
    virtual HRESULT SetPixelShaderConstant(unsigned long Register, const void* pConstantData, unsigned long ConstantCount) = 0;
    virtual HRESULT GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) = 0;
    virtual HRESULT SetClipPlane(unsigned long Index, const float* pPlane) = 0;
    virtual HRESULT SetTextureStageState(unsigned long Stage, D3DTEXTURESTAGESTATETYPE Type, unsigned long Value) = 0;
    virtual HRESULT CopyRects(struct IDirect3DSurface8* pSourceSurface, const RECT* pSourceRectsArray, UINT cRects, struct IDirect3DSurface8* pDestinationSurface, const POINT* pDestPointsArray) = 0;
    virtual HRESULT GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, struct IDirect3DSurface8** ppBackBuffer) = 0;
    virtual HRESULT TestCooperativeLevel() = 0;
    virtual HRESULT Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) = 0;
    virtual HRESULT Present(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const void* pDirtyRegion) = 0;
};

struct IDirect3D8 {
    virtual HRESULT Release() = 0;
    virtual UINT GetAdapterCount() = 0;
    virtual HRESULT GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER8* pIdentifier) = 0;
    virtual UINT GetAdapterModeCount(UINT Adapter) = 0;
    virtual HRESULT EnumAdapterModes(UINT Adapter, UINT Mode, void* pMode) = 0;
    virtual HRESULT GetAdapterDisplayMode(UINT Adapter, void* pMode) = 0;
    virtual HRESULT CheckDeviceType(UINT Adapter, int CheckType, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, BOOL Windowed) = 0;
    virtual HRESULT CheckDeviceFormat(UINT Adapter, int DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, int RType, D3DFORMAT CheckFormat) = 0;
    virtual HRESULT GetDeviceCaps(UINT Adapter, int DeviceType, D3DCAPS8* pCaps) = 0;
    virtual HRESULT CreateDevice(UINT Adapter, int DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface) = 0;
};

inline IDirect3D8* Direct3DCreate8(UINT SDKVersion) { return nullptr; }

#endif
