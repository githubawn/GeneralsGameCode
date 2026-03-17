#pragma once
// Stub d3dx8math.h for web build
#include "d3d8types.h"
#include <math.h>

typedef struct _D3DXVECTOR3 {
    float x, y, z;
} D3DXVECTOR3, *LPD3DXVECTOR3;

#ifndef D3DXMATRIX_DEFINED
#define D3DXMATRIX_DEFINED
// D3DXMATRIX is defined in d3d8types.h (via include above)
#endif

typedef struct _D3DXVECTOR4 {
    float x, y, z, w;
    _D3DXVECTOR4() {}
    _D3DXVECTOR4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
} D3DXVECTOR4, *LPD3DXVECTOR4;

inline D3DXVECTOR4* D3DXVec4Transform(D3DXVECTOR4* pOut, const D3DXVECTOR4* pV, const D3DMATRIX* pM) { return pOut; }
inline float D3DXVec4Dot(const D3DXVECTOR4* pV1, const D3DXVECTOR4* pV2) { return 0.0f; }

typedef struct _D3DXQUATERNION {
    float x, y, z, w;
} D3DXQUATERNION, *LPD3DXQUATERNION;

typedef struct _D3DXPLANE {
    float a, b, c, d;
} D3DXPLANE, *LPD3DXPLANE;

inline D3DXMATRIX* D3DXMatrixMultiply(D3DXMATRIX* pOut, const D3DXMATRIX* pM1, const D3DXMATRIX* pM2) { return pOut; }
inline D3DXMATRIX* D3DXMatrixIdentity(D3DXMATRIX* pOut) { return pOut; }
inline D3DXMATRIX* D3DXMatrixInverse(D3DXMATRIX* pOut, float* pDeterminant, const D3DXMATRIX* pM) { return pOut; }
inline D3DXMATRIX* D3DXMatrixTranspose(D3DXMATRIX* pOut, const D3DXMATRIX* pM) { return pOut; }
inline D3DXMATRIX* D3DXMatrixRotationX(D3DXMATRIX* pOut, float Angle) { return pOut; }
inline D3DXMATRIX* D3DXMatrixRotationY(D3DXMATRIX* pOut, float Angle) { return pOut; }
inline D3DXMATRIX* D3DXMatrixRotationZ(D3DXMATRIX* pOut, float Angle) { return pOut; }
inline D3DXMATRIX* D3DXMatrixRotationAxis(D3DXMATRIX* pOut, const D3DXVECTOR3* pV, float Angle) { return pOut; }
inline D3DXMATRIX* D3DXMatrixTranslation(D3DXMATRIX* pOut, float x, float y, float z) { return pOut; }
inline D3DXMATRIX* D3DXMatrixScaling(D3DXMATRIX* pOut, float sx, float sy, float sz) { return pOut; }

inline D3DXMATRIX* D3DXMatrixPerspectiveFovRH(D3DXMATRIX* pOut, float fovy, float Aspect, float zn, float zf) { return pOut; }
inline D3DXMATRIX* D3DXMatrixOrthoRH(D3DXMATRIX* pOut, float w, float h, float zn, float zf) { return pOut; }
inline D3DXMATRIX* D3DXMatrixLookAtRH(D3DXMATRIX* pOut, const D3DXVECTOR3* pEye, const D3DXVECTOR3* pAt, const D3DXVECTOR3* pUp) { return pOut; }
inline D3DXMATRIX* D3DXMatrixTransformation(D3DXMATRIX* pOut, const D3DXVECTOR3* pScalingCenter, const D3DXQUATERNION* pScalingRotation, const D3DXVECTOR3* pScaling, const D3DXVECTOR3* pRotationCenter, const D3DXQUATERNION* pRotation, const D3DXVECTOR3* pTranslation) { return pOut; }
inline D3DXMATRIX* D3DXMatrixRotationQuaternion(D3DXMATRIX* pOut, const D3DXQUATERNION* pQ) { return pOut; }

inline D3DXQUATERNION* D3DXQuaternionMultiply(D3DXQUATERNION* pOut, const D3DXQUATERNION* pQ1, const D3DXQUATERNION* pQ2) { return pOut; }
inline D3DXQUATERNION* D3DXQuaternionIdentity(D3DXQUATERNION* pOut) { return pOut; }
inline D3DXQUATERNION* D3DXQuaternionConjugate(D3DXQUATERNION* pOut, const D3DXQUATERNION* pQ) { return pOut; }
inline D3DXQUATERNION* D3DXQuaternionSlerp(D3DXQUATERNION* pOut, const D3DXQUATERNION* pQ1, const D3DXQUATERNION* pQ2, float t) { return pOut; }
inline D3DXQUATERNION* D3DXQuaternionRotationMatrix(D3DXQUATERNION* pOut, const D3DXMATRIX* pM) { return pOut; }
inline D3DXQUATERNION* D3DXQuaternionRotationAxis(D3DXQUATERNION* pOut, const D3DXVECTOR3* pV, float Angle) { return pOut; }
inline D3DXQUATERNION* D3DXQuaternionNormalize(D3DXQUATERNION* pOut, const D3DXQUATERNION* pQ) { return pOut; }
inline D3DXQUATERNION* D3DXQuaternionInverse(D3DXQUATERNION* pOut, const D3DXQUATERNION* pQ) { return pOut; }

inline float D3DXPlaneDotCoord(const D3DXPLANE* pP, const D3DXVECTOR3* pV) { return 0.0f; }
inline D3DXPLANE* D3DXPlaneFromPoints(D3DXPLANE* pOut, const D3DXVECTOR3* pV1, const D3DXVECTOR3* pV2, const D3DXVECTOR3* pV3) { return pOut; }
inline D3DXPLANE* D3DXPlaneTransform(D3DXPLANE* pOut, const D3DXPLANE* pP, const D3DXMATRIX* pM) { return pOut; }
inline D3DXPLANE* D3DXPlaneNormalize(D3DXPLANE* pOut, const D3DXPLANE* pP) { return pOut; }

inline D3DXMATRIX* D3DXMatrixPerspectiveFovLH(D3DXMATRIX* pOut, float fovy, float Aspect, float zn, float zf) { return pOut; }
inline D3DXMATRIX* D3DXMatrixOrthoLH(D3DXMATRIX* pOut, float w, float h, float zn, float zf) { return pOut; }
inline D3DXMATRIX* D3DXMatrixLookAtLH(D3DXMATRIX* pOut, const D3DXVECTOR3* pEye, const D3DXVECTOR3* pAt, const D3DXVECTOR3* pUp) { return pOut; }
inline D3DXVECTOR3* D3DXVec3Maximize(D3DXVECTOR3* pOut, const D3DXVECTOR3* pV1, const D3DXVECTOR3* pV2) { return pOut; }
inline D3DXVECTOR3* D3DXVec3Minimize(D3DXVECTOR3* pOut, const D3DXVECTOR3* pV1, const D3DXVECTOR3* pV2) { return pOut; }
inline D3DXVECTOR3* D3DXVec3Normalize(D3DXVECTOR3* pOut, const D3DXVECTOR3* pV) { return pOut; }
inline HRESULT D3DXComputeBoundingBox(const D3DXVECTOR3* pFirstPosition, DWORD NumVertices, DWORD dwStride, D3DVECTOR* pMin, D3DVECTOR* pMax) { return 0; }
