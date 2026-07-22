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

// TheSuperHackers @feature githubawn 20/07/2026 CPU-side BC1/BC2/BC3
// block-compressed texture decoder for the 3DS citro3d backend (see
// Citro3dBackend.cpp's Ensure_Texture). The PICA200 GPU cannot sample DXT
// natively (no fixed-function or shader path for it in citro3d), and bimg --
// which the bgfx backend uses for the identical problem, see
// BgfxBackendTextures.cpp's CopyTextureLevel -- is not built for the 3DS
// target, so this is a small, self-contained replacement covering exactly
// the three block layouts the engine's own art actually uses: DXT1 (BC1),
// DXT2/DXT3 (BC2), DXT4/DXT5 (BC3).
//
// This header is 3DS-only by convention, not enforcement: it has no citro3d
// or libctru dependency at all (pure block-format math, deliberately kept
// that way -- see DXTUtils.h's identical rationale), but it is only ever
// #included from Citro3dBackend.cpp, which IS restricted to the citro3d
// render backend (see that file's own header comment). Do not include this
// from any other translation unit without first checking that restriction
// still makes sense.
//
// TheSuperHackers @info githubawn 20/07/2026 DXT2/DXT4 are documented as
// "premultiplied alpha" variants of DXT3/DXT5 with an IDENTICAL bit layout --
// the premultiplication is purely a convention about what the encoder put
// into the RGB channels before compressing, not a different decode
// algorithm. Confirmed the engine's own DX8/D3D8 path (bitmaphandler.h,
// surfaceclass.cpp, texture.cpp, formconv.cpp -- grep for WW3D_FORMAT_DXT2)
// never special-cases DXT2 differently from DXT3 (nor DXT4 from DXT5)
// anywhere: every switch statement in this codebase buckets them together.
// So decoding DXT2 identically to DXT3 (and DXT4 identically to DXT5) below
// matches this engine's own existing behavior exactly, not a new tradeoff.

#pragma once

namespace GGCDxt
{
    // TheSuperHackers @info githubawn 20/07/2026 5-bit and 6-bit -> 8-bit
    // channel expansion via bit replication (top bits repeated into the
    // newly-opened low bits), the standard S3TC/BC1 endpoint decode -- NOT
    // a simple left-shift, which would leave the low bits zero and darken
    // every color slightly.
    inline unsigned char Expand5To8(unsigned v) { return static_cast<unsigned char>((v << 3) | (v >> 2)); }
    inline unsigned char Expand6To8(unsigned v) { return static_cast<unsigned char>((v << 2) | (v >> 4)); }

    // Reads only the two RGB565 endpoints (the first 4 bytes) of a BC1 color
    // block to test its punchthrough-alpha condition (c0 <= c1) WITHOUT
    // decoding the rest of the block. Used by Citro3dBackend.cpp for a cheap
    // up-front scan over every block's header that decides DXT1's PICA
    // output format (opaque GPU_RGB565 vs 1-bit-alpha GPU_RGBA5551) before
    // any C3D_Tex is allocated -- see that file's Ensure_Texture.
    inline bool BC1BlockHasPunchThroughAlpha(const unsigned char * block)
    {
        const unsigned short c0 = static_cast<unsigned short>(block[0] | (block[1] << 8));
        const unsigned short c1 = static_cast<unsigned short>(block[2] | (block[3] << 8));
        return c0 <= c1;
    }

