#pragma once
// Stub d3d8types.h for web build
#include "WebWin32.h"

typedef enum _D3DFORMAT {
    D3DFMT_UNKNOWN = 0,
    D3DFMT_R8G8B8 = 20,
    D3DFMT_A8R8G8B8 = 21,
    D3DFMT_X8R8G8B8 = 22,
    D3DFMT_R5G6B5 = 23,
    D3DFMT_X1R5G5B5 = 24,
    D3DFMT_A1R5G5B5 = 25,
    D3DFMT_A4R4G4B4 = 26,
    D3DFMT_R3G3B2 = 27,
    D3DFMT_A8 = 28,
    D3DFMT_A8P8 = 29,
    D3DFMT_P8 = 30,
    D3DFMT_L8 = 31,
    D3DFMT_A8L8 = 32,
    D3DFMT_A4L4 = 33,
    D3DFMT_V8U8 = 34,
    D3DFMT_L6V5U5 = 35,
    D3DFMT_X8L8V8U8 = 36,
    D3DFMT_Q8W8V8U8 = 37,
    D3DFMT_V16U16 = 38,
    D3DFMT_W11V11U10 = 39,
    D3DFMT_A2W10V10U10 = 40,
    D3DFMT_UYVY = 0x59565955,
    D3DFMT_YUY2 = 0x32595559,
    D3DFMT_DXT1 = 0x31545844,
    D3DFMT_DXT2 = 0x32545844,
    D3DFMT_DXT3 = 0x33545844,
    D3DFMT_DXT4 = 0x34545844,
    D3DFMT_DXT5 = 0x35545844,
    D3DFMT_D16_LOCKABLE = 70,
    D3DFMT_D32 = 71,
    D3DFMT_D15S1 = 73,
    D3DFMT_D24S8 = 75,
    D3DFMT_D24X8 = 77,
    D3DFMT_D24X4S4 = 79,
    D3DFMT_D16 = 80,
    D3DFMT_VERTEXDATA = 100,
    D3DFMT_INDEX16 = 101,
    D3DFMT_INDEX32 = 102,
} D3DFORMAT;

typedef enum _D3DSWAPEFFECT {
    D3DSWAPEFFECT_DISCARD = 1,
    D3DSWAPEFFECT_FLIP = 2,
    D3DSWAPEFFECT_COPY = 3,
    D3DSWAPEFFECT_FORCE_DWORD = 0x7fffffff
} D3DSWAPEFFECT;

typedef enum _D3DBACKBUFFER_TYPE {
    D3DBACKBUFFER_TYPE_MONO = 0,
    D3DBACKBUFFER_TYPE_LEFT = 1,
    D3DBACKBUFFER_TYPE_RIGHT = 2,
    D3DBACKBUFFER_TYPE_FORCE_DWORD = 0x7fffffff
} D3DBACKBUFFER_TYPE;

typedef enum _D3DPOOL {
    D3DPOOL_DEFAULT = 0,
    D3DPOOL_MANAGED = 1,
    D3DPOOL_SYSTEMMEM = 2,
    D3DPOOL_SCRATCH = 3,
    D3DPOOL_FORCE_DWORD = 0x7fffffff
} D3DPOOL;

typedef enum _D3DDEVTYPE {
    D3DDEVTYPE_HAL = 1,
    D3DDEVTYPE_REF = 2,
    D3DDEVTYPE_SW = 3,
    D3DDEVTYPE_FORCE_DWORD = 0x7fffffff
} D3DDEVTYPE;

typedef struct _D3DPRESENT_PARAMETERS {
    UINT BackBufferWidth;
    UINT BackBufferHeight;
    D3DFORMAT BackBufferFormat;
    UINT BackBufferCount;
    UINT MultiSampleType;
    D3DSWAPEFFECT SwapEffect;
    HWND hDeviceWindow;
    BOOL Windowed;
    BOOL EnableAutoDepthStencil;
    D3DFORMAT AutoDepthStencilFormat;
    DWORD Flags;
    UINT FullScreen_RefreshRateInHz;
    UINT FullScreen_PresentationInterval;
} D3DPRESENT_PARAMETERS;

