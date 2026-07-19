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

 /***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/render2dsentence.cpp                   $*
 *                                                                                             *
 *                       $Author:: Patrick                  $*
 *                                                                                             *
 *								$Modtime:: 8/29/01 11:16a                                             $*
 *                                                                                             *
 *                    $Revision:: 13                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "render2dsentence.h"
#include "surfaceclass.h"
#include "texture.h"
#include "wwprofile.h"
#include "wwmemlog.h"
#include "dx8wrapper.h"


////////////////////////////////////////////////////////////////////////////////////
//	Local constants
////////////////////////////////////////////////////////////////////////////////////
#define no_TEST_PLACEMENT 1	 // Shows alignment markers for text.

#define TEXTURE_OFFSET 2

////////////////////////////////////////////////////////////////////////////////////
//
//	FontCharsAtlasClass
//
////////////////////////////////////////////////////////////////////////////////////
FontCharsAtlasClass* FontCharsAtlasClass::Instance = nullptr;

FontCharsAtlasClass::FontCharsAtlasClass() : FontRefCount(0)
{
}

FontCharsAtlasClass::~FontCharsAtlasClass()
{
	for (int i = 0; i < Pages.Count(); i++) {
		REF_PTR_RELEASE(Pages[i].Texture);
		REF_PTR_RELEASE(Pages[i].Staging);
	}
	Pages.Clear();
}

FontCharsAtlasClass* FontCharsAtlasClass::Get_Instance()
{
	if (!Instance) {
		Instance = new FontCharsAtlasClass();
	}
	return Instance;
}

void FontCharsAtlasClass::_Shutdown()
{
	if (Instance) {
		delete Instance;
		Instance = nullptr;
	}
}

void FontCharsAtlasClass::Add_Ref_Font()
{
	FontRefCount++;
}

void FontCharsAtlasClass::Release_Font()
{
	FontRefCount--;
	if (FontRefCount <= 0) {
		_Shutdown();
	}
}

void FontCharsAtlasClass::Ensure_Glyph(FontCharsClass* font, FontCharsClassCharDataStruct* data)
{
	if (data->AtlasPage != -1) return;
	
	if (data->Width <= 0 || font->Get_Char_Height() <= 0) {
		data->AtlasPage = 0;
		data->AtlasX = 0;
		data->AtlasY = 0;
		return;
	}

	int char_h = font->Get_Char_Height();
	int char_w = data->Width;
	
	PageStruct* cur_page = nullptr;
	int page_idx = -1;
	
	if (Pages.Count() > 0) {
		page_idx = Pages.Count() - 1;
		cur_page = &Pages[page_idx];
		
		if (cur_page->CurrentX + char_w + 1 > 256) {
			cur_page->CurrentX = 0;
			cur_page->CurrentY += cur_page->ShelfHeight;
			cur_page->ShelfHeight = 0;
		}
		
		if (cur_page->CurrentY + char_h + 1 > 256) {
			cur_page = nullptr;
		}
	}
	
	if (!cur_page) {
		PageStruct new_page;
		new_page.Texture = W3DNEW TextureClass(256, 256, WW3D_FORMAT_A4R4G4B4, MIP_LEVELS_1);
		new_page.Texture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		new_page.Texture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		new_page.Texture->Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
		new_page.Texture->Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
		new_page.Texture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
		
		new_page.Staging = NEW_REF(SurfaceClass, (256, 256, WW3D_FORMAT_A4R4G4B4));
		new_page.CurrentX = 0;
		new_page.CurrentY = 0;
		new_page.ShelfHeight = 0;
		new_page.DirtyMinY = 256;
		new_page.DirtyMaxY = -1;
		
		Pages.Add(new_page);
		page_idx = Pages.Count() - 1;
		cur_page = &Pages[page_idx];
	}
	
	if (char_h + 1 > cur_page->ShelfHeight) {
		cur_page->ShelfHeight = char_h + 1;
	}
	
	data->AtlasPage = page_idx;
	data->AtlasX = cur_page->CurrentX;
	data->AtlasY = cur_page->CurrentY;
	
	int stride = 0;
	uint16* locked_ptr = (uint16*)cur_page->Staging->Lock(&stride);
	if (locked_ptr) {
		int dest_inc = (stride >> 1);
		uint16* dest = locked_ptr + (data->AtlasY * dest_inc) + data->AtlasX;
		uint16* src = data->Buffer;
		for (int row = 0; row < char_h; row++) {
			for (int col = 0; col < char_w; col++) {
				dest[col] = *src++;
			}
			dest += dest_inc;
		}
		cur_page->Staging->Unlock();
		
		if (data->AtlasY < cur_page->DirtyMinY) cur_page->DirtyMinY = data->AtlasY;
		if (data->AtlasY + char_h > cur_page->DirtyMaxY) cur_page->DirtyMaxY = data->AtlasY + char_h;
	}
	
	cur_page->CurrentX += char_w + 1;
}

void FontCharsAtlasClass::Flush_Updates()
{
	for (int i = 0; i < Pages.Count(); i++) {
		PageStruct& page = Pages[i];
		if (page.DirtyMinY <= page.DirtyMaxY) {
			RECT rect = { 0, page.DirtyMinY, 256, page.DirtyMaxY };
			DX8Wrapper::_Copy_DX8_Rects(page.Staging->Peek_D3D_Surface(), &rect, 1, page.Texture->Get_Surface_Level()->Peek_D3D_Surface(), nullptr);
			page.DirtyMinY = 256;
			page.DirtyMaxY = -1;
		}
	}
}

void FontCharsAtlasClass::Dump_Atlas(const char* prefix)
{
#pragma pack(push, 1)
	struct BMPHeader {
		unsigned short type;
		unsigned int size;
		unsigned short reserved1;
		unsigned short reserved2;
		unsigned int offBits;
	};
	struct BMPInfo {
		unsigned int size;
		int width;
		int height;
		unsigned short planes;
		unsigned short bitCount;
		unsigned int compression;
		unsigned int sizeImage;
		int xPelsPerMeter;
		int yPelsPerMeter;
		unsigned int clrUsed;
		unsigned int clrImportant;
	};
#pragma pack(pop)

	for (int i = 0; i < Pages.Count(); i++) {
		PageStruct& page = Pages[i];
		int stride = 0;
		unsigned short* locked_ptr = (unsigned short*)page.Staging->Lock(&stride);
		if (locked_ptr) {
			char filename[256];
			sprintf(filename, "%s_page%d.bmp", prefix, i);
			FILE* fp = fopen(filename, "wb");
			if (fp) {
				BMPHeader header;
				BMPInfo info;
				memset(&header, 0, sizeof(header));
				memset(&info, 0, sizeof(info));

				header.type = 0x4D42;
				header.offBits = sizeof(header) + sizeof(info);
				header.size = header.offBits + 256 * 256 * 4;

				info.size = sizeof(info);
				info.width = 256;
				info.height = -256; // Top-down
				info.planes = 1;
				info.bitCount = 32;

				fwrite(&header, sizeof(header), 1, fp);
				fwrite(&info, sizeof(info), 1, fp);

				int dest_inc = (stride >> 1);
				for (int y = 0; y < 256; y++) {
					unsigned short* row = (unsigned short*)locked_ptr + y * dest_inc;
					for (int x = 0; x < 256; x++) {
						unsigned short p = row[x];
						unsigned char a = (p >> 12) & 0xF;
						unsigned char r = (p >> 8) & 0xF;
						unsigned char g = (p >> 4) & 0xF;
						unsigned char b = p & 0xF;
						a = (a << 4) | a;
						r = (r << 4) | r;
						g = (g << 4) | g;
						b = (b << 4) | b;
						unsigned int p32 = (a << 24) | (r << 16) | (g << 8) | b;
						fwrite(&p32, 4, 1, fp);
					}
				}
				fclose(fp);
			}
			page.Staging->Unlock();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////
//
//	Render2DSentenceClass
//
////////////////////////////////////////////////////////////////////////////////////
Render2DSentenceClass::Render2DSentenceClass () :
	Font (nullptr),
	Location (0.0F,0.0F),
	Cursor (0.0F,0.0F),
	TextureSizeHint (0),
	MonoSpaced (false),
	IsClippedEnabled (false),
	ClipRect (0, 0, 0, 0),
	BaseLocation (0, 0),
	WrapWidth (0),
	Centered (false),
	DrawExtents (0, 0, 0, 0),
	ParseHotKey( false ),
	useHardWordWrap( false)
{
	Shader = Render2DClass::Get_Default_Shader ();
}


////////////////////////////////////////////////////////////////////////////////////
//
//	~Render2DSentenceClass
//
////////////////////////////////////////////////////////////////////////////////////
Render2DSentenceClass::~Render2DSentenceClass ()
{
	REF_PTR_RELEASE (Font);
	Reset ();
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Set_Font
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Set_Font (FontCharsClass *font)
{
	Reset ();
	REF_PTR_SET (Font, font);
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Reset_Polys
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Reset_Polys ()
{
	DrawQuads.Clear();
}

////////////////////////////////////////////////////////////////////////////////////
//
//	Reset
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Reset ()
{
	Cursor.Set (0, 0);
	MonoSpaced = false;
	ParseHotKey = false;
	Reset_Sentence_Data ();
}

////////////////////////////////////////////////////////////////////////////////////
//
//	Make_Additive
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Make_Additive ()
{
	Shader.Set_Dst_Blend_Func (ShaderClass::DSTBLEND_ONE);
	Shader.Set_Src_Blend_Func (ShaderClass::SRCBLEND_ONE);
	Shader.Set_Primary_Gradient (ShaderClass::GRADIENT_MODULATE);
	Shader.Set_Secondary_Gradient (ShaderClass::SECONDARY_GRADIENT_DISABLE);

	Set_Shader (Shader);
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Make_Additive
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Set_Shader (ShaderClass shader)
{
	Shader = shader;
}

////////////////////////////////////////////////////////////////////////////////////
//
//	Render
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Render ()
{
	FontCharsAtlasClass::Get_Instance()->Flush_Updates();
	int pages = FontCharsAtlasClass::Get_Instance()->Get_Page_Count();
	for (int i = 0; i < pages; i++) {
		Render2DClass renderer;
		renderer.Set_Coordinate_Range(Render2DClass::Get_Screen_Resolution());
		(*renderer.Get_Shader()) = Shader;
		renderer.Enable_Texturing(TRUE);
		renderer.Set_Texture(FontCharsAtlasClass::Get_Instance()->Get_Page_Texture(i));
		Add_Quads_To(renderer, i);
		renderer.Render();
	}
}

////////////////////////////////////////////////////////////////////////////////////
//
//	Set_Base_Location
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Set_Base_Location (const Vector2 &loc)
{
	BaseLocation = loc;
}

////////////////////////////////////////////////////////////////////////////////////
//
//	Set_Location
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Set_Location (const Vector2 &loc)
{
	Location	= loc;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Text_Extents
//
////////////////////////////////////////////////////////////////////////////////////
Vector2
Render2DSentenceClass::Get_Text_Extents (const WCHAR *text)
{
	Vector2 extent (0, Font->Get_Char_Height());

	while (*text) {
		WCHAR ch = *text++;

		if ( ch != (WCHAR)'\n' ) {
			extent.X += Font->Get_Char_Spacing( ch );
		}
	}

	return extent;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Formatted_Text_Extents
//
////////////////////////////////////////////////////////////////////////////////////
Vector2
Render2DSentenceClass::Get_Formatted_Text_Extents (const WCHAR *text)
{
	return Build_Sentence_Not_Centered(text, nullptr, nullptr, true);
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Reset_Sentence_Data
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Reset_Sentence_Data ()
{
	SentenceData.Clear ();
}

////////////////////////////////////////////////////////////////////////////////////
//
//	Release_Pending_Surfaces
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
//
//	Build_Textures
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
//
//	Draw_Sentence
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Draw_Sentence (uint32 color)
{
	DrawExtents.Set (0, 0, 0, 0);

	for (int index = 0; index < SentenceData.Count (); index ++) {
		SentenceDataStruct &data = SentenceData[index];

		RectClass screen_rect;
		screen_rect.Left = (float)(int)(data.ScreenRect.Left + Location.X);
		screen_rect.Top = (float)(int)(data.ScreenRect.Top + Location.Y);
		screen_rect.Right = (float)(int)(data.ScreenRect.Right + Location.X);
		screen_rect.Bottom = (float)(int)(data.ScreenRect.Bottom + Location.Y);
		RectClass uv_rect = data.UVRect;

		bool add_quad = true;
		if (IsClippedEnabled) {
			if (screen_rect.Right <= ClipRect.Left || screen_rect.Bottom <= ClipRect.Top || screen_rect.Left >= ClipRect.Right || screen_rect.Top >= ClipRect.Bottom) {
				add_quad = false;
			} else {
				RectClass clipped_rect;
				clipped_rect.Left		= max (screen_rect.Left, ClipRect.Left);
				clipped_rect.Right	= min (screen_rect.Right, ClipRect.Right);
				clipped_rect.Top		= max (screen_rect.Top, ClipRect.Top);
				clipped_rect.Bottom	= min (screen_rect.Bottom, ClipRect.Bottom);

				RectClass clipped_uv_rect;
				float percent = ((clipped_rect.Left - screen_rect.Left) / screen_rect.Width ());
				clipped_uv_rect.Left = uv_rect.Left + (uv_rect.Width () * percent);

				percent = ((clipped_rect.Right - screen_rect.Left) / screen_rect.Width ());
				clipped_uv_rect.Right = uv_rect.Left + (uv_rect.Width () * percent);

				percent = ((clipped_rect.Top - screen_rect.Top) / screen_rect.Height ());
				clipped_uv_rect.Top = uv_rect.Top + (uv_rect.Height () * percent);

				percent = ((clipped_rect.Bottom - screen_rect.Top) / screen_rect.Height ());
				clipped_uv_rect.Bottom = uv_rect.Top + (uv_rect.Height () * percent);

				screen_rect = clipped_rect;
				uv_rect = clipped_uv_rect;

				if (screen_rect.Right <= screen_rect.Left || screen_rect.Bottom <= screen_rect.Top) {
					add_quad = false;
				}
			}
		}

		if (add_quad) {
			QuadStruct quad;
			quad.Page = data.Page;
			quad.ScreenRect = screen_rect;
			quad.UVRect = uv_rect;
			quad.Color = color;
			DrawQuads.Add(quad);

			if (DrawExtents.Width () == 0) {
				DrawExtents = screen_rect;
			} else {
				DrawExtents += screen_rect;
			}
		}
	}
}

void Render2DSentenceClass::Add_Quads_To(Render2DClass &target, int page)
{
	for (int i = 0; i < DrawQuads.Count(); i++) {
		if (DrawQuads[i].Page == page) {
			target.Add_Quad(DrawQuads[i].ScreenRect, DrawQuads[i].UVRect, DrawQuads[i].Color);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////
//
//	Record_Sentence_Chunk
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
//
//	Allocate_New_Surface
//
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
//
//	Build_Sentence_Centered
//
////////////////////////////////////////////////////////////////////////////////////
void	Render2DSentenceClass::Build_Sentence_Centered (const WCHAR *text, int *hkX, int *hkY)
{
	float char_height = Font->Get_Char_Height ();
	int notCenteredHotkeyX = 0;
	int notCenteredHotkeyY = 0;
	Vector2 extent = Build_Sentence_Not_Centered(text,&notCenteredHotkeyX, &notCenteredHotkeyY, TRUE);

	Reset_Sentence_Data ();
	Cursor.Set (0, 0);

	bool end = false;
	const WCHAR *word;
	int word_width	= 0;
	int line_width	= 0;
	int charCount = 0;
	int wordCount = 0;
	int hotKeyPosX = 0;
	int hotKeyPosY = 0;
	bool calcHotKeyX = false;
	bool dontBlit = false;

	while (!end)
	{
		word = text;
		word_width = 0;
		line_width = 0;
		charCount = 0;
		wordCount = 0;

		while ( 1 )
		{
			int charWidth = 0;
			while ((*word != 0) && (*word > L' ') && (*word != L'\n')) {
				if( ParseHotKey && (*word == L'&') && (*word+1 != 0) && (*word+1 > L' ') && (*word+1 != L'\n'))
				{
					int offset = 0;
					if (word_width != 0 )
					{
						const WCHAR *word_back = word;
						*word_back--;
						if (*word_back == L' ')
						{
							line_width -= word_width;
							offset =-1;
						}
					}
					*word++;
					calcHotKeyX = true;
				}

				charWidth = Font->Get_Char_Spacing (*word++);
				word_width += charWidth;
				wordCount++;

				if (WrapWidth > 0 && word_width >= WrapWidth && useHardWordWrap)
					break;
			}

			if(WrapWidth > 0 && (line_width + word_width >= WrapWidth))
			{
				if(charCount == 0)
				{
					charCount +=wordCount - 1;
					line_width += word_width - charWidth;
					if(*word == 0)
						end = true;
					break;
				}
				charCount--;
				break;
			}

			if( *word == 0 )
			{
				charCount +=wordCount;
				line_width += word_width;
				end = true;
				break;
			}

			charCount +=wordCount + 1;
			line_width += word_width;

			if(*word != L' ')
				break;

			word_width = Font->Get_Char_Spacing (*word++);
			wordCount = 0;
			line_width += word_width;
		}

		Cursor.X = (int)((extent.X - line_width) / 2);
		if(Cursor.X < 0)
			Cursor.X = 0;
		if(calcHotKeyX)
		{
			calcHotKeyX = false;
			hotKeyPosX = Cursor.X + notCenteredHotkeyX;
		}

		for(int i = 0; i <= charCount; i++) {
			WCHAR ch = *text++;
			dontBlit = false;

			if(ParseHotKey && (ch == L'&') && (*text != 0) && (*text > L' ') && (*text != L'\n'))
			{
				ch = *text++;
				dontBlit = true;
			}
			float char_spacing = Font->Get_Char_Spacing (ch);

			if (ch != L'\n' && ch != L' ' && ch != 0) {
				if (!dontBlit) {
					FontCharsClassCharDataStruct* data = const_cast<FontCharsClassCharDataStruct*>(Font->Get_Char_Data(ch));
					if (data) {
						FontCharsAtlasClass::Get_Instance()->Ensure_Glyph(Font, data);

						SentenceDataStruct sdata;
						sdata.Page = data->AtlasPage;
						sdata.ScreenRect.Left = Cursor.X;
						sdata.ScreenRect.Top = Cursor.Y;
						sdata.ScreenRect.Right = Cursor.X + data->Width;
						sdata.ScreenRect.Bottom = Cursor.Y + char_height;

						sdata.UVRect.Left = data->AtlasX / 256.0f;
						sdata.UVRect.Top = data->AtlasY / 256.0f;
						sdata.UVRect.Right = (data->AtlasX + data->Width) / 256.0f;
						sdata.UVRect.Bottom = (data->AtlasY + char_height) / 256.0f;
						
						SentenceData.Add(sdata);
					}
				} else {
					char_spacing += Font->Get_Extra_Overlap();
					if (ch=='M') {
						char_spacing++;
					}
				}
				Cursor.X += char_spacing;
			} else if (ch == L' ') {
				Cursor.X += char_spacing;
			} else if (ch == L'\n' || ch == 0) {
				break;
			}
		}

		Cursor.X = 0;
		Cursor.Y += char_height;
		line_width = 0;
	}

	if(hkX) *hkX = hotKeyPosX;
	if(hkY) *hkY = hotKeyPosY;
}

////////////////////////////////////////////////////////////////////////////////////
//
//	Build_Sentence_NotCentered
//
////////////////////////////////////////////////////////////////////////////////////
Vector2	Render2DSentenceClass::Build_Sentence_Not_Centered (const WCHAR *text, int *hkX, int *hkY, bool justCalcExtents)
{
	Vector2 cursor = Cursor;
	float maxX = 0;

	int hotKeyPosX = 0;
	int hotKeyPosY = 0;
	bool calcHotKeyX = false;
	bool dontBlit = false;

	if (!justCalcExtents)
	{
		Reset_Sentence_Data ();
	}
	Cursor.Set (0, 0);

	float char_height = Font->Get_Char_Height ();

	while (text != nullptr) {
		WCHAR ch = *text++;
		dontBlit = false;

		if(ParseHotKey && (ch == L'&') && (*text != 0) && (*text > L' ') && (*text != L'\n'))
		{
			hotKeyPosY = Cursor.Y;
			if (calcHotKeyX)
				hotKeyPosX = 0;
			else
				hotKeyPosX = Cursor.X;
			ch = *text++;
			dontBlit = true;
		}
		
		float char_spacing = Font->Get_Char_Spacing (ch);

		if (ch != L'\n' && ch != L' ' && ch != 0) {
			bool wordBiggerThenLine = ((useHardWordWrap) && ( WrapWidth != 0 ) &&((Cursor.X + char_spacing) >= WrapWidth));
			if (wordBiggerThenLine) {
				Cursor.X = 0;
				Cursor.Y += char_height;
			}

			if (!justCalcExtents && !dontBlit) {
				FontCharsClassCharDataStruct* data = const_cast<FontCharsClassCharDataStruct*>(Font->Get_Char_Data(ch));
				if (data) {
					FontCharsAtlasClass::Get_Instance()->Ensure_Glyph(Font, data);

					SentenceDataStruct sdata;
					sdata.Page = data->AtlasPage;
					sdata.ScreenRect.Left = Cursor.X;
					sdata.ScreenRect.Top = Cursor.Y;
					sdata.ScreenRect.Right = Cursor.X + data->Width;
					sdata.ScreenRect.Bottom = Cursor.Y + char_height;

					sdata.UVRect.Left = data->AtlasX / 256.0f;
					sdata.UVRect.Top = data->AtlasY / 256.0f;
					sdata.UVRect.Right = (data->AtlasX + data->Width) / 256.0f;
					sdata.UVRect.Bottom = (data->AtlasY + char_height) / 256.0f;
					
					SentenceData.Add(sdata);
				}
			}
			Cursor.X += char_spacing;
			maxX = max(maxX, Cursor.X);
		} else if (ch == L' ') {
			Cursor.X += char_spacing;
			maxX = max(maxX, Cursor.X);

			if (WrapWidth > 0) {
				const WCHAR *word = text;
				float word_width = 0;
				while ((*word != 0) && (*word > L' ')) {
					if(ParseHotKey && (*word == L'&') && (*word+1 != 0) && (*word+1 > L' ') && (*word+1 != L'\n'))
						*word++;
					word_width += Font->Get_Char_Spacing (*word++);
				}
				if ((Cursor.X + word_width) >= WrapWidth) {
					Cursor.X = 0;
					Cursor.Y += char_height;
					calcHotKeyX = true;
				}
			}
		} else if (ch == L'\n') {
			Cursor.X = 0;
			Cursor.Y += char_height;
		} else if (ch == 0) {
			break;
		}
	}

	Vector2 extent;
	extent.X = maxX + Font->Get_Extra_Overlap();
	extent.Y = Cursor.Y + char_height;

	Cursor = cursor;

	if(hkX) *hkX = hotKeyPosX;
	if(hkY) *hkY = hotKeyPosY;

	return extent;
}

////////////////////////////////////////////////////////////////////////////////////
//
//	Build_Sentence
//
////////////////////////////////////////////////////////////////////////////////////
void
Render2DSentenceClass::Build_Sentence (const WCHAR *text, int *hkX, int *hkY)
{
	if (text == nullptr) {
		return ;
	}

	if (Font == nullptr)
		return;

	if(Centered && (WrapWidth > 0 || wcschr(text,L'\n')))
		Build_Sentence_Centered(text, hkX, hkY);
	else
		Build_Sentence_Not_Centered(text, hkX, hkY);

}


////////////////////////////////////////////////////////////////////////////////////
//
//	FontCharsClass
//
////////////////////////////////////////////////////////////////////////////////////
FontCharsClass::FontCharsClass () :
	OldGDIFont(	nullptr ),
	OldGDIBitmap( nullptr ),
	GDIFont( nullptr ),
	GDIBitmap( nullptr ),
	GDIBitmapBits ( nullptr ),
	MemDC( nullptr ),
	CurrPixelOffset( 0 ),
	PointSize( 0 ),
	CharHeight( 0 ),
	UnicodeCharArray( nullptr ),
	FirstUnicodeChar( 0xFFFF ),
	LastUnicodeChar( 0 ),
	IsBold (false)
{
	AlternateUnicodeFont = nullptr;
	::memset( ASCIICharArray, 0, sizeof (ASCIICharArray) );
	FontCharsAtlasClass::Get_Instance()->Add_Ref_Font();
}


////////////////////////////////////////////////////////////////////////////////////
//
//	~FontCharsClass
//
////////////////////////////////////////////////////////////////////////////////////
FontCharsClass::~FontCharsClass ()
{
	while ( BufferList.Count() ) {
		delete BufferList[0];
		BufferList.Delete(0);
	}

	Free_GDI_Font();
	Free_Character_Arrays();
	FontCharsAtlasClass::Get_Instance()->Release_Font();
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Char_Data
//
////////////////////////////////////////////////////////////////////////////////////
const FontCharsClassCharDataStruct *
FontCharsClass::Get_Char_Data (WCHAR ch)
{
	const FontCharsClassCharDataStruct *retval = nullptr;

	if ( ch < 256 )
	{
		retval = ASCIICharArray[ch];
	}
 	else if ( AlternateUnicodeFont && this != AlternateUnicodeFont )
	{
		return AlternateUnicodeFont->Get_Char_Data( ch );
	}
	else
	{
		Grow_Unicode_Array( ch );
		retval = UnicodeCharArray[ch - FirstUnicodeChar];
	}

	//
	//	If the character wasn't found, then add it to our list
	//
	if ( retval == nullptr ) {
		retval = Store_GDI_Char( ch );
	}

	WWASSERT( retval->Value == ch );
	return retval;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Char_Width
//
////////////////////////////////////////////////////////////////////////////////////
int
FontCharsClass::Get_Char_Width (WCHAR ch)
{
	const FontCharsClassCharDataStruct	* data = Get_Char_Data( ch );
	if ( data != nullptr ) {
		return data->Width;
	}

	return 0;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Get_Char_Spacing
//
////////////////////////////////////////////////////////////////////////////////////
int
FontCharsClass::Get_Char_Spacing (WCHAR ch)
{
	const FontCharsClassCharDataStruct	* data = Get_Char_Data( ch );
	if ( data != nullptr ) {
		if ( data->Width != 0 ) {
			return data->Width - PixelOverlap - CharOverhang;
		}
	}

	return 0;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Blit_Char
//
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Blit_Char (WCHAR ch, uint16 *dest_ptr, int dest_stride, int x, int y)
{
	const FontCharsClassCharDataStruct	* data = Get_Char_Data( ch );
	if ( data != nullptr && data->Width != 0 ) {

		//
		//	Setup the src and destination pointers
		//
		int dest_inc		= (dest_stride >> 1);
		uint16 *src_ptr	= data->Buffer;
		dest_ptr				+= (dest_inc * y) + x;

		//
		//	Simply copy the data from the src buffer to the destination
		//
		for ( int row = 0; row < CharHeight; row ++ ) {
			for ( int col = 0; col < data->Width; col ++ ) {
				uint16 curData = *src_ptr;
				if (col<PixelOverlap) {
					curData |= dest_ptr[col];
				}
				dest_ptr[col] = curData;
				src_ptr++;
			}
			dest_ptr	+= dest_inc;
		}
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Store_GDI_Char
//
////////////////////////////////////////////////////////////////////////////////////
const FontCharsClassCharDataStruct *
FontCharsClass::Store_GDI_Char (WCHAR ch)
{
	int width	= PointSize * 2;
	int height	= PointSize * 2;

	//
	//	Draw the character into the memory DC
	//
	RECT rect = { 0, 0, width, height };
	int xOrigin = 0;
	if (ch == 'W') {
		xOrigin = 1;
	}
	::ExtTextOutW( MemDC, xOrigin, 0, ETO_OPAQUE, &rect, &ch, 1, nullptr);

	//
	//	Get the size of the character we just drew
	//
	SIZE char_size = { 0 };
	::GetTextExtentPoint32W( MemDC, &ch, 1, &char_size );
	char_size.cx += PixelOverlap + xOrigin;
	//
	//	Get a pointer to the surface that this character should use
	//
	Update_Current_Buffer( char_size.cx );
	uint16* curr_buffer_p = BufferList[BufferList.Count () - 1]->Buffer;
	curr_buffer_p += CurrPixelOffset;

	//
	//	Copy the BMP contents to the buffer
	//
	int stride = (((width * 3) + 3) & ~3);
	for (int row = 0; row < char_size.cy; row ++) {

		//
		//	Compute the indices into the BMP and surface
		//
		int index = (row * stride);

		//
		//	Loop over each column
		//
		for (int col = 0; col < char_size.cx; col ++) {

			//
			//	Get the pixel color at this location
			//
			uint8 pixel_value = GDIBitmapBits[index];
			index += 3;
#ifdef TEST_PLACEMENT
 			if (row==CharHeight-1&&col==0) {
 				pixel_value = 0xff;
 			}
 			if (row==CharHeight-2&&col==1) {
 				pixel_value = 0xff;
 			}
 			if (row==0&&col==0) {
 				pixel_value = 0xff;
 			}
 			if (row==1&&col==1) {
 				pixel_value = 0xff;
 			}
 			if (row==CharHeight-1&&col==char_size.cx-1-PixelOverlap) {
 				pixel_value = 0xff;
 			}
 			if (row==CharHeight-2&&col==char_size.cx-2-PixelOverlap) {
 				pixel_value = 0xff;
 			}
 			if (row==0&&col==char_size.cx-1-PixelOverlap) {
 				pixel_value = 0xff;
 			}
 			if (row==1&&col==char_size.cx-2-PixelOverlap) {
 				pixel_value = 0xff;
 			}
 			if (pixel_value == 0x00) {
 				pixel_value = 0x40;
 			}
#endif

			uint16 pixel_color = 0;
			if (pixel_value != 0) {
				pixel_color = 0x0FFF;
			}

			//
			//	Convert the pixel intensity from 8bit to 4bit and
			// store it in our buffer
			//
			uint8 alpha_value	= ((pixel_value >> 4) & 0xF);
			*curr_buffer_p++	= pixel_color | (alpha_value << 12);
		}
	}

	//
	//	Save information about this character in our list
	//
	FontCharsClassCharDataStruct *char_data	= W3DNEW FontCharsClassCharDataStruct;
	char_data->Value				= ch;
	char_data->Width				= char_size.cx;
	char_data->Buffer				= BufferList[BufferList.Count () - 1]->Buffer + CurrPixelOffset;
	char_data->AtlasPage = -1;

	//
	//	Insert this character into our array
	//
	if ( ch < 256 ) {
		ASCIICharArray[ch] = char_data;
	} else {
		UnicodeCharArray[ch - FirstUnicodeChar] = char_data;
	}

	//
	//	Advance the character position
	//
	CurrPixelOffset += ((char_size.cx+PixelOverlap) * CharHeight);

	//
	//	Return the index of the entry we just added
	//
	return char_data;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Update_Current_Buffer
//
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Update_Current_Buffer (int char_width)
{
	//
	//	Check to see if we need to allocate a new buffer
	//
	bool needs_new_buffer = (BufferList.Count () == 0);
	if (needs_new_buffer == false) {

		//
		//	Would we extend past this buffer?
		//
		if ( (CurrPixelOffset + (char_width * CharHeight)) > CHAR_BUFFER_LEN ) {
			needs_new_buffer = true;
		}
	}

	//
	//	Do we need to create a new surface?
	//
	if (needs_new_buffer)
	{
		FontCharsBuffer* new_buffer = W3DNEW FontCharsBuffer;
		BufferList.Add( new_buffer );
		CurrPixelOffset = 0;
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Create_GDI_Font
//
////////////////////////////////////////////////////////////////////////////////////
bool
FontCharsClass::Create_GDI_Font (const char *font_name)
{
	HDC screen_dc = ::GetDC ((HWND)WW3D::Get_Window());

	const char *fontToUseForGenerals = "Arial";
	bool doingGenerals = false;
	if (strcmp(font_name, "Generals")==0) {
		font_name = fontToUseForGenerals;
		doingGenerals = true;
	}

	//
	//	Calculate the height of the font in logical units
	//
	const int dotsPerInch = 96; // always use 96.	jba.
	int font_height = -MulDiv (PointSize, dotsPerInch, 72);

	int fontWidth = 0; // use font default.
	if (doingGenerals) {
		//fontWidth = -font_height*0.35f; //2 pixels tighter.
		fontWidth = -font_height*0.40f; // one pixel tighter
	}
	PixelOverlap = (-font_height)/8;

	// Sanity check in case of perversion. :)
	if (PixelOverlap<0) PixelOverlap = 0;
	if (PixelOverlap>4) PixelOverlap = 4;
	//
	//	Create the Windows font
	//
	DWORD bold		= IsBold ? FW_BOLD : FW_NORMAL;
	DWORD italic	= 0;
	GDIFont			= ::CreateFont (font_height, fontWidth, 0, 0, bold, italic,
								FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
								CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
								VARIABLE_PITCH, font_name);

	//
	// Set-up the fields of the BITMAPINFOHEADER
	//	Note: Top-down DIBs use negative height in Win32.
	//
	BITMAPINFOHEADER bitmap_info = { 0 };
	bitmap_info.biSize				= sizeof (BITMAPINFOHEADER);
	bitmap_info.biWidth				= PointSize * 2;
	bitmap_info.biHeight				= -(PointSize * 2);
	bitmap_info.biPlanes				= 1;
	bitmap_info.biBitCount			= 24;
	bitmap_info.biCompression		= BI_RGB;
	bitmap_info.biSizeImage			= ((PointSize * PointSize * 4) * 3);
	bitmap_info.biXPelsPerMeter	= 0;
	bitmap_info.biYPelsPerMeter	= 0;
	bitmap_info.biClrUsed			= 0;
	bitmap_info.biClrImportant		= 0;

	//
	// Create a bitmap that we can access the bits directly of
	//
	GDIBitmap	= ::CreateDIBSection (	screen_dc,
													(const BITMAPINFO *)&bitmap_info,
													DIB_RGB_COLORS,
													(void **)&GDIBitmapBits,
													nullptr,
													0L);

	//
	//	Create a device context we can select the font and bitmap into
	//
	MemDC = ::CreateCompatibleDC (screen_dc);

	//
	// Release our temporary screen DC
	//
	::ReleaseDC ((HWND)WW3D::Get_Window(), screen_dc);

	//
	//	Now select the BMP and font into the DC
	//
	OldGDIBitmap	= (HBITMAP)::SelectObject (MemDC, GDIBitmap);
	OldGDIFont		= (HFONT)::SelectObject (MemDC, GDIFont);
	::SetBkColor (MemDC, RGB (0, 0, 0));
	::SetTextColor (MemDC, RGB (255, 255, 255));

	//
	//	Lookup the pixel height of the font
	//
	TEXTMETRIC text_metric = { 0 };
	::GetTextMetrics (MemDC, &text_metric);
	CharHeight = text_metric.tmHeight;
	CharAscent = text_metric.tmAscent;
	CharOverhang = text_metric.tmOverhang;
	if (doingGenerals) {
		CharOverhang = 0;
	}

	return GDIFont != nullptr && GDIBitmap != nullptr;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Free_GDI_Font
//
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Free_GDI_Font ()
{
	//
	//	Select the old font back into the DC and delete
	// our font object
	//
	if ( GDIFont != nullptr ) {
		::SelectObject( MemDC, OldGDIFont );
		::DeleteObject( GDIFont );
		GDIFont = nullptr;
	}

	//
	//	Select the old bitmap back into the DC and delete
	// our bitmap object
	//
	if ( GDIBitmap != nullptr ) {
		::SelectObject( MemDC, OldGDIBitmap );
		::DeleteObject( GDIBitmap );
		GDIBitmap = nullptr;
	}

	//
	//	Delete our memory DC
	//
	if ( MemDC != nullptr ) {
		::DeleteDC( MemDC );
		MemDC = nullptr;
	}
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Initialize_GDI_Font
//
////////////////////////////////////////////////////////////////////////////////////
bool
FontCharsClass::Initialize_GDI_Font (const char *font_name, int point_size, bool is_bold)
{
	//
	//	Build a unique name from the font name and its size
	//
	Name.Format ("%s%d", font_name, point_size);

	//
	//	Remember these settings
	//
	GDIFontName	= font_name;
	PointSize	= point_size;
	IsBold		= is_bold;

	//
	//	Create the actual font object
	//
	return Create_GDI_Font (font_name);
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Is_Font
//
////////////////////////////////////////////////////////////////////////////////////
bool
FontCharsClass::Is_Font (const char *font_name, int point_size, bool is_bold)
{
	bool retval = false;

	//
	//	Check to see if both the name and height matches...
	//
	if (	(GDIFontName.Compare_No_Case (font_name) == 0) &&
			(point_size == PointSize) &&
			(is_bold == IsBold))
	{
		retval = true;
	}

	return retval;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Grow_Unicode_Array
//
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Grow_Unicode_Array (WCHAR ch)
{
	//
	//	Don't do anything if character is in the ASCII range
	//
	if ( ch < 256 ) {
		return ;
	}

	//
	//	Don't do anything if character is in the currently allocated range
	//
	if ( ch >= FirstUnicodeChar && ch <= LastUnicodeChar ) {
		return ;
	}

	uint16 first_index	= min( FirstUnicodeChar, static_cast<uint16>(ch) );
	uint16 last_index		= max( LastUnicodeChar, static_cast<uint16>(ch) );
	uint16 count			= (last_index - first_index) + 1;

	//
	//	Allocate enough memory to hold the new cells
	//
	FontCharsClassCharDataStruct **new_array = W3DNEWARRAY FontCharsClassCharDataStruct *[count];
	::memset (new_array, 0, sizeof (FontCharsClassCharDataStruct *) * count);

	//
	//	Copy the contents of the old array into the new array
	//
	if ( UnicodeCharArray != nullptr ) {
		int start_offset	= (FirstUnicodeChar - first_index);
		int old_count		= (LastUnicodeChar - FirstUnicodeChar) + 1;
		::memcpy (&new_array[start_offset], UnicodeCharArray, sizeof (FontCharsClassCharDataStruct *) * old_count);

		//
		//	Delete the old array
		//
		delete [] UnicodeCharArray;
		UnicodeCharArray = nullptr;
	}

	FirstUnicodeChar	= first_index;
	LastUnicodeChar	= last_index;
	UnicodeCharArray	= new_array;
}


////////////////////////////////////////////////////////////////////////////////////
//
//	Free_Character_Arrays
//
////////////////////////////////////////////////////////////////////////////////////
void
FontCharsClass::Free_Character_Arrays ()
{
	if ( UnicodeCharArray != nullptr ) {

		int count = (LastUnicodeChar - FirstUnicodeChar) + 1;

		//
		//	Delete each member of the unicode array
		//
		for (int index = 0; index < count; index ++) {
			delete UnicodeCharArray[index];
			UnicodeCharArray[index] = nullptr;
		}

		//
		//	Delete the array itself
		//
		delete [] UnicodeCharArray;
		UnicodeCharArray = nullptr;
	}

	//
	//	Delete each member of the ascii character array
	//
	for (int index = 0; index < 256; index ++) {
		delete ASCIICharArray[index];
		ASCIICharArray[index] = nullptr;
	}
}
