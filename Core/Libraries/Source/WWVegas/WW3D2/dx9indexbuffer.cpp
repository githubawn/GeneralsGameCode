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
#include "dx9indexbuffer.h"

#include <d3d9.h>

#include "Backend/DX9ExBackend.h"
#include "WWDebug/wwdebug.h"

DX9IndexBufferClass::DX9IndexBufferClass(unsigned short index_count, UsageType usage)
	:
	IndexBufferClass(BUFFER_TYPE_DX9EX, index_count),
	index_buffer(nullptr)
{
	IDirect3DDevice9Ex * device = DX9ExBackend::Get_Device();
	WWASSERT(device);
	if (device == nullptr)
	{
		return;
	}

	unsigned usage_flags = D3DUSAGE_WRITEONLY | ((usage == USAGE_DYNAMIC) ? D3DUSAGE_DYNAMIC : 0);

	// D3D9Ex forbids D3DPOOL_MANAGED -- always D3DPOOL_DEFAULT, unlike the
	// DX8 path which uses D3DPOOL_MANAGED for non-dynamic buffers.
	HRESULT hr = device->CreateIndexBuffer(
		sizeof(WORD) * index_count,
		usage_flags,
		D3DFMT_INDEX16,
		D3DPOOL_DEFAULT,
		&index_buffer,
		nullptr);

	if (FAILED(hr) || index_buffer == nullptr)
	{
		WWDEBUG_SAY(("DX9IndexBufferClass: CreateIndexBuffer failed (hr=0x%08lX)", static_cast<unsigned long>(hr)));
	}
}

DX9IndexBufferClass::~DX9IndexBufferClass()
{
	if (index_buffer != nullptr)
	{
		index_buffer->Release();
		index_buffer = nullptr;
	}
}

void * DX9IndexBufferClass::Lock_Raw(unsigned offset_bytes, unsigned size_bytes, unsigned flags)
{
	WWASSERT(index_buffer);
	void * data = nullptr;
	HRESULT hr = index_buffer->Lock(offset_bytes, size_bytes, &data, flags);
	if (FAILED(hr))
	{
		WWDEBUG_SAY(("DX9IndexBufferClass::Lock_Raw failed (hr=0x%08lX)", static_cast<unsigned long>(hr)));
		return nullptr;
	}
	return data;
}

void DX9IndexBufferClass::Unlock_Raw()
{
	WWASSERT(index_buffer);
	index_buffer->Unlock();
}
