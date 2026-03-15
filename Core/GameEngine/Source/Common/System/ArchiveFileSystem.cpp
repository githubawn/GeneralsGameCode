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

//----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//----------------------------------------------------------------------------
//
// Project:   Generals
//
// Module:    Game Engine Common
//
// File name: ArchiveFileSystem.cpp
//
// Created:   11/26/01 TR
//
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//         Includes
//----------------------------------------------------------------------------

#include "PreRTS.h"
#include "Common/ArchiveFile.h"
#include "Common/ArchiveFileSystem.h"
#include "Common/AsciiString.h"
#include "Common/PerfTimer.h"
#include <stdio.h>


//----------------------------------------------------------------------------
//         Externals
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Defines
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Types
//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
//         Private Data
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Public Data
//----------------------------------------------------------------------------

ArchiveFileSystem *TheArchiveFileSystem = nullptr;

double s_totalOldTime = 0.0;
double s_totalNewTime = 0.0;
double s_totalIOTime = 0.0; // Time spent in openArchiveFile (I/O + BIG parsing)
int s_archiveCount = 0;
static ArchivedDirectoryInfo s_fairOldRoot; // Persistent tree for fair comparison


//----------------------------------------------------------------------------
//         Private Prototypes
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Functions
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Public Functions
//----------------------------------------------------------------------------

//------------------------------------------------------
// ArchivedFileInfo
//------------------------------------------------------
ArchiveFileSystem::ArchiveFileSystem()
{
}

ArchiveFileSystem::~ArchiveFileSystem()
{
	ArchiveFileMap::iterator iter = m_archiveFileMap.begin();
	while (iter != m_archiveFileMap.end()) {
		ArchiveFile *file = iter->second;
		delete file;
		iter++;
	}
}

void ArchiveFileSystem::loadIntoDirectoryTree(ArchiveFile *archiveFile, Bool overwrite)
{

	FilenameList filenameList;

	archiveFile->getFileListInDirectory("", "", "*", filenameList, TRUE);

	// Benchmarking start
	LARGE_INTEGER freq, start, end;
	QueryPerformanceFrequency(&freq);

	// --- Benchmark OLD version (Fair comparison against a persistent tree) ---
	QueryPerformanceCounter(&start);
	{
		FilenameListIter oldIter = filenameList.begin();
		while (oldIter != filenameList.end())
		{
			ArchivedDirectoryInfo *dirInfo = &s_fairOldRoot;
			AsciiString path;
			AsciiString token;
			AsciiString tokenizer = *oldIter;
			tokenizer.toLower();
			Bool infoInPath = tokenizer.nextToken(&token, "\\/");

			while (infoInPath && (!token.find('.') || tokenizer.find('.')))
			{
				path.concat(token);
				path.concat('\\');
				ArchivedDirectoryInfoMap::iterator tempiter = dirInfo->m_directories.find(token);
				if (tempiter == dirInfo->m_directories.end())
				{
					dirInfo = &(dirInfo->m_directories[token]);
					dirInfo->m_path = path;
					dirInfo->m_directoryName = token;
				}
				else
				{
					dirInfo = &tempiter->second;
				}
				infoInPath = tokenizer.nextToken(&token, "\\/");
			}
			// Simulate the insertion phase
			dirInfo->m_files.insert(dirInfo->m_files.end(), std::make_pair(token, archiveFile));
			oldIter++;
		}
	}
	QueryPerformanceCounter(&end);
	double oldTime = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;

	// --- Benchmark NEW version ---
	QueryPerformanceCounter(&start);
	FilenameListIter it = filenameList.begin(); // Reset iterator
	char scratch[AsciiString::MAX_LEN + 1];
	while (it != filenameList.end())
	{
		ArchivedDirectoryInfo *dirInfo = &m_rootDirectory;

		const char* src = it->str();
		int len = it->getLength();
		if (len > AsciiString::MAX_LEN) len = AsciiString::MAX_LEN;

		// 1. Lowercase and unify slashes in one pass into scratch buffer
		for (int i = 0; i < len; ++i) {
			char c = src[i];
			if (c >= 'A' && c <= 'Z') scratch[i] = (char)(c + ('a' - 'A'));
			else if (c == '/') scratch[i] = '\\';
			else scratch[i] = c;
		}
		scratch[len] = 0;

		AsciiString path;
		char* currentSegment = scratch;
		char* nextSep = strchr(currentSegment, '\\');

		while (nextSep != nullptr)
		{
			*nextSep = 0; // Temporarily terminate segment

			// Condition for directory: segment has no dot, OR there is another dot later in the path
			bool isDirectory = (strchr(currentSegment, '.') == nullptr) || (strchr(nextSep + 1, '.') != nullptr);

			if (isDirectory) {
				AsciiString token(currentSegment);
				path.concat(token);
				path.concat('\\');

				ArchivedDirectoryInfoMap::iterator tempiter = dirInfo->m_directories.find(token);
				if (tempiter == dirInfo->m_directories.end())
				{
					dirInfo = &(dirInfo->m_directories[token]);
					dirInfo->m_path = path;
					dirInfo->m_directoryName = token;
				}
				else
				{
					dirInfo = &tempiter->second;
				}

				*nextSep = '\\'; // Restore slash
				currentSegment = nextSep + 1;
				nextSep = strchr(currentSegment, '\\');
			}
			else {
				*nextSep = '\\'; // Restore slash and treat as part of the filename
				break;
			}
		}

		AsciiString token(currentSegment);

		ArchivedFileLocationMap::iterator fileIt;
		if (overwrite)
		{
			// When overwriting, try place the new value at the beginning of the key list.
			fileIt = dirInfo->m_files.find(token);
		}
		else
		{
			// Append to the end of the key list.
			fileIt = dirInfo->m_files.end();
		}


		dirInfo->m_files.insert(fileIt, std::make_pair(token, archiveFile));

#if defined(DEBUG_LOGGING) && ENABLE_FILESYSTEM_LOGGING
		{
			const stl::const_range<ArchivedFileLocationMap> range = stl::get_range(dirInfo->m_files, token, 0);
			if (range.distance() >= 2)
			{
				ArchivedFileLocationMap::const_iterator rangeIt0;
				ArchivedFileLocationMap::const_iterator rangeIt1;

				if (overwrite)
				{
					rangeIt0 = range.begin;
					rangeIt1 = std::next(rangeIt0);

					DEBUG_LOG(("ArchiveFileSystem::loadIntoDirectoryTree - adding file %s, archived in %s, overwriting same file in %s",
						it->str(),
						rangeIt0->second->getName().str(),
						rangeIt1->second->getName().str()
					));
				}
				else
				{
					rangeIt1 = std::prev(range.end);
					rangeIt0 = std::prev(rangeIt1);

					DEBUG_LOG(("ArchiveFileSystem::loadIntoDirectoryTree - adding file %s, archived in %s, overwritten by same file in %s",
						it->str(),
						rangeIt1->second->getName().str(),
						rangeIt0->second->getName().str()
					));
				}
			}
			else
			{
				DEBUG_LOG(("ArchiveFileSystem::loadIntoDirectoryTree - adding file %s, archived in %s", it->str(), archiveFile->getName().str()));
			}
		}
#endif

		it++;
	}
	QueryPerformanceCounter(&end);
	double newTime = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;

	s_totalOldTime += oldTime;
	s_totalNewTime += newTime;
	s_archiveCount++;

	DEBUG_LOG(("ArchiveFileSystem - Indexing Bench [%s]: OLD: %.4f s, NEW: %.4f s (%.1f%% speedup)", 
		archiveFile->getName().str(), oldTime, newTime, (oldTime > 1e-9) ? (1.0 - newTime/oldTime)*100.0 : 0.0));
}

