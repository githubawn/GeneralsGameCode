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


#include "thread.h"
#include "Except.h"
#include "wwdebug.h"
#pragma warning ( push )
#pragma warning ( disable : 4201 )
#include "systimer.h"
#pragma warning ( pop )

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
// TheSuperHackers @bugfix bobtista 14/06/2026 Real thread support on non-Windows
// via pthreads. Previously ThreadClass was stubbed out (Execute() just returned),
// so worker threads such as the W3D texture loader never ran — leaving all
// textures unloaded (a black screen on Android).
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#endif

ThreadClass::ThreadClass(const char *thread_name, ExceptionHandlerType exception_handler) : handle(0), running(false), thread_priority(0)
{
	if (thread_name) {
		size_t nameLen = strlcpy(ThreadName, thread_name, ARRAY_SIZE(ThreadName));
		(void)nameLen; assert(nameLen < ARRAY_SIZE(ThreadName));
	} else {
		strcpy(ThreadName, "No name");
	}

	ExceptionHandler = exception_handler;
}

ThreadClass::~ThreadClass()
{
	Stop();
}

void __cdecl ThreadClass::Internal_Thread_Function(void* params)
{
	ThreadClass* tc=reinterpret_cast<ThreadClass*>(params);
	tc->running=true;
	tc->ThreadID = GetCurrentThreadId();

#ifdef _WIN32
	Register_Thread_ID(tc->ThreadID, tc->ThreadName);

#if defined(_MSC_VER)
	// MSVC supports structured exception handling (__try/__except)
	if (tc->ExceptionHandler != nullptr) {
		__try {
			tc->Thread_Function();
		} __except(tc->ExceptionHandler(GetExceptionCode(), GetExceptionInformation())) {};
	} else {
		tc->Thread_Function();
	}
#elif defined(__GNUC__) && defined(_WIN32)
	// GCC/MinGW-w64 doesn't support MSVC's __try/__except syntax
	// Call Thread_Function directly without SEH support
	tc->Thread_Function();
#else
	#error "ThreadClass::Internal_Thread_Function: Unsupported compiler. This code requires MSVC or GCC/MinGW-w64 targeting Windows."
#endif

#else //_WIN32
	tc->Thread_Function();
#endif //_WIN32

#ifdef _WIN32
	Unregister_Thread_ID(tc->ThreadID, tc->ThreadName);
#endif // _WIN32
	tc->handle=0;
	tc->ThreadID = 0;
}

#ifdef _UNIX
void *ThreadClass::Internal_Thread_Entry(void *params)
{
	Internal_Thread_Function(params);
	return nullptr;
}
#endif

void ThreadClass::Execute()
{
	WWASSERT(!handle);	// Only one thread at a time!
	#ifdef _UNIX
		#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
			// Emscripten with pthreads disabled: run synchronously or stub
			handle = 0;
			running = false;
		#elif defined(__3DS__)
			// TheSuperHackers @bugfix githubawn 18/07/2026 libctru's
			// pthread_create default stack size is far smaller than what
			// this engine needs -- the exact same class of bug already
			// found and fixed for the MAIN thread (see
			// ThreeDSPlatformStubs.cpp's __stacksize__ override: a large
			// local buffer overflowing a too-small stack corrupts adjacent
			// memory, which manifests later as a jump to a null/garbage
			// function pointer (PC=0) in unrelated code, not as an obvious
			// crash at the actual overflow site). This background thread
			// (LoaderThreadClass, textureloader.cpp -- decodes DDS/TGA
			// pixel data) was never given the same treatment and only now
			// gets exercised heavily enough (real match loading, not just
			// the menu) to overflow its default stack.
			pthread_t tid = 0;
			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setstacksize(&attr, 1 * 1024 * 1024); // 1MB
			if (pthread_create(&tid, &attr, &Internal_Thread_Entry, this) == 0)
			{
				handle = (unsigned long)tid;
			}
			pthread_attr_destroy(&attr);
		#else
			pthread_t tid = 0;
			if (pthread_create(&tid, nullptr, &Internal_Thread_Entry, this) == 0)
			{
				handle = (unsigned long)tid;
			}
		#endif
	#else
		handle=_beginthread(&Internal_Thread_Function,0,this);
		SetThreadPriority((HANDLE)handle,THREAD_PRIORITY_NORMAL+thread_priority);
		WWDEBUG_SAY(("ThreadClass::Execute: Started thread %s, thread ID is %X", ThreadName, handle));
	#endif
}

void ThreadClass::Set_Priority(int priority)
{
	#ifdef _UNIX
		// assert(0);
		return;
	#else
		thread_priority=priority;
		if (handle) SetThreadPriority((HANDLE)handle,THREAD_PRIORITY_NORMAL+thread_priority);
	#endif
}

void ThreadClass::Stop(unsigned ms)
{
	#ifdef _UNIX
		running = false;
		(void)ms;
		#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
			handle = 0;
		#else
			if (handle)
			{
				pthread_join((pthread_t)handle, nullptr);
				handle = 0;
			}
		#endif
		return;
	#else
		running=false;
		unsigned time=TIMEGETTIME();
		while (handle) {
			if ((TIMEGETTIME()-time)>ms) {
				int res=TerminateThread((HANDLE)handle,0);
				res;	// just to silence compiler warnings
				WWASSERT(res);	// Thread still not killed!
				handle=0;
			}
			Sleep(0);
		}
	#endif
}

void ThreadClass::Sleep_Ms(unsigned ms)
{
	Sleep(ms);
}

#ifndef _UNIX
HANDLE test_event = ::CreateEvent (nullptr, FALSE, FALSE, "");
#endif

void ThreadClass::Switch_Thread()
{
	#ifdef _UNIX
		sched_yield();
		return;
	#else
		//	::SwitchToThread ();
		::WaitForSingleObject (test_event, 1);
		//	Sleep(1);	// Note! Parameter can not be 0 (or the thread switch doesn't occur)
	#endif
}

// Return calling thread's unique thread id
unsigned ThreadClass::_Get_Current_Thread_ID()
{
	#ifdef _UNIX
		#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
			return 1;
		#else
			return (unsigned)(uintptr_t)pthread_self();
		#endif
	#else
		return GetCurrentThreadId();
	#endif
}

bool ThreadClass::Is_Running()
{
	return !!handle;
}
