/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// TileData.h
// Class to hold 1 tile's data.
// Author: John Ahlquist, April 2001

#pragma once

#include "Lib/BaseType.h"
#include "WWLib/refcount.h"
#include "Common/AsciiString.h"

typedef struct {
	Int blendNdx;
	UnsignedByte horiz;
	UnsignedByte vert;
	UnsignedByte rightDiagonal;
	UnsignedByte leftDiagonal;
	UnsignedByte inverted;
	UnsignedByte longDiagonal;
	Int customBlendEdgeClass; // Class of texture for a blend edge.  -1 means use alpha.
} TBlendTileInfo;

#define INVERTED_MASK	0x1		//AND this with TBlendTileInfo.inverted to get actual inverted state
#define FLIPPED_MASK	0x2		//AND this with TBlendTileInfo.inverted to get forced flip state (for horizontal/vertical flips).
#define TILE_PIXEL_EXTENT 64
#define TILE_BYTES_PER_PIXEL 4
#define DATA_LEN_BYTES TILE_PIXEL_EXTENT*TILE_PIXEL_EXTENT*TILE_BYTES_PER_PIXEL
#define DATA_LEN_PIXELS TILE_PIXEL_EXTENT*TILE_PIXEL_EXTENT
#define TILE_PIXEL_EXTENT_MIP1 32
#define TILE_PIXEL_EXTENT_MIP2 16
#define TILE_PIXEL_EXTENT_MIP3 8
#define TILE_PIXEL_EXTENT_MIP4 4
#define TILE_PIXEL_EXTENT_MIP5 2
#define TILE_PIXEL_EXTENT_MIP6 1
// TheSuperHackers @bugfix githubawn 20/07/2026 The terrain tile atlas is built
// by constructing a TextureClass with these explicit dimensions
// (TerrainTextureClass, TerrainTex.cpp), which bypasses
// TextureLoader::Validate_Texture_Size entirely -- so unlike every texture
// loaded from a file, it is never clamped to the hardware's MaxTextureWidth.
// The PICA200's hard limit is 1024, so a 2048-wide atlas makes C3D_TexInit
// fail outright (observed as "[ggc-tex] C3D_TexInit FAILED w=2048 h=1024"),
// leaving ALL terrain untextured no matter what else the renderer supports.
// 1024 is this define's own original value ("was 1024 jba" below), i.e. a
// configuration the surrounding tile-packing code was written against and
// still guards for (see the `surface_desc.Width < TEXTURE_WIDTH` checks in
// TerrainTex.cpp), not a new untested size. It also quarters the atlas's
// memory footprint, which matters on this platform's ~44MB GPU heap.
//
// Tradeoff, stated plainly: WorldHeightMap::updateTileTexturePositions packs
// tiles into a tilesPerRow x tilesPerRow grid derived from this define, so
// halving it quarters the number of distinct terrain tiles a map can place.
// The packer already handles overflow without failing (a tile class it cannot
// place keeps positionInTexture 0,0 and samples the atlas origin), so a
// texture-heavy map degrades to repeated/wrong terrain art rather than
// crashing. That is strictly better than today's outcome, where the atlas
// fails to upload at all and NO terrain is textured.
#if defined(__3DS__)
#define TEXTURE_WIDTH 1024
#else
#define TEXTURE_WIDTH 2048 // was 1024 jba
#endif

/** This class holds the bitmap data from the .tga texture files.  It is used to
create the D3D texture in the game and 3d windows, and to create DIB data for the
2d window. */
class TileData : public RefCountClass
{
protected:

	// data is bgrabgrabgra to be compatible with windows blt. jba.
	// Also, first byte is lower left pixel, not upper left pixel.
	// so 0,0 is lower left, not upper left.
	UnsignedByte m_tileData[DATA_LEN_BYTES];
	/// Mipped down copies of the tile data.
	UnsignedByte m_tileDataMip32[TILE_PIXEL_EXTENT_MIP1*TILE_PIXEL_EXTENT_MIP1*TILE_BYTES_PER_PIXEL];
	UnsignedByte m_tileDataMip16[TILE_PIXEL_EXTENT_MIP2*TILE_PIXEL_EXTENT_MIP2*TILE_BYTES_PER_PIXEL];
	UnsignedByte m_tileDataMip8[TILE_PIXEL_EXTENT_MIP3*TILE_PIXEL_EXTENT_MIP3*TILE_BYTES_PER_PIXEL];
	UnsignedByte m_tileDataMip4[TILE_PIXEL_EXTENT_MIP4*TILE_PIXEL_EXTENT_MIP4*TILE_BYTES_PER_PIXEL];
	UnsignedByte m_tileDataMip2[TILE_PIXEL_EXTENT_MIP5*TILE_PIXEL_EXTENT_MIP5*TILE_BYTES_PER_PIXEL];
	UnsignedByte m_tileDataMip1[TILE_PIXEL_EXTENT_MIP6*TILE_PIXEL_EXTENT_MIP6*TILE_BYTES_PER_PIXEL];

public:
	ICoord2D	m_tileLocationInTexture;


protected:
	/** doMip - generates the next mip level mipping pHiRes down to pLoRes.
				pLoRes is 1/2 the width of pHiRes, and both are square. */
	static void doMip(UnsignedByte *pHiRes, Int hiRow, UnsignedByte *pLoRes);



public:
	TileData();

public:
	UnsignedByte *getDataPtr() {return(m_tileData);};
	static Int dataLen() {return(DATA_LEN_BYTES);};

	void updateMips();

	Bool hasRGBDataForWidth(Int width);
	UnsignedByte *getRGBDataForWidth(Int width);
};
