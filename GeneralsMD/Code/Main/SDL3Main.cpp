/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#if defined(SAGE_USE_SDL3)

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
// Defined in Core/.../GameNetwork/udp.cpp (__EMSCRIPTEN__) — opens the LAN relay WebSocket.
extern "C" void ggc_ws_connect(void);
#endif

#include <SDL3/SDL.h>
// TheSuperHackers @build bobtista 13/06/2026 On Android SDLActivity invokes the
// "SDL_main" symbol via JNI; SDL_main.h remaps main->SDL_main so the entry point
// is exported from libz_generals.so. Harmless on desktop SDL3 targets.
#include <SDL3/SDL_main.h>
// TheSuperHackers @build githubawn 18/06/2026 iOS asset-dir redirect needs these.
#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#include <unistd.h>   // chdir
#include <cstdlib>    // getenv, setenv
#include <cstdio>     // snprintf
#endif
#endif
#if defined(__ANDROID__)
#include <SDL3/SDL_system.h>  // SDL_GetAndroidExternalStoragePath
#include <unistd.h>            // chdir
#include <cstdlib>            // setenv
#include <signal.h>
#include <unwind.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <android/log.h>

// TheSuperHackers @build bobtista 13/06/2026 Crash backtrace handler. Android's
// debuggerd does not produce a tombstone for stack-overflow SIGSEGVs, so install
// our own handler on an alternate signal stack that unwinds and symbolizes via
// dladdr -> logcat (tag "ggc-crash").
namespace
{
	struct GgcBtState { void **current; void **end; };

	_Unwind_Reason_Code ggc_unwind_cb(_Unwind_Context *ctx, void *arg)
	{
		GgcBtState *st = (GgcBtState *)arg;
		uintptr_t pc = _Unwind_GetIP(ctx);
		if (pc != 0 && st->current != st->end) {
			*st->current++ = (void *)pc;
		}
		return _URC_NO_REASON;
	}

	void ggc_crash_handler(int sig)
	{
		void *frames[64];
		GgcBtState st = { frames, frames + 64 };
		_Unwind_Backtrace(ggc_unwind_cb, &st);
		int n = (int)(st.current - frames);
		__android_log_print(6, "ggc-crash", "*** signal %d, %d frames ***", sig, n);
		for (int i = 0; i < n; i++) {
			Dl_info info;
			const char *sym = "?";
			char demangled[512];
			uintptr_t off = 0;
			if (dladdr(frames[i], &info) && info.dli_sname) {
				int status = 0;
				size_t len = sizeof(demangled);
				char *d = abi::__cxa_demangle(info.dli_sname, demangled, &len, &status);
				sym = (status == 0 && d) ? d : info.dli_sname;
				off = (uintptr_t)frames[i] - (uintptr_t)info.dli_saddr;
			}
			__android_log_print(6, "ggc-crash", "  #%02d %s+%zu", i, sym, (size_t)off);
		}
		_exit(1);
	}

	void ggc_install_crash_handler()
	{
		static char alt_stack[SIGSTKSZ < 65536 ? 65536 : SIGSTKSZ];
		stack_t ss;
		ss.ss_sp = alt_stack;
		ss.ss_size = sizeof(alt_stack);
		ss.ss_flags = 0;
		sigaltstack(&ss, nullptr);
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = ggc_crash_handler;
		sa.sa_flags = SA_ONSTACK;
		sigaction(SIGSEGV, &sa, nullptr);
		sigaction(SIGABRT, &sa, nullptr);
	}
}
#endif

#include "Common/GameEngine.h"
#include "Common/GameMemory.h"
#include "Common/GlobalData.h"
#include "Common/CriticalSection.h"
#include "Common/version.h"
#include "BuildVersion.h"
#include "GeneratedVersion.h"
#include "SDL3GameEngine.h"

// TheSuperHackers @build bobtista 13/06/2026 The memory manager and the string /
// memory-pool critical sections are set up by WinMain on Windows; the SDL entry
// must do the same before any engine code (which uses AsciiString) runs.
static CriticalSection ggcCritSecAscii, ggcCritSecUnicode, ggcCritSecDma, ggcCritSecPool, ggcCritSecDebugLog;

