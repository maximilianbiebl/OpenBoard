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

REM Resolve PROJECT_ROOT to an absolute path for Inno Setup
pushd %SCRIPT_PATH%\..\..
set PROJECT_ROOT=%CD%
popd

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
set WINDEPLOYQT=%QT_BIN%\windeployqt.exe
set BASE_QT_TRANSLATIONS_DIRECTORY=%QT_DIR%\translations

set PATH=%QT_BIN%;%PATH%

if not exist "%QT_BIN%\qmake.exe" (
    echo ERROR: qmake.exe not found in %QT_BIN%
    echo Set QT_DIR / QT_BIN or run setup-windows-env.ps1 first.
    GOTO EXIT_WITH_ERROR
)

if not exist "%LRELEASE%" (
    echo ERROR: lrelease.exe not found in %QT_BIN%
    echo Install Qt Tools ^(qttools^) and rerun setup-windows-env.ps1
    GOTO EXIT_WITH_ERROR
)

if not exist "%WINDEPLOYQT%" (
    echo ERROR: windeployqt.exe not found in %QT_BIN%
    echo Ensure Qt is fully installed.
    GOTO EXIT_WITH_ERROR
)

echo === Environment ===
echo QT_DIR   = %QT_DIR%
echo QT_BIN   = %QT_BIN%
echo BUILD_DIR= %BUILD_DIR%

cd /d %PROJECT_ROOT%

REM Clean previous build output (keep installer dir separate so it survives)
if exist %BUILD_DIR%\product rmdir /S /Q %BUILD_DIR%\product
if exist %BUILD_DIR%\objects rmdir /S /Q %BUILD_DIR%\objects
if exist %BUILD_DIR%\moc rmdir /S /Q %BUILD_DIR%\moc
if exist %BUILD_DIR%\rcc rmdir /S /Q %BUILD_DIR%\rcc
if exist %BUILD_DIR%\ui rmdir /S /Q %BUILD_DIR%\ui

echo === Running qmake ===
"%QT_BIN%\qmake.exe" %APPLICATION_NAME%.pro CONFIG+=release
IF ERRORLEVEL 1 (
    echo ERROR: qmake failed
    GOTO EXIT_WITH_ERROR
)

echo === Running lrelease ===
call "%LRELEASE%" "%APPLICATION_NAME%.pro"
IF ERRORLEVEL 1 (
    echo WARNING: lrelease failed - translations may be missing
)

echo === Reading version ===
if not exist "build\win32\release\version" (
    echo ERROR: build\win32\release\version not found after qmake.
    echo qmake must create this file. Check OpenBoard.pro.
    GOTO EXIT_WITH_ERROR
)
set /p VERSION= < build\win32\release\version
set VERSION=%VERSION: =%
echo Version: %VERSION%

echo === Running nmake ===
nmake release-install
IF ERRORLEVEL 1 (
    echo ERROR: nmake failed
    GOTO EXIT_WITH_ERROR
)

if not exist "build\win32\release\product\%APPLICATION_NAME%.exe" (
    echo ERROR: %APPLICATION_NAME%.exe not found after build.
    GOTO EXIT_WITH_ERROR
)

echo === Running windeployqt ===
"%WINDEPLOYQT%" ^
    --release ^
    --no-translations ^
    --no-system-d3d-compiler ^
    --no-opengl-sw ^
    "build\win32\release\product\%APPLICATION_NAME%.exe"
IF ERRORLEVEL 1 (
    echo WARNING: windeployqt reported errors - some DLLs may be missing
)

REM Copy Qt6OpenGL explicitly (windeployqt sometimes misses it)
if exist "%QT_BIN%\Qt6OpenGL.dll" (
    xcopy /Y "%QT_BIN%\Qt6OpenGL.dll" "build\win32\release\product\"
)

echo === Copying ThirdParty runtime DLLs ===
set PRODUCT_DIR=build\win32\release\product

REM vcpkg runtime DLLs (zlib, libjpeg, libcurl etc.) not handled by windeployqt
set VCPKG_BIN=C:\vcpkg\installed\x64-windows\bin
for %%F in (z.dll jpeg8.dll jpeg62.dll libcurl.dll bz2.dll) do (
    if exist "%VCPKG_BIN%\%%F" xcopy /Y "%VCPKG_BIN%\%%F" "%PRODUCT_DIR%\" >nul
)

REM Poppler + its dependency DLLs (from oschwartz10612 package)
if exist "..\OpenBoard-ThirdParty\poppler\bin" (
    xcopy /Y "..\OpenBoard-ThirdParty\poppler\bin\*.dll" "%PRODUCT_DIR%\" >nul 2>nul
)

REM QuaZip DLL (may live in bin\, lib\win32\, or build\quazip\)
for %%D in (..\OpenBoard-ThirdParty\quazip\bin ..\OpenBoard-ThirdParty\quazip\lib\win32 ..\OpenBoard-ThirdParty\quazip\build\quazip) do (
    if exist "%%D\quazip1-qt6.dll" xcopy /Y "%%D\quazip1-qt6.dll" "%PRODUCT_DIR%\" >nul 2>nul
)

echo === Copying customizations ===
set CUSTOMIZATIONS=build\win32\release\product\customizations
if not exist "%CUSTOMIZATIONS%" mkdir "%CUSTOMIZATIONS%"
xcopy /s /y resources\customizations "%CUSTOMIZATIONS%\" 2>nul

echo === Copying startupHints ===
set STARTUP_HINTS=build\win32\release\product\startupHints
if not exist "%STARTUP_HINTS%" mkdir "%STARTUP_HINTS%"
xcopy /s /y resources\startupHints "%STARTUP_HINTS%\" 2>nul

echo === Copying Qt translations ===
set I18N_DIR=build\win32\release\product\i18n
if not exist "%I18N_DIR%" mkdir "%I18N_DIR%"
xcopy /y "%BASE_QT_TRANSLATIONS_DIRECTORY%\qt_*.qm" "%I18N_DIR%\" 2>nul

if not exist "%INNO_EXE%" (
    echo ERROR: Inno Setup compiler not found.
    echo Set INNO_EXE or run setup-windows-env.ps1 first.
    GOTO EXIT_WITH_ERROR
)

echo === Building installer ===
set PROJECT_ROOT=%PROJECT_ROOT%
call "%INNO_EXE%" "%SCRIPT_PATH%\%APPLICATION_NAME%.iss" /F"%APPLICATION_NAME%_Installer_%VERSION%"
IF ERRORLEVEL 1 (
    echo ERROR: Inno Setup failed
    GOTO EXIT_WITH_ERROR
)

GOTO END

:EXIT_WITH_ERROR
echo.
echo *** BUILD FAILED ***
echo.
cd /d %SCRIPT_PATH%
exit /b 1

:END
echo.
echo === Build finished successfully ===
echo Installer: %INSTALLER_DIR%\%APPLICATION_NAME%_Installer_%VERSION%.exe
echo.
cd /d %SCRIPT_PATH%
exit /b 0
