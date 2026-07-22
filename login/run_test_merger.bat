@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0test_merger"
C:\Qt\5.15.2\msvc2019_64\bin\qmake.exe test_merger.pro
nmake
release\test_merger.exe
