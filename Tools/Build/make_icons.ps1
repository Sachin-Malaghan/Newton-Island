# Draws the CURIO ISLES app icon (the red ball flying off a ramp over Newton's Island, in the game's
# style) and writes every size Android, iOS and the website need. Re-run after changing the design.
Add-Type -AssemblyName System.Drawing
$root = Resolve-Path "$PSScriptRoot\..\.."

function C([int]$rgb, [int]$a = 255) { [System.Drawing.Color]::FromArgb($a, ($rgb -shr 16) -band 255, ($rgb -shr 8) -band 255, $rgb -band 255) }
function B([int]$rgb, [int]$a = 255) { New-Object System.Drawing.SolidBrush (C $rgb $a) }

function New-Icon([int]$N) {
    $bmp = New-Object System.Drawing.Bitmap $N, $N
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $s = $N / 1024.0
    function P([double]$x, [double]$y) { New-Object System.Drawing.PointF ([float]($x * $s)), ([float]($y * $s)) }
    function Ell($brush, [double]$cx, [double]$cy, [double]$rx, [double]$ry) { $g.FillEllipse($brush, [float](($cx - $rx) * $s), [float](($cy - $ry) * $s), [float](2 * $rx * $s), [float](2 * $ry * $s)) }

    # Sky.
    $rect = New-Object System.Drawing.RectangleF 0, 0, $N, $N
    $sky = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, (C 0x5fb8f0), (C 0xffe9c2), 90
    $g.FillRectangle($sky, $rect)

    # Sun glow.
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddEllipse([float](420 * $s), [float](-60 * $s), [float](760 * $s), [float](760 * $s))
    $glow = New-Object System.Drawing.Drawing2D.PathGradientBrush $path
    $glow.CenterColor = C 0xfff3c4 230
    $glow.SurroundColors = [System.Drawing.Color[]]@((C 0xfff3c4 0))
    $g.FillPath($glow, $path)
    Ell (B 0xfff6d8) 800 250 92 92

    # Clouds.
    foreach ($c in @(@(210, 210, 1.0), @(560, 120, 0.7))) {
        $k = $c[2]
        Ell (B 0xffffff 235) $c[0] $c[1] (120 * $k) (48 * $k)
        Ell (B 0xffffff 235) ($c[0] - 60 * $k) ($c[1] + 12 * $k) (70 * $k) (40 * $k)
        Ell (B 0xffffff 235) ($c[0] + 30 * $k) ($c[1] - 30 * $k) (70 * $k) (50 * $k)
    }

    # Hills.
    Ell (B 0xa6d98a) 250 1010 620 330
    Ell (B 0x7cc473) 860 1060 600 330
    $g.FillRectangle((B 0x5fae5a), 0, [float](880 * $s), $N, [float](160 * $s))
    $g.FillRectangle((B 0x3e8a4a), 0, [float](960 * $s), $N, [float](80 * $s))

    # Ramp: side panel, braces, plank.
    $g.FillPolygon((B 0xf0c98f), [System.Drawing.PointF[]]@((P 90 470), (P 150 470), (P 640 880), (P 90 880)))
    $brace = New-Object System.Drawing.Pen (C 0xc48a52), ([float](18 * $s))
    $g.DrawLine($brace, (P 250 880), (P 250 560))
    $g.DrawLine($brace, (P 420 880), (P 420 700))
    $g.DrawLine($brace, (P 100 870), (P 500 640))
    $plank = New-Object System.Drawing.Pen (C 0x8a5a2b), ([float](40 * $s))
    $plank.StartCap = 'Round'; $plank.EndCap = 'Round'
    $g.DrawLine($plank, (P 100 468), (P 150 468))
    $g.DrawLine($plank, (P 150 468), (P 640 876))

    # Dotted flight path.
    $t = 0
    foreach ($d in @(@(700, 610), @(760, 540), @(815, 490), @(865, 460))) {
        Ell (B 0xffffff (235 - $t * 40)) $d[0] $d[1] (22 - $t * 3) (22 - $t * 3)
        $t++
    }

    # The ball: rim, body, stripe, highlight.
    Ell (B 0x1f2440 50) 596 742 150 40
    Ell (B 0xd8413c) 560 560 170 170
    Ell (B 0xff5d57) 560 560 146 146
    $stripe = New-Object System.Drawing.Pen (C 0xfff1e6), ([float](48 * $s))
    $stripe.StartCap = 'Round'; $stripe.EndCap = 'Round'
    $g.DrawLine($stripe, (P 450 610), (P 670 510))
    Ell (B 0xffffff 150) 500 480 38 38

    # A star: you solved it.
    $pts = @()
    for ($i = 0; $i -lt 10; $i++) {
        $a = -[Math]::PI / 2 + $i * [Math]::PI / 5
        $r = if ($i % 2 -eq 0) { 92 } else { 42 }
        $pts += (P (850 + [Math]::Cos($a) * $r) (760 + [Math]::Sin($a) * $r))
    }
    $shadowPts = $pts | ForEach-Object { New-Object System.Drawing.PointF $_.X, ([float]($_.Y + 10 * $s)) }
    $g.FillPolygon((B 0xd99a1e), [System.Drawing.PointF[]]$shadowPts)
    $g.FillPolygon((B 0xffc93c), [System.Drawing.PointF[]]$pts)

    $g.Dispose()
    return $bmp
}

function Save([System.Drawing.Bitmap]$src, [int]$size, [string]$path) {
    New-Item -ItemType Directory -Force (Split-Path $path) | Out-Null
    $dst = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($dst)
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.DrawImage($src, 0, 0, $size, $size)
    $g.Dispose()
    $dst.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $dst.Dispose()
}

$icon = New-Icon 1024

# Android (legacy launcher icons; UE copies Build/Android/res over its defaults)
$android = @{ 'drawable-ldpi' = 36; 'drawable-mdpi' = 48; 'drawable-hdpi' = 72; 'drawable-xhdpi' = 96; 'drawable-xxhdpi' = 144; 'drawable-xxxhdpi' = 192; 'drawable' = 192 }
foreach ($k in $android.Keys) { Save $icon $android[$k] (Join-Path $root "Build\Android\res\$k\icon.png") }
Save $icon 512 (Join-Path $root 'Build\Android\PlayStore\icon-512.png')

# iOS: single 1024 icon in the asset catalog (Xcode 14+), plus the legacy Graphics folder
$appicon = Join-Path $root 'Build\IOS\Resources\Assets.xcassets\AppIcon.appiconset'
Save $icon 1024 (Join-Path $appicon 'Icon1024.png')
Set-Content (Join-Path $appicon 'Contents.json') -Encoding ascii -Value @'
{
  "images" : [ { "filename" : "Icon1024.png", "idiom" : "universal", "platform" : "ios", "size" : "1024x1024" } ],
  "info" : { "author" : "xcode", "version" : 1 }
}
'@
Set-Content (Join-Path $root 'Build\IOS\Resources\Assets.xcassets\Contents.json') -Encoding ascii -Value '{ "info" : { "author" : "xcode", "version" : 1 } }'
Save $icon 1024 (Join-Path $root 'Build\IOS\Resources\Graphics\Icon1024.png')

# Website
Save $icon 512 (Join-Path $root 'Website\assets\icon-512.png')
Save $icon 180 (Join-Path $root 'Website\assets\apple-touch-icon.png')
Save $icon 32 (Join-Path $root 'Website\assets\favicon-32.png')
$icon.Dispose()
'icons written'
