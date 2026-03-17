#pragma once
#include "d3d8.h"

// Basic D3DX8 Texture stubs
inline HRESULT D3DXCreateTextureFromFileInMemory(LPDIRECT3DDEVICE8 pDevice, LPCVOID pSrcData, UINT SrcDataSize, LPDIRECT3DTEXTURE8* ppTexture) { return 0; }
inline HRESULT D3DXGetImageInfoFromFileInMemory(LPCVOID pSrcData, UINT SrcDataSize, void* pSrcInfo) { return 0; }
