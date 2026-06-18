/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

// TheSuperHackers @build bobtista 13/06/2026 No-op implementations of the
// Windows-only subsystems whose real .cpp bodies are compiled out on non-Windows
// (crash minidumper, worker process, IME manager). The engine still references a
// few of their symbols at link time; provide harmless stubs so the shared
// library (libz_generals.so) links. None of these features are available on
// Android.

#if !defined(_WIN32)

#include "PreRTS.h"
#include "Common/MiniDumper.h"
#include "Common/WorkerProcess.h"
#include "Common/StackDump.h"
#include "Common/OSDisplay.h"
#include "GameClient/IMEManager.h"

// --- Crash stack-dump global ------------------------------------------------
AsciiString g_LastErrorDump;

// --- OS busy-cursor state (Win32 SetCursor) ---------------------------------
void OSDisplaySetBusyState(Bool, Bool) {}

// --- Crash minidumper -------------------------------------------------------
MiniDumper* TheMiniDumper = nullptr;

MiniDumper::MiniDumper() {}
Bool MiniDumper::IsInitialized() const { return FALSE; }
void MiniDumper::TriggerMiniDump(DumpType) {}
void MiniDumper::shutdownMiniDumper() {}
void MiniDumper::initMiniDumper(const AsciiString&) {}

// --- Worker process ---------------------------------------------------------
WorkerProcess::WorkerProcess() : m_processHandle(nullptr), m_exitcode(0) {}
bool WorkerProcess::startProcess(UnicodeString) { return false; }
void WorkerProcess::update() {}
bool WorkerProcess::isRunning() const { return false; }
bool WorkerProcess::isDone() const { return true; }
DWORD WorkerProcess::getExitCode() const { return m_exitcode; }
AsciiString WorkerProcess::getStdOutput() const { return AsciiString::TheEmptyString; }
void WorkerProcess::kill() {}
bool WorkerProcess::fetchStdOutput() { return true; }

// --- IME (text composition) -------------------------------------------------
IMEManagerInterface* TheIMEManager = nullptr;
IMEManagerInterface* CreateIMEManagerInterface() { return nullptr; }

// --- pthread_cancel ---------------------------------------------------------
// Android's bionic libc does not implement thread cancellation. The GamespySDK
// Linux thread backend calls pthread_cancel(); provide a no-op so it links.
// (Online play is stubbed on Android, so the cancellation never matters.)
#if defined(__ANDROID__)
#include <pthread.h>
extern "C" int pthread_cancel(pthread_t) { return 0; }
#endif

#endif // !_WIN32
