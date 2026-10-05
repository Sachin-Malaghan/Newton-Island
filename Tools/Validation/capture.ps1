# Launches the game (editor build, -game) and runs the -CICapture script: every screen and each level in
# build / running / solved states, Fun and Student Mode. Screenshots: Saved/Screenshots/WindowsEditor/CurioIsles_*.png
#   -Phone   19.5:9 window (phone shape)
param([int]$Width = 1600, [int]$Height = 900, [switch]$Phone)
$engine = if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.8' }
$root = Resolve-Path "$PSScriptRoot\..\.."
$project = Join-Path $root 'CurioIsles.uproject'
$tag = 'desktop'
if ($Phone) { $Width = 1560; $Height = 720; $tag = 'phone' }
& "$engine\Engine\Binaries\Win64\UnrealEditor.exe" "$project" -game -windowed "-ResX=$Width" "-ResY=$Height" -nosplash -unattended `
    -CICapture "-CICaptureTag=$tag" | Out-Null
Get-ChildItem (Join-Path $root 'Saved\Screenshots') -Recurse -Filter "CurioIsles_${tag}_*.png" | Select-Object Name, Length
