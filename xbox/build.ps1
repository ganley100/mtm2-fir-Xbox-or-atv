<#
.SYNOPSIS
  Build the Xbox Dev Mode app (UWP x64): SDL2 for UWP with MSVC, then the game
  with clang-cl. Then run package.ps1 to make the installable package.

.EXAMPLE
  xbox\build.ps1                       # the full game (src\recomp\gen)
  xbox\build.ps1 -Variant demo         # the 1998 demo (src\recomp\gen-demo)
  xbox\build.ps1 -NoLift               # without the lift: checks the toolchain

  Needs Visual Studio 2022 with "Desktop development with C++", "Universal
  Windows Platform development" and "C++ Clang tools for Windows", git, and
  pcrecomp beside this repository. docs\xbox.md.
#>
param(
  [ValidateSet('full', 'demo')] [string] $Variant = 'full',
  [switch] $NoLift,
  [string] $Pcrecomp = (Join-Path $PSScriptRoot '..\..\pcrecomp'),
  [string] $Config = 'Release'
)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$deps = Join-Path $PSScriptRoot 'deps'
$out = Join-Path $PSScriptRoot 'build'

function Run([string] $exe, [string[]] $args_) {
  Write-Host "> $exe $($args_ -join ' ')"
  & $exe @args_
  if ($LASTEXITCODE -ne 0) { throw "$exe failed ($LASTEXITCODE)" }
}

# ---- SDL2 and SDL2_ttf, the same versions as the Android build
New-Item -ItemType Directory -Force $deps | Out-Null
if (-not (Test-Path "$deps\SDL")) {
  Run git @('clone', '--depth', '1', '--branch', 'release-2.30.9', 'https://github.com/libsdl-org/SDL', "$deps\SDL")
}
if (-not (Test-Path "$deps\SDL_ttf")) {
  Run git @('clone', '--depth', '1', '--branch', 'release-2.22.0', 'https://github.com/libsdl-org/SDL_ttf', "$deps\SDL_ttf")
  Run git @('-C', "$deps\SDL_ttf", 'submodule', 'update', '--init', '--depth', '1', 'external/freetype')
}

# ---- Visual Studio's x64 developer environment, into this session
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -property installationPath
if (-not $vs) { throw 'Visual Studio 2022 not found' }
if (-not $env:VCToolsInstallDir) {
  cmd /c "`"$vs\VC\Auxiliary\Build\vcvarsall.bat`" x64 >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] }
  }
}
$clang = @("$vs\VC\Tools\Llvm\x64\bin\clang-cl.exe", "$env:ProgramFiles\LLVM\bin\clang-cl.exe") |
  Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $clang) { $clang = (Get-Command clang-cl -ErrorAction SilentlyContinue).Source }
if (-not $clang) { throw 'clang-cl not found: install "C++ Clang tools for Windows" in the Visual Studio Installer' }

# ---- 1. SDL2 for UWP, with MSVC (shared with every variant)
$sdlBuild = "$out\sdl2-uwp"
if (-not (Test-Path "$sdlBuild\$Config\SDL2.dll")) {
  Run cmake @('-S', "$deps\SDL", '-B', $sdlBuild, '-G', 'Visual Studio 17 2022', '-A', 'x64',
              '-DCMAKE_SYSTEM_NAME=WindowsStore', '-DCMAKE_SYSTEM_VERSION=10.0',
              '-DSDL_SHARED=ON', '-DSDL_STATIC=OFF', '-DSDL_TEST=OFF')
  Run cmake @('--build', $sdlBuild, '--config', $Config, '--parallel')
}

# ---- 2. the game, with clang-cl
$gameBuild = "$out\$Variant"
Run cmake @('-S', $PSScriptRoot, '-B', $gameBuild, '-G', 'Ninja', "-DCMAKE_BUILD_TYPE=$Config",
            "-DCMAKE_C_COMPILER=$clang", "-DCMAKE_CXX_COMPILER=$clang",
            "-DSDL2_UWP_BUILD=$sdlBuild", "-DSDL2_SOURCE=$deps\SDL", "-DSDL_TTF_SOURCE=$deps\SDL_ttf",
            "-DPCRECOMP=$((Resolve-Path $Pcrecomp).Path)", "-DMTM_VARIANT=$Variant",
            "-DMTM_NO_LIFT=$(if ($NoLift) { 'ON' } else { 'OFF' })")
Run cmake @('--build', $gameBuild, '--parallel')

Copy-Item "$sdlBuild\$Config\SDL2.dll" "$gameBuild\mtm" -Force
Write-Host "built: $gameBuild\mtm\mtm.exe (+ SDL2.dll). Next: xbox\package.ps1 -Variant $Variant"