void ArchiveFileSystem::loadMods()
{
	if (TheGlobalData->m_modBIG.isNotEmpty())
	{
		ArchiveFile *archiveFile = openArchiveFile(TheGlobalData->m_modBIG.str());

		if (archiveFile != nullptr) {
			DEBUG_LOG(("ArchiveFileSystem::loadMods - loading %s into the directory tree.", TheGlobalData->m_modBIG.str()));
			loadIntoDirectoryTree(archiveFile, TRUE);
			m_archiveFileMap[TheGlobalData->m_modBIG] = archiveFile;
			DEBUG_LOG(("ArchiveFileSystem::loadMods - %s inserted into the archive file map.", TheGlobalData->m_modBIG.str()));
		}
		else
		{
			DEBUG_LOG(("ArchiveFileSystem::loadMods - could not openArchiveFile(%s)", TheGlobalData->m_modBIG.str()));
		}
	}

	if (TheGlobalData->m_modDir.isNotEmpty())
	{
#ifdef DEBUG_LOGGING
		Bool ret =
#endif
		loadBigFilesFromDirectory(TheGlobalData->m_modDir, "*.big", TRUE);
		DEBUG_ASSERTLOG(ret, ("loadBigFilesFromDirectory(%s) returned FALSE!", TheGlobalData->m_modDir.str()));
	}

	showBenchmarkSummary();
}

