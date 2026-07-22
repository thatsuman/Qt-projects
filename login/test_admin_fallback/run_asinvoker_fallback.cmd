@echo off
setlocal EnableExtensions

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set "ROOT=%~dp0.."

copy /Y "%ROOT%\release\login.exe" "%ROOT%\release\login_asInvoker.exe" >nul
mt.exe -manifest "%ROOT%\test_admin_fallback\asInvoker.manifest" -outputresource:"%ROOT%\release\login_asInvoker.exe";#1 >nul
if errorlevel 1 exit /b %ERRORLEVEL%

set "LOGIN_FORCE_UNELEVATED=1"
set "LOGIN_SUPPRESS_ADMIN_DIALOG=1"
set "PATH=C:\Qt\5.15.2\msvc2019_64\bin;%PATH%"

"%ROOT%\release\login_asInvoker.exe"
echo EXIT:%ERRORLEVEL%
exit /b %ERRORLEVEL%