    // Decodes one 8-byte BC1 color block into 16 RGBA8 texels, outR/outG/
    // outB/outA indexed in row-major block order (texel i is at local
    // position (i % 4, i / 4), matching the 2-bit-per-texel index field's own
    // bit order -- index bits for texel 0 are the LOWEST 2 bits of the
    // 32-bit index word).
    //
    // forceOpaqueColor selects between BC1's own two sub-modes (c0 > c1:
    // 4-color opaque; c0 <= c1: 3-color + a 4th "transparent black" index,
    // DXT1's 1-bit punchthrough alpha) and BC2/BC3's color part, which reuses
    // this exact 8-byte layout but is ALWAYS decoded in 4-color opaque mode
    // regardless of how c0/c1 compare -- those formats get their alpha from
    // a separate explicit block instead (see DecodeBC2AlphaBlock/
    // DecodeBC3AlphaBlock below), so outA is not written here when
    // forceOpaqueColor is true (pass nullptr).
    inline void DecodeBC1ColorBlock(const unsigned char * block, bool forceOpaqueColor,
                                     unsigned char outR[16], unsigned char outG[16], unsigned char outB[16],
                                     unsigned char * outA)
    {
        const unsigned short c0 = static_cast<unsigned short>(block[0] | (block[1] << 8));
        const unsigned short c1 = static_cast<unsigned short>(block[2] | (block[3] << 8));

        unsigned char r[4];
        unsigned char g[4];
        unsigned char b[4];
        r[0] = Expand5To8((c0 >> 11) & 0x1f);
        g[0] = Expand6To8((c0 >> 5) & 0x3f);
        b[0] = Expand5To8(c0 & 0x1f);
        r[1] = Expand5To8((c1 >> 11) & 0x1f);
        g[1] = Expand6To8((c1 >> 5) & 0x3f);
        b[1] = Expand5To8(c1 & 0x1f);

        const bool punchThrough = !forceOpaqueColor && (c0 <= c1);
        if (punchThrough)
        {
            // 3-color mode: c2 is the midpoint of c0/c1; index 3 is
            // "transparent black" (color value itself is irrelevant since
            // alpha will be 0, but zero it anyway rather than leave garbage).
            r[2] = static_cast<unsigned char>((static_cast<unsigned>(r[0]) + r[1]) / 2);
            g[2] = static_cast<unsigned char>((static_cast<unsigned>(g[0]) + g[1]) / 2);
            b[2] = static_cast<unsigned char>((static_cast<unsigned>(b[0]) + b[1]) / 2);
            r[3] = 0; g[3] = 0; b[3] = 0;
        }
        else
        {
            // 4-color opaque mode: c2/c3 are the 1/3 and 2/3 interpolants.
            r[2] = static_cast<unsigned char>((2u * r[0] + r[1]) / 3);
            g[2] = static_cast<unsigned char>((2u * g[0] + g[1]) / 3);
            b[2] = static_cast<unsigned char>((2u * b[0] + b[1]) / 3);
            r[3] = static_cast<unsigned char>((r[0] + 2u * r[1]) / 3);
            g[3] = static_cast<unsigned char>((g[0] + 2u * g[1]) / 3);
            b[3] = static_cast<unsigned char>((b[0] + 2u * b[1]) / 3);
        }

        const unsigned indices = static_cast<unsigned>(block[4])
            | (static_cast<unsigned>(block[5]) << 8)
            | (static_cast<unsigned>(block[6]) << 16)
            | (static_cast<unsigned>(block[7]) << 24);
        for (int i = 0; i < 16; ++i)
        {
            const unsigned idx = (indices >> (2 * i)) & 0x3;
            outR[i] = r[idx];
            outG[i] = g[idx];
            outB[i] = b[idx];
            if (outA != nullptr)
            {
                outA[i] = (punchThrough && idx == 3) ? 0 : 255;
            }
        }
    }

    // Decodes one 8-byte BC2 (DXT2/DXT3) explicit-alpha block: 16 texels x
    // 4 bits each, packed as a single 64-bit little-endian field (texel 0 in
    // the lowest 4 bits), expanded to 8-bit via the same bit-replication
    // principle as the 5/6-bit color channels above ((a<<4)|a).
    inline void DecodeBC2AlphaBlock(const unsigned char * block, unsigned char outA[16])
    {
        for (int i = 0; i < 16; ++i)
        {
            const int byteIndex = i / 2;
            const unsigned nibble = (i & 1) == 0 ? (block[byteIndex] & 0x0f) : (block[byteIndex] >> 4);
            outA[i] = static_cast<unsigned char>((nibble << 4) | nibble);
        }
    }

    // Decodes one 8-byte BC3 (DXT4/DXT5) interpolated-alpha block: two 8-bit
    // endpoints followed by a 48-bit (6-byte) field of 3-bit-per-texel
    // indices, same little-endian/texel-0-in-lowest-bits convention as the
    // color block's 2-bit indices above.
    inline void DecodeBC3AlphaBlock(const unsigned char * block, unsigned char outA[16])
    {
        const unsigned char a0 = block[0];
        const unsigned char a1 = block[1];

        unsigned char table[8];
        table[0] = a0;
        table[1] = a1;
        if (a0 > a1)
        {
            // 6 interpolated values between the endpoints (8-value mode).
            for (int i = 0; i < 6; ++i)
            {
                table[2 + i] = static_cast<unsigned char>(
                    ((6 - i) * static_cast<unsigned>(a0) + (1 + i) * static_cast<unsigned>(a1)) / 7);
            }
        }
        else
        {
            // 4 interpolated values plus fixed fully-transparent/fully-opaque
            // endpoints (6-value mode) -- BC3's equivalent of BC1's
            // punchthrough sub-mode, just for an 8-bit channel instead of 1.
            for (int i = 0; i < 4; ++i)
            {
                table[2 + i] = static_cast<unsigned char>(
                    ((4 - i) * static_cast<unsigned>(a0) + (1 + i) * static_cast<unsigned>(a1)) / 5);
            }
            table[6] = 0;
            table[7] = 255;
        }

        unsigned long long indices = 0;
        for (int i = 0; i < 6; ++i)
        {
            indices |= static_cast<unsigned long long>(block[2 + i]) << (8 * i);
        }
        for (int i = 0; i < 16; ++i)
        {
            const unsigned idx = static_cast<unsigned>((indices >> (3 * i)) & 0x7);
            outA[i] = table[idx];
        }
    }
}
