#pragma once

#ifndef DX9_COMPAT_H
#define DX9_COMPAT_H

#include <d3d9.h>
#include <d3dx9.h>

// DirectX 8 to 9 compatibility stubs and mappings

// Removed constants/enums mappings
#ifndef D3DSDT_TESSFACTOR
#define D3DSDT_TESSFACTOR D3DSDT_FLOAT1
#endif

// Mapping d3d9 Create Flags if they differ (mostly same but renamed)
// D3DCREATE_PUREDEVICE is in d3d9.h
// D3DCREATE_HARDWARE_VERTEXPROCESSING is in d3d9.h

// Handle SetRenderTarget signature difference in internal wrapper code if needed.
// However, the best place is inside the DX9Wrapper class.

// DX9 D3DCAPS9 -> DX9 D3DCAPS9 (already mapped by refactor.py but some members changed)
// MaxSimultaneousTextures -> MaxSimultaneousTextures (same)
// VertexShaderVersion -> VertexShaderVersion (same 0xFFFE0101)

#endif // DX9_COMPAT_H

