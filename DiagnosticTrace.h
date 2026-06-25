#ifndef DIAGNOSTIC_TRACE_H
#define DIAGNOSTIC_TRACE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>

inline void LogDiagnosticTrace(const char* funcName)
{
	FILE* f = fopen("trace.txt", "a");
	if (f)
	{
		fprintf(f, "TRACE: %s\n", funcName);
		
		void* stack[16];
		USHORT frames = CaptureStackBackTrace(1, 16, stack, NULL);
		for (USHORT i = 0; i < frames; i++)
		{
			HMODULE hModule = NULL;
			GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			                  (LPCSTR)stack[i], &hModule);
			
			char modulePath[MAX_PATH] = "Unknown";
			if (hModule)
			{
				char fullPath[MAX_PATH] = {0};
				GetModuleFileNameA(hModule, fullPath, MAX_PATH);
				char* fileName = strrchr(fullPath, '\\');
				if (fileName)
				{
					strcpy(modulePath, fileName + 1);
				}
				else
				{
					strcpy(modulePath, fullPath);
				}
			}
			
			if (hModule)
			{
				DWORD_PTR offset = (DWORD_PTR)stack[i] - (DWORD_PTR)hModule;
				fprintf(f, "  [%d] %s+0x%p (Address: %p)\n", i, modulePath, (void*)offset, stack[i]);
			}
			else
			{
				fprintf(f, "  [%d] %s (Address: %p)\n", i, modulePath, stack[i]);
			}
		}
		fclose(f);
	}
}

#endif // DIAGNOSTIC_TRACE_H
