param(
    [string]$AssetsDirectory = "",
    [int]$EdgeThreshold = 22,
    [int]$Padding = 10,
    [double]$TransparentMarginRatio = 0.11
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($AssetsDirectory)) {
    $AssetsDirectory = Join-Path $repoRoot "tools\dev_attention\assets"
}
$assetsRoot = [System.IO.Path]::GetFullPath($AssetsDirectory)

function Convert-ToArgbBitmap {
    param([System.Drawing.Image]$Image)

    $bitmap = New-Object System.Drawing.Bitmap $Image.Width, $Image.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        $graphics.DrawImage($Image, 0, 0, $Image.Width, $Image.Height)
    } finally {
        $graphics.Dispose()
    }

    return $bitmap
}

function Get-IsEdgeBlack {
    param(
        [System.Drawing.Color]$Color,
        [int]$Threshold
    )

    return (
        $Color.A -gt 0 -and
        $Color.R -le $Threshold -and
        $Color.G -le $Threshold -and
        $Color.B -le $Threshold
    )
}

function Remove-ConnectedEdgeBackground {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [int]$Threshold
    )

    $width = $Bitmap.Width
    $height = $Bitmap.Height
    $visited = New-Object 'bool[,]' $width, $height
    $queue = New-Object System.Collections.Generic.Queue[System.Drawing.Point]

    $enqueue = {
        param([int]$X, [int]$Y)
        if ($X -lt 0 -or $Y -lt 0 -or $X -ge $width -or $Y -ge $height) { return }
        if ($visited[$X, $Y]) { return }
        $color = $Bitmap.GetPixel($X, $Y)
        if (-not (Get-IsEdgeBlack -Color $color -Threshold $Threshold)) { return }
        $visited[$X, $Y] = $true
        $queue.Enqueue([System.Drawing.Point]::new($X, $Y))
    }

    for ($x = 0; $x -lt $width; $x++) {
        & $enqueue $x 0
        & $enqueue $x ($height - 1)
    }
    for ($y = 0; $y -lt $height; $y++) {
        & $enqueue 0 $y
        & $enqueue ($width - 1) $y
    }

    while ($queue.Count -gt 0) {
        $point = $queue.Dequeue()
        $neighbors = @(
            [System.Drawing.Point]::new($point.X - 1, $point.Y),
            [System.Drawing.Point]::new($point.X + 1, $point.Y),
            [System.Drawing.Point]::new($point.X, $point.Y - 1),
            [System.Drawing.Point]::new($point.X, $point.Y + 1)
        )
        foreach ($neighbor in $neighbors) {
            & $enqueue $neighbor.X $neighbor.Y
        }
    }

    for ($x = 0; $x -lt $width; $x++) {
        for ($y = 0; $y -lt $height; $y++) {
            if ($visited[$x, $y]) {
                $Bitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
            }
        }
    }
}

