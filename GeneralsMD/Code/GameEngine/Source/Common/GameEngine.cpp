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

// GameEngine.cpp /////////////////////////////////////////////////////////////////////////////////
// Implementation of the Game Engine singleton
// Author: Michael S. Booth, April 2001

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#if defined(__ANDROID__)
#include <android/log.h>
#include <cxxabi.h>
#include <typeinfo>
#include <exception>
#endif

#if defined(__3DS__)
#include <cxxabi.h>
#include <typeinfo>
#include <exception>
#endif

#include "Common/ActionManager.h"
#include "Common/AudioAffect.h"
#include "Common/BuildAssistant.h"
#include "Common/CRCDebug.h"
#include "Common/FramePacer.h"
#include "Common/Radar.h"
#include "Common/PlayerTemplate.h"
#include "Common/Team.h"
#include "Common/PlayerList.h"
#include "Common/GameAudio.h"
#include "Common/GameEngine.h"
#include "Common/INI.h"
#include "Common/INIException.h"
#include "Common/MessageStream.h"
#include "Common/ThingFactory.h"
#include "Common/file.h"
#include "Common/FileSystem.h"
#include "Common/ArchiveFileSystem.h"
#include "Common/LocalFileSystem.h"
#include "Common/GlobalData.h"
#include "Common/PerfTimer.h"
#include "Common/RandomValue.h"
#include "Common/NameKeyGenerator.h"
#include "Common/ModuleFactory.h"
#include "Common/Debug.h"
#include "Common/GameState.h"
#include "Common/GameStateMap.h"
#include "Common/Science.h"
#include "Common/FunctionLexicon.h"
#include "Common/CommandLine.h"
#include "Common/DamageFX.h"
#include "Common/MultiplayerSettings.h"
#include "Common/Recorder.h"
#include "Common/SpecialPower.h"
#include "Common/TerrainTypes.h"
#include "Common/Upgrade.h"
#include "Common/OptionPreferences.h"
#include "Common/Xfer.h"
#include "Common/XferCRC.h"
#include "Common/GameLOD.h"
#include "Common/Registry.h"
#include "Common/GameCommon.h"	// FOR THE ALLOW_DEBUG_CHEATS_IN_RELEASE #define

#include "GameLogic/Armor.h"
#include "GameLogic/AI.h"
#include "GameLogic/CaveSystem.h"
#include "GameLogic/CrateSystem.h"
#include "GameLogic/Damage.h"
#include "GameLogic/VictoryConditions.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/RankInfo.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/SidesList.h"

#include "GameClient/ClientInstance.h"
#include "GameClient/FXList.h"
#include "GameClient/GameClient.h"
#include "GameClient/Keyboard.h"
#include "GameClient/Shell.h"
#include "GameClient/GameText.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/Water.h"
#include "GameClient/TerrainRoads.h"
#include "GameClient/MetaEvent.h"
#include "GameClient/MapUtil.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GlobalLanguage.h"
#include "GameClient/Drawable.h"
#include "GameClient/GUICallbacks.h"

#include "GameNetwork/NetworkInterface.h"
#include "GameNetwork/WOLBrowser/WebBrowser.h"
#include "GameNetwork/LANAPI.h"
#include "GameNetwork/GameSpy/GameResultsThread.h"

#include "Common/version.h"


//-------------------------------------------------------------------------------------------------

#ifdef DEBUG_CRC
class DeepCRCSanityCheck : public SubsystemInterface
{
public:
	DeepCRCSanityCheck() {}
	virtual ~DeepCRCSanityCheck() {}

	virtual void init() {}
	virtual void reset();
	virtual void update() {}

protected:
};

DeepCRCSanityCheck *TheDeepCRCSanityCheck = nullptr;

void DeepCRCSanityCheck::reset()
{
	static Int timesThrough = 0;
	static UnsignedInt lastCRC = 0;

	AsciiString fname;
	fname.format("%sCRCAfter%dMaps.dat", TheGlobalData->getPath_UserData().str(), timesThrough);
	UnsignedInt thisCRC = TheGameLogic->getCRC( CRC_RECALC, fname );

	DEBUG_LOG(("DeepCRCSanityCheck: CRC is %X", thisCRC));
	DEBUG_ASSERTCRASH(timesThrough == 0 || thisCRC == lastCRC,
		("CRC after reset did not match beginning CRC!\nNetwork games won't work after this.\nOld: 0x%8.8X, New: 0x%8.8X",
		lastCRC, thisCRC));
	lastCRC = thisCRC;

	timesThrough++;
}
#endif // DEBUG_CRC

//-------------------------------------------------------------------------------------------------
/// The GameEngine singleton instance
GameEngine *TheGameEngine = nullptr;

//-------------------------------------------------------------------------------------------------
SubsystemInterfaceList* TheSubsystemList = nullptr;

//-------------------------------------------------------------------------------------------------
#if defined(__SWITCH__)
#include <cstdio>
extern "C" unsigned int svcOutputDebugString(const char *, unsigned long);
static void ggc_switch_trace(const char *a, const char *b, const char *c)
{
	char buf[192];
	int n = snprintf(buf, sizeof(buf), "%s%s%s", a, b ? b : "?", c);
	if (n > 0) svcOutputDebugString(buf, (unsigned)n);
	// Also append to a file on the SD (flushed immediately) so emulators that do
	// NOT capture svcOutputDebugString (e.g. Eden) still leave a boot trace to read.
	// The game has chdir'd into the data dir, so a relative path lands in generalszh/.
	FILE *f = std::fopen("ggc_boot.txt", "a");
	if (f) { std::fwrite(buf, 1, (size_t)(n > 0 ? n : 0), f); std::fflush(f); std::fclose(f); }
}
#elif defined(__3DS__)
// TheSuperHackers @diagnostic githubawn 16/07/2026 Root-causing a crash in
// resetSubsystems() (TheGameLogic->reset()) only reached once boot got far
// enough on 3DS to survive past Terrain.ini parsing. File-based, same
// rationale as the Switch tracer above: bypasses SDL_Log's own
// sdmc:/3ds/SDL_Log.txt path so it's independent of that mechanism.
#include <cstdio>
static void ggc_switch_trace(const char *a, const char *b, const char *c)
{
	char buf[192];
	int n = snprintf(buf, sizeof(buf), "%s%s%s", a, b ? b : "?", c);
	FILE *f = std::fopen("ggc_boot.txt", "a");
	if (f) { std::fwrite(buf, 1, (size_t)(n > 0 ? n : 0), f); std::fflush(f); std::fclose(f); }
}
// TheSuperHackers @diagnostic githubawn 16/07/2026 __ctru_heap_size/
// __ctru_linear_heap_size are set by ThreeDSPlatformStubs.cpp's
// __system_allocateHeaps override (a ProbeAlloc step-down loop run before
// main()), but that runs too early to use this file-based tracer (no chdir
// yet). Report the discovered sizes here instead, as early in init() as
// this tracer becomes usable.
extern "C" { extern unsigned int __ctru_heap_size; extern unsigned int __ctru_linear_heap_size; }
#include <malloc.h>
#endif

