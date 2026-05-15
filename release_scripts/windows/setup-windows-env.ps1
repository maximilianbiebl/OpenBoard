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
$thirdParty = Resolve-Path (Join-Path $projectRoot "..\OpenBoard-ThirdParty") -ErrorAction SilentlyContinue
if ($thirdParty) {
  Set-UserEnv -Name "OPENBOARD_THIRDPARTY" -Value $thirdParty.Path
} else {
  Write-Warning "OpenBoard-ThirdParty not found next to the repository. Ensure it is available before building."
}

Write-Section "Done"
Write-Host "Restart your terminal to pick up updated user PATH entries."
