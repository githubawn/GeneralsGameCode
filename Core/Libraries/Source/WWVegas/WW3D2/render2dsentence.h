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
 *                     $Archive:: /Commando/Code/ww3d2/render2dsentence.h                     $*
 *                                                                                             *
 *                       Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                     $Modtime:: 8/29/01 10:58a                                              $*
 *                                                                                             *
 *                    $Revision:: 6                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "always.h"
#include "render2d.h"
#include "Vector.h"
#include "vector2i.h"
#include "wwstring.h"
#include "win.h"

/*
** FontCharsClass
*/
class	SurfaceClass;

//
//	Private data structures
//
class FontCharsClassCharDataStruct
{
	W3DMPO_CODE(FontCharsClassCharDataStruct)
public:
	WCHAR				Value;
	short				Width;
	uint16 *		Buffer;

	short				AtlasPage;
	short				AtlasX;
	short				AtlasY;
};

enum { CHAR_BUFFER_LEN		= 32768 };

class FontCharsBuffer
{
	W3DMPO_CODE(FontCharsBuffer)
public:
	uint16			Buffer[CHAR_BUFFER_LEN];
};


class FontCharsClass : public RefCountClass
{
	W3DMPO_CODE(FontCharsClass)

public:
	FontCharsClass();
	virtual ~FontCharsClass() override;

	// TR: Hack for unicode font support
	FontCharsClass					*AlternateUnicodeFont;


	bool	Initialize_GDI_Font( const char *font_name, int point_size, bool is_bold );
	bool	Is_Font( const char *font_name, int point_size, bool is_bold );
	const char * Get_Name()			{ return Name; }

	int	Get_Char_Height()			{ return CharHeight; }
	int	Get_Char_Width( WCHAR ch );
	int	Get_Char_Spacing( WCHAR ch );

	int Get_Extra_Overlap() {return PixelOverlap;}

	void	Blit_Char( WCHAR ch, uint16 *dest_ptr, int dest_stride, int x, int y );

public:
	const FontCharsClassCharDataStruct* Get_Char_Data( WCHAR ch );
private:

	//
	//	Private methods
	//
	bool							Create_GDI_Font( const char *font_name );
	void							Free_GDI_Font();
	const FontCharsClassCharDataStruct *	Store_GDI_Char( WCHAR ch );
	void							Update_Current_Buffer( int char_width );

	void							Grow_Unicode_Array( WCHAR ch );
	void							Free_Character_Arrays();

	//
	//	Private member data
	//
	StringClass							Name;
	DynamicVectorClass<FontCharsBuffer*>	BufferList;
	int									CurrPixelOffset;
	int									CharHeight;
	int									CharAscent;
	int									CharOverhang;
	int									PixelOverlap;
	int									PointSize;
	StringClass							GDIFontName;
	HFONT									OldGDIFont;
	HBITMAP								OldGDIBitmap;
	HBITMAP								GDIBitmap;
	HFONT									GDIFont;
	uint8 *								GDIBitmapBits;
	HDC									MemDC;
	FontCharsClassCharDataStruct *					ASCIICharArray[256];
	FontCharsClassCharDataStruct **					UnicodeCharArray;
	uint16								FirstUnicodeChar;
	uint16								LastUnicodeChar;
	bool									IsBold;
};

/*
** FontCharsAtlasClass
*/

class FontCharsAtlasClass
{
public:
	static FontCharsAtlasClass* Get_Instance();
	static void _Shutdown();

	void Ensure_Glyph(FontCharsClass* font, FontCharsClassCharDataStruct* data);
	void Flush_Updates();
	void Release_Font();
	void Add_Ref_Font();
	void Dump_Atlas(const char* prefix = "font_atlas");

	int Get_Page_Count() const { return Pages.Count(); }
	TextureClass* Get_Page_Texture(int page) { return Pages[page].Texture; }

	struct PageStruct {
		TextureClass* Texture;
		SurfaceClass* Staging;
		int CurrentX;
		int CurrentY;
		int ShelfHeight;
		int DirtyMinY;
		int DirtyMaxY;

		friend bool operator==(const PageStruct& a, const PageStruct& b) {
			return a.Texture == b.Texture && a.Staging == b.Staging && a.CurrentX == b.CurrentX && a.CurrentY == b.CurrentY && a.ShelfHeight == b.ShelfHeight && a.DirtyMinY == b.DirtyMinY && a.DirtyMaxY == b.DirtyMaxY;
		}
		friend bool operator!=(const PageStruct& a, const PageStruct& b) {
			return !(a == b);
		}
	};

private:
	FontCharsAtlasClass();
	~FontCharsAtlasClass();

	DynamicVectorClass<PageStruct> Pages;
	int FontRefCount;

	static FontCharsAtlasClass* Instance;
};

/*
** Render2DSentenceClass
*/
class Render2DSentenceClass {
public:
	//Render2DSentenceClass( FontCharsClass * font );
	Render2DSentenceClass();
	~Render2DSentenceClass();

	void				Render ();
	virtual	void	Reset ();
	void				Reset_Polys ();

