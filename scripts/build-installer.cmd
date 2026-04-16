@echo off
setlocal
cd /d "%~dp0"

set "ISCC="
if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"

if not defined ISCC (
	echo Inno Setup 6 not found. Install from https://jrsoftware.org/isdl.php
	echo Then re-run this script from release\Windows ^(same folder as apikulture.exe^).
	exit /b 1
)

if not exist "apikulture.exe" (
	echo apikulture.exe not found in %CD%
	exit /b 1
)
if not exist "slint_cpp.dll" (
	echo slint_cpp.dll not found in %CD%
	exit /b 1
)

"%ISCC%" "%~dp0APIkulture.iss"
if errorlevel 1 exit /b 1

echo.
echo Installer: %~dp0dist\APIkulture-1.0.0-Windows-userSetup.exe
exit /b 0
