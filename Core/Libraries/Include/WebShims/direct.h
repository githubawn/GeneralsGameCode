#pragma once
// Stub direct.h for web build
#include <unistd.h>
#include <sys/stat.h>

#define _mkdir(p) mkdir(p, 0755)
#define _chdir chdir
#define _getcwd getcwd
