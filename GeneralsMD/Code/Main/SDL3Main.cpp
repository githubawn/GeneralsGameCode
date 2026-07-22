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
#include <dirent.h>           // opendir/readdir (locate the game data dir)
#include <strings.h>          // strcasecmp
#include <cstring>

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

#if defined(__SWITCH__)
#include <unistd.h>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
extern "C" {
	uint32_t romfsMountSelf(const char* name);
	uint32_t romfsUnmount(const char* name);
	const void* socketGetDefaultInitConfig(void);
	uint32_t socketInitialize(const void* config);
	void socketExit(void);
	// libnx SVC wrapper: routes a debug string to the emulator/host log (visible in
	// Ryujinx "Guest" log and yuzu Debug log). Our out-of-band boot trace channel.
	uint32_t svcOutputDebugString(const char* str, uint64_t size);
	// TheSuperHackers @bugfix githubawn 01/07/2026 The no-romfs NRO reads its .big
	// archives from the SD card, but the libnx sdmc: device is not guaranteed to be
	// mounted for a bare NRO. Mount it explicitly before we try to reach the data.
	uint32_t fsdevMountSdmc(void);
}

// Boot trace helper: emit a line to the emulator/host debug log.
static void ggcSwitchLog(const char* msg)
{
	svcOutputDebugString(msg, strlen(msg));
}
#endif

#if defined(__3DS__)
#include <exception>
#include <cstdlib>

