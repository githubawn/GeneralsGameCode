@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
cd /d C:\code\GGC4\scratch
cl /nologo /std:c++20 /EHsc ns_spike.cpp /Fe:ns_spike.exe
echo CL_EXIT=%ERRORLEVEL%
