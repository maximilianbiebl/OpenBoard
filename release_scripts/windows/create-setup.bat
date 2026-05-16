@echo off
@echo off
REM --------------------------------------------------------------------
REM This program is free software: you can redistribute it and/or modify
REM it under the terms of the GNU General Public License as published by
REM the Free Software Foundation, either version 2 of the License, or
REM (at your option) any later version.
REM 
REM This program is distributed in the hope that it will be useful,
REM but WITHOUT ANY WARRANTY; without even the implied warranty of
REM MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
REM GNU General Public License for more details.
REM 
REM You should have received a copy of the GNU General Public License
REM along with this program.  If not, see <http://www.gnu.org/licenses/>.
REM ---------------------------------------------------------------------

set SCRIPT_PATH=%~dp0
set PROJECT_ROOT=%SCRIPT_PATH%\..\..

set APPLICATION_NAME=OpenBoard
if "%QT_DIR%"=="" set QT_DIR=C:\Qt\6.6.3\msvc2022_64
if "%QT_BIN%"=="" set QT_BIN=%QT_DIR%\bin
if "%INNO_EXE%"=="" (
    for %%I in (iscc.exe) do set INNO_EXE=%%~$PATH:I
)
if "%INNO_EXE%"=="" if exist "%ProgramFiles(x86)%\Inno Setup 6\iscc.exe" set INNO_EXE=%ProgramFiles(x86)%\Inno Setup 6\iscc.exe
if "%INNO_EXE%"=="" if exist "%ProgramFiles%\Inno Setup 6\iscc.exe" set INNO_EXE=%ProgramFiles%\Inno Setup 6\iscc.exe
if "%OPENBOARD_VCVARS%"=="" if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set OPENBOARD_VCVARS=%%I\VC\Auxiliary\Build\vcvars64.bat
)
if exist "%OPENBOARD_VCVARS%" call "%OPENBOARD_VCVARS%"
set BUILD_DIR=%PROJECT_ROOT%\build\win32\release
set INSTALLER_DIR=%BUILD_DIR%\installer
set LRELEASE=%QT_BIN%\lrelease.exe
set BASE_QT_TRANSLATIONS_DIRECTORY=%QT_DIR%\translations

set PATH=%QT_BIN%;%PATH%

echo %PATH%

cd %PROJECT_ROOT%

set /p VERSION= < build\win32\release\version
REM remove the last character that is a space
set VERSION=%VERSION: =%

echo "VERSION :  %VERSION%"

if not exist "%INNO_EXE%" (
    echo "Inno Setup compiler not found. Set INNO_EXE or run setup-windows-env.ps1"
    GOTO EXIT_WITH_ERROR
)

call "%INNO_EXE%" "%SCRIPT_PATH%\%APPLICATION_NAME%.iss" /F"%APPLICATION_NAME%_Installer_%VERSION%"

:EXIT_WITH_ERROR
echo "Error found"
cd %SCRIPT_PATH%
GOTO EOF

GOTO END

:END
echo "%APPLICATION_NAME% setup created"
echo "Installer output: %INSTALLER_DIR%"

:EOF
