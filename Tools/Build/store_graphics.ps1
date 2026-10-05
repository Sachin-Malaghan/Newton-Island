# Builds Google Play store graphics from the capture screenshots (run Tools\Validation\capture.ps1 -Phone first).
# Output: Build/Android/PlayStore/ (feature graphic 1024x500, phone screenshots 1440x720 = Play's 2:1 limit).
Add-Type -AssemblyName System.Drawing
$root = Resolve-Path "$PSScriptRoot\..\.."
$src = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$out = Join-Path $root 'Build\Android\PlayStore'
New-Item -ItemType Directory -Force $out | Out-Null

$jpeg = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$q = New-Object System.Drawing.Imaging.EncoderParameters 1
$q.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), 92L

function Load([string]$name) {
    $p = Join-Path $src "CurioIsles_$name.png"
    if (-not (Test-Path $p)) { throw "missing screenshot $p - run Tools\Validation\capture.ps1 -Phone first" }
    [System.Drawing.Image]::FromFile($p)
}

# Draws $img into a WxH frame, cropping to fill (fx, fy = 0..1 focus point).
function Fill([System.Drawing.Image]$img, [int]$W, [int]$H, [double]$fx = 0.5, [double]$fy = 0.5) {
    $bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = 'HighQualityBicubic'; $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAliasGridFit'
    $s = [Math]::Max($W / $img.Width, $H / $img.Height)
    $sw = $W / $s; $sh = $H / $s
    $sx = ($img.Width - $sw) * $fx; $sy = ($img.Height - $sh) * $fy
    $g.DrawImage($img, (New-Object System.Drawing.Rectangle 0, 0, $W, $H), [float]$sx, [float]$sy, [float]$sw, [float]$sh, 'Pixel')
    return @($bmp, $g)
}

# ---- Feature graphic 1024 x 500: the aiming shot with the title over it ----
$img = Load 'phone_30b_canyon_aiming'
$r = Fill $img 1024 500 0.35 0.3
$bmp = $r[0]; $g = $r[1]
$shade = New-Object System.Drawing.Drawing2D.LinearGradientBrush (New-Object System.Drawing.Rectangle 0, 0, 1024, 500), ([System.Drawing.Color]::FromArgb(120, 16, 40, 74)), ([System.Drawing.Color]::FromArgb(0, 16, 40, 74)), 90
$g.FillRectangle($shade, 0, 0, 1024, 500)
$fmt = New-Object System.Drawing.StringFormat
$fmt.Alignment = 'Center'
$title = New-Object System.Drawing.Font 'Segoe UI', 92, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$sub = New-Object System.Drawing.Font 'Segoe UI', 30, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$g.DrawString('CURIO ISLES', $title, (New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(110, 16, 40, 74))), 516, 56, $fmt)
$g.DrawString('CURIO ISLES', $title, (New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 250, 240))), 512, 50, $fmt)
$g.DrawString('Break it.  Fix it.  Understand it.', $sub, (New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 250, 240))), 512, 166, $fmt)
$g.Dispose(); $img.Dispose()
$bmp.Save((Join-Path $out 'feature-graphic-1024x500.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

# ---- Phone screenshots, 1440 x 720 ----
$shots = [ordered]@{
    '01-title' = 'phone_01_title'
    '02-pull-back-and-fire' = 'phone_30b_canyon_aiming'
    '03-machine-fixed' = 'phone_32_canyon_solved'
    '04-first-roll' = 'phone_11_first_roll_sliders'
    '05-student-mode' = 'phone_16_student_solved'
    '06-brake' = 'phone_22_brake_solved_student'
    '07-splash' = 'phone_21_brake_splash'
    '08-worlds' = 'phone_02_levels'
}
foreach ($k in $shots.Keys) {
    $img = Load $shots[$k]
    $r = Fill $img 1440 720 0.5 0.5
    $r[1].Dispose(); $img.Dispose()
    $r[0].Save((Join-Path $out "screenshot-$k.jpg"), $jpeg, $q)
    $r[0].Dispose()
}

Get-ChildItem $out | Select-Object Name, @{ n = 'KB'; e = { [math]::Round($_.Length / 1KB) } }
