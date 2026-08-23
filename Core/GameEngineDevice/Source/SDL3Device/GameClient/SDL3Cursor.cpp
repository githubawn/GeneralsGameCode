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

#include "Lib/BaseType.h"

#include <cstring>
#include <memory>
#include <SDL3_image/SDL_image.h>
#include <vector>

#include "Common/Debug.h"
#include "Common/file.h"
#include "Common/FileSystem.h"
#include "SDL3Device/GameClient/SDL3Cursor.h"

AnimatedCursor* SDL3CursorManager::m_cursorResources[Mouse::NUM_MOUSE_CURSORS][MAX_2D_CURSOR_DIRECTIONS] = {nullptr};

void SDL3CursorManager::init()
{
	shutdown();
}

void SDL3CursorManager::shutdown()
{
	for (int i = 0; i < Mouse::NUM_MOUSE_CURSORS; ++i)
	{
		for (int j = 0; j < MAX_2D_CURSOR_DIRECTIONS; ++j)
		{
			if (m_cursorResources[i][j])
			{
				delete m_cursorResources[i][j];
				m_cursorResources[i][j] = nullptr;
			}
		}
	}
}

SDL_Cursor* SDL3CursorManager::getCursor(Mouse::MouseCursor cursor, int direction)
{
	if (cursor < 0 || cursor >= Mouse::NUM_MOUSE_CURSORS)
		return nullptr;
	if (direction < 0 || direction >= MAX_2D_CURSOR_DIRECTIONS)
		direction = 0;

	AnimatedCursor* anim = m_cursorResources[cursor][direction];
	return anim ? anim->getCursor() : nullptr;
}

const AnimatedCursor* SDL3CursorManager::getAnimatedCursor(Mouse::MouseCursor cursor, int direction)
{
	if (cursor < Mouse::FIRST_CURSOR || cursor >= Mouse::NUM_MOUSE_CURSORS)
		return nullptr;

	if (direction < 0 || direction >= MAX_2D_CURSOR_DIRECTIONS)
		direction = 0;

	return m_cursorResources[cursor][direction];
}

void SDL3CursorManager::initResources(Mouse* mouse)
{
	if (!mouse)
		return;

	for (Int cursor = Mouse::FIRST_CURSOR; cursor < Mouse::NUM_MOUSE_CURSORS; cursor++)
	{
		for (Int direction = 0; direction < mouse->m_cursorInfo[cursor].numDirections && direction < MAX_2D_CURSOR_DIRECTIONS; direction++)
		{
			if (!m_cursorResources[cursor][direction] && !mouse->m_cursorInfo[cursor].textureName.isEmpty())
			{
				char resourcePath[256];
				if (mouse->m_cursorInfo[cursor].numDirections > 1)
					snprintf(resourcePath, sizeof(resourcePath), "Data/Cursors/%s%d.ani", mouse->m_cursorInfo[cursor].textureName.str(), direction);
				else
					snprintf(resourcePath, sizeof(resourcePath), "Data/Cursors/%s.ani", mouse->m_cursorInfo[cursor].textureName.str());

				m_cursorResources[cursor][direction] = loadANI(resourcePath);
				DEBUG_ASSERTCRASH(m_cursorResources[cursor][direction], ("MissingCursor %s\n", resourcePath));
			}
		}
	}
}

