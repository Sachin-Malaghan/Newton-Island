# Builds and runs the standalone CURIO ISLES sim harness with MSVC (no Unreal needed). Seconds, not minutes.
#   powershell -ExecutionPolicy Bypass -File Tools\SimHarness\run.ps1                 # everything
#   powershell -ExecutionPolicy Bypass -File Tools\SimHarness\run.ps1 -Level w1.08 -Trace
param([string]$Level = '', [switch]$Trace, [switch]$Fast, [switch]$Verbose)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path "$PSScriptRoot\..\.."
$out = Join-Path $root 'Intermediate\SimHarness'
New-Item -ItemType Directory -Force $out | Out-Null
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$core = Join-Path $root 'Source\CurioIsles\Private\Core'
$srcs = @("$PSScriptRoot\harness.cpp") + (Get-ChildItem "$core\*.cpp" | ForEach-Object { $_.FullName })
$srcList = ($srcs | ForEach-Object { "`"$_`"" }) -join ' '
# /fp:precise (no FMA contraction): the same rules the game module builds with. /MDd /D_DEBUG enables
# the debug CRT's heap statistics for the no-allocations check (optimised code, debug heap).
$cmd = "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul 2>nul && cl /nologo /std:c++17 /O2 /MDd /D_DEBUG /EHsc /W4 /fp:precise /utf-8 /Fo`"$out\\`" /Fe`"$out\cisim_harness.exe`" $srcList"
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "harness build failed" }
$a = @('-root', (Join-Path $root 'Content\Islands'))
if ($Level) { $a += '-level', $Level }
if ($Trace) { $a += '-trace' }
if ($Fast) { $a += '-fast' }
if ($Verbose) { $a += '-verbose' }
& "$out\cisim_harness.exe" @a
exit $LASTEXITCODE
