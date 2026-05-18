[CmdletBinding()]
param(
  [string]$QtVersion = "6.6.3",
  [string]$QtArch = "msvc2022_64",
  [string]$QtBase = "C:\Qt",
  [string[]]$QtModules = @("qtdeclarative", "qtmultimedia", "qtwebengine", "qtwebchannel", "qtpositioning", "qtsvg", "qt5compat", "qttools"),
  [switch]$SkipQtDownload
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Test-Command {
  param([string]$Name)
  return Get-Command $Name -ErrorAction SilentlyContinue
}

function Write-Section {
  param([string]$Title)
  Write-Host ""
  Write-Host "=== $Title ==="
}

function Add-PathEntry {
  param([string]$PathEntry)
  if ([string]::IsNullOrWhiteSpace($PathEntry)) {
    return
  }
  if (-not (Test-Path $PathEntry)) {
    return
  }
  $escaped = [Regex]::Escape($PathEntry)
  if ($env:Path -notmatch $escaped) {
    $env:Path = "$PathEntry;$env:Path"
  }
  $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
  if ($userPath -notmatch $escaped) {
    $updated = if ([string]::IsNullOrWhiteSpace($userPath)) { $PathEntry } else { "$userPath;$PathEntry" }
    [Environment]::SetEnvironmentVariable("Path", $updated, "User")
  }
}

function Set-UserEnv {
  param([string]$Name, [string]$Value)
  if ([string]::IsNullOrWhiteSpace($Value)) {
    return
  }
  $current = [Environment]::GetEnvironmentVariable($Name, "User")
  if ($current -ne $Value) {
    [Environment]::SetEnvironmentVariable($Name, $Value, "User")
  }
  Set-Item -Path "Env:$Name" -Value $Value
}

function Ensure-WingetPackage {
  param(
    [string]$Id,
    [string]$DisplayName,
    [scriptblock]$IsInstalled,
    [string]$Override
  )
  if (& $IsInstalled) {
    Write-Host "$DisplayName already installed."
    return
  }
  if (-not $script:WingetAvailable) {
    Write-Warning "winget not available. Install $DisplayName manually."
    return
  }
  $args = @("install", "--id", $Id, "-e", "--accept-package-agreements", "--accept-source-agreements")
  if ($Override) {
    $args += @("--override", $Override)
  }
  Write-Host "Installing $DisplayName..."
  & winget @args
}

Write-Section "Checking winget"
$script:WingetAvailable = $null -ne (Test-Command winget)
if (-not $script:WingetAvailable) {
  Write-Warning "winget not found. Install the App Installer from Microsoft Store to enable automatic installs."
}

Write-Section "Ensuring Git"
Ensure-WingetPackage -Id "Git.Git" -DisplayName "Git" -IsInstalled { $null -ne (Test-Command git) }
if (Test-Command git) {
  $gitPath = (Get-Command git).Source
  Add-PathEntry (Split-Path $gitPath -Parent)
}

Write-Section "Ensuring Python 3.x"
Ensure-WingetPackage -Id "Python.Python.3.12" -DisplayName "Python 3.12" -IsInstalled {
  $py = Test-Command python
  if (-not $py) { return $false }
  $version = & python --version 2>&1
  return $version -match '^Python 3\.'
}

Write-Section "Ensuring Visual Studio 2022 Build Tools"
$vswherePath = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstall = $null
if (Test-Path $vswherePath) {
  $vsInstall = & $vswherePath -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if (-not $vsInstall) {
  Ensure-WingetPackage -Id "Microsoft.VisualStudio.2022.BuildTools" -DisplayName "Visual Studio 2022 Build Tools" -IsInstalled { $false } -Override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --norestart"
  if (Test-Path $vswherePath) {
    $vsInstall = & $vswherePath -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  }
}
if ($vsInstall) {
  $vcvars = Join-Path $vsInstall "VC\Auxiliary\Build\vcvars64.bat"
  if (Test-Path $vcvars) {
    Set-UserEnv -Name "OPENBOARD_VCVARS" -Value $vcvars
  }
}

Write-Section "Ensuring Inno Setup 6"
Ensure-WingetPackage -Id "JRSoftware.InnoSetup" -DisplayName "Inno Setup 6" -IsInstalled {
  (Test-Command iscc) -or
  (Test-Path (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\iscc.exe")) -or
  (Test-Path (Join-Path ${env:ProgramFiles} "Inno Setup 6\iscc.exe"))
}
$innoExe = $null
if (Test-Command iscc) {
  $innoExe = (Get-Command iscc).Source
} else {
  $candidate = Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\iscc.exe"
  if (Test-Path $candidate) {
    $innoExe = $candidate
  } else {
    $candidate = Join-Path ${env:ProgramFiles} "Inno Setup 6\iscc.exe"
    if (Test-Path $candidate) {
      $innoExe = $candidate
    }
  }
}
if ($innoExe) {
  Set-UserEnv -Name "INNO_EXE" -Value $innoExe
  Add-PathEntry (Split-Path $innoExe -Parent)
}

Write-Section "Ensuring Qt $QtVersion ($QtArch)"
$qtDir = if ([string]::IsNullOrWhiteSpace($env:QT_DIR)) {
  Join-Path (Join-Path $QtBase $QtVersion) $QtArch
} else {
  $env:QT_DIR
}
$qtBin = Join-Path $qtDir "bin"
$qtQmake = Join-Path $qtBin "qmake.exe"
if (-not (Test-Path $qtQmake) -and -not $SkipQtDownload) {
  if (Test-Command python) {
    Write-Host "Installing Qt via aqtinstall..."
    & python -m pip install --user --upgrade pip
    & python -m pip install --user --upgrade aqtinstall
    $moduleArgs = @()
    if ($QtModules.Count -gt 0) {
      $moduleArgs = @("--modules") + $QtModules
    }
    & python -m aqt install-qt windows desktop $QtVersion $QtArch -O $QtBase @moduleArgs
  } else {
    Write-Warning "Python is required to download Qt. Install Python first."
  }
}
if (Test-Path $qtQmake) {
    Set-UserEnv -Name "QT_DIR" -Value $qtDir
    Set-UserEnv -Name "QT_BIN" -Value $qtBin
    Add-PathEntry $qtBin
    $qtLrelease = Join-Path $qtBin "lrelease.exe"
    if (-not (Test-Path $qtLrelease)) {
      Write-Warning "lrelease.exe not found in $qtBin. Ensure the Qt Tools module (qttools) is installed."
    }
} else {
  Write-Warning "Qt not found at $qtDir. Set QT_DIR to your Qt installation."
}

Write-Section "Checking OpenBoard-ThirdParty"
$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$thirdPartyPath = Join-Path $projectRoot "..\OpenBoard-ThirdParty"
$thirdParty = $null
if (Test-Path $thirdPartyPath) {
  $thirdParty = Resolve-Path $thirdPartyPath
  Set-UserEnv -Name "OPENBOARD_THIRDPARTY" -Value $thirdParty.Path
  Write-Host "OpenBoard-ThirdParty found at $($thirdParty.Path)"
} else {
  Write-Warning "OpenBoard-ThirdParty not found next to the repository."
  Write-Warning "Expected location: $(Resolve-Path (Join-Path $projectRoot '..') -ErrorAction SilentlyContinue)\OpenBoard-ThirdParty"
  Write-Warning "Clone or create the ThirdParty directory before building."
}

Write-Section "Checking Poppler"
if ($thirdParty) {
  $popplerBase = Join-Path $thirdParty.Path "poppler"
  $popplerInc  = Join-Path $popplerBase "include"
  $popplerLib  = Join-Path $popplerBase "lib"
  $popplerBin  = Join-Path $popplerBase "bin"

  if ((Test-Path $popplerInc) -and (Test-Path $popplerLib)) {
    Write-Host "Poppler found at $popplerBase"
  } else {
    Write-Warning "Poppler not found in $popplerBase"
    Write-Host ""
    Write-Host "To install poppler for Windows:"
    Write-Host "  Option A – vcpkg (recommended if you have vcpkg):"
    Write-Host "    vcpkg install poppler:x64-windows"
    Write-Host "    Then copy installed files to $popplerBase"
    Write-Host ""
    Write-Host "  Option B – Pre-built release from https://github.com/oschwartz10612/poppler-windows/releases"
    Write-Host "    1. Download the latest Release-xx.xx.x.zip"
    Write-Host "    2. Extract and place in:"
    Write-Host "       $popplerBase\include\   (poppler headers)"
    Write-Host "       $popplerBase\lib\       (import .lib files)"
    Write-Host "       $popplerBase\bin\       (runtime .dll files)"
    Write-Host ""

    $download = Read-Host "Auto-download pre-built poppler from GitHub? (y/N)"
    if ($download -eq 'y' -or $download -eq 'Y') {
      try {
        Write-Host "Fetching latest poppler release info..."
        $releases = Invoke-RestMethod -Uri "https://api.github.com/repos/oschwartz10612/poppler-windows/releases/latest" -ErrorAction Stop
        $asset = $releases.assets | Where-Object { $_.name -match '^Release-.*\.zip$' } | Select-Object -First 1
        if (-not $asset) {
          Write-Warning "Could not find a release ZIP in the latest release. Download manually."
        } else {
          $zipPath = Join-Path $env:TEMP "poppler-windows.zip"
          $extractPath = Join-Path $env:TEMP "poppler-extract"
          Write-Host "Downloading $($asset.browser_download_url) ..."
          Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zipPath -UseBasicParsing
          Write-Host "Extracting..."
          if (Test-Path $extractPath) { Remove-Item $extractPath -Recurse -Force }
          Expand-Archive -Path $zipPath -DestinationPath $extractPath
          # The archive typically has one top-level folder (e.g. poppler-xx.xx.x)
          $innerDir = Get-ChildItem $extractPath -Directory | Select-Object -First 1
          if ($innerDir) {
            if (-not (Test-Path $popplerBase)) { New-Item -ItemType Directory -Path $popplerBase | Out-Null }
            # Copy Library subfolder as include/lib/bin
            $libDir = Join-Path $innerDir.FullName "Library"
            if (Test-Path $libDir) {
              Copy-Item (Join-Path $libDir "include") $popplerBase -Recurse -Force
              Copy-Item (Join-Path $libDir "lib")     $popplerBase -Recurse -Force
              Copy-Item (Join-Path $libDir "bin")     $popplerBase -Recurse -Force
              Write-Host "Poppler installed to $popplerBase"
            } else {
              Write-Warning "Unexpected archive structure. Copy headers/libs/bins manually to $popplerBase"
            }
          }
          Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
          Remove-Item $extractPath -Recurse -Force -ErrorAction SilentlyContinue
        }
      } catch {
        Write-Warning "Auto-download failed: $_"
        Write-Warning "Download poppler manually from https://github.com/oschwartz10612/poppler-windows/releases"
      }
    }
  }
} else {
  Write-Warning "Skipping poppler check — OpenBoard-ThirdParty directory not set up yet."
}

Write-Section "Checking QuaZip"
if ($thirdParty) {
  $quazipBase = Join-Path $thirdParty.Path "quazip"
  $quazipLibDir = Join-Path $quazipBase "lib\win32"
  $acceptedLibs = @("quazip.lib","quazip1-qt6.lib","quazip-qt6.lib","quazip1-qt5.lib","quazip-qt5.lib","quazip1.lib")
  $quazipLibFound = $false
  foreach ($lib in $acceptedLibs) {
    if (Test-Path (Join-Path $quazipLibDir $lib)) {
      $quazipLibFound = $true
      Write-Host "QuaZip lib found: $lib"
      break
    }
  }
  if (-not $quazipLibFound) {
    Write-Warning "QuaZip import library not found in $quazipLibDir"
    Write-Host "QuaZip must be built from source. Steps:"
    Write-Host "  1. Place QuaZip sources in $quazipBase (must contain CMakeLists.txt)"
    Write-Host "  2. Open 'x64 Native Tools Command Prompt for VS 2022'"
    Write-Host "  3. Run:"
    Write-Host "       cd /d `"$quazipBase`""
    Write-Host "       if not exist build mkdir build"
    Write-Host "       cd build"
    Write-Host "       cmake .. -G `"NMake Makefiles`" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=`"$($qtDir)`""
    Write-Host "       nmake"
    Write-Host "  4. Copy the generated .lib to $quazipLibDir"
  }
}

Write-Section "Done"
Write-Host "Restart your terminal to pick up updated user PATH entries."
Write-Host ""
Write-Host "Build with:  release_scripts\windows\release.win7.vc9.bat"