AnimatedCursor* SDL3CursorManager::loadANI(const char* filepath)
{
	File* file = TheFileSystem->openFile(filepath, File::READ | File::BINARY);
	if (!file)
	{
		return nullptr;
	}

	Int size = file->size();
	if (size <= 0)
	{
		file->close();
		return nullptr;
	}

	std::vector<char> buffer(size);
	Int bytesRead = file->read(buffer.data(), size);
	file->close();

	if (bytesRead <= 0)
	{
		return nullptr;
	}

	SDL_IOStream* io = SDL_IOFromConstMem(buffer.data(), bytesRead);
	if (!io)
	{
		return nullptr;
	}

	IMG_Animation* anim = IMG_LoadAnimation_IO(io, true);
	if (!anim)
	{
		DEBUG_LOG(("loadANI: IMG_LoadAnimation_IO failed for %s. Error: %s", filepath, SDL_GetError()));
		return nullptr;
	}

	if (anim->count == 0 || !anim->frames || !anim->frames[0])
	{
		DEBUG_LOG(("loadANI: Invalid or empty animation loaded for %s.", filepath));
		IMG_FreeAnimation(anim);
		return nullptr;
	}

	auto cursor = std::make_unique<AnimatedCursor>();

	// Calculate hotspot
	int hot_spot_x = 0;
	int hot_spot_y = 0;

	// RIFF/ACON parser header offsets
	const uint8_t* rawData = (const uint8_t*)buffer.data();
	if (bytesRead >= 24 && memcmp(rawData, "RIFF", 4) == 0 && memcmp(rawData + 8, "ACON", 4) == 0)
	{
		size_t offset = 12;
		while (offset + 8 <= (size_t)bytesRead)
		{
			const uint8_t* chunkHeader = rawData + offset;
			uint32_t chunkSize = *(const uint32_t*)(chunkHeader + 4);

			if (memcmp(chunkHeader, "anih", 4) == 0 && chunkSize >= 36 && offset + 8 + 36 <= (size_t)bytesRead)
			{
				// anih structure: cbSize(4), cFrames(4), cSteps(4), cx(4), cy(4), cBitCount(4), cPlanes(4), JifRate(4), flags(4)
				// flags bit 1 indicates if icon/cursor structures provide individual hotspots
			}
			else if (memcmp(chunkHeader, "icon", 4) == 0 && chunkSize >= 10 && offset + 8 + 10 <= (size_t)bytesRead)
			{
				const uint8_t* iconData = rawData + offset + 8;
				if (iconData[2] == 2 && iconData[3] == 0) // Type: 2 for Cursor
				{
					hot_spot_x = iconData[4] | (iconData[5] << 8);
					hot_spot_y = iconData[6] | (iconData[7] << 8);
					break;
				}
			}

			// RIFF chunks are word-aligned (padded to multiple of 2)
			size_t paddedSize = (chunkSize + 1) & ~1;
			offset += 8 + paddedSize;
		}
	}

	if (anim->count > 1)
	{
		// TODO: Future expansion for animated system cursor support if needed
		cursor->m_cursor = SDL_CreateColorCursor(anim->frames[0], hot_spot_x, hot_spot_y);
	}
	else
	{
		cursor->m_cursor = SDL_CreateColorCursor(anim->frames[0], hot_spot_x, hot_spot_y);
	}

	if (!cursor->m_cursor)
	{
		DEBUG_LOG(("loadANI: Failed to create cursor from %s. hot=(%d, %d), count=%d. Error: %s", filepath, hot_spot_x, hot_spot_y, anim->count, SDL_GetError()));
	}

	// Splitscreen: keep the decoded pixels. SDL_Cursor is opaque and only the window manager can
	// draw it, so seat cursors - which we draw ourselves - had no art for the 27 cursor states that
	// ship no texture, and fell back to the arrow. Copy to tightly-packed ARGB8888 while the
	// surfaces are still alive; IMG_FreeAnimation below releases them.
	cursor->m_hotSpotX = hot_spot_x;
	cursor->m_hotSpotY = hot_spot_y;
	cursor->m_frames.resize(anim->count);
	for (int i = 0; i < anim->count; ++i)
	{
		SDL_Surface *src = anim->frames[i];
		if (src == nullptr)
			continue;

		// Convert rather than assume: .ani frames are commonly 4bpp or 8bpp indexed.
		SDL_Surface *conv = SDL_ConvertSurface(src, SDL_PIXELFORMAT_ARGB8888);
		if (conv == nullptr)
			continue;

		CursorFrameRGBA &f = cursor->m_frames[i];
		f.m_width  = conv->w;
		f.m_height = conv->h;
		f.m_pixels.resize((size_t)conv->w * (size_t)conv->h * 4u);

		// Copy row by row: the surface pitch is not necessarily w*4.
		const UnsignedByte *srcBits = (const UnsignedByte *)conv->pixels;
		for (int y = 0; y < conv->h; ++y)
			memcpy(&f.m_pixels[(size_t)y * (size_t)conv->w * 4u],
				srcBits + (size_t)y * (size_t)conv->pitch,
				(size_t)conv->w * 4u);

		SDL_DestroySurface(conv);
	}

	IMG_FreeAnimation(anim);
	return cursor.release();
}
