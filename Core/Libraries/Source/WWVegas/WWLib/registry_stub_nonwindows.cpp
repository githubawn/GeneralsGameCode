/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

// TheSuperHackers @build bobtista 13/06/2026 The real RegistryClass (registry.cpp)
// is Windows-only. A handful of engine sites (W3DDisplay, dx8wrapper, WWAudio)
// still reference it to read/write display & audio preferences. On non-Windows
// there is no system registry, so provide a no-op implementation: reads return
// the caller's supplied default, writes are dropped. Settings simply fall back
// to engine defaults, which is the desired behaviour on Android.

#ifndef _WIN32

#include "registry.h"
#include <cstring>

bool RegistryClass::IsLocked = false;

RegistryClass::RegistryClass(const char * /*sub_key*/, bool /*create*/)
{
	Key = 0;
	IsValid = false;
}

RegistryClass::~RegistryClass()
{
}

int RegistryClass::Get_Int(const char * /*name*/, int def_value)
{
	return def_value;
}

void RegistryClass::Set_Int(const char * /*name*/, int /*value*/)
{
}

char *RegistryClass::Get_String(const char * /*name*/, char *value, int value_size,
	const char *default_value)
{
	if (value != nullptr && value_size > 0)
	{
		if (default_value != nullptr)
		{
			std::strncpy(value, default_value, value_size - 1);
			value[value_size - 1] = '\0';
		}
		else
		{
			value[0] = '\0';
		}
	}
	return value;
}

void RegistryClass::Set_String(const char * /*name*/, const char * /*value*/)
{
}

#endif // !_WIN32