function Get-AlphaBounds {
    param([System.Drawing.Bitmap]$Bitmap)

    $minX = $Bitmap.Width
    $minY = $Bitmap.Height
    $maxX = -1
    $maxY = -1

    for ($x = 0; $x -lt $Bitmap.Width; $x++) {
        for ($y = 0; $y -lt $Bitmap.Height; $y++) {
            if ($Bitmap.GetPixel($x, $y).A -gt 0) {
                if ($x -lt $minX) { $minX = $x }
                if ($y -lt $minY) { $minY = $y }
                if ($x -gt $maxX) { $maxX = $x }
                if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }

    if ($maxX -lt $minX -or $maxY -lt $minY) {
        throw "No visible pixels remained after background removal."
    }

    return [System.Drawing.Rectangle]::new($minX, $minY, ($maxX - $minX + 1), ($maxY - $minY + 1))
}

function New-CroppedSquareBitmap {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [System.Drawing.Rectangle]$Bounds,
        [int]$Padding
    )

    $cropX = [Math]::Max(0, $Bounds.X - $Padding)
    $cropY = [Math]::Max(0, $Bounds.Y - $Padding)
    $cropRight = [Math]::Min($Bitmap.Width, $Bounds.Right + $Padding)
    $cropBottom = [Math]::Min($Bitmap.Height, $Bounds.Bottom + $Padding)
    $cropWidth = $cropRight - $cropX
    $cropHeight = $cropBottom - $cropY
    $side = [Math]::Max($cropWidth, $cropHeight)

    $square = New-Object System.Drawing.Bitmap $side, $side, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($square)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality

        $destX = [Math]::Floor(($side - $cropWidth) / 2)
        $destY = [Math]::Floor(($side - $cropHeight) / 2)
        $destRect = [System.Drawing.Rectangle]::new([int]$destX, [int]$destY, $cropWidth, $cropHeight)
        $srcRect = [System.Drawing.Rectangle]::new($cropX, $cropY, $cropWidth, $cropHeight)
        $graphics.DrawImage($Bitmap, $destRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
    } finally {
        $graphics.Dispose()
    }

    return $square
}

function New-TransparentMarginBitmap {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [double]$MarginRatio
    )

    $ratio = [Math]::Max(0.0, [Math]::Min(0.35, $MarginRatio))
    $side = $Bitmap.Width
    $margin = [int][Math]::Round($side * $ratio)
    if ($margin -le 0) {
        return $Bitmap.Clone([System.Drawing.Rectangle]::new(0, 0, $Bitmap.Width, $Bitmap.Height), [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    }

    $padded = New-Object System.Drawing.Bitmap $side, $side, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($padded)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality

        $targetSide = [Math]::Max(1, $side - ($margin * 2))
        $destRect = [System.Drawing.Rectangle]::new($margin, $margin, $targetSide, $targetSide)
        $graphics.DrawImage($Bitmap, $destRect)
    } finally {
        $graphics.Dispose()
    }

    return $padded
}

function New-ScaledPngBytes {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [int]$Size
    )

    $scaled = New-Object System.Drawing.Bitmap $Size, $Size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($scaled)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.DrawImage($Bitmap, [System.Drawing.Rectangle]::new(0, 0, $Size, $Size))

        $stream = New-Object System.IO.MemoryStream
        try {
            $scaled.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
            return $stream.ToArray()
        } finally {
            $stream.Dispose()
        }
    } finally {
        $graphics.Dispose()
        $scaled.Dispose()
    }
}

function Write-IcoFile {
    param(
        [string]$Path,
        [hashtable[]]$Entries
    )

    $stream = [System.IO.File]::Create($Path)
    $writer = New-Object System.IO.BinaryWriter($stream)
    try {
        $writer.Write([UInt16]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]$Entries.Count)

        $offset = 6 + ($Entries.Count * 16)
        foreach ($entry in $Entries) {
            $size = [int]$entry.Size
            $png = [byte[]]$entry.Bytes
            $writer.Write([byte]($(if ($size -ge 256) { 0 } else { $size })))
            $writer.Write([byte]($(if ($size -ge 256) { 0 } else { $size })))
            $writer.Write([byte]0)
            $writer.Write([byte]0)
            $writer.Write([UInt16]1)
            $writer.Write([UInt16]32)
            $writer.Write([UInt32]$png.Length)
            $writer.Write([UInt32]$offset)
            $offset += $png.Length
        }

        foreach ($entry in $Entries) {
            $writer.Write([byte[]]$entry.Bytes)
        }
    } finally {
        $writer.Dispose()
        $stream.Dispose()
    }
}