void ArchiveFileSystem::showBenchmarkSummary()
{
	char msg[1024];

	if (s_archiveCount == 0)
	{
		snprintf(msg, sizeof(msg), "Asset Loading Optimization: No archives were indexed during initialization.");
	}
	else
	{
		snprintf(msg, sizeof(msg), 
			"Loading Optimization Benchmark Summary:\n\n"
			"Archives Indexed: %d\n"
			"Total BIG Parsing (I/O): %.4f s\n"
			"Total OLD Indexing (Fair): %.4f s\n"
			"Total NEW Indexing: %.4f s\n\n"
			"Indexing Speedup: %.1f%%\n\n"
			"Note: These results are also saved to 'Benchmark_Loading.txt' in the game folder.",
			s_archiveCount, s_totalIOTime, s_totalOldTime, s_totalNewTime,
			(s_totalOldTime > 1e-9) ? (1.0 - s_totalNewTime / s_totalOldTime) * 100.0 : 0.0);
	}

	// Log to file as well, just in case the popup is missed on mobile devices
	FILE *f = fopen("Benchmark_Loading.txt", "w");
	if (f)
	{
		fprintf(f, "Loading Optimization Benchmark Results\n");
		fprintf(f, "======================================\n");
		fprintf(f, "Archives Indexed: %d\n", s_archiveCount);
		fprintf(f, "Total BIG Parsing (I/O): %.6f s\n", s_totalIOTime);
		fprintf(f, "Total OLD Indexing (Fair): %.6f s\n", s_totalOldTime);
		fprintf(f, "Total NEW Indexing: %.6f s\n", s_totalNewTime);
		fprintf(f, "Indexing Speedup: %.2f%%\n", (s_totalOldTime > 1e-9) ? (1.0 - s_totalNewTime / s_totalOldTime) * 100.0 : 0.0);
		fclose(f);
	}

	MessageBoxA(NULL, msg, "Asset Loading Optimization", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}

Bool ArchiveFileSystem::doesFileExist(const Char *filename, FileInstance instance) const
{
	ArchivedDirectoryInfoResult result = const_cast<ArchiveFileSystem*>(this)->getArchivedDirectoryInfo(filename);

	if (!result.valid())
		return false;

	stl::const_range<ArchivedFileLocationMap> range = stl::get_range(result.dirInfo->m_files, result.lastToken, instance);

	return range.valid();
}

ArchivedDirectoryInfo* ArchiveFileSystem::friend_getArchivedDirectoryInfo(const Char* directory)
{
	ArchivedDirectoryInfoResult result = getArchivedDirectoryInfo(directory);

	return result.dirInfo;
}

ArchiveFileSystem::ArchivedDirectoryInfoResult ArchiveFileSystem::getArchivedDirectoryInfo(const Char* directory)
{
	ArchivedDirectoryInfoResult result;
	ArchivedDirectoryInfo* dirInfo = &m_rootDirectory;

	AsciiString token;
	AsciiString tokenizer = directory;
	tokenizer.toLower();
	Bool infoInPath = tokenizer.nextToken(&token, "\\/");

	while (infoInPath && (!token.find('.') || tokenizer.find('.')))
	{
		ArchivedDirectoryInfoMap::iterator tempiter = dirInfo->m_directories.find(token);
		if (tempiter != dirInfo->m_directories.end())
		{
			dirInfo = &tempiter->second;
			infoInPath = tokenizer.nextToken(&token, "\\/");
		}
		else
		{
			// the directory doesn't exist
			result.dirInfo = nullptr;
			result.lastToken = AsciiString::TheEmptyString;
			return result;
		}
	}

	result.dirInfo = dirInfo;
	result.lastToken = token;
	return result;
}

File * ArchiveFileSystem::openFile(const Char *filename, Int access, FileInstance instance)
{
	ArchiveFile* archive = getArchiveFile(filename, instance);

	if (archive == nullptr)
		return nullptr;

	return archive->openFile(filename, access);
}

Bool ArchiveFileSystem::getFileInfo(const AsciiString& filename, FileInfo *fileInfo, FileInstance instance) const
{
	if (fileInfo == nullptr) {
		return FALSE;
	}

	if (filename.isEmpty()) {
		return FALSE;
	}

	ArchiveFile* archive = getArchiveFile(filename, instance);

	if (archive == nullptr)
		return FALSE;

	return archive->getFileInfo(filename, fileInfo);
}

ArchiveFile* ArchiveFileSystem::getArchiveFile(const AsciiString& filename, FileInstance instance) const
{
	ArchivedDirectoryInfoResult result = const_cast<ArchiveFileSystem*>(this)->getArchivedDirectoryInfo(filename.str());

	if (!result.valid())
		return nullptr;

	stl::const_range<ArchivedFileLocationMap> range = stl::get_range(result.dirInfo->m_files, result.lastToken, instance);

	if (!range.valid())
		return nullptr;
	
	return range.get()->second;
}

void ArchiveFileSystem::getFileListInDirectory(const AsciiString& currentDirectory, const AsciiString& originalDirectory, const AsciiString& searchName, FilenameList &filenameList, Bool searchSubdirectories) const
{
	ArchiveFileMap::const_iterator it = m_archiveFileMap.begin();
	while (it != m_archiveFileMap.end()) {
		it->second->getFileListInDirectory(currentDirectory, originalDirectory, searchName, filenameList, searchSubdirectories);
		it++;
	}
}
