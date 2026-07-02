@echo off
call "C:\code\vc6\VC6SP6\VC98\Bin\VCVARS32.BAT"
cd /d C:\code\GGC4\scratch
echo === which link ===
where link
echo === compile+link ===
CL /nologo /GX ns_spike_vc6.cpp /Fens_spike_vc6.exe
echo CL_EXIT=%ERRORLEVEL%