typedef struct _D3DADAPTER_IDENTIFIER8 {
    char Driver[512];
    char Description[512];
    char DeviceName[32];
    GUID DeviceIdentifier;
} D3DADAPTER_IDENTIFIER8;

typedef struct _D3DCOLORVALUE {
    float r;
    float g;
    float b;
    float a;
} D3DCOLORVALUE;

typedef struct _D3DVECTOR {
    float x;
    float y;
    float z;
} D3DVECTOR;

struct ID3DXBuffer : public IUnknown {
    virtual LPVOID STDMETHODCALLTYPE GetBufferPointer() = 0;
    virtual DWORD STDMETHODCALLTYPE GetBufferSize() = 0;
};
typedef struct ID3DXBuffer* LPD3DXBUFFER;

typedef struct _D3DLOCKED_RECT {
    int Pitch;
    void* pBits;
} D3DLOCKED_RECT;

#define D3DLOCK_READONLY            0x00000010L
#define D3DLOCK_DISCARD             0x00002000L
#define D3DLOCK_NOOVERWRITE         0x00001000L
#define D3DLOCK_NOSYSLOCK           0x00000800L

typedef struct _D3DVERTEXBUFFER_DESC {
    D3DFORMAT Format;
    int Type;
    DWORD Usage;
    DWORD Pool;
    UINT Size;
    DWORD FVF;
} D3DVERTEXBUFFER_DESC;

typedef struct _D3DINDEXBUFFER_DESC {
    D3DFORMAT Format;
    int Type;
    DWORD Usage;
    DWORD Pool;
    UINT Size;
} D3DINDEXBUFFER_DESC;

typedef DWORD D3DCOLOR;