// TheSuperHackers @feature githubawn 19/07/2026 Match-exit crash groundwork:
// the 3DS build has none of the fatal-path instrumentation the other
// platforms have (compare the Android SIGSEGV/SIGABRT handler above) --
// libctru provides no POSIX signal handlers to install one the same way, so
// an uncaught C++ exception previously reached std::terminate and vanished
// with zero trace. Install a terminate handler that logs via SDL_Log first.
// See GeneralsMD/Code/Main/ThreeDSPlatformStubs.cpp's __wrap_abort (wired up
// via -Wl,--wrap=abort in cmake/toolchains/nintendo-3ds.cmake) for what
// happens to the abort() call below -- that is the actual fatal stop; this
// handler's job is only to get a reason logged before it.
namespace
{
	void ggc_3ds_terminate_handler()
	{
		// Recursion guard: if SDL_Log or anything else on this path itself
		// ends up calling std::terminate again, do not recurse -- go
		// straight to abort() (which __wrap_abort logs and halts on).
		static bool s_ggcTerminating = false;
		if (s_ggcTerminating)
		{
			abort();
		}
		s_ggcTerminating = true;

		SDL_Log("[ggc-fatal] std::terminate");

		if (std::exception_ptr eptr = std::current_exception())
		{
			try
			{
				std::rethrow_exception(eptr);
			}
			catch (const std::exception & e)
			{
				SDL_Log("[ggc-fatal] uncaught exception: %s", e.what());
			}
			catch (...)
			{
				SDL_Log("[ggc-fatal] uncaught exception of unknown type (not std::exception)");
			}
		}

		abort();
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

#if defined(__ANDROID__)
// TheSuperHackers @bugfix githubawn 29/06/2026 Returns true if the directory
// contains at least one ".big" archive, used to pick the real game data dir from
// a list of candidates on Android (shared storage vs the app's private dir).
static bool GGC_DirHasBigArchive(const char *dir)
{
	DIR *d = opendir(dir);
	if (d == NULL)
		return false;

	bool found = false;
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL)
	{
		const char *name = ent->d_name;
		const size_t len = strlen(name);
		if (len > 4 && strcasecmp(name + len - 4, ".big") == 0)
		{
			found = true;
			break;
		}
	}

	closedir(d);
	return found;
}
#endif

int main(int argc, char **argv)
{
	__argc = argc;
	__argv = argv;

#if defined(__SWITCH__)
	ggcSwitchLog("[ggc] main: entered\n");
	// TheSuperHackers @build githubawn 03/07/2026 TLS probe: OpenAL and bgfx both crash
	// (va=0x0) reading a C++ thread_local on the main thread. Log the thread pointer
	// registers and a real thread_local access to confirm the main-thread TLS setup.
	{
		unsigned long tp_rw = 0, tp_ro = 0;
		__asm__ volatile("mrs %0, tpidr_el0" : "=r"(tp_rw));
		__asm__ volatile("mrs %0, tpidrro_el0" : "=r"(tp_ro));
		static thread_local volatile int ggcTlsTest = 0x5a5a;
		char b[160];
		snprintf(b, sizeof(b), "[ggc] tpidr_el0=0x%lx tpidrro_el0=0x%lx &tls=%p\n",
			tp_rw, tp_ro, (void*)&ggcTlsTest);
		ggcSwitchLog(b);
		snprintf(b, sizeof(b), "[ggc] tls read = 0x%x\n", ggcTlsTest);
		ggcSwitchLog(b);
	}
	socketInitialize(socketGetDefaultInitConfig());
	// TheSuperHackers @build githubawn 03/07/2026 The Switch build ships as a homebrew
	// NRO (with a NACP) that reads its .big archives loose from the SD card
	// (sdmc:/generalszh), the same model RetroArch uses. Do NOT call romfsMountSelf
	// (in NRO mode it parses the self-NRO's embedded romfs asset, which this NRO does
	// not have, and that read crashes the emulator FS service). Do NOT call
	// fsdevMountSdmc either: libnx already auto-mounts sdmc: in homebrew __appInit, and
	// re-mounting collides and returns a misleading LibnxError_OutOfMemory (0x559).
	// The data dir is located and chdir'd below (after SDL_Init).
	ggcSwitchLog("[ggc] socket init done; sdmc: auto-mounted by libnx\n");
	setenv("GENERALS_USER_DIR", "sdmc:/switch/GeneralsZH", 1);
#elif defined(__3DS__)
	// TheSuperHackers @bugfix githubawn 14/07/2026 Same gap as the Switch
	// branch above: GlobalData::BuildUserDataPathFromRegistry() (GlobalData.cpp)
	// reads GENERALS_USER_DIR on non-Windows and falls back to "." when unset,
	// which produces a malformed relative path (".\Command and Conquer...")
	// once concatenated with the leaf name -- the 3DS sdmc archive backend's
	// stricter path validation rejects it (CreateDirectory "Invalid path"),
	// which then crashed startup shortly after. Point it at a clean absolute
	// sdmc path instead, same as Switch's sdmc:/switch/GeneralsZH.
	setenv("GENERALS_USER_DIR", "sdmc:/3ds/GeneralsZH", 1);

	// TheSuperHackers @feature githubawn 19/07/2026 Match-exit crash
	// groundwork: install the terminate handler as early as possible, before
	// any engine code that could throw runs. See the handler's own comment
	// above for why this exists at all.
	std::set_terminate(ggc_3ds_terminate_handler);
#endif

#if defined(__ANDROID__)
	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
#endif

#if defined(__ANDROID__) || defined(__3DS__)
	// TheSuperHackers @feature bobtista 15/06/2026 We translate touch events to
	// left-mouse ourselves (SDL3Mouse::addSDL3FingerEvent); disable SDL's own
	// touch->mouse synthesis so a single tap does not produce duplicate clicks.
	// TheSuperHackers @bugfix githubawn 16/07/2026 3DS has the exact same
	// touch->mouse translation (SDL_n3dstouch.c sends finger events the same
	// way Android does) and the same double-click risk if SDL's built-in
	// synthesis is left enabled -- extend the existing Android-only guard
	// rather than duplicating it.
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
#endif

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
	{
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return 1;
	}

#if defined(__SWITCH__)
	// TheSuperHackers @bugfix githubawn 03/07/2026 Locate the loose .big archives on
	// the SD card and chdir there so the engine's relative paths (Data\INI\...)
	// resolve. libnx already auto-mounts sdmc: in homebrew __appInit, so we only need
	// to find the data dir. CRITICAL: probe with ABSOLUTE paths only. A relative
	// fopen() before any successful chdir crashes inside libnx fsdev_getfspath (it
	// indexes fsdev_fsdevices[cwd] with cwd == -1 when no default device is set).
	{
		const char* basePath = SDL_GetBasePath();
		char argvDir[256];
		argvDir[0] = 0;
		if (argc > 0 && argv && argv[0])
		{
			size_t len = 0;
			while (argv[0][len] && len < sizeof(argvDir) - 1) { argvDir[len] = argv[0][len]; ++len; }
			argvDir[len] = 0;
			for (size_t i = len; i-- > 0; )
			{
				if (argvDir[i] == '/' || argvDir[i] == '\\') { argvDir[i] = 0; break; }
			}
		}

		const char* candidates[] = {
			"sdmc:/generalszh",
			"sdmc:/switch/generalszh",
			basePath,
			argvDir[0] ? argvDir : nullptr,
			"sdmc:/",
		};

		for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
		{
			const char* c = candidates[i];
			if (c == nullptr || c[0] == '\0') continue;
			// Absolute probe (crash-safe: device resolved by name, not cwd).
			char probePath[512];
			snprintf(probePath, sizeof(probePath), "%s/GeneralsZH.big", c);
			FILE* probe = fopen(probePath, "rb");
			char b[600];
			snprintf(b, sizeof(b), "[ggc] probe %s = %s\n", probePath, probe ? "OPEN" : "no");
			ggcSwitchLog(b);
			if (probe)
			{
				fclose(probe);
				const int cd = chdir(c);
				snprintf(b, sizeof(b), "[ggc] chdir %s = %d\n", c, cd);
				ggcSwitchLog(b);
				break;
			}
		}
	}
#elif defined(__3DS__)
	// TheSuperHackers @build githubawn 14/07/2026 New 3DS build ships as a homebrew
	// .3dsx/CIA that reads its .big archives loose from the SD card (sdmc:/generalszh),
	// same model as the Switch build above (docs/3ds-port-plan.md Phase 2). libctru
	// auto-mounts sdmc: at startup, so this only needs to locate the data dir and
	// chdir there so the engine's relative paths (Data\INI\...) resolve.
	{
		const char* basePath = SDL_GetBasePath();
		char argvDir[256];
		argvDir[0] = 0;
		if (argc > 0 && argv && argv[0])
		{
			size_t len = 0;
			while (argv[0][len] && len < sizeof(argvDir) - 1) { argvDir[len] = argv[0][len]; ++len; }
			argvDir[len] = 0;
			for (size_t i = len; i-- > 0; )
			{
				if (argvDir[i] == '/' || argvDir[i] == '\\') { argvDir[i] = 0; break; }
			}
		}

		const char* candidates[] = {
			"sdmc:/generalszh",
			"sdmc:/3ds/generalszh",
			basePath,
			argvDir[0] ? argvDir : nullptr,
			"sdmc:/",
		};

		for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
		{
			const char* c = candidates[i];
			if (c == nullptr || c[0] == '\0') continue;
			char probePath[512];
			snprintf(probePath, sizeof(probePath), "%s/GeneralsZH.big", c);
			FILE* probe = fopen(probePath, "rb");
			SDL_Log("[ggc] probe %s = %s", probePath, probe ? "OPEN" : "no");
			if (probe)
			{
				fclose(probe);
				const int cd = chdir(c);
				SDL_Log("[ggc] chdir %s = %d", c, cd);
				break;
			}
		}
	}
#endif
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
	// process starts with cwd "/", so switch to the directory that actually holds
	// the game data.
	// TheSuperHackers @bugfix githubawn 29/06/2026 The app's private external
	// storage dir (SDL_GetAndroidExternalStoragePath) is empty on most installs;
	// the game data lives in shared storage (e.g. /sdcard/Games/Generals). Probe a
	// list of candidate directories and chdir to the first one that actually
	// contains .big archives, instead of unconditionally using the (empty) private
	// dir, which made the very first INI load (GameData) throw INI_CANT_OPEN_FILE
	// and abort init. Reading shared storage needs the MANAGE_EXTERNAL_STORAGE
	// ("All files access") grant declared in the manifest. Keep the user-data root
	// (saves / prefs / crash logs) in the always-writable private dir.
	{
		const char *userDir = SDL_GetAndroidExternalStoragePath();
		if (userDir != NULL && userDir[0] != '\0')
		{
			setenv("GENERALS_USER_DIR", userDir, 1);
		}

		const char *candidates[] = {
			getenv("GENERALS_DATA_DIR"),          // explicit override wins
			"/sdcard/Games/Generals",
			"/storage/emulated/0/Games/Generals",
			userDir,                              // last resort (data pushed into app dir)
		};

		const char *dataDir = NULL;
		for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
		{
			const char *c = candidates[i];
			if (c == NULL || c[0] == '\0')
				continue;
			if (GGC_DirHasBigArchive(c))
			{
				dataDir = c;
				break;
			}
		}

		if (dataDir == NULL)
			dataDir = userDir;  // nothing matched; fall back so chdir/logging still runs

		if (dataDir != NULL && dataDir[0] != '\0')
		{
			if (chdir(dataDir) == 0)
			{
				SDL_Log("[ggc] data dir (cwd): %s", dataDir);
			}
			else
			{
				SDL_Log("[ggc] chdir to data dir failed: %s", dataDir);
			}
		}
		else
		{
			SDL_Log("[ggc] could not resolve a game data directory");
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
#if defined(__3DS__)
	// TheSuperHackers @bugfix githubawn 16/07/2026 SDL3's n3ds video backend
	// does not implement SDL_GetDesktopDisplayMode (returns null), so
	// windowW/windowH silently stayed at the kDefaultWindowWidth/Height
	// (800x600) fallback above. W3DDisplay::setResolution overrides
	// TheGlobalData's display width/height from SDL_GetWindowSize() (see
	// W3DDisplay.cpp), so this SDL window's reported size is what
	// Render2DClass::Set_Coordinate_Range ultimately lays the whole UI out
	// against; an 800x600-vs-real mismatch there is what put the main menu
	// logo at the wrong position/size and left the background and button
	// text entirely outside the visible area. Force the real size
	// unconditionally; there is no separate "windowed" mode on 3DS to
	// preserve here.
	//
	// TheSuperHackers @tweak githubawn 16/07/2026 320x240, not the top
	// screen's 400x240: Citro3dBackend now draws the interactive UI to the
	// bottom screen (see the comment on C3D_FrameDrawOn(m_bottomTarget) in
	// Citro3dBackend::Begin_Scene) since the New 3DS top screen has no touch
	// digitizer at all and an RTS needs mouse-equivalent click/drag
	// interaction. This must match Citro3dBackend's bottom C3D_RenderTarget
	// (C3D_RenderTargetCreate(240, 320, ...), pre-rotation logical 320x240).
	windowW = 320;
	windowH = 240;
#endif
#if defined(__3DS__)
	// TheSuperHackers @bugfix githubawn 16/07/2026 Requesting a 320x240 size
	// is not enough on its own. SDL3's n3ds video backend
	// (src/video/n3ds/SDL_n3dsvideo.c) registers the top and bottom screens
	// as two SEPARATE SDL displays, and N3DS_CreateWindow picks which one a
	// new window belongs to via SDL_GetDisplayForWindow -> ...ForWindowPosition
	// -> GetDisplayForRect(window->x, window->y, ...), i.e. purely by the
	// window's Y position (top screen: y=0..239, bottom: y=240..479, see
	// N3DS_GetDisplayBounds). SDL_CreateWindow's plain (title, w, h, flags)
	// form never sets an explicit position, so it defaults into the TOP
	// screen's display every time regardless of the requested size -- the
	// window's SDL-reported size then silently stayed tied to whatever the
	// top screen's display mode is instead of our request. This is separate
	// from (and in addition to) Citro3dBackend drawing the actual pixels to
	// GFX_BOTTOM (Citro3dBackend::Begin_Scene) -- that part already lands on
	// the physical bottom screen since citro3d output is driven directly,
	// not through SDL's own render path. But W3DDisplay::setResolution
	// still reads the window's SDL-reported size to drive
	// TheGlobalData's display width/height (and therefore
	// Render2DClass::Set_Coordinate_Range's whole UI coordinate layout), so
	// leaving the window mis-associated with the top screen's display left
	// the UI laid out for the wrong (400x240) canvas even though the pixels
	// were physically appearing on the (320x240) bottom screen -- exactly
	// the "logo in the wrong place/duplicated, menu buttons and text and
	// background missing" symptom. Explicitly position the window at
	// y=GSP_SCREEN_WIDTH (240) so SDL associates it with the bottom screen's
	// display and reports the correct 320x240 size.
	SDL_PropertiesID windowProps = SDL_CreateProperties();
	SDL_SetStringProperty(windowProps, SDL_PROP_WINDOW_CREATE_TITLE_STRING, kWindowTitle);
	SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_X_NUMBER, 0);
	SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_Y_NUMBER, 240);
	SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, windowW);
	SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, windowH);
	SDL_SetNumberProperty(windowProps, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, windowFlags);
	TheSDL3Window = SDL_CreateWindowWithProperties(windowProps);
	SDL_DestroyProperties(windowProps);
#else
	TheSDL3Window = SDL_CreateWindow(kWindowTitle, windowW, windowH, windowFlags);
#endif
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

#if defined(__SWITCH__)
#if !defined(GGC_SWITCH_SD_DATA)
	romfsUnmount("romfs");
#endif
	socketExit();
#endif

	return result;
}

#endif