template<class SUBSYSTEM>
void initSubsystem(
	SUBSYSTEM*& sysref,
	AsciiString name,
	SUBSYSTEM* sys,
	Xfer *pXfer,
	const char* path1 = nullptr,
	const char* path2 = nullptr)
{
	sysref = sys;
#if defined(__SWITCH__) || defined(__3DS__)
	// TheSuperHackers @diagnostic githubawn 03/07/2026 Trace subsystem init so we can
	// see exactly which one hangs after bgfx::init on Switch (boot never reaches the
	// main loop / first rendered frame).
	ggc_switch_trace("[ggc] initSubsystem: ", name.str(), " ...\n");
	ggc_switch_trace("[ggc] initSubsystem: sys=", sys ? "non-null" : "NULL", "\n");
#endif
#if defined(__3DS__)
	// TheSuperHackers @diagnostic githubawn 16/07/2026 Root-causing a
	// silently-swallowed exception during subsystem init (boot trace showed
	// TheThingFactory's "sys=non-null" line but never its own "DONE" line,
	// nor any trace at all for every subsequent initSubsystem call including
	// TheGameLogic -- consistent with an exception unwinding straight past
	// all of them to GameEngine::init()'s outer catch blocks, which then
	// apparently failed to actually terminate via RELEASE_CRASH). Catch here
	// (tightest possible scope) to log the real exception before re-throwing
	// so the existing outer handling is unchanged.
	try
	{
		TheSubsystemList->initSubsystem(sys, path1, path2, pXfer, name);
	}
	catch (INIException &e)
	{
		ggc_switch_trace("[ggc] initSubsystem EXCEPTION (INIException): ", e.mFailureMessage ? e.mFailureMessage : "(null)", "\n");
		throw;
	}
	catch (ErrorCode ec)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%d", (int)ec);
		ggc_switch_trace("[ggc] initSubsystem EXCEPTION (ErrorCode): ", buf, "\n");
		throw;
	}
	catch (...)
	{
		ggc_switch_trace("[ggc] initSubsystem EXCEPTION (unknown type)\n", "", "");
		throw;
	}
#else
	TheSubsystemList->initSubsystem(sys, path1, path2, pXfer, name);
#endif
#if defined(__SWITCH__) || defined(__3DS__)
	ggc_switch_trace("[ggc] initSubsystem: ", name.str(), " DONE\n");
#endif
#if defined(__3DS__)
	// TheSuperHackers @diagnostic githubawn 18/07/2026 By object #20 of the
	// ~663 map-object placement loop, general heap was ALREADY at ~97.5MB/
	// 119.1MB used -- the placement loop itself only adds ~9.5MB across all
	// 663 objects. That means the real cost is boot-time init, before any
	// map ever loads, not per-object load. Every major subsystem here
	// (TheThingFactory/Object.ini, TheWeaponStore, TheArmorStore,
	// TheLocomotorStore, TheSpecialPowerStore, TheFXListStore,
	// TheParticleSystemManager, TheUpgradeCenter, etc.) is loaded
	// unconditionally regardless of which match/factions are actually
	// chosen. Log real heap usage after every one to find which is
	// actually large instead of continuing to guess.
	{
		struct mallinfo mi = mallinfo();
		char buf[192];
		snprintf(buf, sizeof(buf), "[ggc] HEAP after %s: used=%u free=%u\n",
			name.str(), (unsigned)mi.uordblks, (unsigned)mi.fordblks);
		ggc_switch_trace(buf, "", "");
	}
#endif
}

//-------------------------------------------------------------------------------------------------
extern HINSTANCE ApplicationHInstance;  ///< our application instance
// TheSuperHackers @build bobtista 13/06/2026 ATL CComModule is Windows-only (the
// embedded web browser); not referenced on other platforms.
#if defined(_WIN32)
extern CComModule _Module;
#endif

//-------------------------------------------------------------------------------------------------
static void updateTGAtoDDS();

//-------------------------------------------------------------------------------------------------
static void updateWindowTitle()
{
	// TheSuperHackers @tweak Now prints product and version information in the Window title.

	DEBUG_ASSERTCRASH(TheVersion != nullptr, ("TheVersion is null"));
	DEBUG_ASSERTCRASH(TheGameText != nullptr, ("TheGameText is null"));

	UnicodeString title;

	if (rts::ClientInstance::getInstanceId() > 1u)
	{
		UnicodeString str;
		str.format(L"Instance:%.2u", rts::ClientInstance::getInstanceId());
		title.concat(str);
	}

	UnicodeString productString = TheVersion->getUnicodeProductString();

	if (!productString.isEmpty())
	{
		if (!title.isEmpty())
			title.concat(L" ");
		title.concat(productString);
	}

#if RTS_GENERALS
	const WideChar* defaultGameTitle = L"Command and Conquer Generals";
#elif RTS_ZEROHOUR
	const WideChar* defaultGameTitle = L"Command and Conquer Generals Zero Hour";
#endif
	UnicodeString gameTitle = TheGameText->FETCH_OR_SUBSTITUTE("GUI:Command&ConquerGenerals", defaultGameTitle);

	if (!gameTitle.isEmpty())
	{
		UnicodeString gameTitleFinal;
		UnicodeString gameVersion = TheVersion->getUnicodeVersion();

		if (productString.isEmpty())
		{
			gameTitleFinal = gameTitle;
		}
		else
		{
			UnicodeString gameTitleFormat = TheGameText->FETCH_OR_SUBSTITUTE("Version:GameTitle", L"for %ls");
			gameTitleFinal.format(gameTitleFormat.str(), gameTitle.str());
		}

		if (!title.isEmpty())
			title.concat(L" ");
		title.concat(gameTitleFinal.str());
		title.concat(L" ");
		title.concat(gameVersion.str());
	}

	if (!title.isEmpty())
	{
		AsciiString titleA;
		titleA.translate(title);	//get ASCII version for Win 9x

		extern HWND ApplicationHWnd;  ///< our application window handle
		if (ApplicationHWnd) {
			//Set it twice because Win 9x does not support SetWindowTextW.
			::SetWindowText(ApplicationHWnd, titleA.str());
			::SetWindowTextW(ApplicationHWnd, title.str());
		}
	}
}

//-------------------------------------------------------------------------------------------------
GameEngine::GameEngine()
{
	// initialize to non garbage values
	m_logicTimeAccumulator = 0.0f;
	m_quitting = FALSE;
	m_isActive = FALSE;

#if defined(_WIN32)
	_Module.Init(nullptr, ApplicationHInstance, nullptr);
#endif
}

//-------------------------------------------------------------------------------------------------
GameEngine::~GameEngine()
{
	//extern std::vector<std::string>	preloadTextureNamesGlobalHack;
	//preloadTextureNamesGlobalHack.clear();

	delete TheMapCache;
	TheMapCache = nullptr;

//	delete TheShell;
//	TheShell = nullptr;

	TheGameResultsQueue->endThreads();

	// TheSuperHackers @fix helmutbuhler 03/06/2025
	// Reset all subsystems before deletion to prevent crashing due to cross dependencies.
	reset();

	TheSubsystemList->shutdownAll();
	delete TheSubsystemList;
	TheSubsystemList = nullptr;

	delete TheNetwork;
	TheNetwork = nullptr;

	delete TheCommandList;
	TheCommandList = nullptr;

	delete TheNameKeyGenerator;
	TheNameKeyGenerator = nullptr;

	delete TheFileSystem;
	TheFileSystem = nullptr;

	delete TheGameLODManager;
	TheGameLODManager = nullptr;

	Drawable::killStaticImages();

#if defined(_WIN32)
	_Module.Term();
#endif

#ifdef PERF_TIMERS
	PerfGather::termPerfDump();
#endif
}

