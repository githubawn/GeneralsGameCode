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

/******************************************************************************
*
* NAME
*     $Archive:  $
*
* DESCRIPTION
*     Web Browser
*
* PROGRAMMER
*     Bryan Cleveland
*     $Author:  $
*
* VERSION INFO
*     $Revision:  $
*     $Modtime:  $
*
******************************************************************************/

#pragma once

#include "Common/SubsystemInterface.h"
#include <windows.h>
#include <Common/GameMemory.h>
#include <Lib/BaseType.h>
// TheSuperHackers @build bobtista 13/06/2026 The WOL embedded browser is a
// COM/ATL component (Windows-only). Guard the ATL bits; WebBrowserURL (INI
// data) stays cross-platform.
#if defined(_WIN32)
#include <atlbase.h>
#include "EABrowserDispatch/BrowserDispatch.h"
#include "FEBDispatch.h"
#endif

class GameWindow;

class WebBrowserURL : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( WebBrowserURL, "WebBrowserURL" )

public:

	WebBrowserURL();
	// virtual destructor prototype defined by memory pool object

	const FieldParse *getFieldParse() const { return m_URLFieldParseTable; }

	AsciiString m_tag;
	AsciiString m_url;

	WebBrowserURL *m_next;

	static const FieldParse m_URLFieldParseTable[];		///< the parse table for INI definition

};



#if defined(_WIN32)
class WebBrowser :
		public FEBDispatch<WebBrowser, IBrowserDispatch, &IID_IBrowserDispatch>,
		public SubsystemInterface
	{
	public:
		virtual void init() override;
		virtual void reset() override;
		virtual void update() override;

		// Create an instance of the embedded browser for Dune Emperor.
		virtual Bool createBrowserWindow(const char *tag, GameWindow *win) = 0;
		virtual void closeBrowserWindow(GameWindow *win) = 0;

		WebBrowserURL *makeNewURL(AsciiString tag);
		WebBrowserURL *findURL(AsciiString tag);

	protected:
		// Protected to prevent direct construction via new, use CreateInstance() instead.
		WebBrowser();
		virtual ~WebBrowser() override;

		// Protected to prevent copy and assignment
		WebBrowser(const WebBrowser&);
		const WebBrowser& operator=(const WebBrowser&);

//		Bool RetrievePageURL(const char* page, char* url, int size);
//		Bool RetrieveHTMLPath(char* path, int size);

	protected:
		ULONG mRefCount;
		WebBrowserURL *m_urlList;

	//---------------------------------------------------------------------------
	// IUnknown methods
	//---------------------------------------------------------------------------
	protected:
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) IUNKNOWN_NOEXCEPT override;
		ULONG STDMETHODCALLTYPE AddRef() IUNKNOWN_NOEXCEPT override;
		ULONG STDMETHODCALLTYPE Release() IUNKNOWN_NOEXCEPT override;

	//---------------------------------------------------------------------------
	// IBrowserDispatch methods
	//---------------------------------------------------------------------------
	public:
		STDMETHOD(TestMethod)(Int num1);
	};

extern CComObject<WebBrowser> *TheWebBrowser;

#else // !_WIN32

// Non-Windows no-op stub: the embedded browser (COM/ATL) does nothing here.
// Keeps WebBrowser.cpp's URL-list code and the WOL menu callers building.
class WebBrowser : public SubsystemInterface
{
public:
	virtual void init() override;
	virtual void reset() override;
	virtual void update() override;

	virtual Bool createBrowserWindow(const char *tag, GameWindow *win) { return FALSE; }
	virtual void closeBrowserWindow(GameWindow *win) {}

	WebBrowserURL *makeNewURL(AsciiString tag);
	WebBrowserURL *findURL(AsciiString tag);

	WebBrowser();
	virtual ~WebBrowser() override;

protected:
	ULONG mRefCount;
	WebBrowserURL *m_urlList;
};

extern WebBrowser *TheWebBrowser;

#endif // _WIN32