typedef struct _D3DMATRIX {
    union {
        struct {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float m[4][4];
    };
    _D3DMATRIX() {}
    _D3DMATRIX(float r00, float r01, float r02, float r03,
               float r10, float r11, float r12, float r13,
               float r20, float r21, float r22, float r23,
               float r30, float r31, float r32, float r33) {
        m[0][0]=r00; m[0][1]=r01; m[0][2]=r02; m[0][3]=r03;
        m[1][0]=r10; m[1][1]=r11; m[1][2]=r12; m[1][3]=r13;
        m[2][0]=r20; m[2][1]=r21; m[2][2]=r22; m[2][3]=r23;
        m[3][0]=r30; m[3][1]=r31; m[3][2]=r32; m[3][3]=r33;
    }
} D3DMATRIX;

struct D3DXMATRIX : public D3DMATRIX {
    D3DXMATRIX() : D3DMATRIX() {}
    D3DXMATRIX(const float* p) {
        for(int i=0; i<4; ++i) for(int j=0; j<4; ++j) m[i][j] = p[i*4+j];
    }
    D3DXMATRIX(float _11, float _12, float _13, float _14,
               float _21, float _22, float _23, float _24,
               float _31, float _32, float _33, float _34,
               float _41, float _42, float _43, float _44)
        : D3DMATRIX(_11, _12, _13, _14, _21, _22, _23, _24, _31, _32, _33, _34, _41, _42, _43, _44) {}
};

// D3DFVF constants
#define D3DFVF_XYZ              0x002
#define D3DFVF_XYZRHW           0x004
#define D3DFVF_XYZB1            0x006
#define D3DFVF_XYZB2            0x008
#define D3DFVF_XYZB3            0x00a
#define D3DFVF_XYZB4            0x00c
#define D3DFVF_XYZB5            0x00e
#define D3DFVF_NORMAL           0x010
#define D3DFVF_PSIZE            0x020
#define D3DFVF_DIFFUSE          0x040
#define D3DFVF_SPECULAR         0x080
#define D3DFVF_TEX1             0x100
#define D3DFVF_TEX2             0x200
#define D3DFVF_TEX3             0x300
#define D3DFVF_TEX4             0x400
#define D3DFVF_TEX5             0x500
#define D3DFVF_TEX6             0x600
#define D3DFVF_TEX7             0x700
#define D3DFVF_TEX8             0x800
#define D3DFVF_LASTBETA_UBYTE4  0x1000
#define D3DFVF_LASTBETA_D3DCOLOR 0x2000

typedef enum _D3DRENDERSTATETYPE {
    D3DRS_ZENABLE = 7,
    D3DRS_FILLMODE = 8,
    D3DRS_SHADEMODE = 9,
    D3DRS_ZWRITEENABLE = 14,
    D3DRS_ALPHATESTENABLE = 15,
    D3DRS_LASTPIXEL = 16,
    D3DRS_SRCBLEND = 19,
    D3DRS_DESTBLEND = 20,
    D3DRS_CULLMODE = 22,
    D3DRS_ZBIAS = 7, // Duplicate of ZENABLE in some docs, but used in code
    D3DRS_FOGCOLOR = 34,
    D3DRS_FOGTABLEMODE = 35,
    D3DRS_FOGSTART = 36,
    D3DRS_FOGEND = 37,
    D3DRS_FOGDENSITY = 38,
    D3DRS_FOGVERTEXMODE = 140,
    D3DRS_AMBIENT = 139,
} D3DRENDERSTATETYPE;

typedef enum _D3DFOGMODE {
    D3DFOG_NONE = 0,
    D3DFOG_EXP = 1,
    D3DFOG_EXP2 = 2,
    D3DFOG_LINEAR = 3,
    D3DFOG_FORCE_DWORD = 0x7fffffff
} D3DFOGMODE;

typedef enum _D3DTRANSFORMSTATETYPE {
    D3DTS_VIEW = 2,
    D3DTS_PROJECTION = 3,
    D3DTS_WORLD = 256,
} D3DTRANSFORMSTATETYPE;

typedef enum _D3DTEXTURESTAGESTATETYPE {
    D3DTSS_COLOROP = 1,
} D3DTEXTURESTAGESTATETYPE;

inline UINT D3DXGetFVFVertexSize(DWORD FVF) {
    UINT size = 0;
    if ((FVF & 0xE) == D3DFVF_XYZ) size += 3 * 4;
    else if ((FVF & 0xE) == D3DFVF_XYZRHW) size += 4 * 4;
    else if ((FVF & 0xE) >= D3DFVF_XYZB1 && (FVF & 0xE) <= D3DFVF_XYZB5) size += (((FVF & 0xE) >> 1) + 2) * 4;
    
    if (FVF & D3DFVF_NORMAL) size += 3 * 4;
    if (FVF & D3DFVF_PSIZE) size += 4;
    if (FVF & D3DFVF_DIFFUSE) size += 4;
    if (FVF & D3DFVF_SPECULAR) size += 4;
    
    UINT texCount = (FVF & 0xF00) >> 8;
    size += texCount * 2 * 4; // Assuming 2 floats per tex coord for simplicity
    
    return size;
}

#define D3DX_PI    ((FLOAT)  3.141592654f)
#define D3DX_16F_EPSILON  ((FLOAT) 4.8828125e-4f)

struct ID3DXMesh : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE DrawSubset(DWORD AttribId) = 0;
    virtual DWORD STDMETHODCALLTYPE GetNumFaces() = 0;
    virtual DWORD STDMETHODCALLTYPE GetNumVertices() = 0;
    virtual DWORD STDMETHODCALLTYPE GetFVF() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeclaration(DWORD* pDeclaration) = 0;
    virtual DWORD STDMETHODCALLTYPE GetOptions() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDevice(struct IDirect3DDevice8** ppDevice) = 0;
    virtual HRESULT STDMETHODCALLTYPE CloneMeshFVF(DWORD Options, DWORD FVF, struct IDirect3DDevice8* pDevice, struct ID3DXMesh** ppCloneMesh) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVertexBuffer(struct IDirect3DVertexBuffer8** ppVB) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIndexBuffer(struct IDirect3DIndexBuffer8** ppIB) = 0;
    virtual HRESULT STDMETHODCALLTYPE LockVertexBuffer(DWORD Flags, LPVOID* ppData) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnlockVertexBuffer() = 0;
    virtual HRESULT STDMETHODCALLTYPE LockIndexBuffer(DWORD Flags, LPVOID* ppData) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnlockIndexBuffer() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAttributeTable(void* pAttribTable, DWORD* pAttribTableSize) = 0;
};
typedef struct ID3DXMesh *LPD3DXMESH;

struct ID3DXSkinExtent : IUnknown {
};
typedef struct ID3DXSkinExtent *LPD3DXSKINEXTENT;