	FontCharsClass *	Peek_Font()						{ return Font; }
	void	Set_Font( FontCharsClass *font );

	void	Set_Location( const Vector2 & loc );
	void	Set_Base_Location( const Vector2 & loc );
	bool	Set_Wrapping_Width (float width)					{ if(WrapWidth == width)
																											return false;
																										WrapWidth = width;
																										return true;	}
	bool	Set_Word_Wrap_Centered( bool isCentered ) { if(Centered == isCentered)
																											return false;
																										Centered = isCentered;
																										return true;}
	void Set_Hot_Key_Parse( bool parseHotKey ){ ParseHotKey = parseHotKey; }
	void Set_Use_Hard_Word_Wrap( bool useHardWrap){ useHardWordWrap = useHardWrap;	}
	//
	// Clipping support
	//
	void	Set_Clipping_Rect( const RectClass &rect )	{ ClipRect = rect; IsClippedEnabled = true; }
	bool	Is_Clipping_Enabled() const				{ return IsClippedEnabled; }
	void	Enable_Clipping( bool onoff )						{ IsClippedEnabled = onoff; }

	//
	//	Shader modification support
	//
	void			Make_Additive ();
	ShaderClass	Get_Shader () const						{ return Shader; }
	void			Set_Shader (ShaderClass shader);

//	void	Draw_Block( const RectClass & screen, unsigned long color = 0xFFFFFFFF );

	const RectClass & Get_Draw_Extents()			{ return DrawExtents; }
////	const Vector2 & Get_Cursor()						{ return Cursor; }

	Vector2	Get_Text_Extents( const WCHAR * text );
	Vector2	Get_Formatted_Text_Extents( const WCHAR * text );

	//
	//	Sentence control
	//
	void	Build_Sentence (const WCHAR *text, int *hkX, int *hkY);
	void	Draw_Sentence (uint32 color = 0xFFFFFFFF);
	
	void	Add_Quads_To(Render2DClass &target, int page);

	//
	//	Texture hint
	//
	void	Set_Texture_Size_Hint( int hint )				{ TextureSizeHint = hint; }
	int	Get_Texture_Size_Hint() const				{ return TextureSizeHint; }

	void	Set_Mono_Spaced( bool onoff )						{ MonoSpaced = onoff; }

public:
	//
	//	Public structures
	//
	struct SentenceDataStruct {
		short				Page;
		RectClass			ScreenRect;
		RectClass			UVRect;

		friend bool operator==(const SentenceDataStruct& a, const SentenceDataStruct& b) {
			return a.Page == b.Page && 
				a.ScreenRect.Left == b.ScreenRect.Left && a.ScreenRect.Right == b.ScreenRect.Right &&
				a.ScreenRect.Top == b.ScreenRect.Top && a.ScreenRect.Bottom == b.ScreenRect.Bottom &&
				a.UVRect.Left == b.UVRect.Left && a.UVRect.Right == b.UVRect.Right &&
				a.UVRect.Top == b.UVRect.Top && a.UVRect.Bottom == b.UVRect.Bottom;
		}
		friend bool operator!=(const SentenceDataStruct& a, const SentenceDataStruct& b) {
			return !(a == b);
		}
	};

	struct QuadStruct {
		short				Page;
		RectClass			ScreenRect;
		RectClass			UVRect;
		uint32				Color;

		friend bool operator==(const QuadStruct& a, const QuadStruct& b) {
			return a.Page == b.Page && a.Color == b.Color && 
				a.ScreenRect.Left == b.ScreenRect.Left && a.ScreenRect.Right == b.ScreenRect.Right &&
				a.ScreenRect.Top == b.ScreenRect.Top && a.ScreenRect.Bottom == b.ScreenRect.Bottom &&
				a.UVRect.Left == b.UVRect.Left && a.UVRect.Right == b.UVRect.Right &&
				a.UVRect.Top == b.UVRect.Top && a.UVRect.Bottom == b.UVRect.Bottom;
		}
		friend bool operator!=(const QuadStruct& a, const QuadStruct& b) {
			return !(a == b);
		}
	};

private:
	//
	//	Private methods
	//
	void	Reset_Sentence_Data ();
	void	Build_Sentence_Centered (const WCHAR *text, int *hkX, int *hkY);
	Vector2	Build_Sentence_Not_Centered (const WCHAR *text, int *hkX, int *hkY,bool justCalcExtents = false );
	//
	//	Private member data
	//
	DynamicVectorClass<SentenceDataStruct>		SentenceData;
	DynamicVectorClass<QuadStruct>				DrawQuads;
	FontCharsClass	*						Font;
	Vector2											BaseLocation;
	Vector2											Location;
	Vector2											Cursor;
	int													TextureSizeHint;
	bool												MonoSpaced;
	float												WrapWidth;
	bool												Centered;			// Determines whether or not to center each line
	RectClass										ClipRect;
	RectClass										DrawExtents;
	bool												IsClippedEnabled;
	bool												ParseHotKey;
	bool												useHardWordWrap;

	ShaderClass									Shader;
};