//-------------------------------------------------------------------------------------------------
Bool GameEngine::isTimeFrozen()
{
	// TheSuperHackers @fix The time can no longer be frozen in Network games. It would disconnect the player.
	if (TheNetwork != nullptr)
		return false;

	if (TheTacticalView != nullptr)
	{
		if (TheTacticalView->isTimeFrozen() && !TheTacticalView->isCameraMovementFinished())
			return true;
	}

	if (TheScriptEngine != nullptr)
	{
		if (TheScriptEngine->isTimeFrozenDebug() || TheScriptEngine->isTimeFrozenScript())
			return true;
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
Bool GameEngine::isGameHalted()
{
	if (TheNetwork != nullptr)
	{
		if (TheNetwork->isStalling())
			return true;
	}
	else
	{
		if (TheGameLogic != nullptr && TheGameLogic->isGamePaused())
			return true;
	}

	return false;
}

/** -----------------------------------------------------------------------------------------------
 * Initialize the game engine by initializing the GameLogic and GameClient.
 */
void GameEngine::init()
{
	try {
#if defined(__3DS__)
		{
			char buf[64];
			snprintf(buf, sizeof(buf), "heap=%uKB linear=%uKB\n", __ctru_heap_size / 1024, __ctru_linear_heap_size / 1024);
			ggc_switch_trace("[ggc] 3DS ProbeAlloc heap sizes: ", buf, "");
		}
#endif
		//create an INI object to use for loading stuff
		INI ini;

#ifdef DEBUG_LOGGING
		if (TheVersion)
		{
			DEBUG_LOG(("================================================================================"));
			DEBUG_LOG(("Generals version %s", TheVersion->getAsciiVersion().str()));
			DEBUG_LOG(("Build date: %s", TheVersion->getAsciiBuildTime().str()));
			DEBUG_LOG(("Build location: %s", TheVersion->getAsciiBuildLocation().str()));
			DEBUG_LOG(("Build user: %s", TheVersion->getAsciiBuildUser().str()));
			DEBUG_LOG(("Build git revision: %s", TheVersion->getAsciiGitCommitCount().str()));
			DEBUG_LOG(("Build git version: %s", TheVersion->getAsciiGitTagOrHash().str()));
			DEBUG_LOG(("Build git commit time: %s", TheVersion->getAsciiGitCommitTime().str()));
			DEBUG_LOG(("Build git commit author: %s", Version::getGitCommitAuthorName()));
			DEBUG_LOG(("================================================================================"));
		}
#endif

	#if defined(PERF_TIMERS) || defined(DUMP_PERF_STATS)
		DEBUG_LOG(("Calculating CPU frequency for performance timers."));
		InitPrecisionTimer();
	#endif
	#ifdef PERF_TIMERS
		PerfGather::initPerfDump("AAAPerfStats", PerfGather::PERF_NETTIME);
	#endif




	#ifdef DUMP_PERF_STATS////////////////////////////////////////////////////////////
	__int64 startTime64;//////////////////////////////////////////////////////////////
	__int64 endTime64,freq64;///////////////////////////////////////////////////////////
	GetPrecisionTimerTicksPerSec(&freq64);///////////////////////////////////////////////
	GetPrecisionTimer(&startTime64);////////////////////////////////////////////////////
  char Buf[256];//////////////////////////////////////////////////////////////////////
	#endif//////////////////////////////////////////////////////////////////////////////


		TheSubsystemList = MSGNEW("GameEngineSubsystem") SubsystemInterfaceList;

		TheSubsystemList->addSubsystem(this);

		// initialize the random number system
		InitRandom();

		// Create the low-level file system interface
		TheFileSystem = createFileSystem();

		// not part of the subsystem list, because it should normally never be reset!
		TheNameKeyGenerator = MSGNEW("GameEngineSubsystem") NameKeyGenerator;
		TheNameKeyGenerator->init();


    	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheNameKeyGenerator  = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		// not part of the subsystem list, because it should normally never be reset!
		TheCommandList = MSGNEW("GameEngineSubsystem") CommandList;
		TheCommandList->init();

    	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheCommandList  = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		XferCRC xferCRC;
		xferCRC.open("lightCRC");


		initSubsystem(TheLocalFileSystem, "TheLocalFileSystem", createLocalFileSystem(), nullptr);


    	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheLocalFileSystem  = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		initSubsystem(TheArchiveFileSystem, "TheArchiveFileSystem", createArchiveFileSystem(), nullptr); // this MUST come after TheLocalFileSystem creation

    	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheArchiveFileSystem  = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		DEBUG_ASSERTCRASH(TheWritableGlobalData,("TheWritableGlobalData expected to be created"));
		initSubsystem(TheWritableGlobalData, "TheWritableGlobalData", TheWritableGlobalData, &xferCRC, "Data\\INI\\Default\\GameData", "Data\\INI\\GameData");
		TheWritableGlobalData->parseCustomDefinition();


	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After  TheWritableGlobalData = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////



	#if defined(RTS_DEBUG)
		// If we're in Debug, load the Debug settings as well.
		ini.loadFileDirectory( "Data\\INI\\GameDataDebug", INI_LOAD_OVERWRITE, nullptr );
	#endif

		// special-case: parse command-line parameters after loading global data
		CommandLine::parseCommandLineForEngineInit();

		TheArchiveFileSystem->loadMods();

		// doesn't require resets so just create a single instance here.
		TheGameLODManager = MSGNEW("GameEngineSubsystem") GameLODManager;
		TheGameLODManager->init();

		// after parsing the command line, we may want to perform dds stuff. Do that here.
		if (TheGlobalData->m_shouldUpdateTGAToDDS) {
			// update any out of date targas here.
			updateTGAtoDDS();
		}

		// read the water settings from INI (must do prior to initing GameClient, apparently)
		ini.loadFileDirectory( "Data\\INI\\Default\\Water", INI_LOAD_OVERWRITE, &xferCRC );
		ini.loadFileDirectory( "Data\\INI\\Water", INI_LOAD_OVERWRITE, &xferCRC );
		ini.loadFileDirectory( "Data\\INI\\Default\\Weather", INI_LOAD_OVERWRITE, &xferCRC );
		ini.loadFileDirectory( "Data\\INI\\Weather", INI_LOAD_OVERWRITE, &xferCRC );



	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After water INI's = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


#ifdef DEBUG_CRC
		initSubsystem(TheDeepCRCSanityCheck, "TheDeepCRCSanityCheck", MSGNEW("GameEngineSubystem") DeepCRCSanityCheck, nullptr);
#endif // DEBUG_CRC
		initSubsystem(TheGameText, "TheGameText", CreateGameTextInterface(), nullptr);
		updateWindowTitle();

	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheGameText = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


#if RETAIL_COMPATIBLE_CRC
		if (xferCRC.getCRC() == 0xA1E7F8E6)
			TheNameKeyGenerator->verifyNameKeyID(1);
#endif

		initSubsystem(TheScienceStore,"TheScienceStore", MSGNEW("GameEngineSubsystem") ScienceStore(), &xferCRC, "Data\\INI\\Default\\Science", "Data\\INI\\Science");
		initSubsystem(TheMultiplayerSettings,"TheMultiplayerSettings", MSGNEW("GameEngineSubsystem") MultiplayerSettings(), &xferCRC, "Data\\INI\\Default\\Multiplayer", "Data\\INI\\Multiplayer");
		initSubsystem(TheTerrainTypes,"TheTerrainTypes", MSGNEW("GameEngineSubsystem") TerrainTypeCollection(), &xferCRC, "Data\\INI\\Default\\Terrain", "Data\\INI\\Terrain");
		initSubsystem(TheTerrainRoads,"TheTerrainRoads", MSGNEW("GameEngineSubsystem") TerrainRoadCollection(), &xferCRC, "Data\\INI\\Default\\Roads", "Data\\INI\\Roads");
		initSubsystem(TheGlobalLanguageData,"TheGlobalLanguageData",MSGNEW("GameEngineSubsystem") GlobalLanguage, nullptr); // must be before the game text
		TheGlobalLanguageData->parseCustomDefinition();
	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheGlobalLanguageData = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////
		initSubsystem(TheAudio,"TheAudio", createAudioManager(TheGlobalData->m_headless), nullptr);
		if (!TheAudio->isMusicAlreadyLoaded())
#if defined(__SWITCH__)
			// TheSuperHackers @bugfix githubawn 04/07/2026 Switch uses DummyAudioManager
			// (no OpenAL yet), so no menu music is loaded and isMusicAlreadyLoaded() is
			// false. The retail "quit if music not loaded" check then set m_quitting
			// during init, so the main loop was skipped and NOTHING ever rendered
			// (the long-standing "stuck on loading"). Do not quit on Switch.
			{ /* keep running with silent audio */ }
#else
			setQuitting(TRUE);
#endif

#if RTS_ZEROHOUR && RETAIL_COMPATIBLE_CRC
		TheNameKeyGenerator->syncNameKeyID();
#endif

	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheAudio = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		initSubsystem(TheFunctionLexicon,"TheFunctionLexicon", createFunctionLexicon(), nullptr);
		initSubsystem(TheModuleFactory,"TheModuleFactory", createModuleFactory(), nullptr);
		initSubsystem(TheMessageStream,"TheMessageStream", createMessageStream(), nullptr);
		initSubsystem(TheSidesList,"TheSidesList", MSGNEW("GameEngineSubsystem") SidesList(), nullptr);
		initSubsystem(TheCaveSystem,"TheCaveSystem", MSGNEW("GameEngineSubsystem") CaveSystem(), nullptr);
		initSubsystem(TheRankInfoStore,"TheRankInfoStore", MSGNEW("GameEngineSubsystem") RankInfoStore(), &xferCRC, nullptr, "Data\\INI\\Rank");
		initSubsystem(ThePlayerTemplateStore,"ThePlayerTemplateStore", MSGNEW("GameEngineSubsystem") PlayerTemplateStore(), &xferCRC, "Data\\INI\\Default\\PlayerTemplate", "Data\\INI\\PlayerTemplate");
		initSubsystem(TheParticleSystemManager,"TheParticleSystemManager", createParticleSystemManager(TheGlobalData->m_headless), nullptr);

	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheParticleSystemManager = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		initSubsystem(TheFXListStore,"TheFXListStore", MSGNEW("GameEngineSubsystem") FXListStore(), &xferCRC, "Data\\INI\\Default\\FXList", "Data\\INI\\FXList");
		initSubsystem(TheWeaponStore,"TheWeaponStore", MSGNEW("GameEngineSubsystem") WeaponStore(), &xferCRC, nullptr, "Data\\INI\\Weapon");
		initSubsystem(TheObjectCreationListStore,"TheObjectCreationListStore", MSGNEW("GameEngineSubsystem") ObjectCreationListStore(), &xferCRC, "Data\\INI\\Default\\ObjectCreationList", "Data\\INI\\ObjectCreationList");
		initSubsystem(TheLocomotorStore,"TheLocomotorStore", MSGNEW("GameEngineSubsystem") LocomotorStore(), &xferCRC, nullptr, "Data\\INI\\Locomotor");
		initSubsystem(TheSpecialPowerStore,"TheSpecialPowerStore", MSGNEW("GameEngineSubsystem") SpecialPowerStore(), &xferCRC, "Data\\INI\\Default\\SpecialPower", "Data\\INI\\SpecialPower");
		initSubsystem(TheDamageFXStore,"TheDamageFXStore", MSGNEW("GameEngineSubsystem") DamageFXStore(), &xferCRC, nullptr, "Data\\INI\\DamageFX");
		initSubsystem(TheArmorStore,"TheArmorStore", MSGNEW("GameEngineSubsystem") ArmorStore(), &xferCRC, nullptr, "Data\\INI\\Armor");
		initSubsystem(TheBuildAssistant,"TheBuildAssistant", MSGNEW("GameEngineSubsystem") BuildAssistant, nullptr);


	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheBuildAssistant = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////



		initSubsystem(TheThingFactory,"TheThingFactory", createThingFactory(), &xferCRC, "Data\\INI\\Default\\Object", "Data\\INI\\Object");

	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheThingFactory = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


#if RETAIL_COMPATIBLE_CRC
		if (xferCRC.getCRC() == 0x6209AF6E)
			TheNameKeyGenerator->verifyNameKeyID(2265);
#endif

		initSubsystem(TheUpgradeCenter,"TheUpgradeCenter", MSGNEW("GameEngineSubsystem") UpgradeCenter, &xferCRC, "Data\\INI\\Default\\Upgrade", "Data\\INI\\Upgrade");

#if defined(__ANDROID__) || defined(__SWITCH__)
		// TheSuperHackers @feature bobtista 14/06/2026 Force the game's render
		// resolution to the phone's native screen resolution on every boot, before
		// the display/GameClient initializes (Android has no meaningful Options.ini
		// display mode). The window was created full-screen at the device size.
		// TheSuperHackers @bugfix githubawn 04/07/2026 Also on Switch: the window/touch
		// are 1280x720 but m_xResolution defaulted to something else, so the UI hit-test
		// coords did not match the touch coords and menu clicks missed. Force display
		// res = window size so click coords line up (and the scene renders at native res).
		{
			extern bool GGC_GetDeviceResolution_SDL3(int *w, int *h);
			int devW = 0, devH = 0;
			if (GGC_GetDeviceResolution_SDL3(&devW, &devH) && TheWritableGlobalData != nullptr)
			{
				TheWritableGlobalData->m_xResolution = devW;
				TheWritableGlobalData->m_yResolution = devH;
			}
		}
#if !defined(SAGE_USE_OPENAL)
		// TheSuperHackers @feature bobtista 15/06/2026 No real audio backend on
		// Android yet (DummyAudioManager). The -noaudio command-line flag is gated
		// behind RTS_DEBUG, so disable audio directly here so audio/speech INI and
		// EVA data paths do not run.
		// TheSuperHackers @bugfix githubawn 28/06/2026 Only force-disable audio when
		// there is no real backend. With SAGE_USE_OPENAL the OpenAL+FFmpeg backend is
		// functional on Android, so leaving audio enabled here restores music/sound
		// (otherwise the OpenSL device opens but every channel is muted by m_audioOn).
		if (TheWritableGlobalData != nullptr)
		{
			TheWritableGlobalData->m_audioOn = false;
			TheWritableGlobalData->m_speechOn = false;
			TheWritableGlobalData->m_soundsOn = false;
			TheWritableGlobalData->m_musicOn = false;
		}
#endif
#endif

		initSubsystem(TheGameClient,"TheGameClient", createGameClient(), nullptr);


	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheGameClient = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		initSubsystem(TheAI,"TheAI", MSGNEW("GameEngineSubsystem") AI(), &xferCRC,  "Data\\INI\\Default\\AIData", "Data\\INI\\AIData");
		initSubsystem(TheGameLogic,"TheGameLogic", createGameLogic(), nullptr);
		initSubsystem(TheTeamFactory,"TheTeamFactory", MSGNEW("GameEngineSubsystem") TeamFactory(), nullptr);
		initSubsystem(TheCrateSystem,"TheCrateSystem", MSGNEW("GameEngineSubsystem") CrateSystem(), &xferCRC, "Data\\INI\\Default\\Crate", "Data\\INI\\Crate");
		initSubsystem(ThePlayerList,"ThePlayerList", MSGNEW("GameEngineSubsystem") PlayerList(), nullptr);
		initSubsystem(TheRecorder,"TheRecorder", createRecorder(), nullptr);
		initSubsystem(TheRadar,"TheRadar", createRadar(TheGlobalData->m_headless), nullptr);
		initSubsystem(TheVictoryConditions,"TheVictoryConditions", createVictoryConditions(), nullptr);



	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheVictoryConditions = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		AsciiString fname;
		fname.format("Data\\%s\\CommandMap", GetRegistryLanguage().str());
		initSubsystem(TheMetaMap,"TheMetaMap", MSGNEW("GameEngineSubsystem") MetaMap(), nullptr, fname.str(), "Data\\INI\\CommandMap");

#if defined(RTS_DEBUG)
		ini.loadFileDirectory("Data\\INI\\CommandMapDebug", INI_LOAD_MULTIFILE, nullptr);
#endif

#if defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
		ini.loadFileDirectory("Data\\INI\\CommandMapDemo", INI_LOAD_MULTIFILE, nullptr);
#endif

		TheMetaMap->generateMetaMap();
		TheMetaMap->verifyMetaMap();


		initSubsystem(TheActionManager,"TheActionManager", MSGNEW("GameEngineSubsystem") ActionManager(), nullptr);
		//initSubsystem((CComObject<WebBrowser> *)TheWebBrowser,"(CComObject<WebBrowser> *)TheWebBrowser", (CComObject<WebBrowser> *)createWebBrowser(), nullptr);
		initSubsystem(TheGameStateMap,"TheGameStateMap", MSGNEW("GameEngineSubsystem") GameStateMap, nullptr );
		initSubsystem(TheGameState,"TheGameState", MSGNEW("GameEngineSubsystem") GameState, nullptr );

		// Create the interface for sending game results
		initSubsystem(TheGameResultsQueue,"TheGameResultsQueue", GameResultsInterface::createNewGameResultsInterface(), nullptr);


	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheGameResultsQueue = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		xferCRC.close();
		TheWritableGlobalData->m_iniCRC = xferCRC.getCRC();
		DEBUG_LOG(("INI CRC is 0x%8.8X", TheGlobalData->m_iniCRC));

		TheSubsystemList->postProcessLoadAll();

		TheFramePacer->setFramesPerSecondLimit(TheGlobalData->m_framesPerSecondLimit);

		TheAudio->setOn(TheGlobalData->m_audioOn && TheGlobalData->m_musicOn, AudioAffect_Music);
		TheAudio->setOn(TheGlobalData->m_audioOn && TheGlobalData->m_soundsOn, AudioAffect_Sound);
		TheAudio->setOn(TheGlobalData->m_audioOn && TheGlobalData->m_sounds3DOn, AudioAffect_Sound3D);
		TheAudio->setOn(TheGlobalData->m_audioOn && TheGlobalData->m_speechOn, AudioAffect_Speech);

		// We're not in a network game yet, so set the network singleton to nullptr.
		TheNetwork = nullptr;

		//Create a default ini file for options if it doesn't already exist.
		//OptionPreferences prefs( TRUE );

		// If we turn m_quitting to FALSE here, then we throw away any requests to quit that
		// took place during loading. :-\ - jkmcd
		// If this really needs to take place, please make sure that pressing cancel on the audio
		// load music dialog will still cause the game to quit.
		// m_quitting = FALSE;

		// initialize the MapCache
		TheMapCache = MSGNEW("GameEngineSubsystem") MapCache;
#if defined(__SWITCH__)
		ggc_switch_trace("[ggc] MapCache->updateCache() ...\n", "", "");
#endif
		TheMapCache->updateCache();
#if defined(__SWITCH__)
		ggc_switch_trace("[ggc] MapCache->updateCache() DONE\n", "", "");
#endif


	#ifdef DUMP_PERF_STATS///////////////////////////////////////////////////////////////////////////
	GetPrecisionTimer(&endTime64);//////////////////////////////////////////////////////////////////
	sprintf(Buf,"----------------------------------------------------------------------------After TheMapCache->updateCache = %f seconds",((double)(endTime64-startTime64)/(double)(freq64)));
  startTime64 = endTime64;//Reset the clock ////////////////////////////////////////////////////////
	DEBUG_LOG(("%s", Buf));////////////////////////////////////////////////////////////////////////////
	#endif/////////////////////////////////////////////////////////////////////////////////////////////


		if (TheGlobalData->m_buildMapCache)
		{
			// just quit, since the map cache has already updated
			//populateMapListbox(nullptr, true, true);
			m_quitting = TRUE;
		}

		// load the initial shell screen
		//TheShell->push( "Menus/MainMenu.wnd" );

#if defined(__ANDROID__) || defined(GGC_RENDER_BACKEND_BGFX)
		// TheSuperHackers @feature bobtista 14/06/2026 Android bring-up: skip only
		// the intro/logo movies (no video backend), but keep the 3D shell map
		// enabled so it renders behind the main menu. With m_playIntro=FALSE and
		// m_afterIntro=TRUE, GameClient::update() takes the post-intro branch and
		// calls TheShell->showShellMap(TRUE) + showShell() itself, which loads the
		// shell map and pushes the main menu on top.
		// TheSuperHackers @diagnostic githubawn 21/06/2026 Also force this on the
		// win32-bgfx build so the shell-map "background battle" loads for on-device
		// A/B comparison against Android.
		TheWritableGlobalData->m_playIntro = FALSE;
		TheWritableGlobalData->m_afterIntro = TRUE;
#endif

		// This allows us to run a map from the command line
		if (TheGlobalData->m_initialFile.isEmpty() == FALSE)
		{
			AsciiString fname = TheGlobalData->m_initialFile;
			fname.toLower();

			if (fname.endsWithNoCase(".map"))
			{
				TheWritableGlobalData->m_shellMapOn = FALSE;
				TheWritableGlobalData->m_playIntro = FALSE;
				TheWritableGlobalData->m_pendingFile = TheGlobalData->m_initialFile;

				// shutdown the top, but do not pop it off the stack
	//			TheShell->hideShell();

				// send a message to the logic for a new game
				GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_NEW_GAME );
				msg->appendIntegerArgument(GAME_SINGLE_PLAYER);
				msg->appendIntegerArgument(DIFFICULTY_NORMAL);
				msg->appendIntegerArgument(0);
				InitRandom(0);
			}
		}

		// TheSuperHackers @feature bobtista 17/04/2026 Load a save game file
		// from the command line. Deferred to the first update tick via
		// MSG_NEW_GAME so the game loop and UI systems are fully initialized
		// before the load occurs. The actual loadGame() call happens in the
		// update() method when m_loadSaveGame is non-empty.
		if (TheGlobalData->m_loadSaveGame.isEmpty() == FALSE)
		{
			TheWritableGlobalData->m_shellMapOn = FALSE;
			TheWritableGlobalData->m_playIntro = FALSE;
		}

		//
		if (TheMapCache && TheGlobalData->m_shellMapOn)
		{
			AsciiString lowerName = TheGlobalData->m_shellMapName;
			lowerName.toLower();

			MapCache::const_iterator it = TheMapCache->find(lowerName);
			if (it == TheMapCache->end())
			{
#if defined(__ANDROID__)
				// TheSuperHackers @bugfix bobtista 15/06/2026 On Android the map
				// cache may not list the shell map, but it still loads directly
				// from MapsZH.big via the archive file system. Keep the shell map
				// enabled instead of disabling it on a cache miss.
				__android_log_print(4, "ggc-shell",
					"shell map '%s' not in MapCache (%d entries) - keeping enabled",
					lowerName.str(), (int)TheMapCache->size());
#elif defined(GGC_RENDER_BACKEND_BGFX)
				// TheSuperHackers @diagnostic githubawn 21/06/2026 Keep the shell map
				// enabled on a cache miss for the win32-bgfx A/B harness too.
#else
				TheWritableGlobalData->m_shellMapOn = FALSE;
#endif
			}
		}

		if(!TheGlobalData->m_playIntro && TheGlobalData->m_loadSaveGame.isEmpty())
		{
			TheWritableGlobalData->m_afterIntro = TRUE;
		}

	}
	catch (ErrorCode ec)
	{
		if (ec == ERROR_INVALID_D3D)
		{
			RELEASE_CRASHLOCALIZED("ERROR:D3DFailurePrompt", "ERROR:D3DFailureMessage");
		}
	}
	catch (INIException e)
	{
		if (e.mFailureMessage)
			RELEASE_CRASH((e.mFailureMessage));
		else
			RELEASE_CRASH(("Uncaught Exception during initialization."));

	}
	catch (...)
	{
		RELEASE_CRASH(("Uncaught Exception during initialization."));
	}

	if(!TheGlobalData->m_playIntro && TheGlobalData->m_loadSaveGame.isEmpty())
	{
		TheWritableGlobalData->m_afterIntro = TRUE;
	}

#if defined(__SWITCH__) || defined(__3DS__)
	ggc_switch_trace("[ggc] GameEngine::init resetSubsystems() ...\n", "", "");
	ggc_switch_trace("[ggc] TheGameLogic=", TheGameLogic ? "non-null" : "NULL", "\n");
	ggc_switch_trace("[ggc] TheSubsystemList=", TheSubsystemList ? "non-null" : "NULL", "\n");
#endif
	resetSubsystems();
#if defined(__SWITCH__) || defined(__3DS__)
	ggc_switch_trace("[ggc] GameEngine::init resetSubsystems() DONE\n", "", "");
#endif

	HideControlBar();
#if defined(__SWITCH__) || defined(__3DS__)
	ggc_switch_trace("[ggc] GameEngine::init RETURNING (main loop next)\n", "", "");
#endif
}

/** -----------------------------------------------------------------------------------------------
	* Reset all necessary parts of the game engine to be ready to accept new game data
	*/
void GameEngine::reset()
{

	WindowLayout *background = TheWindowManager->winCreateLayout("Menus/BlankWindow.wnd");
	DEBUG_ASSERTCRASH(background,("We Couldn't Load Menus/BlankWindow.wnd"));
	background->hide(FALSE);
	background->bringForward();
	background->getFirstWindow()->winClearStatus(WIN_STATUS_IMAGE);
	Bool deleteNetwork = false;
	if (TheGameLogic->isInMultiplayerGame())
		deleteNetwork = true;

	resetSubsystems();

	if (deleteNetwork)
	{
		DEBUG_ASSERTCRASH(TheNetwork, ("Deleting null TheNetwork!"));
		delete TheNetwork;
		TheNetwork = nullptr;
	}
	if(background)
	{
		background->destroyWindows();
		deleteInstance(background);
		background = nullptr;
	}
}

/// -----------------------------------------------------------------------------------------------
void GameEngine::resetSubsystems()
{
	// TheSuperHackers @fix xezon 09/06/2025 Reset GameLogic first to purge all world objects early.
	// This avoids potentially catastrophic issues when objects and subsystems have cross dependencies.
	TheGameLogic->reset();

	TheSubsystemList->resetAll();
}

/// -----------------------------------------------------------------------------------------------
Bool GameEngine::canUpdateGameLogic()
{
	// Must be first.
	TheGameLogic->preUpdate();

	TheFramePacer->setTimeFrozen(isTimeFrozen());
	TheFramePacer->setGameHalted(isGameHalted());

	if (TheNetwork != nullptr)
	{
		return canUpdateNetworkGameLogic();
	}
	else
	{
		return canUpdateRegularGameLogic();
	}
}

/// -----------------------------------------------------------------------------------------------
Bool GameEngine::canUpdateNetworkGameLogic()
{
	DEBUG_ASSERTCRASH(TheNetwork != nullptr, ("TheNetwork is null"));

	if (TheNetwork->isFrameDataReady())
	{
		// Important: The Network is definitely no longer stalling.
		TheFramePacer->setGameHalted(false);

		return true;
	}

	return false;
}

/// -----------------------------------------------------------------------------------------------
Bool GameEngine::canUpdateRegularGameLogic()
{
	const Bool enabled = TheFramePacer->isLogicTimeScaleEnabled();
	const Int logicTimeScaleFps = TheFramePacer->getLogicTimeScaleFps();
	const Int maxRenderFps = TheFramePacer->getFramesPerSecondLimit();

#if defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
	const Bool useFastMode = TheGlobalData->m_TiVOFastMode;
#else	//always allow this cheat key if we're in a replay game.
	const Bool useFastMode = TheGlobalData->m_TiVOFastMode && TheGameLogic->isInReplayGame();
#endif

	if (useFastMode || !enabled || logicTimeScaleFps >= maxRenderFps)
	{
		// Logic time scale is uncapped or larger equal Render FPS. Update straight away.
		return true;
	}
	else
	{
		// TheSuperHackers @tweak xezon 06/08/2025
		// The logic time step is now decoupled from the render update.
		const Real targetFrameTime = 1.0f / logicTimeScaleFps;
		m_logicTimeAccumulator += min(TheFramePacer->getUpdateTime(), targetFrameTime);

		if (m_logicTimeAccumulator >= targetFrameTime)
		{
			m_logicTimeAccumulator -= targetFrameTime;
			return true;
		}
	}

	return false;
}

/// -----------------------------------------------------------------------------------------------
DECLARE_PERF_TIMER(GameEngine_update)

/** -----------------------------------------------------------------------------------------------
 * Update the game engine by updating the GameClient and GameLogic singletons.
 */
void GameEngine::update()
{
#if defined(__SWITCH__)
	static int s_ggcFirstUpd = 3;   // trace the first 3 frames
	{ static bool s_ggcEnt = true; if (s_ggcEnt) { ggc_switch_trace("[ggc] GE::update ENTERED\n", "", ""); s_ggcEnt = false; } }
#endif
	USE_PERF_TIMER(GameEngine_update)
	{
		{
			// VERIFY CRC needs to be in this code block.  Please to not pull TheGameLogic->update() inside this block.
			VERIFY_CRC

#if defined(__SWITCH__)
			if (s_ggcFirstUpd) ggc_switch_trace("[ggc] GE::update frame1: before Radar\n", "", "");
#endif
			TheRadar->UPDATE();

			/// @todo Move audio init, update, etc, into GameClient update

#if defined(__SWITCH__)
			if (s_ggcFirstUpd) ggc_switch_trace("[ggc] GE::update frame1: before Audio\n", "", "");
#endif
			TheAudio->UPDATE();
#if defined(__SWITCH__)
			if (s_ggcFirstUpd) ggc_switch_trace("[ggc] GE::update frame1: before GameClient\n", "", "");
#endif
			TheGameClient->UPDATE();
#if defined(__SWITCH__)
			if (s_ggcFirstUpd) ggc_switch_trace("[ggc] GE::update frame1: after GameClient\n", "", "");
#endif
			TheMessageStream->propagateMessages();
#if defined(__SWITCH__)
			if (s_ggcFirstUpd) ggc_switch_trace("[ggc] GE::update frame1: after propagateMessages\n", "", "");
#endif

			// TheSuperHackers @bugfix bobtista 30/04/2026 Defer visual
			// command-line replay loading until after the no-logo shell startup
			// runs. The replay menu starts playback from live shell UI state,
			// and direct loading before that leaves replay/control-bar windows
			// in a different state.
			if (TheGlobalData->m_loadReplayGame.isEmpty() == FALSE)
			{
				AsciiString replayGame = TheGlobalData->m_loadReplayGame;
				TheWritableGlobalData->m_loadReplayGame.clear();

				if (TheRecorder->playbackFile(replayGame))
				{
					if (TheShell)
					{
						TheShell->hideShell();
					}
					TheWritableGlobalData->m_afterIntro = FALSE;
				}
				else
				{
					DEBUG_LOG(("Failed to load replay '%s'", replayGame.str()));
					m_quitting = TRUE;
				}
			}

			if (TheNetwork != nullptr)
			{
				TheNetwork->UPDATE();
			}
		}

		const Bool canUpdate = canUpdateGameLogic();
		const Bool canUpdateLogic = canUpdate && !TheFramePacer->isGameHalted() && !TheFramePacer->isTimeFrozen();
		const Bool canUpdateScript = canUpdate && !TheFramePacer->isGameHalted();
#if defined(__SWITCH__)
		if (s_ggcFirstUpd) ggc_switch_trace("[ggc] GE::update frame1: after Net; canLogic=", canUpdateLogic ? "1\n" : "0\n", "");
#endif

		if (canUpdateLogic)
		{
			TheGameClient->step();
#if defined(__SWITCH__)
			if (s_ggcFirstUpd) ggc_switch_trace("[ggc] GE::update frame1: after GameClient->step\n", "", "");
#endif
			TheGameLogic->UPDATE();
#if defined(__SWITCH__)
			if (s_ggcFirstUpd) ggc_switch_trace("[ggc] GE::update frame1: after GameLogic->UPDATE\n", "", "");
#endif
		}
		else if (canUpdateScript)
		{
			// TheSuperHackers @info Still update the Script Engine to allow
			// for scripted camera movements while the time is frozen.
			TheScriptEngine->UPDATE();
		}
#if defined(__SWITCH__)
		if (s_ggcFirstUpd) { ggc_switch_trace("[ggc] GE::update frameN: END OF FRAME\n", "", ""); s_ggcFirstUpd--; }
#endif
	}
}

// Horrible reference, but we really, really need to know if we are windowed.
extern bool DX8Wrapper_IsWindowed;
extern HWND ApplicationHWnd;

/** -----------------------------------------------------------------------------------------------
 * The "main loop" of the game engine. It will not return until the game exits.
 */
void GameEngine::execute()
{
#if defined(__SWITCH__)
	ggc_switch_trace("[ggc] GameEngine::execute() entered (main loop)\n", "", "");
#endif
#if defined(RTS_DEBUG)
	DWORD startTime = timeGetTime() / 1000;
#endif

	// TheSuperHackers @feature bobtista 17/04/2026 Deferred save game load.
	// Load before the main loop. The shell/intro are already suppressed by
	// m_shellMapOn=FALSE and m_playIntro=FALSE set in init(). After loading,
	// hide all shell UI so the game is immediately playable.
	if (TheGlobalData->m_loadSaveGame.isEmpty() == FALSE)
	{
		AvailableGameInfo gameInfo;
		gameInfo.filename = TheGlobalData->m_loadSaveGame;
		gameInfo.next = nullptr;
		gameInfo.prev = nullptr;

		AsciiString fullPath = TheGameState->getFilePathInSaveDirectory(gameInfo.filename);
		TheGameState->getSaveGameInfoFromFile(fullPath, &gameInfo.saveGameInfo);

		TheGameLogic->prepareNewGame(GAME_SINGLE_PLAYER, DIFFICULTY_NORMAL, 0);

		if (TheGameState->loadGame(gameInfo) == SC_OK)
		{
			if (TheShell)
			{
				TheShell->hideShell();
			}
			TheWritableGlobalData->m_afterIntro = FALSE;
		}
		else
		{
			DEBUG_LOG(("Failed to load save game '%s'", TheGlobalData->m_loadSaveGame.str()));
			TheWritableGlobalData->m_loadSaveGame.clear();
			m_quitting = TRUE;
		}
	}

#if defined(__SWITCH__)
	{
		char b[128];
		int n = snprintf(b, sizeof(b), "[ggc] execute: reached while, m_quitting=%d loadSaveEmpty=%d\n",
			(int)m_quitting, (int)TheGlobalData->m_loadSaveGame.isEmpty());
		if (n > 0) svcOutputDebugString(b, (unsigned)n);
	}
#endif
	// pretty basic for now
	while( !m_quitting )
	{

		//if (TheGlobalData->m_vTune)
		{
#ifdef PERF_TIMERS
			PerfGather::resetAll();
#endif
		}

		{

#if defined(RTS_DEBUG)
			{
				// enter only if in benchmark mode
				if (TheGlobalData->m_benchmarkTimer > 0)
				{
					DWORD currentTime = timeGetTime() / 1000;
					if (TheGlobalData->m_benchmarkTimer < currentTime - startTime)
					{
						if (TheGameLogic->isInGame())
						{
							if (TheRecorder->getMode() == RECORDERMODETYPE_RECORD)
							{
								TheRecorder->stopRecording();
							}
							TheGameLogic->clearGameData();
						}
						TheGameEngine->setQuitting(TRUE);
					}
				}
			}
#endif

			{
				try
				{
					// compute a frame
					update();
				}
				catch (INIException e)
				{
					// Release CRASH doesn't return, so don't worry about executing additional code.
					if (e.mFailureMessage)
						RELEASE_CRASH((e.mFailureMessage));
					else
						RELEASE_CRASH(("Uncaught Exception in GameEngine::update"));
				}
				catch (...)
				{
#if defined(__ANDROID__)
					// TheSuperHackers @diagnostic bobtista 15/06/2026 Identify the
					// exception type/message so shell-map load failures are debuggable.
					try { throw; }
					catch (const std::exception& se) {
						__android_log_print(6, "ggc-crash", "update std::exception: %s", se.what());
					}
					catch (ErrorCode ec) {
						__android_log_print(6, "ggc-crash", "update threw ErrorCode: 0x%08x", (unsigned)ec);
					}
					catch (...) {
						const std::type_info *ti = abi::__cxa_current_exception_type();
						__android_log_print(6, "ggc-crash", "update threw type: %s", ti ? ti->name() : "(unknown)");
					}
#endif
#if defined(__3DS__)
					// TheSuperHackers @diagnostic githubawn 19/07/2026 Same rationale as the
					// __ANDROID__ block above: ReleaseCrashInfo.txt only records "Uncaught
					// Exception in GameEngine::update" with an empty "Last error", so a real
					// crash-on-exit report (seen when quitting a match) gives no clue which
					// exception fired or why. File-based via ggc_switch_trace (not SDL_Log)
					// since this runs on the way to a fatal RELEASE_CRASH and SDL's own
					// logging state may not be reliable at that point.
					try { throw; }
					catch (const std::exception& se) {
						ggc_switch_trace("[ggc] update EXCEPTION std::exception: ", se.what(), "\n");
					}
					catch (ErrorCode ec) {
						char buf[32];
						snprintf(buf, sizeof(buf), "0x%08x", (unsigned)ec);
						ggc_switch_trace("[ggc] update EXCEPTION ErrorCode: ", buf, "\n");
					}
					catch (...) {
						const std::type_info *ti = abi::__cxa_current_exception_type();
						ggc_switch_trace("[ggc] update EXCEPTION type: ", ti ? ti->name() : "(unknown)", "\n");
					}
#endif
					// try to save info off
					try
					{
						if (TheRecorder && TheRecorder->getMode() == RECORDERMODETYPE_RECORD && TheRecorder->isMultiplayer())
							TheRecorder->cleanUpReplayFile();
					}
					catch (...)
					{
					}
					RELEASE_CRASH(("Uncaught Exception in GameEngine::update"));
				}
			}

#if defined(__SWITCH__)
			{ static int f=0; if (f<5) { char t[72]; snprintf(t,sizeof(t),"[ggc] execute: frame %d update() returned\n", f); ggc_switch_trace(t,"",""); } }
#endif
			TheFramePacer->update();
#if defined(__SWITCH__)
			{ static int f=0; if (f<5) { char t[72]; snprintf(t,sizeof(t),"[ggc] execute: frame %d FramePacer done\n", f++); ggc_switch_trace(t,"",""); } }
#endif
		}

#ifdef PERF_TIMERS
		if (!m_quitting && TheGameLogic->isInGame() && !TheGameLogic->isInShellGame() && !TheGameLogic->isGamePaused())
		{
			PerfGather::dumpAll(TheGameLogic->getFrame());
			PerfGather::displayGraph(TheGameLogic->getFrame());
			PerfGather::resetAll();
		}
#endif

	}
#if defined(__SWITCH__)
	ggc_switch_trace("[ggc] execute: loop EXITED (game quitting)\n", "", "");
#endif
}

/** -----------------------------------------------------------------------------------------------
	* Factory for the message stream
	*/
MessageStream *GameEngine::createMessageStream()
{
	// if you change this update the tools that use the engine systems
	// like GUIEdit, it creates a message stream to run in "test" mode
	return MSGNEW("GameEngineSubsystem") MessageStream;
}

//-------------------------------------------------------------------------------------------------
FileSystem *GameEngine::createFileSystem()
{
	return MSGNEW("GameEngineSubsystem") FileSystem;
}

//-------------------------------------------------------------------------------------------------
Bool GameEngine::isMultiplayerSession()
{
	return TheRecorder->isMultiplayer();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
#define CONVERT_EXEC1	"..\\Build\\nvdxt -list buildDDS.txt -dxt5 -full -outdir Art\\Textures > buildDDS.out"

// TheSuperHackers @build githubawn 17/06/2026 TARGET_OS_IPHONE detection so the
// dev-time TGA->DDS pipeline (which shells out via system()) can be excluded on
// iOS, where system() is marked unavailable by the SDK.
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

void updateTGAtoDDS()
{
	// Here's the scoop. We're going to traverse through all of the files in the Art\Textures folder
	// and determine if there are any .tga files that are newer than associated .dds files. If there
	// are, then we will re-run the compression tool on them.

	File *fp = TheLocalFileSystem->openFile("buildDDS.txt", File::WRITE | File::CREATE | File::TRUNCATE | File::TEXT);
	if (!fp) {
		return;
	}

	FilenameList files;
	TheLocalFileSystem->getFileListInDirectory("Art\\Textures\\", "", "*.tga", files, TRUE);
	FilenameList::iterator it;
	for (it = files.begin(); it != files.end(); ++it) {
		AsciiString filenameTGA = *it;
		AsciiString filenameDDS = *it;
		FileInfo infoTGA;
		TheLocalFileSystem->getFileInfo(filenameTGA, &infoTGA);

		// skip the water textures, since they need to be NOT compressed
		filenameTGA.toLower();
		if (strstr(filenameTGA.str(), "caust"))
		{
			continue;
		}
		// and the recolored stuff.
		if (strstr(filenameTGA.str(), "zhca"))
		{
			continue;
		}

		// replace tga with dds
		filenameDDS.truncateBy(3); // tga
		filenameDDS.concat("dds");

		Bool needsToBeUpdated = FALSE;
		FileInfo infoDDS;
		if (TheFileSystem->doesFileExist(filenameDDS.str())) {
			TheFileSystem->getFileInfo(filenameDDS, &infoDDS);
			if (infoTGA.timestampHigh > infoDDS.timestampHigh ||
					(infoTGA.timestampHigh == infoDDS.timestampHigh &&
					 infoTGA.timestampLow > infoDDS.timestampLow)) {
				needsToBeUpdated = TRUE;
			}
		} else {
			needsToBeUpdated = TRUE;
		}

		if (!needsToBeUpdated) {
			continue;
		}

		filenameTGA.concat("\n");
		fp->write(filenameTGA.str(), filenameTGA.getLength());
	}

	fp->close();

#if defined(__APPLE__) && TARGET_OS_IPHONE
	// system() is unavailable on iOS; the nvdxt TGA->DDS dev pipeline does not
	// apply to mobile builds. Skip it.
#else
	system(CONVERT_EXEC1);
#endif
}
