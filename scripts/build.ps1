<#
.SYNOPSIS
    Configure and build the project.

.DESCRIPTION
    Ninja needs the MSVC environment, and neither cmake nor ninja are on PATH on
    this machine (they ship inside the VS Build Tools). This script locates them,
    runs vcvars64 in a child shell and drives the build, so no global PATH changes
    are needed.

.EXAMPLE
    ./scripts/build.ps1                 # debug build
    ./scripts/build.ps1 -Config Release
    ./scripts/build.ps1 -Test           # build then run ctest
    ./scripts/build.ps1 -Run            # build then launch the app
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release')]
    [string]$Config = 'Debug',
    [switch]$Clean,
    [switch]$Test,
    [switch]$Run
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build/$Config"

$vsRoot = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools'
$cmake = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
$qtPrefix = 'C:/Qt/6.8.3/msvc2022_64'

foreach ($tool in @($cmake, $ninja, $vcvars)) {
    if (-not (Test-Path $tool)) { throw "Not found: $tool" }
}
if (-not (Test-Path $qtPrefix)) { throw "Qt not found at $qtPrefix" }

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Removing $buildDir" -ForegroundColor Yellow
    Remove-Item $buildDir -Recurse -Force
}

$ctestCmd = if ($Test) { "`"$cmake`" --build . --target all && ctest --output-on-failure" } else { '' }

$script = @"
@echo off
call "$vcvars" >nul || exit /b 1
"$cmake" -S "$root" -B "$buildDir" -G Ninja ^
    -DCMAKE_MAKE_PROGRAM="$ninja" ^
    -DCMAKE_BUILD_TYPE=$Config ^
    -DCMAKE_PREFIX_PATH=$qtPrefix ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON || exit /b 1
"$cmake" --build "$buildDir" || exit /b 1
"@

$batPath = Join-Path $env:TEMP "mv-build-$PID.bat"
Set-Content -Path $batPath -Value $script -Encoding ascii
try {
    & cmd /c "`"$batPath`""
    if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
} finally {
    Remove-Item $batPath -ErrorAction SilentlyContinue
}

Write-Host "Build succeeded: $buildDir" -ForegroundColor Green

if ($Test) {
    Push-Location $buildDir
    try {
        & (Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe') `
            --output-on-failure -C $Config
        if ($LASTEXITCODE -ne 0) { throw "Tests failed" }
    } finally { Pop-Location }
    Write-Host "Tests passed" -ForegroundColor Green
}

if ($Run) {
    $env:PATH = "$($qtPrefix -replace '/', '\')\bin;$env:PATH"
    & (Join-Path $buildDir 'bin/krave.exe')
}