namespace
{
	const char * const kWindowTitle = "Command & Conquer Generals Zero Hour";
	const int kDefaultWindowWidth = 800;
	const int kDefaultWindowHeight = 600;
}

int __argc = 0;
char **__argv = NULL;

// TheSuperHackers @build bobtista 13/06/2026 String/CSF file paths normally
// defined in WinMain.cpp (Windows entry); provide them here for the SDL entry.
const char *g_strFile = "data\\Generals.str";
const char *g_csfFile = "data\\%s\\Generals.csf";

SDL_Window *TheSDL3Window = NULL;
// TheSuperHackers @build githubawn 17/06/2026 On Apple (macOS/iOS) bgfx needs a
// CAMetalLayer as its native window handle. SDL_Metal_CreateView gives us one;
// BgfxBackend reads it from this global (declared extern there). Defined
// unconditionally so the symbol exists on every platform; only set on Apple.
void *TheSDL3MetalLayer = NULL;
void *ApplicationHWnd = NULL;

extern Int GameMain();
// TheSuperHackers @build bobtista 13/06/2026 Defined in CommandLine.cpp; creates
// TheWritableGlobalData. WinMain reaches it via parseCommandLineForStartup(); the
// SDL entry has no command line, so call it directly.
extern void createGlobalData();