function New-CompositePreview {
    param(
        [System.Drawing.Bitmap]$Calm,
        [System.Drawing.Bitmap]$Alert,
        [string]$Path
    )

    $width = 900
    $height = 420
    $preview = New-Object System.Drawing.Bitmap $width, $height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($preview)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(255, 7, 13, 15))
        $tile = 24
        for ($x = 0; $x -lt $width; $x += $tile) {
            for ($y = 0; $y -lt $height; $y += $tile) {
                $even = (([int]($x / $tile) + [int]($y / $tile)) % 2) -eq 0
                $brushColor = if ($even) { [System.Drawing.Color]::FromArgb(255, 24, 34, 36) } else { [System.Drawing.Color]::FromArgb(255, 11, 19, 21) }
                $brush = New-Object System.Drawing.SolidBrush $brushColor
                try {
                    $graphics.FillRectangle($brush, $x, $y, $tile, $tile)
                } finally {
                    $brush.Dispose()
                }
            }
        }

        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.DrawImage($Calm, [System.Drawing.Rectangle]::new(70, 35, 350, 350))
        $graphics.DrawImage($Alert, [System.Drawing.Rectangle]::new(480, 35, 350, 350))
        $preview.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $preview.Dispose()
    }
}

function Convert-TaskBoardIcon {
    param([string]$BaseName)

    $sourcePath = Join-Path $assetsRoot ("{0}_source.png" -f $BaseName)
    $pngPath = Join-Path $assetsRoot ("{0}.png" -f $BaseName)
    $icoPath = Join-Path $assetsRoot ("{0}.ico" -f $BaseName)

    if (-not (Test-Path -LiteralPath $sourcePath)) {
        if (-not (Test-Path -LiteralPath $pngPath)) {
            throw "Missing source image: $sourcePath"
        }
        Copy-Item -LiteralPath $pngPath -Destination $sourcePath
    }

    $loadedImage = [System.Drawing.Bitmap]::FromFile($sourcePath)
    try {
        $sourceBitmap = Convert-ToArgbBitmap -Image $loadedImage
    } finally {
        $loadedImage.Dispose()
    }

    try {
        Remove-ConnectedEdgeBackground -Bitmap $sourceBitmap -Threshold $EdgeThreshold
        $bounds = Get-AlphaBounds -Bitmap $sourceBitmap
        $squareBitmap = New-CroppedSquareBitmap -Bitmap $sourceBitmap -Bounds $bounds -Padding $Padding
        try {
            $paddedBitmap = New-TransparentMarginBitmap -Bitmap $squareBitmap -MarginRatio $TransparentMarginRatio
            try {
                $paddedBitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)

                $entries = @()
                foreach ($size in @(256, 128, 64, 48, 32, 16)) {
                    $entries += @{
                        Size = $size
                        Bytes = (New-ScaledPngBytes -Bitmap $paddedBitmap -Size $size)
                    }
                }
                Write-IcoFile -Path $icoPath -Entries $entries

                return [pscustomobject]@{
                    base_name = $BaseName
                    png = $pngPath
                    ico = $icoPath
                    source = $sourcePath
                    width = $paddedBitmap.Width
                    height = $paddedBitmap.Height
                }
            } finally {
                $paddedBitmap.Dispose()
            }
        } finally {
            $squareBitmap.Dispose()
        }
    } finally {
        $sourceBitmap.Dispose()
    }
}

$calm = Convert-TaskBoardIcon -BaseName "task_board_calm"
$alert = Convert-TaskBoardIcon -BaseName "task_board_alert"

$calmBitmap = [System.Drawing.Bitmap]::FromFile($calm.png)
$alertBitmap = [System.Drawing.Bitmap]::FromFile($alert.png)
try {
    $previewPath = Join-Path $repoRoot "dist\private_tools\task_board_icon_preview.png"
    $previewParent = Split-Path -Parent $previewPath
    if (-not (Test-Path -LiteralPath $previewParent)) {
        New-Item -ItemType Directory -Force -Path $previewParent | Out-Null
    }
    New-CompositePreview -Calm $calmBitmap -Alert $alertBitmap -Path $previewPath
} finally {
    $calmBitmap.Dispose()
    $alertBitmap.Dispose()
}

Write-Host "task_board_calm_png=$($calm.png)"
Write-Host "task_board_calm_ico=$($calm.ico)"
Write-Host "task_board_alert_png=$($alert.png)"
Write-Host "task_board_alert_ico=$($alert.ico)"
Write-Host "task_board_icon_preview=$previewPath"
