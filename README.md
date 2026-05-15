![GitHub Repo stars](https://img.shields.io/github/stars/OpenBoard-org/openboard)
![GitHub Repo forks](https://img.shields.io/github/forks/OpenBoard-org/openboard)
# OpenBoard
[![latest release](https://img.shields.io/github/v/release/OpenBoard-org/openboard.svg)]()
[![Commits since last release](https://img.shields.io/github/commits-since/OpenBoard-org/openboard/v1.7.7/dev)]()
[![Github Repo Contributors](https://img.shields.io/github/contributors/OpenBoard-org/openboard.svg)]()
[![downloads v1.7.7](https://img.shields.io/github/downloads/OpenBoard-org/openboard/v1.7.7/total)]()
[![Github All Releases](https://img.shields.io/github/downloads/OpenBoard-org/OpenBoard/total.svg)]()

OpenBoard is an open-source cross-platform interactive white board application designed primarily for use in schools. It was originally forked from Open-Sankoré, which was itself based on Uniboard.

### Installing
1.7.7 installers are available for Windows, macOS and Debian on the [Downloads page](https://github.com/OpenBoard-org/OpenBoard/wiki/Downloads).

### Supported platforms 

| Version   | officially maintained platforms | branch |
|------------|--------------------------------------------------------|----|
| 1.7.7 (latest stable)     | Windows 10+, macOS 12+ (for both `x64_64` and `arm64`), Debian 12  | `master` |
| 1.8.0 (active development)     | Windows 10+, macOS 12+ (for both `x64_64` and `arm64`), Debian 12 | `dev` |

### Community-driven packages
On Linux, Debian is the only officially maintained platform. For other platforms, you can thank the awesome community of OpenBoard that provides community-driven packages on a number of other distributions. Check on [this page](https://github.com/OpenBoard-org/OpenBoard/wiki/Downloads) to see if you find what you're looking for. If you actually want to provide support and to be referenced on this page, please open an issue with the relevant information, and we'll be glad to add your contribution.

### Building from source
If you didn't find any installer for your platform, or if you want to modify OpenBoard, you can find instructions on how to build OpenBoard from source on the [wiki](https://github.com/OpenBoard-org/OpenBoard/wiki/Build-OpenBoard-from-source).

#### Windows installer (EXE)
The Windows release scripts can generate an installer (`.exe`) using Inno Setup and a Qt 6.x build.

1. Clone `OpenBoard-ThirdParty` next to this repository (same parent folder).
2. Open PowerShell and run:
   - `.\release_scripts\windows\setup-windows-env.ps1`
   - For Qt 6.7.3: `.\release_scripts\windows\setup-windows-env.ps1 -QtVersion 6.7.3`
3. Restart your terminal so the environment variables are picked up.
4. Build the installer:
   - `release_scripts\windows\release.win7.vc9.bat`
   - If the build output already exists, you can run `release_scripts\windows\create-setup.bat` to only package it.
5. The installer is written to `install/win32/OpenBoard_Installer_<version>.exe`.

Notes:
- The batch scripts default to Qt 6.6.3 but work with other Qt 6.x versions (including 6.7.3) as long as `QT_DIR`/`QT_BIN` point to that Qt installation.
- `qmake.exe` and `lrelease.exe` must be available in `QT_BIN` (install the Qt Tools module `qttools`).

### Qt support
OpenBoard can be compiled with the latest open-source binaries of Qt 6. Support for Qt 5.15 was recently dropped, but you can still build OpenBoard with it, after addressing some minor compiling issues.

### Contribute
You can contribute to OpenBoard by helping on translations, creating new web widgets or improving existing ones, or by developing new features and submit them as pull requests.

#### Translations
OpenBoard is available, thanks to the work of its community, in 35 languages. If you want to contribute to OpenBoard or its dedicated website by adding a new language or updating an existing one, your help will be greatly appreciated.

If you don't know how to do it, you can ask for help here : [Discussions](https://github.com/OpenBoard-org/OpenBoard/discussions). Don't hesitate!

#### Web Widgets

Web Widgets are websites that you can place and use directly on the board ! And to turn a website to an OpenBoard web widget is really simple !

Download and install OpenBoard, then develop your web app in it. You'll even find a web inspector to help you debug your site.

You'll find documentation on how to create a Web Widget from scratch or turn your already built website into an OpenBoard Web Widget [here](https://github.com/OpenBoard-org/OpenBoard/wiki/Creating-Web-Widgets).