int main(int argc, char **argv)
{
	__argc = argc;
	__argv = argv;

#if defined(__ANDROID__)
	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
	// TheSuperHackers @feature bobtista 15/06/2026 We translate touch events to
	// left-mouse ourselves (SDL3Mouse::addSDL3FingerEvent); disable SDL's own
	// touch->mouse synthesis so a single tap does not produce duplicate clicks.
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
#endif

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
	{
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return 1;
	}

	// TheSuperHackers @build bobtista 13/06/2026 Initialize the memory manager
	// before any engine code runs (AsciiString and the memory pools rely on it).
	// Mirrors WinMain.cpp.
	TheAsciiStringCriticalSection = &ggcCritSecAscii;
	TheUnicodeStringCriticalSection = &ggcCritSecUnicode;
	TheDmaCriticalSection = &ggcCritSecDma;
	TheMemoryPoolCriticalSection = &ggcCritSecPool;
	TheDebugLogCriticalSection = &ggcCritSecDebugLog;
	initMemoryManager();

	// TheSuperHackers @build bobtista 13/06/2026 Create the version singleton
	// (WinMain does this before GameMain; the engine logs the product string
	// during init via TheVersion).
	TheVersion = NEW Version;
	TheVersion->setVersion(VERSION_MAJOR, VERSION_MINOR, VERSION_BUILDNUM, VERSION_LOCALBUILDNUM,
		AsciiString(VERSION_BUILDUSER), AsciiString(VERSION_BUILDLOC),
		AsciiString(__TIME__), AsciiString(__DATE__));

	// Create the global data singleton (TheWritableGlobalData) before GameMain.
	createGlobalData();

#if defined(__EMSCRIPTEN__)
	// Mount IndexedDB filesystem for options/saves persistence.
	EM_ASM({
		FS.mkdir('/preferences');
		FS.mount(IDBFS, {}, '/preferences');
		FS.syncfs(true, function (err) {
			if (err) {
				console.error("Failed to sync IndexedDB: ", err);
			} else {
				console.log("IndexedDB sync success.");
			}
			// TheSuperHackers @feature githubawn 27/06/2026 Seed a default Skirmish.ini in
			// the user-data dir (getPath_UserData = GENERALS_USER_DIR + leaf). FPS is the
			// skirmish game-speed; default it to 61 (uncapped) on web so the match isn't
			// pinned to 30. Written after syncfs so the IDBFS load doesn't clobber it.
			try {
				var dir = '/preferences/Command and Conquer Generals Zero Hour Data';
				FS.mkdirTree(dir);
				var ini =
					'Color = -1\n' +
					'FPS = 61\n' +
					'Map = maps/alpine assault/alpine assault.map\n' +
					'PlayerTemplate = -1\n' +
					'StartingCash = 1000000\n' +
					'SuperweaponRestrict = No\n' +
					'UserName = Player\n';
				FS.writeFile(dir + '/Skirmish.ini', ini);
				FS.syncfs(false, function () {});
			} catch (e) { console.error('Skirmish.ini seed failed', e); }
		});
	});
	setenv("GENERALS_USER_DIR", "/preferences", 1);

	// TheSuperHackers @feature githubawn 27/06/2026 Start connecting to the LAN WebSocket
	// relay (relay.py) early so the socket is open well before the user reaches the
	// Multiplayer LAN lobby. See udp.cpp (__EMSCRIPTEN__).
	ggc_ws_connect();
	// Note: the ?ip=N local-IP override is applied in IPEnumeration::getAddresses (post
	// engine-init), not here, because GlobalData re-init would clobber an early m_defaultIP.
#endif

#if defined(__ANDROID__)
	// Install after SDL_Init so SDL's own handlers don't replace ours.
	ggc_install_crash_handler();
#endif

#if defined(__ANDROID__)
	// TheSuperHackers @build bobtista 13/06/2026 The engine loads its .big
	// archives and Data/ tree relative to the working directory. On Android the
	// process starts with cwd "/", so switch to the app's external files dir
	// where the game assets were pushed, and point the user-data root there too.
	{
		const char *assetDir = SDL_GetAndroidExternalStoragePath();
		if (assetDir != NULL && assetDir[0] != '\0')
		{
			if (chdir(assetDir) == 0)
			{
				SDL_Log("[ggc] asset dir (cwd): %s", assetDir);
			}
			else
			{
				SDL_Log("[ggc] chdir to asset dir failed: %s", assetDir);
			}
			setenv("GENERALS_USER_DIR", assetDir, 1);
		}
		else
		{
			SDL_Log("[ggc] SDL_GetAndroidExternalStoragePath returned null");
		}
	}
#endif

#if defined(__APPLE__) && TARGET_OS_IPHONE
	// TheSuperHackers @build githubawn 18/06/2026 On iOS the app is sandboxed and
	// starts with an arbitrary cwd; the engine loads its .big archives and Data/
	// tree relative to the working directory. Point cwd (and the user-data root)
	// at the app's Documents container, where the game assets are placed (and
	// which UIFileSharingEnabled exposes in the Files app).
	{
		const char *home = getenv("HOME");
		if (home != NULL && home[0] != '\0')
		{
			char docs[1024];
			snprintf(docs, sizeof(docs), "%s/Documents", home);
			const int chdirRc = chdir(docs);
			if (chdirRc == 0)
			{
				SDL_Log("[ggc] asset dir (cwd): %s", docs);
			}
			else
			{
				SDL_Log("[ggc] chdir to asset dir failed: %s", docs);
			}
			setenv("GENERALS_USER_DIR", docs, 1);

			// TheSuperHackers @build githubawn 18/06/2026 Boot diagnostic: write a
			// file (readable over USB via devicectl) recording cwd and whether the
			// game data is reachable, so a crash-on-launch can be diagnosed without
			// the iOS console.
			{
				char logp[1200];
				snprintf(logp, sizeof(logp), "%s/ggc_ios_boot.log", docs);
				FILE *bl = fopen(logp, "w");
				if (bl != NULL)
				{
					char cwd[1024]; cwd[0] = '\0';
					getcwd(cwd, sizeof(cwd));
					fprintf(bl, "chdirRc=%d cwd=%s\n", chdirRc, cwd);
					FILE *big = fopen("GeneralsZH.big", "rb");
					fprintf(bl, "GeneralsZH.big=%s\n", big ? "OPEN_OK" : "OPEN_FAIL");
					if (big) fclose(big);
					FILE *ini = fopen("INIZH.big", "rb");
					fprintf(bl, "INIZH.big=%s\n", ini ? "OPEN_OK" : "OPEN_FAIL");
					if (ini) fclose(ini);
					fprintf(bl, "reached: pre-GameMain\n");
					fclose(bl);
				}
			}
		}
		else
		{
			SDL_Log("[ggc] HOME not set; cannot locate Documents asset dir");
		}
	}
#endif

	Uint32 windowFlags = SDL_WINDOW_RESIZABLE;
#if defined(__APPLE__)
	// TheSuperHackers @feature githubawn 28/06/2026 The desktop-GL build needs a
	// GL-capable window (bgfx attaches its own NSOpenGLContext to the contentView);
	// the Metal/MoltenVK builds need a Metal-backed view. Pick the matching flag.
#if defined(GGC_BGFX_RENDERER_GLSL)
	windowFlags |= SDL_WINDOW_OPENGL;
#else
	windowFlags |= SDL_WINDOW_METAL;
#endif
#endif
	int windowW = kDefaultWindowWidth;
	int windowH = kDefaultWindowHeight;
	{
		const SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
		const SDL_DisplayMode *desktopMode = SDL_GetDesktopDisplayMode(primaryDisplay);
		if (desktopMode != nullptr && desktopMode->w > 0 && desktopMode->h > 0)
		{
			windowW = desktopMode->w;
			windowH = desktopMode->h;
		}
	}
	bool wantWindowed = false;
	for (int argi = 1; argi < argc; ++argi)
	{
		if (strcmp(argv[argi], "-win") == 0)
		{
			wantWindowed = true;
			windowW = kDefaultWindowWidth;
			windowH = kDefaultWindowHeight;
			break;
		}
	}
	TheSDL3Window = SDL_CreateWindow(kWindowTitle, windowW, windowH, windowFlags);
	if (TheSDL3Window == NULL)
	{
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		SDL_Quit();
		return 1;
	}
	if (!wantWindowed)
	{
		SDL_SetWindowFullscreenMode(TheSDL3Window, nullptr);
		SDL_SetWindowFullscreen(TheSDL3Window, true);
		SDL_SyncWindow(TheSDL3Window);
	}

#if defined(__APPLE__) && !defined(GGC_BGFX_RENDERER_GLSL)
	// TheSuperHackers @build githubawn 17/06/2026 Create a Metal-backed view and
	// hand its CAMetalLayer to bgfx (via TheSDL3MetalLayer). Without this, bgfx
	// would make its own layer on the content view and fight SDL's.
	// Skipped for the desktop-GL build: bgfx's GL backend installs an
	// NSOpenGLContext on the contentView instead, and a CAMetalLayer there would
	// leave GL with no drawable.
	{
		SDL_MetalView metalView = SDL_Metal_CreateView(TheSDL3Window);
		if (metalView != NULL)
		{
			TheSDL3MetalLayer = SDL_Metal_GetLayer(metalView);
		}
		else
		{
			SDL_Log("SDL_Metal_CreateView failed: %s", SDL_GetError());
		}
	}
#endif

#if defined(__ANDROID__)
	// TheSuperHackers @bugfix bobtista 15/06/2026 On Android the SurfaceView is
	// created asynchronously and the fullscreen/orientation transition destroys
	// and recreates it. Engine init (which calls bgfx::init ~several seconds
	// later) does not pump SDL events, so if we proceed before the surface has
	// settled, bgfx binds a stale/destroyed ANativeWindow and crashes inside
	// ANativeWindow_setBuffersGeometry (or renders nothing). Pump events here
	// until the native window handle is non-null and stable for a short window.
	{
		void *prevWin = nullptr;
		int stableFrames = 0;
		for (int i = 0; i < 300 && stableFrames < 12; ++i)
		{
			SDL_PumpEvents();
			SDL_Event ev;
			while (SDL_PollEvent(&ev)) { /* drain pending events */ }
			SDL_PropertiesID props = SDL_GetWindowProperties(TheSDL3Window);
			void *nwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, NULL);
			if (nwh != nullptr && nwh == prevWin)
			{
				++stableFrames;
			}
			else
			{
				stableFrames = 0;
			}
			prevWin = nwh;
			SDL_Delay(16);
		}
		SDL_Log("[ggc] Android surface settled: nwh=%p stableFrames=%d", prevWin, stableFrames);
	}
#endif

	ApplicationHWnd = TheSDL3Window;

	Int result = GameMain();

	SDL_DestroyWindow(TheSDL3Window);
	TheSDL3Window = NULL;
	ApplicationHWnd = NULL;
	SDL_Quit();

	return result;
}

#endif
