/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// This file must never see D3D8 headers -- same reasoning as
// Backend/DX9ExBackend.cpp. It opts out of the shared PCH (see
// CMakeLists.txt SKIP_PRECOMPILE_HEADERS).
#include "Utility/CppMacros.h" // Must be first (see the PCH's own ordering)
#include "dx9vertexbuffer.h"

#include <d3d9.h>

#include "Backend/DX9ExBackend.h"
#include "fvfinfoclass.h"
#include "WWDebug/wwdebug.h"

DX9VertexBufferClass::DX9VertexBufferClass(unsigned FVF, unsigned short VertexCount, UsageType usage)
	:
	VertexBufferClass(BUFFER_TYPE_DX9EX, FVF, VertexCount),
	VertexBuffer(nullptr)
{
	Create_Vertex_Buffer(usage);
}

DX9VertexBufferClass::~DX9VertexBufferClass()
{
	if (VertexBuffer != nullptr)
	{
		VertexBuffer->Release();
		VertexBuffer = nullptr;
	}
}

void DX9VertexBufferClass::Create_Vertex_Buffer(UsageType usage)
{
	WWASSERT(!VertexBuffer);

	IDirect3DDevice9Ex * device = DX9ExBackend::Get_Device();
	WWASSERT(device);
	if (device == nullptr)
	{
		return;
	}

	unsigned usage_flags = D3DUSAGE_WRITEONLY | ((usage == USAGE_DYNAMIC) ? D3DUSAGE_DYNAMIC : 0);

	// D3D9Ex forbids D3DPOOL_MANAGED -- always D3DPOOL_DEFAULT, unlike the
	// DX8 path which uses D3DPOOL_MANAGED for non-dynamic buffers.
	HRESULT hr = device->CreateVertexBuffer(
		FVF_Info().Get_FVF_Size() * VertexCount,
		usage_flags,
		FVF_Info().Get_FVF(),
		D3DPOOL_DEFAULT,
		&VertexBuffer,
		nullptr);

	if (FAILED(hr) || VertexBuffer == nullptr)
	{
		WWDEBUG_SAY(("DX9VertexBufferClass: CreateVertexBuffer failed (hr=0x%08lX)", static_cast<unsigned long>(hr)));
	}
}

void * DX9VertexBufferClass::Lock_Raw(unsigned offset_bytes, unsigned size_bytes, unsigned flags)
{
	WWASSERT(VertexBuffer);
	void * data = nullptr;
	HRESULT hr = VertexBuffer->Lock(offset_bytes, size_bytes, &data, flags);
	if (FAILED(hr))
	{
		WWDEBUG_SAY(("DX9VertexBufferClass::Lock_Raw failed (hr=0x%08lX)", static_cast<unsigned long>(hr)));
		return nullptr;
	}
	return data;
}

void DX9VertexBufferClass::Unlock_Raw()
{
	WWASSERT(VertexBuffer);
	VertexBuffer->Unlock();
}
