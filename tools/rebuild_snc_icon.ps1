param(
    [string]$SourceImagePath = "",
    [string]$IconPath = (Join-Path (Split-Path -Parent $PSScriptRoot) "resources\snc_tray_icon.ico"),
    [int]$EdgeThreshold = 18,
    [int]$Padding = 10
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

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

function Read-IcoImagePayload {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 22) {
        throw "ICO file is too small: $Path"
    }

    $count = [BitConverter]::ToUInt16($bytes, 4)
    if ($count -lt 1) {
        throw "ICO contains no images: $Path"
    }

    $entryOffset = 6
    $imageSize = [BitConverter]::ToUInt32($bytes, $entryOffset + 8)
    $imageOffset = [BitConverter]::ToUInt32($bytes, $entryOffset + 12)
    if (($imageOffset + $imageSize) -gt $bytes.Length) {
        throw "ICO image payload is out of range: $Path"
    }

    $payload = New-Object byte[] $imageSize
    [Array]::Copy($bytes, [int]$imageOffset, $payload, 0, [int]$imageSize)
    return $payload
}

function Get-SourceBitmap {
    param([string]$Path)

    $payload = Read-IcoImagePayload -Path $Path
    $pngSignature = [byte[]](137,80,78,71,13,10,26,10)
    $isPng = $true
    for ($i = 0; $i -lt $pngSignature.Length; $i++) {
        if ($payload[$i] -ne $pngSignature[$i]) {
            $isPng = $false
            break
        }
    }

    if ($isPng) {
        $stream = New-Object System.IO.MemoryStream(,$payload)
        try {
            $image = [System.Drawing.Bitmap]::FromStream($stream)
            try {
                return Convert-ToArgbBitmap -Image $image
            } finally {
                $image.Dispose()
            }
        } finally {
            $stream.Dispose()
        }
    }

    $headerSize = [BitConverter]::ToUInt32($payload, 0)
    if ($headerSize -ne 40) {
        throw "Unsupported ICO payload format in $Path"
    }

    $width = [BitConverter]::ToInt32($payload, 4)
    $height = [BitConverter]::ToInt32($payload, 8) / 2
    $bitCount = [BitConverter]::ToUInt16($payload, 14)
    $compression = [BitConverter]::ToUInt32($payload, 16)
    if ($compression -ne 0) {
        throw "Compressed ICO bitmap payload is not supported in $Path"
    }
    if ($bitCount -ne 4) {
        throw "Only 4bpp ICO bitmap payload is supported right now in $Path"
    }

    $paletteOffset = 40
    $paletteCount = 16
    $palette = @()
    for ($i = 0; $i -lt $paletteCount; $i++) {
        $entryOffset = $paletteOffset + ($i * 4)
        $b = $payload[$entryOffset]
        $g = $payload[$entryOffset + 1]
        $r = $payload[$entryOffset + 2]
        $palette += [System.Drawing.Color]::FromArgb(255, $r, $g, $b)
    }

    $xorStride = [int]([Math]::Ceiling(($width * $bitCount) / 32.0) * 4)
    $xorOffset = $paletteOffset + ($paletteCount * 4)
    $andStride = [int]([Math]::Ceiling($width / 32.0) * 4)
    $andOffset = $xorOffset + ($xorStride * $height)

    $bitmap = New-Object System.Drawing.Bitmap $width, $height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($y = 0; $y -lt $height; $y++) {
        $sourceY = $height - 1 - $y
        $xorRow = $xorOffset + ($sourceY * $xorStride)
        $andRow = $andOffset + ($sourceY * $andStride)
        for ($x = 0; $x -lt $width; $x++) {
            $byteIndex = [int]($x / 2)
            $packed = $payload[$xorRow + $byteIndex]
            if (($x % 2) -eq 0) {
                $paletteIndex = ($packed -shr 4) -band 0x0F
            } else {
                $paletteIndex = $packed -band 0x0F
            }

            $maskByte = $payload[$andRow + [int]($x / 8)]
            $maskBit = 7 - ($x % 8)
            $transparent = (($maskByte -shr $maskBit) -band 0x01) -eq 1

            if ($transparent) {
                $bitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
            } else {
                $bitmap.SetPixel($x, $y, $palette[$paletteIndex])
            }
        }
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

$resolvedIconPath = [System.IO.Path]::GetFullPath($IconPath)
$sourceBitmap = if ([string]::IsNullOrWhiteSpace($SourceImagePath)) {
    Get-SourceBitmap -Path $resolvedIconPath
} else {
    $resolvedSourceImagePath = [System.IO.Path]::GetFullPath($SourceImagePath)
    $loadedImage = [System.Drawing.Bitmap]::FromFile($resolvedSourceImagePath)
    try {
        Convert-ToArgbBitmap -Image $loadedImage
    } finally {
        $loadedImage.Dispose()
    }
}
try {
    Remove-ConnectedEdgeBackground -Bitmap $sourceBitmap -Threshold $EdgeThreshold
    $bounds = Get-AlphaBounds -Bitmap $sourceBitmap
    $squareBitmap = New-CroppedSquareBitmap -Bitmap $sourceBitmap -Bounds $bounds -Padding $Padding
    try {
        $iconDir = Split-Path -Parent $resolvedIconPath
        $previewDir = Join-Path $iconDir "..\dist\private_tools"
        $previewDir = [System.IO.Path]::GetFullPath($previewDir)
        if (-not (Test-Path -LiteralPath $previewDir)) {
            New-Item -ItemType Directory -Force -Path $previewDir | Out-Null
        }
        $previewPath = Join-Path $previewDir "snc_icon_preview.png"
        $sourcePngPath = Join-Path $iconDir "snc_tray_icon.png"
        $squareBitmap.Save($sourcePngPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $squareBitmap.Save($previewPath, [System.Drawing.Imaging.ImageFormat]::Png)

        $sizes = @(256, 64, 48, 32, 16)
        $entries = @()
        foreach ($size in $sizes) {
            $entries += @{
                Size = $size
                Bytes = (New-ScaledPngBytes -Bitmap $squareBitmap -Size $size)
            }
        }

        Write-IcoFile -Path $resolvedIconPath -Entries $entries
        Write-Output "snc_icon=$resolvedIconPath"
        Write-Output "snc_icon_png=$sourcePngPath"
        Write-Output "snc_icon_preview=$previewPath"
    } finally {
        $squareBitmap.Dispose()
    }
} finally {
    $sourceBitmap.Dispose()
}
