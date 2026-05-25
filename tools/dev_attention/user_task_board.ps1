param(
    [string]$TaskFilePath = "dist/private_reports/user_tasks.json",
    [string]$ReportFilePath = "dist/private_reports/user_reports.json",
    [string]$SuggestionFilePath = "dist/private_reports/user_suggestions.json",
    [string]$ProjectProgressFilePath = "dist/private_reports/project_progress_estimate.json",
    [string]$StopFilePath = "dist/private_reports/user_task_board.stop",
    [string]$RenderPreviewPath = "",
    [ValidateSet("tasks", "reports", "suggestions")]
    [string]$InitialMode = "tasks"
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class StrategicNexusTaskBoardNative
{
    [DllImport("dwmapi.dll")]
    public static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int attrValue, int attrSize);

    [DllImport("kernel32.dll")]
    public static extern bool FreeConsole();

    [DllImport("kernel32.dll")]
    public static extern IntPtr GetConsoleWindow();

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool DrawMenuBar(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    public static extern int SetCurrentProcessExplicitAppUserModelID(string appId);

    [DllImport("shell32.dll")]
    public static extern void SHChangeNotify(uint wEventId, uint uFlags, IntPtr dwItem1, IntPtr dwItem2);

    [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
    public static extern int SetWindowTheme(IntPtr hwnd, string pszSubAppName, string pszSubIdList);
}
"@

$taskBoardAppUserModelId = "StrategicNexus.UserTaskBoard"
[void][StrategicNexusTaskBoardNative]::SetCurrentProcessExplicitAppUserModelID($taskBoardAppUserModelId)

$consoleWindow = [StrategicNexusTaskBoardNative]::GetConsoleWindow()
if ($consoleWindow -ne [IntPtr]::Zero) {
    [StrategicNexusTaskBoardNative]::ShowWindow($consoleWindow, 0) | Out-Null
}
[StrategicNexusTaskBoardNative]::FreeConsole() | Out-Null

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Resolve-ProjectPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $repoRoot $Path
}

$taskPath = Resolve-ProjectPath $TaskFilePath
$reportPath = Resolve-ProjectPath $ReportFilePath
$suggestionPath = Resolve-ProjectPath $SuggestionFilePath
$projectProgressPath = Resolve-ProjectPath $ProjectProgressFilePath
$roadmapPath = Join-Path $repoRoot "docs/DEVELOPMENT_ROADMAP.md"
$stopPath = Resolve-ProjectPath $StopFilePath
$errorLogPath = Resolve-ProjectPath "dist/private_reports/user_task_board_error.log"
$statePath = Resolve-ProjectPath "dist/private_reports/user_task_board_state.json"
$firstRunStartMenuShortcutMarkerPath = Resolve-ProjectPath "dist/private_reports/user_task_board_start_menu_shortcut.checked"
$shortcutInstallerScriptPath = Join-Path $repoRoot "tools/dev_attention/install_user_task_board_shortcut.ps1"
$calmIconPath = Join-Path $repoRoot "tools/dev_attention/assets/task_board_calm.ico"
$alertIconPath = Join-Path $repoRoot "tools/dev_attention/assets/task_board_alert.ico"
$taskPanelTexturePath = Join-Path $repoRoot "tools/dev_attention/assets/task_panel_texture_v7.png"
$isRenderPreview = -not [string]::IsNullOrWhiteSpace($RenderPreviewPath)
$script:lastShortcutIconPath = ""
$singleInstanceMutexName = "StrategicNexusUserTaskBoard"
$createdNewMutex = $false
$singleInstanceMutex = $null
if (-not $isRenderPreview) {
    $singleInstanceMutex = New-Object System.Threading.Mutex($true, $singleInstanceMutexName, [ref]$createdNewMutex)
    if (-not $createdNewMutex) {
        return
    }
}

function Ensure-Directory {
    param([string]$Path)

    $directory = Split-Path -Parent $Path
    if ($directory -and -not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
}

function Write-TaskBoardError {
    param(
        [string]$Context,
        [object]$ErrorObject
    )

    try {
        Ensure-Directory $errorLogPath
        $timestamp = (Get-Date).ToString("o")
        $message = if ($ErrorObject -is [System.Exception]) {
            $ErrorObject.ToString()
        } elseif ($ErrorObject -and $ErrorObject.Exception) {
            $ErrorObject.Exception.ToString()
        } else {
            [string]$ErrorObject
        }
        Add-Content -LiteralPath $errorLogPath -Value "[$timestamp] $Context`r`n$message`r`n---"
    } catch {
    }
}

function Read-TaskBoardState {
    try {
        if (-not (Test-Path -LiteralPath $statePath)) {
            return $null
        }

        $raw = Get-Content -Raw -LiteralPath $statePath
        if ([string]::IsNullOrWhiteSpace($raw)) {
            return $null
        }

        return $raw | ConvertFrom-Json
    } catch {
        Write-TaskBoardError "Read-TaskBoardState failed" $_
        return $null
    }
}

function Save-TaskBoardState {
    try {
        if ($isRenderPreview -or $null -eq $form -or $form.IsDisposed -or $null -eq $list) {
            return
        }

        $selectedTag = ""
        if ($list.SelectedItems.Count -gt 0) {
            $selectedTag = [string]$list.SelectedItems[0].Tag
        } elseif (-not [string]::IsNullOrWhiteSpace($script:selectedBoardTag)) {
            $selectedTag = [string]$script:selectedBoardTag
        }

        $windowBounds = if ($form.WindowState -eq [System.Windows.Forms.FormWindowState]::Normal) {
            $form.Bounds
        } else {
            $form.RestoreBounds
        }
        $windowState = if ($form.WindowState -eq [System.Windows.Forms.FormWindowState]::Minimized) {
            "Normal"
        } else {
            [string]$form.WindowState
        }

        $state = [pscustomobject]@{
            schema_version = 1
            updated_at = (Get-Date).ToString("o")
            mode = $script:currentBoardMode
            selected_tag = $selectedTag
            detail_scroll_offset = [int]$script:detailsScrollOffset
            window = [pscustomobject]@{
                left = [int]$windowBounds.Left
                top = [int]$windowBounds.Top
                width = [int]$windowBounds.Width
                height = [int]$windowBounds.Height
                window_state = $windowState
            }
        }

        Ensure-Directory $statePath
        $state | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $statePath -Encoding UTF8
    } catch {
        Write-TaskBoardError "Save-TaskBoardState failed" $_
    }
}

$savedState = Read-TaskBoardState

function Test-WindowBoundsVisible {
    param(
        [int]$Left,
        [int]$Top,
        [int]$Width,
        [int]$Height
    )

    try {
        if ($Width -lt 520 -or $Height -lt 360) {
            return $false
        }

        $bounds = New-Object System.Drawing.Rectangle $Left, $Top, $Width, $Height
        foreach ($screen in [System.Windows.Forms.Screen]::AllScreens) {
            $intersection = [System.Drawing.Rectangle]::Intersect($bounds, $screen.WorkingArea)
            if ($intersection.Width -ge 160 -and $intersection.Height -ge 120) {
                return $true
            }
        }
    } catch {
        Write-TaskBoardError "Test-WindowBoundsVisible failed" $_
    }

    return $false
}

function Get-NearestScreenWorkingArea {
    param([System.Drawing.Rectangle]$Bounds)

    try {
        $bestArea = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
        $bestScore = -1
        foreach ($screen in [System.Windows.Forms.Screen]::AllScreens) {
            $area = $screen.WorkingArea
            $intersection = [System.Drawing.Rectangle]::Intersect($Bounds, $area)
            $score = $intersection.Width * $intersection.Height
            if ($score -gt $bestScore) {
                $bestScore = $score
                $bestArea = $area
            }
        }

        return $bestArea
    } catch {
        Write-TaskBoardError "Get-NearestScreenWorkingArea failed" $_
        return [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
    }
}

function Get-ClampedWindowBounds {
    param(
        [int]$Left,
        [int]$Top,
        [int]$Width,
        [int]$Height
    )

    $width = [int][Math]::Max(620, $Width)
    $height = [int][Math]::Max(440, $Height)
    $bounds = New-Object System.Drawing.Rectangle $Left, $Top, $width, $height
    $area = Get-NearestScreenWorkingArea -Bounds $bounds
    $minimumVisibleWidth = [Math]::Min(220, $width)
    $minimumVisibleHeight = [Math]::Min(80, $height)

    $minimumLeft = $area.Left - $width + $minimumVisibleWidth
    $maximumLeft = $area.Right - $minimumVisibleWidth
    $minimumTop = $area.Top
    $maximumTop = $area.Bottom - $minimumVisibleHeight

    if ($maximumLeft -lt $minimumLeft) {
        $maximumLeft = $minimumLeft
    }
    if ($maximumTop -lt $minimumTop) {
        $maximumTop = $minimumTop
    }

    $clampedLeft = [int][Math]::Min([Math]::Max($Left, $minimumLeft), $maximumLeft)
    $clampedTop = [int][Math]::Min([Math]::Max($Top, $minimumTop), $maximumTop)

    return New-Object System.Drawing.Rectangle $clampedLeft, $clampedTop, $width, $height
}

function Apply-SavedWindowState {
    param(
        [System.Windows.Forms.Form]$TargetForm,
        [object]$State
    )

    try {
        if ($isRenderPreview -or $null -eq $State -or $null -eq $State.window) {
            return
        }

        $window = $State.window
        $left = [int]$window.left
        $top = [int]$window.top
        $width = [int][Math]::Max(620, [int]$window.width)
        $height = [int][Math]::Max(440, [int]$window.height)
        $bounds = Get-ClampedWindowBounds -Left $left -Top $top -Width $width -Height $height
        $left = $bounds.Left
        $top = $bounds.Top
        $width = $bounds.Width
        $height = $bounds.Height

        $TargetForm.StartPosition = "Manual"
        $TargetForm.SetBounds($left, $top, $width, $height)
        if ([string]$window.window_state -eq "Maximized") {
            $script:pendingWindowState = "Maximized"
        }
    } catch {
        Write-TaskBoardError "Apply-SavedWindowState failed" $_
    }
}

function Get-SavedBoardMode {
    if ($isRenderPreview -or $null -eq $savedState) {
        return $InitialMode
    }

    $mode = [string]$savedState.mode
    if (@("tasks", "reports", "suggestions") -contains $mode) {
        return $mode
    }

    return $InitialMode
}

function Get-SavedSelectedTag {
    if ($isRenderPreview -or $null -eq $savedState) {
        return ""
    }

    return [string]$savedState.selected_tag
}

function Get-SavedDetailsScrollOffset {
    if ($isRenderPreview -or $null -eq $savedState) {
        return 0
    }

    try {
        return [Math]::Max(0, [int]$savedState.detail_scroll_offset)
    } catch {
        return 0
    }
}

[System.Windows.Forms.Application]::SetUnhandledExceptionMode([System.Windows.Forms.UnhandledExceptionMode]::CatchException)
[System.Windows.Forms.Application]::EnableVisualStyles()
[System.Windows.Forms.Application]::add_ThreadException({
    param($sender, $event)
    Write-TaskBoardError "WinForms thread exception" $event.Exception
})
[AppDomain]::CurrentDomain.add_UnhandledException({
    param($sender, $event)
    Write-TaskBoardError "Unhandled domain exception" $event.ExceptionObject
})

function Ensure-TaskFile {
    Ensure-Directory $taskPath
    if (-not (Test-Path -LiteralPath $taskPath)) {
        @"
{
  "schema_version": 1,
  "updated_at": "",
  "tasks": []
}
"@ | Set-Content -LiteralPath $taskPath -Encoding UTF8
    }
}

function Ensure-ReportFile {
    Ensure-Directory $reportPath
    if (-not (Test-Path -LiteralPath $reportPath)) {
        @"
{
  "schema_version": 1,
  "updated_at": "",
  "reports": []
}
"@ | Set-Content -LiteralPath $reportPath -Encoding UTF8
    }
}

function Ensure-SuggestionFile {
    Ensure-Directory $suggestionPath
    if (-not (Test-Path -LiteralPath $suggestionPath)) {
        @"
{
  "schema_version": 1,
  "updated_at": "",
  "suggestions": []
}
"@ | Set-Content -LiteralPath $suggestionPath -Encoding UTF8
    }
}

function Get-ProgressPercent {
    param($Item)

    if ($null -eq $Item -or $null -eq $Item.progress_percent) {
        return 0
    }

    try {
        return [Math]::Max(0, [Math]::Min(100, [int]$Item.progress_percent))
    } catch {
        return 0
    }
}

function New-PanelTexture {
    param(
        [string]$Path,
        [System.Drawing.Color]$BaseColor,
        [System.Drawing.Color]$AccentColor,
        [int]$TextureWidth = 360,
        [int]$TextureHeight = 360,
        [int]$WatermarkOffsetY = 0
    )

    try {
        if (Test-Path -LiteralPath $Path) {
            return
        }

        Ensure-Directory $Path
        $bitmap = New-Object System.Drawing.Bitmap 360, 360
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.Clear($BaseColor)

        $random = New-Object System.Random (42 + $Path.Length)
        for ($i = 0; $i -lt 210; ++$i) {
            $alpha = $random.Next(6, 24)
            $starColor = [System.Drawing.Color]::FromArgb($alpha, 168, 226, 216)
            $brush = New-Object System.Drawing.SolidBrush $starColor
            $x = $random.Next(0, 360)
            $y = $random.Next(0, 360)
            $size = $random.Next(1, 3)
            $graphics.FillEllipse($brush, $x, $y, $size, $size)
            $brush.Dispose()
        }

        $gridPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(8, $AccentColor.R, $AccentColor.G, $AccentColor.B)), 1
        for ($line = 0; $line -le 360; $line += 45) {
            $graphics.DrawLine($gridPen, $line, 0, $line, 360)
            $graphics.DrawLine($gridPen, 0, $line, 360, $line)
        }
        $gridPen.Dispose()

        $diagonalPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(14, $AccentColor.R, $AccentColor.G, $AccentColor.B)), 1
        for ($line = -360; $line -lt 720; $line += 90) {
            $graphics.DrawLine($diagonalPen, $line, 0, $line + 360, 360)
        }
        $diagonalPen.Dispose()

        $constellationPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(24, $AccentColor.R, $AccentColor.G, $AccentColor.B)), 1
        $nodeBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(36, 196, 226, 210))
        for ($i = 0; $i -lt 9; ++$i) {
            $x1 = $random.Next(16, 344)
            $y1 = $random.Next(16, 344)
            $x2 = [Math]::Min(356, [Math]::Max(4, $x1 + $random.Next(-68, 68)))
            $y2 = [Math]::Min(356, [Math]::Max(4, $y1 + $random.Next(-68, 68)))
            $graphics.DrawLine($constellationPen, $x1, $y1, $x2, $y2)
            $graphics.FillRectangle($nodeBrush, $x1 - 1, $y1 - 1, 3, 3)
            $graphics.FillRectangle($nodeBrush, $x2 - 1, $y2 - 1, 3, 3)
        }
        $nodeBrush.Dispose()
        $constellationPen.Dispose()

        $graphics.Dispose()
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose()
    } catch {
        Write-TaskBoardError "New-PanelTexture failed for $Path" $_
    }
}

function New-CorporatePanelTexture {
    param(
        [string]$Path,
        [System.Drawing.Color]$BaseColor,
        [System.Drawing.Color]$AccentColor,
        [int]$TextureWidth = 360,
        [int]$TextureHeight = 360
    )

    try {
        if (Test-Path -LiteralPath $Path) {
            return
        }

        Ensure-Directory $Path
        $bitmap = New-Object System.Drawing.Bitmap $TextureWidth, $TextureHeight
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.Clear($BaseColor)

        $gridPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(7, $AccentColor.R, $AccentColor.G, $AccentColor.B)), 1
        for ($line = 0; $line -le $TextureWidth; $line += 72) {
            $graphics.DrawLine($gridPen, $line, 0, $line, $TextureHeight)
        }
        for ($line = 0; $line -le $TextureHeight; $line += 72) {
            $graphics.DrawLine($gridPen, 0, $line, $TextureWidth, $line)
        }
        $gridPen.Dispose()

        $diagonalPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(10, $AccentColor.R, $AccentColor.G, $AccentColor.B)), 1
        for ($line = -$TextureHeight; $line -lt ($TextureWidth + $TextureHeight); $line += 120) {
            $graphics.DrawLine($diagonalPen, $line, 0, $line + $TextureHeight, $TextureHeight)
        }
        $diagonalPen.Dispose()

        $cx = [int]($TextureWidth / 2)
        $cy = [int](($TextureHeight / 2) + $WatermarkOffsetY)
        $scale = [Math]::Min($TextureWidth, $TextureHeight)
        $symbolWidth = [int]($scale * 0.72)
        $symbolHeight = [int]($scale * 0.58)
        $sx = $symbolWidth / 220.0
        $sy = $symbolHeight / 170.0

        function New-MarkPoint {
            param([double]$X, [double]$Y)
            return New-Object System.Drawing.Point (
                [int]($cx + ($X * $sx))),
                ([int]($cy + ($Y * $sy)))
        }

        function New-MarkPolygon {
            param([object[]]$Pairs)
            $points = @()
            foreach ($pair in $Pairs) {
                $points += New-MarkPoint $pair[0] $pair[1]
            }
            return $points
        }

        $markPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(28, $AccentColor.R, $AccentColor.G, $AccentColor.B)), 2
        $thinPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(16, $AccentColor.R, $AccentColor.G, $AccentColor.B)), 1
        $markBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(12, $AccentColor.R, $AccentColor.G, $AccentColor.B))
        $softBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(7, $AccentColor.R, $AccentColor.G, $AccentColor.B))

        $core = New-MarkPolygon @(
            @(-18, -76), @(18, -76), @(34, -30), @(16, 62), @(0, 82), @(-16, 62), @(-34, -30)
        )
        $leftWing = New-MarkPolygon @(
            @(-108, -28), @(-42, -78), @(-56, -18), @(-25, -2), @(-64, 24), @(-96, 62), @(-76, 2)
        )
        $rightWing = New-MarkPolygon @(
            @(108, -28), @(42, -78), @(56, -18), @(25, -2), @(64, 24), @(96, 62), @(76, 2)
        )
        $crown = New-MarkPolygon @(
            @(-42, -88), @(-18, -112), @(0, -84), @(18, -112), @(42, -88), @(22, -78), @(-22, -78)
        )
        $lowerChevron = New-MarkPolygon @(
            @(-70, 70), @(-28, 44), @(0, 92), @(28, 44), @(70, 70), @(0, 118)
        )

        foreach ($poly in @($leftWing, $rightWing, $core, $crown, $lowerChevron)) {
            $graphics.FillPolygon($softBrush, $poly)
            $graphics.DrawPolygon($markPen, $poly)
        }

        $ringSize = [int]($scale * 0.31)
        $outerRingSize = [int]($scale * 0.43)
        $graphics.DrawEllipse($thinPen, ($cx - [int]($ringSize / 2)), ($cy - [int]($ringSize / 2)), $ringSize, $ringSize)
        $graphics.DrawEllipse($thinPen, ($cx - [int]($outerRingSize / 2)), ($cy - [int]($outerRingSize / 2)), $outerRingSize, $outerRingSize)
        $graphics.DrawLine($thinPen, (New-MarkPoint -78 -55), (New-MarkPoint 78 55))
        $graphics.DrawLine($thinPen, (New-MarkPoint 78 -55), (New-MarkPoint -78 55))
        $graphics.DrawLine($markPen, (New-MarkPoint 0 -116), (New-MarkPoint 0 120))
        $arcSize = [int]($scale * 0.62)
        $arcLeft = $cx - [int]($arcSize / 2)
        $arcTop = $cy - [int]($arcSize / 2)
        $graphics.DrawArc($markPen, $arcLeft, $arcTop, $arcSize, $arcSize, 198, 86)
        $graphics.DrawArc($markPen, $arcLeft, $arcTop, $arcSize, $arcSize, 18, 86)
        $softBrush.Dispose()
        $markBrush.Dispose()
        $thinPen.Dispose()
        $markPen.Dispose()

        $graphics.Dispose()
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose()
    } catch {
        Write-TaskBoardError "New-CorporatePanelTexture failed for $Path" $_
    }
}

New-CorporatePanelTexture -Path $taskPanelTexturePath -BaseColor ([System.Drawing.Color]::FromArgb(6, 24, 31)) -AccentColor ([System.Drawing.Color]::FromArgb(39, 137, 130)) -TextureWidth 300 -TextureHeight 470 -WatermarkOffsetY 28

function Read-Tasks {
    Ensure-TaskFile

    try {
        $raw = Get-Content -Raw -LiteralPath $taskPath
        if ([string]::IsNullOrWhiteSpace($raw)) {
            return @()
        }

        $json = $raw | ConvertFrom-Json
        if (-not $json.tasks) {
            return @()
        }

        return @($json.tasks | Where-Object {
            $_.status -ne "done" -and (Get-ProgressPercent $_) -lt 100
        } | ForEach-Object {
            $_ | Add-Member -NotePropertyName board_kind -NotePropertyValue "task" -Force
            $_
        })
    } catch {
        Write-TaskBoardError "Read-Tasks failed" $_
        return @([pscustomobject]@{
            id = "task_file_error"
            title = "Chyba cteni souboru ukolu"
            severity = "high"
            task = "Task board nedokazal nacist soubor ukolu."
            why = $_.Exception.Message
            instructions = "Rekni Codexu, at zkontroluje dist/private_reports/user_tasks.json."
            status = "open"
        })
    }
}

function Read-Reports {
    Ensure-ReportFile

    try {
        $raw = Get-Content -Raw -LiteralPath $reportPath
        if ([string]::IsNullOrWhiteSpace($raw)) {
            return @()
        }

        $json = $raw | ConvertFrom-Json
        if (-not $json.reports) {
            return @()
        }

        return @($json.reports | Sort-Object created_at -Descending | Select-Object -First 80 | ForEach-Object {
            $_ | Add-Member -NotePropertyName board_kind -NotePropertyValue "report" -Force
            $_
        })
    } catch {
        Write-TaskBoardError "Read-Reports failed" $_
        return @()
    }
}

function Read-Suggestions {
    Ensure-SuggestionFile

    try {
        $raw = Get-Content -Raw -LiteralPath $suggestionPath
        if ([string]::IsNullOrWhiteSpace($raw)) {
            return @()
        }

        $json = $raw | ConvertFrom-Json
        if (-not $json.suggestions) {
            return @()
        }

        return @($json.suggestions | Sort-Object created_at -Descending | Select-Object -First 80 | ForEach-Object {
            $_ | Add-Member -NotePropertyName board_kind -NotePropertyValue "suggestion" -Force
            $_
        })
    } catch {
        Write-TaskBoardError "Read-Suggestions failed" $_
        return @()
    }
}

function Read-ProjectProgressEstimate {
    try {
        if (Test-Path -LiteralPath $projectProgressPath) {
            $raw = Get-Content -Raw -LiteralPath $projectProgressPath
            if (-not [string]::IsNullOrWhiteSpace($raw)) {
                $json = $raw | ConvertFrom-Json
                return [pscustomobject]@{
                    percent = [int][Math]::Max(0, [Math]::Min(100, [int]$json.percent))
                    remaining = if ($json.remaining) { [string]$json.remaining } else { "nezpocitano" }
                    confidence = if ($json.confidence) { [string]$json.confidence } else { "nizka" }
                    basis = if ($json.basis) { [string]$json.basis } else { "automaticky odhad" }
                }
            }
        }

        if (Test-Path -LiteralPath $roadmapPath) {
            $roadmapText = Get-Content -Raw -LiteralPath $roadmapPath
            $matches = [regex]::Matches(
                $roadmapText,
                "(?ms)^##\s+(?<number>[0-9]+[A-Z]?)\.\s+(?<title>[^\r\n]+).*?^Status:\s*\r?\n(?<status>[A-Z_]+)")

            if ($matches.Count -gt 0) {
                $weights = @{
                    "NOT_STARTED" = 0
                    "BLOCKED" = 5
                    "IN_PROGRESS" = 35
                    "IMPLEMENTED" = 70
                    "VERIFIED" = 100
                }
                $total = 0
                $known = 0
                foreach ($match in $matches) {
                    $status = [string]$match.Groups["status"].Value
                    if ($weights.ContainsKey($status)) {
                        $total += [int]$weights[$status]
                        $known++
                    }
                }

                if ($known -gt 0) {
                    $percent = [int][Math]::Round($total / $known)
                    $remaining = if ($percent -lt 20) {
                        "6-12 tydnu Freework + testovani"
                    } elseif ($percent -lt 40) {
                        "5-10 tydnu Freework + testovani"
                    } elseif ($percent -lt 60) {
                        "3-7 tydnu Freework + testovani"
                    } elseif ($percent -lt 80) {
                        "2-5 tydnu Freework + testovani"
                    } elseif ($percent -lt 95) {
                        "1-3 tydny doladeni/testovani"
                    } else {
                        "finalni doladeni"
                    }
                    $confidence = if ($known -ge 10 -and $percent -ge 35) { "stredni" } else { "nizka" }
                    return [pscustomobject]@{
                        percent = $percent
                        remaining = $remaining
                        confidence = $confidence
                        basis = "automaticky odhad z $known roadmap priorit"
                    }
                }
            }
        }

        if (-not (Test-Path -LiteralPath $projectProgressPath)) {
            return [pscustomobject]@{
                percent = 18
                remaining = "nezpocitano"
                confidence = "nizka"
                basis = "vychozi odhad"
            }
        }

        $raw = Get-Content -Raw -LiteralPath $projectProgressPath
        if ([string]::IsNullOrWhiteSpace($raw)) {
            throw "Project progress estimate is empty."
        }

        $json = $raw | ConvertFrom-Json
        $percent = 0
        if ($null -ne $json.percent) {
            $percent = [int][Math]::Max(0, [Math]::Min(100, [int]$json.percent))
        }

        return [pscustomobject]@{
            percent = $percent
            remaining = if ($json.remaining) { [string]$json.remaining } else { "nezpocitano" }
            confidence = if ($json.confidence) { [string]$json.confidence } else { "nizka" }
            basis = if ($json.basis) { [string]$json.basis } else { "manualni odhad" }
        }
    } catch {
        Write-TaskBoardError "Read-ProjectProgressEstimate failed" $_
        return [pscustomobject]@{
            percent = 0
            remaining = "chyba odhadu"
            confidence = "nizka"
            basis = "chyba cteni"
        }
    }
}

$form = New-Object System.Windows.Forms.Form
$form.Text = "Strategic Nexus - tvoje aktualni ukoly"
$form.Width = 860
$form.Height = 620
$form.StartPosition = "CenterScreen"
$form.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::None
$form.ShowInTaskbar = $true
Apply-SavedWindowState -TargetForm $form -State $savedState
$script:currentTaskBoardIcon = New-Object System.Drawing.Icon $calmIconPath
$form.Icon = $script:currentTaskBoardIcon
$form.BackColor = [System.Drawing.Color]::FromArgb(164, 122, 69)
$form.ForeColor = [System.Drawing.Color]::FromArgb(211, 228, 221)
$script:allowHardExit = $false
$script:draggingWindow = $false
$script:dragOffset = [System.Drawing.Point]::Empty
$script:lastHasTasksForTaskbarIcon = $null

function Start-TaskBoardWindowDrag {
    param([System.Windows.Forms.MouseEventArgs]$Event)

    if ($Event.Button -ne [System.Windows.Forms.MouseButtons]::Left) {
        return
    }
    if ($form.WindowState -ne [System.Windows.Forms.FormWindowState]::Normal) {
        return
    }

    $mouse = [System.Windows.Forms.Control]::MousePosition
    $script:draggingWindow = $true
    $script:dragOffset = New-Object System.Drawing.Point ($mouse.X - $form.Left), ($mouse.Y - $form.Top)
}

function Move-TaskBoardWindowDuringDrag {
    if (-not $script:draggingWindow) {
        return
    }

    $mouse = [System.Windows.Forms.Control]::MousePosition
    $form.Left = $mouse.X - $script:dragOffset.X
    $form.Top = $mouse.Y - $script:dragOffset.Y
}

function Stop-TaskBoardWindowDrag {
    if (-not $script:draggingWindow) {
        return
    }

    $script:draggingWindow = $false
    $bounds = Get-ClampedWindowBounds -Left $form.Left -Top $form.Top -Width $form.Width -Height $form.Height
    $form.SetBounds($bounds.Left, $bounds.Top, $bounds.Width, $bounds.Height)
    Save-TaskBoardState
}

function Add-TaskBoardWindowDragHandlers {
    param([System.Windows.Forms.Control]$Control)

    $Control.Add_MouseDown({
        param($sender, $event)
        Start-TaskBoardWindowDrag $event
    })
    $Control.Add_MouseMove({
        Move-TaskBoardWindowDuringDrag
    })
    $Control.Add_MouseUp({
        Stop-TaskBoardWindowDrag
    })
}

function Enable-DarkTitleBar {
    param([System.Windows.Forms.Form]$TargetForm)

    try {
        $enabled = 1
        [void][StrategicNexusTaskBoardNative]::DwmSetWindowAttribute($TargetForm.Handle, 20, [ref]$enabled, 4)
        [void][StrategicNexusTaskBoardNative]::DwmSetWindowAttribute($TargetForm.Handle, 19, [ref]$enabled, 4)
    } catch {
        Write-TaskBoardError "Enable-DarkTitleBar failed" $_
    }
}

function Enable-DarkListViewTheme {
    param([System.Windows.Forms.ListView]$TargetList)

    try {
        if ($null -eq $TargetList -or $TargetList.IsDisposed) {
            return
        }

        [void][StrategicNexusTaskBoardNative]::SetWindowTheme($TargetList.Handle, "DarkMode_Explorer", $null)
        $TargetList.BackColor = [System.Drawing.Color]::FromArgb(7, 22, 29)
        $TargetList.ForeColor = [System.Drawing.Color]::FromArgb(211, 232, 226)
    } catch {
        Write-TaskBoardError "Enable-DarkListViewTheme failed" $_
    }
}

$titleBar = New-Object System.Windows.Forms.Panel
$titleBar.Left = 0
$titleBar.Top = 0
$titleBar.Width = $form.ClientSize.Width
$titleBar.Height = 30
$titleBar.Anchor = [System.Windows.Forms.AnchorStyles](
    [int][System.Windows.Forms.AnchorStyles]::Top -bor
    [int][System.Windows.Forms.AnchorStyles]::Left -bor
    [int][System.Windows.Forms.AnchorStyles]::Right)
$titleBar.BackColor = [System.Drawing.Color]::FromArgb(4, 8, 11)
$form.Controls.Add($titleBar)

$titleLabel = New-Object System.Windows.Forms.Label
$titleLabel.Left = 12
$titleLabel.Top = 0
$titleLabel.Height = $titleBar.Height
$titleLabel.Width = 260
$titleLabel.Text = "Strategic Nexus"
$titleLabel.TextAlign = "MiddleLeft"
$titleLabel.Font = New-Object System.Drawing.Font("Bahnschrift", 9)
$titleLabel.ForeColor = [System.Drawing.Color]::FromArgb(188, 215, 207)
$titleLabel.BackColor = [System.Drawing.Color]::Transparent
$titleBar.Controls.Add($titleLabel)

$minimizeButton = New-Object System.Windows.Forms.Button
$minimizeButton.Text = "-"
$minimizeButton.Width = 40
$minimizeButton.Height = $titleBar.Height
$minimizeButton.FlatStyle = "Flat"
$minimizeButton.FlatAppearance.BorderSize = 0
$minimizeButton.BackColor = [System.Drawing.Color]::FromArgb(4, 8, 11)
$minimizeButton.ForeColor = [System.Drawing.Color]::FromArgb(188, 215, 207)
$minimizeButton.Font = New-Object System.Drawing.Font("Bahnschrift", 10)
$titleBar.Controls.Add($minimizeButton)

$closeButton = New-Object System.Windows.Forms.Button
$closeButton.Text = "X"
$closeButton.Width = 40
$closeButton.Height = $titleBar.Height
$closeButton.FlatStyle = "Flat"
$closeButton.FlatAppearance.BorderSize = 0
$closeButton.BackColor = [System.Drawing.Color]::FromArgb(4, 8, 11)
$closeButton.ForeColor = [System.Drawing.Color]::FromArgb(188, 215, 207)
$closeButton.Font = New-Object System.Drawing.Font("Bahnschrift", 9)
$titleBar.Controls.Add($closeButton)

$header = New-Object System.Windows.Forms.Label
$header.Left = 0
$header.Top = $titleBar.Height
$header.Width = $form.ClientSize.Width
$header.Height = 54
$header.Anchor = [System.Windows.Forms.AnchorStyles](
    [int][System.Windows.Forms.AnchorStyles]::Top -bor
    [int][System.Windows.Forms.AnchorStyles]::Left -bor
    [int][System.Windows.Forms.AnchorStyles]::Right)
$header.TextAlign = "MiddleLeft"
$header.Padding = New-Object System.Windows.Forms.Padding(14, 0, 14, 0)
$header.Font = New-Object System.Drawing.Font("Bahnschrift", 14, [System.Drawing.FontStyle]::Bold)
$header.ForeColor = [System.Drawing.Color]::FromArgb(216, 232, 225)
$header.BackColor = [System.Drawing.Color]::FromArgb(12, 58, 54)
$form.Controls.Add($header)

$projectProgressLabel = New-Object System.Windows.Forms.Label
$projectProgressLabel.Left = 0
$projectProgressLabel.Top = 0
$projectProgressLabel.Width = 280
$projectProgressLabel.Height = $header.Height
$projectProgressLabel.Anchor = [System.Windows.Forms.AnchorStyles](
    [int][System.Windows.Forms.AnchorStyles]::Top -bor
    [int][System.Windows.Forms.AnchorStyles]::Right)
$projectProgressLabel.TextAlign = "MiddleRight"
$projectProgressLabel.Padding = New-Object System.Windows.Forms.Padding(8, 0, 14, 0)
$projectProgressLabel.Font = New-Object System.Drawing.Font("Bahnschrift", 8.6)
$projectProgressLabel.ForeColor = [System.Drawing.Color]::FromArgb(224, 205, 166)
$projectProgressLabel.BackColor = [System.Drawing.Color]::FromArgb(12, 58, 54)
$header.Controls.Add($projectProgressLabel)

$modePanel = New-Object System.Windows.Forms.Panel
$modePanel.Left = 0
$modePanel.Top = $titleBar.Height + $header.Height
$modePanel.Width = $form.ClientSize.Width
$modePanel.Height = 38
$modePanel.Anchor = [System.Windows.Forms.AnchorStyles](
    [int][System.Windows.Forms.AnchorStyles]::Top -bor
    [int][System.Windows.Forms.AnchorStyles]::Left -bor
    [int][System.Windows.Forms.AnchorStyles]::Right)
$modePanel.BackColor = [System.Drawing.Color]::FromArgb(4, 12, 17)
$form.Controls.Add($modePanel)

$tasksModeButton = New-Object System.Windows.Forms.Button
$tasksModeButton.Text = "Ukoly pro me"
$tasksModeButton.Width = 142
$tasksModeButton.Height = 28
$tasksModeButton.Left = 12
$tasksModeButton.Top = 5
$tasksModeButton.FlatStyle = "Flat"
$tasksModeButton.FlatAppearance.BorderSize = 1
$tasksModeButton.Font = New-Object System.Drawing.Font("Bahnschrift", 9.5)
$modePanel.Controls.Add($tasksModeButton)

$reportsModeButton = New-Object System.Windows.Forms.Button
$reportsModeButton.Text = "Reporty"
$reportsModeButton.Width = 110
$reportsModeButton.Height = 28
$reportsModeButton.Left = $tasksModeButton.Right + 8
$reportsModeButton.Top = 5
$reportsModeButton.FlatStyle = "Flat"
$reportsModeButton.FlatAppearance.BorderSize = 1
$reportsModeButton.Font = New-Object System.Drawing.Font("Bahnschrift", 9.5)
$modePanel.Controls.Add($reportsModeButton)

$suggestionsModeButton = New-Object System.Windows.Forms.Button
$suggestionsModeButton.Text = "Navrhy"
$suggestionsModeButton.Width = 110
$suggestionsModeButton.Height = 28
$suggestionsModeButton.Left = $reportsModeButton.Right + 8
$suggestionsModeButton.Top = 5
$suggestionsModeButton.FlatStyle = "Flat"
$suggestionsModeButton.FlatAppearance.BorderSize = 1
$suggestionsModeButton.Font = New-Object System.Drawing.Font("Bahnschrift", 9.5)
$modePanel.Controls.Add($suggestionsModeButton)

$contentPanel = New-Object System.Windows.Forms.Panel
$contentPanel.Left = 0
$contentPanel.Top = $modePanel.Bottom
$contentPanel.Width = $form.ClientSize.Width
$contentPanel.Height = $form.ClientSize.Height - $titleBar.Height - $header.Height - $modePanel.Height - 46
$contentPanel.Anchor = [System.Windows.Forms.AnchorStyles](
    [int][System.Windows.Forms.AnchorStyles]::Top -bor
    [int][System.Windows.Forms.AnchorStyles]::Bottom -bor
    [int][System.Windows.Forms.AnchorStyles]::Left -bor
    [int][System.Windows.Forms.AnchorStyles]::Right)
$contentPanel.BackColor = [System.Drawing.Color]::FromArgb(5, 10, 15)
$form.Controls.Add($contentPanel)

$list = New-Object System.Windows.Forms.ListView
$list.Dock = "None"
$list.Width = 288
$list.View = "Details"
$list.FullRowSelect = $true
$list.MultiSelect = $false
$list.HideSelection = $false
$list.Scrollable = $false
$list.BorderStyle = "None"
$list.BackColor = [System.Drawing.Color]::FromArgb(7, 22, 29)
$list.BackgroundImage = [System.Drawing.Image]::FromFile($taskPanelTexturePath)
$list.BackgroundImageTiled = $true
$list.ForeColor = [System.Drawing.Color]::FromArgb(211, 232, 226)
$list.Font = New-Object System.Drawing.Font("Bahnschrift", 9.5)
$list.OwnerDraw = $true
$list.Columns.Add("Priorita", 70) | Out-Null
$list.Columns.Add("Info", 78) | Out-Null
$list.Columns.Add("Nazev", 140) | Out-Null
$contentPanel.Controls.Add($list)

$listScrollTrack = New-Object System.Windows.Forms.Panel
$listScrollTrack.Width = 12
$listScrollTrack.Top = 24
$listScrollTrack.Height = 120
$listScrollTrack.BackColor = [System.Drawing.Color]::FromArgb(10, 28, 34)
$listScrollTrack.Visible = $false
$contentPanel.Controls.Add($listScrollTrack)

$listScrollThumb = New-Object System.Windows.Forms.Panel
$listScrollThumb.Left = 2
$listScrollThumb.Top = 2
$listScrollThumb.Width = 8
$listScrollThumb.Height = 48
$listScrollThumb.BackColor = [System.Drawing.Color]::FromArgb(83, 123, 116)
$listScrollTrack.Controls.Add($listScrollThumb)

$separator = New-Object System.Windows.Forms.Panel
$separator.Dock = "None"
$separator.Width = 1
$separator.BackColor = [System.Drawing.Color]::FromArgb(17, 40, 43)
$contentPanel.Controls.Add($separator)

$detailsPanel = New-Object System.Windows.Forms.Panel
$detailsPanel.Left = $list.Width + $listScrollTrack.Width + $separator.Width
$detailsPanel.Top = 0
$detailsPanel.Width = $form.ClientSize.Width - $list.Width - $listScrollTrack.Width - $separator.Width
$detailsPanel.Height = $contentPanel.ClientSize.Height
$detailsPanel.Anchor = [System.Windows.Forms.AnchorStyles](
    [int][System.Windows.Forms.AnchorStyles]::Top -bor
    [int][System.Windows.Forms.AnchorStyles]::Bottom -bor
    [int][System.Windows.Forms.AnchorStyles]::Left -bor
    [int][System.Windows.Forms.AnchorStyles]::Right)
$detailsPanel.AutoScroll = $false
$detailsPanel.BackColor = [System.Drawing.Color]::FromArgb(5, 9, 14)
$contentPanel.Controls.Add($detailsPanel)

$details = New-Object System.Windows.Forms.Panel
$details.AutoSize = $false
$details.Left = 18
$details.Top = 18
$details.Width = 500
$details.Height = 500
$details.BackColor = [System.Drawing.Color]::FromArgb(5, 9, 14)
$details.Margin = New-Object System.Windows.Forms.Padding(0)
$detailsPanel.Controls.Add($details)

$detailsScrollTrack = New-Object System.Windows.Forms.Panel
$detailsScrollTrack.Width = 12
$detailsScrollTrack.Top = 12
$detailsScrollTrack.Height = 120
$detailsScrollTrack.Anchor = [System.Windows.Forms.AnchorStyles](
    [int][System.Windows.Forms.AnchorStyles]::Top -bor
    [int][System.Windows.Forms.AnchorStyles]::Bottom -bor
    [int][System.Windows.Forms.AnchorStyles]::Right)
$detailsScrollTrack.BackColor = [System.Drawing.Color]::FromArgb(10, 28, 34)
$detailsScrollTrack.Visible = $false
$detailsPanel.Controls.Add($detailsScrollTrack)

$detailsScrollThumb = New-Object System.Windows.Forms.Panel
$detailsScrollThumb.Left = 2
$detailsScrollThumb.Top = 2
$detailsScrollThumb.Width = 8
$detailsScrollThumb.Height = 48
$detailsScrollThumb.BackColor = [System.Drawing.Color]::FromArgb(83, 123, 116)
$detailsScrollTrack.Controls.Add($detailsScrollThumb)

$script:detailsScrollOffset = 0
$script:detailsScrollDragging = $false
$script:detailsScrollDragStartY = 0
$script:detailsScrollDragStartOffset = 0


$buttonPanel = New-Object System.Windows.Forms.Panel
$buttonPanel.Left = 0
$buttonPanel.Top = $form.ClientSize.Height - 46
$buttonPanel.Width = $form.ClientSize.Width
$buttonPanel.Height = 46
$buttonPanel.Anchor = [System.Windows.Forms.AnchorStyles](
    [int][System.Windows.Forms.AnchorStyles]::Bottom -bor
    [int][System.Windows.Forms.AnchorStyles]::Left -bor
    [int][System.Windows.Forms.AnchorStyles]::Right)
$buttonPanel.BackColor = [System.Drawing.Color]::FromArgb(4, 9, 13)
$form.Controls.Add($buttonPanel)

$hardExitButton = New-Object System.Windows.Forms.Button
$hardExitButton.Text = "Tvrde zavrit"
$hardExitButton.Width = 120
$hardExitButton.Height = 28
$hardExitButton.Left = 12
$hardExitButton.Top = 9
$hardExitButton.FlatStyle = "Flat"
$hardExitButton.FlatAppearance.BorderColor = [System.Drawing.Color]::FromArgb(164, 122, 69)
$hardExitButton.FlatAppearance.MouseOverBackColor = [System.Drawing.Color]::FromArgb(31, 47, 45)
$hardExitButton.BackColor = [System.Drawing.Color]::FromArgb(8, 22, 28)
$hardExitButton.ForeColor = [System.Drawing.Color]::FromArgb(211, 228, 221)
$hardExitButton.Font = New-Object System.Drawing.Font("Bahnschrift", 9)
$buttonPanel.Controls.Add($hardExitButton)

$copySelectionButton = New-Object System.Windows.Forms.Button
$copySelectionButton.Text = "Kopirovat ID"
$copySelectionButton.Width = 120
$copySelectionButton.Height = 28
$copySelectionButton.Left = $hardExitButton.Right + 8
$copySelectionButton.Top = 9
$copySelectionButton.FlatStyle = "Flat"
$copySelectionButton.FlatAppearance.BorderColor = [System.Drawing.Color]::FromArgb(92, 79, 63)
$copySelectionButton.FlatAppearance.MouseOverBackColor = [System.Drawing.Color]::FromArgb(31, 47, 45)
$copySelectionButton.BackColor = [System.Drawing.Color]::FromArgb(8, 22, 28)
$copySelectionButton.ForeColor = [System.Drawing.Color]::FromArgb(142, 165, 159)
$copySelectionButton.Font = New-Object System.Drawing.Font("Bahnschrift", 9)
$buttonPanel.Controls.Add($copySelectionButton)

$script:detailsText = ""

function Set-TaskBoardLayout {
    $frame = 2
    $titleBar.SetBounds(0, 0, $form.ClientSize.Width, 30)
    $closeButton.SetBounds($titleBar.ClientSize.Width - $closeButton.Width, 0, $closeButton.Width, $titleBar.Height)
    $minimizeButton.SetBounds($closeButton.Left - $minimizeButton.Width, 0, $minimizeButton.Width, $titleBar.Height)
    $titleLabel.Width = [Math]::Max(80, $minimizeButton.Left - $titleLabel.Left - 8)
    $header.SetBounds($frame, $titleBar.Bottom + $frame, $form.ClientSize.Width - ($frame * 2), 54)
    $projectProgressLabel.SetBounds([Math]::Max(0, $header.ClientSize.Width - 320), 0, 320, $header.Height)
    $modePanel.SetBounds($frame, $header.Bottom, $form.ClientSize.Width - ($frame * 2), 38)
    $buttonPanel.SetBounds($frame, $form.ClientSize.Height - $buttonPanel.Height - $frame, $form.ClientSize.Width - ($frame * 2), $buttonPanel.Height)
    $contentPanel.SetBounds($frame, $modePanel.Bottom, $form.ClientSize.Width - ($frame * 2), $buttonPanel.Top - $modePanel.Bottom)
    $list.SetBounds(0, 0, 288, $contentPanel.ClientSize.Height)
    $detailsPanel.SetBounds(
        $list.Width + $listScrollTrack.Width + $separator.Width,
        0,
        [Math]::Max(260, $contentPanel.ClientSize.Width - $list.Width - $listScrollTrack.Width - $separator.Width),
        $contentPanel.ClientSize.Height)
    $separator.SetBounds($list.Width + $listScrollTrack.Width, 0, 1, $contentPanel.ClientSize.Height)
    $detailsScrollTrack.SetBounds(
        [Math]::Max(0, $detailsPanel.ClientSize.Width - 16),
        12,
        12,
        [Math]::Max(40, $detailsPanel.ClientSize.Height - 24))
    $listScrollTrack.SetBounds(
        $list.Width,
        26,
        12,
        [Math]::Max(40, $contentPanel.ClientSize.Height - 32))
    $titleBar.BringToFront()
    $header.BringToFront()
    $projectProgressLabel.BringToFront()
    $modePanel.BringToFront()
    $buttonPanel.BringToFront()
    $listScrollTrack.BringToFront()
}

$script:currentItems = @()
$script:currentBoardMode = Get-SavedBoardMode
$script:pendingSelectedTag = Get-SavedSelectedTag
$script:selectedBoardTag = $script:pendingSelectedTag
$script:pendingDetailsScrollOffset = Get-SavedDetailsScrollOffset
$script:restoreSavedDetailsScroll = $script:pendingDetailsScrollOffset -gt 0
$script:listScrollOffset = 0
$script:listScrollDragging = $false
$script:listScrollDragStartY = 0
$script:listScrollDragStartOffset = 0
$script:suppressListSelectionChanged = $false

function Get-DetailsScrollMaximum {
    $contentHeight = [Math]::Max(0, $details.Height + 36)
    return [Math]::Max(0, $contentHeight - $detailsPanel.ClientSize.Height)
}

function Update-DetailsScrollbar {
    try {
        $maxOffset = Get-DetailsScrollMaximum
        if ($maxOffset -le 0) {
            $script:detailsScrollOffset = 0
            $details.Top = 18
            $detailsScrollTrack.Visible = $false
            return
        }

        $trackHeight = [Math]::Max(40, $detailsScrollTrack.Height)
        $contentHeight = [Math]::Max(1, $details.Height + 36)
        $thumbHeight = [int][Math]::Max(34, [Math]::Min($trackHeight, ($detailsPanel.ClientSize.Height / $contentHeight) * $trackHeight))
        $travel = [Math]::Max(1, $trackHeight - $thumbHeight)
        $thumbTop = [int](($script:detailsScrollOffset / [Math]::Max(1, $maxOffset)) * $travel)

        $detailsScrollThumb.SetBounds(2, $thumbTop, 8, $thumbHeight)
        $detailsScrollTrack.Visible = $true
        $detailsScrollTrack.BringToFront()
    } catch {
        Write-TaskBoardError "Update-DetailsScrollbar failed" $_
    }
}

function Set-DetailsScrollOffset {
    param([int]$Offset)

    $maxOffset = Get-DetailsScrollMaximum
    $script:detailsScrollOffset = [int][Math]::Max(0, [Math]::Min($Offset, $maxOffset))
    $details.Top = 18 - $script:detailsScrollOffset
    Update-DetailsScrollbar
    Save-TaskBoardState
}

function Invoke-DetailsMouseWheel {
    param($Event)

    if ($null -eq $Event) {
        return
    }

    $step = if ($Event.Delta -lt 0) { 72 } else { -72 }
    Set-DetailsScrollOffset ($script:detailsScrollOffset + $step)
}

function Set-TaskBoardIcon {
    param(
        [bool]$HasTasks,
        [int]$TaskCount
    )

    try {
        $iconPath = if ($HasTasks) { $alertIconPath } else { $calmIconPath }
        $newIcon = New-Object System.Drawing.Icon $iconPath
        $oldIcon = $script:currentTaskBoardIcon
        $script:currentTaskBoardIcon = $newIcon

        $form.Icon = $newIcon
        $notifyIcon.Icon = $newIcon
        $notifyIcon.Text = if ($TaskCount -eq 0) { "Strategic Nexus - zadne ukoly" } else { "Strategic Nexus - ukoly: $TaskCount" }
        Update-TaskBoardShortcutIcons -IconPath $iconPath

        if ($oldIcon -and -not [object]::ReferenceEquals($oldIcon, $newIcon)) {
            $oldIcon.Dispose()
        }
    } catch {
        Write-TaskBoardError "Set-TaskBoardIcon failed" $_
    }
}

function Invoke-TaskbarIconRefresh {
    try {
        if ($isRenderPreview -or $form.IsDisposed -or $form.Handle -eq [IntPtr]::Zero) {
            return
        }

        $swpNoSize = 0x0001
        $swpNoMove = 0x0002
        $swpNoZOrder = 0x0004
        $swpNoActivate = 0x0010
        $swpFrameChanged = 0x0020
        $flags = [uint32]($swpNoSize -bor $swpNoMove -bor $swpNoZOrder -bor $swpNoActivate -bor $swpFrameChanged)

        [StrategicNexusTaskBoardNative]::SetWindowPos($form.Handle, [IntPtr]::Zero, 0, 0, 0, 0, $flags) | Out-Null
        [StrategicNexusTaskBoardNative]::DrawMenuBar($form.Handle) | Out-Null
        $form.Invalidate()
        $form.Update()
    } catch {
        Write-TaskBoardError "Invoke-TaskbarIconRefresh failed" $_
    }
}

function Get-TaskBoardShortcutPaths {
    $paths = @()
    $shortcutName = "Strategic Nexus Task Board.lnk"

    $desktop = [Environment]::GetFolderPath("Desktop")
    if ($desktop) {
        $paths += Join-Path $desktop $shortcutName
    }

    $startup = [Environment]::GetFolderPath("Startup")
    if ($startup) {
        $paths += Join-Path $startup $shortcutName
    }

    $programs = [Environment]::GetFolderPath("Programs")
    if ($programs) {
        $paths += Join-Path (Join-Path $programs "Strategic Nexus") $shortcutName
    }

    $taskbarPinned = Join-Path $env:APPDATA "Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar"
    if ($taskbarPinned) {
        $paths += Join-Path $taskbarPinned $shortcutName
    }

    return @($paths | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -Unique)
}

function Update-TaskBoardShortcutIcons {
    param([string]$IconPath)

    if ($isRenderPreview -or [string]::IsNullOrWhiteSpace($IconPath)) {
        return
    }
    if ($script:lastShortcutIconPath -eq $IconPath) {
        return
    }

    try {
        $shortcutPaths = @(Get-TaskBoardShortcutPaths)
        if ($shortcutPaths.Count -eq 0) {
            $script:lastShortcutIconPath = $IconPath
            return
        }

        $shell = New-Object -ComObject WScript.Shell
        $changed = $false
        $desiredIcon = "$IconPath,0"
        foreach ($shortcutPath in $shortcutPaths) {
            $shortcut = $shell.CreateShortcut($shortcutPath)
            if ($shortcut.IconLocation -ne $desiredIcon) {
                $shortcut.IconLocation = $desiredIcon
                $shortcut.Save()
                $changed = $true
            }
        }

        if ($changed) {
            [StrategicNexusTaskBoardNative]::SHChangeNotify(0x08000000, 0, [IntPtr]::Zero, [IntPtr]::Zero)
        }
        $script:lastShortcutIconPath = $IconPath
    } catch {
        Write-TaskBoardError "Update-TaskBoardShortcutIcons failed" $_
    }
}

function Format-TaskDetails {
    param($Item)

    if (-not $Item) {
        if ($script:currentBoardMode -eq "reports") {
            return "Zadne reporty.`r`n`r`nDulezite souhrny prace, rizika a rozhodnuti se objevi tady."
        }
        if ($script:currentBoardMode -eq "suggestions") {
            return "Zadne navrhy.`r`n`r`nKdyz prijdu na lepsi smer, vec k dodefinovani nebo uzitecne zlepseni, objevi se tady."
        }
        return "Zadne aktualni ukoly pro tebe.`r`n`r`nKdyz budu potrebovat test, rozhodnuti nebo tvoji akci, objevi se tady."
    }

    if ($Item.board_kind -eq "suggestion") {
        return @"
TYP
NAVRH

NAZEV
$($Item.title)

OBLAST
$($Item.category)

DULEZITOST
$($Item.severity)

PRINOS
$($Item.benefit)

NAVRH
$($Item.proposal)

PROC TO STOJI ZA UVAHU
$($Item.why)

DOPORUCENY KROK
$($Item.next)

STAV
$($Item.status)

CAS
$($Item.created_at)

ID
$($Item.id)
"@
    }

    if ($Item.board_kind -eq "report") {
        return @"
TYP
REPORT

NAZEV
$($Item.title)

KATEGORIE
$($Item.category)

DULEZITOST
$($Item.severity)

SHRNUTI
$($Item.summary)

PROC JE TO DULEZITE
$($Item.why)

NAVAZUJICI KROK
$($Item.next)

CAS
$($Item.created_at)

ID
$($Item.id)
"@
    }

    $progress = Get-ProgressPercent $Item
    return @"
TYP
UKOL

NAZEV
$($Item.title)

PRIORITA
$($Item.severity)

HOTOVO
$progress%

CO POTREBUJU
$($Item.task)

PROC
$($Item.why)

CO UDELAT
$($Item.instructions)

ID
$($Item.id)
"@
}

function Set-DetailsText {
    param([string]$Text)

    try {
        if ($null -eq $Text) {
            $Text = ""
        }
        $script:detailsText = $Text

        $panelWidth = [int]$detailsPanel.ClientSize.Width
        $panelHeight = [int]$detailsPanel.ClientSize.Height
        $availableWidth = [int][Math]::Max(260, $panelWidth - 58)
        $details.Width = $availableWidth
        $minimumHeight = [int][Math]::Max(220, [Math]::Max(0, $panelHeight - 36))
        $details.Height = $minimumHeight

        $details.SuspendLayout()
        $details.Controls.Clear()

        $headingFont = New-Object System.Drawing.Font("Bahnschrift", 9.75, [System.Drawing.FontStyle]::Bold)
        $bodyFont = New-Object System.Drawing.Font("Cascadia Mono", 10.25)
        $headingColor = [System.Drawing.Color]::FromArgb(196, 148, 82)
        $bodyColor = [System.Drawing.Color]::FromArgb(196, 219, 211)
        $mutedColor = [System.Drawing.Color]::FromArgb(158, 185, 178)

        function Split-VisualLine {
            param(
                [string]$Line,
                [System.Drawing.Font]$Font,
                [int]$Width
            )

            if ([string]::IsNullOrWhiteSpace($Line)) {
                return @($Line)
            }

            $words = @($Line -split "\s+")
            $wrapped = @()
            $current = ""
            foreach ($word in $words) {
                $candidate = if ([string]::IsNullOrEmpty($current)) { $word } else { "$current $word" }
                $measured = [System.Windows.Forms.TextRenderer]::MeasureText(
                    $candidate,
                    $Font,
                    (New-Object System.Drawing.Size 0, 0),
                    [System.Windows.Forms.TextFormatFlags]::NoPadding)
                if ($measured.Width -le ($Width - 8) -or [string]::IsNullOrEmpty($current)) {
                    $current = $candidate
                    continue
                }

                $wrapped += $current
                $current = $word
            }

            if (-not [string]::IsNullOrEmpty($current)) {
                $wrapped += $current
            }

            return $wrapped
        }

        $headingNames = @(
            "TYP",
            "NAZEV",
            "KATEGORIE",
            "OBLAST",
            "DULEZITOST",
            "PRINOS",
            "NAVRH",
            "SHRNUTI",
            "PROC JE TO DULEZITE",
            "PROC TO STOJI ZA UVAHU",
            "NAVAZUJICI KROK",
            "DOPORUCENY KROK",
            "STAV",
            "CAS",
            "PRIORITA",
            "HOTOVO",
            "CO POTREBUJU",
            "PROC",
            "CO UDELAT",
            "ID")
        $lines = $Text -split "`r?`n", -1
        $renderedLines = New-Object System.Collections.Generic.List[string]
        $lineStyles = New-Object System.Collections.Generic.List[string]
        foreach ($line in $lines) {
            $isHeading = $headingNames -contains $line.Trim()
            $visualLines = if ($isHeading) {
                @($line)
            } else {
                Split-VisualLine -Line $line -Font $bodyFont -Width ([Math]::Max(120, $details.Width - 34))
            }

            foreach ($visualLine in $visualLines) {
                $renderedLines.Add([string]$visualLine)
                if ($isHeading) {
                    $lineStyles.Add("heading")
                } elseif ([string]::IsNullOrWhiteSpace($visualLine)) {
                    $lineStyles.Add("blank")
                } else {
                    $lineStyles.Add("body")
                }
            }
        }

        $richText = New-Object System.Windows.Forms.RichTextBox
        $richText.Left = 14
        $richText.Top = 14
        $richText.Width = [Math]::Max(120, $details.Width - 34)
        $richText.BorderStyle = [System.Windows.Forms.BorderStyle]::None
        $richText.BackColor = $details.BackColor
        $richText.ForeColor = $bodyColor
        $richText.Font = $bodyFont
        $richText.ReadOnly = $true
        $richText.Multiline = $true
        $richText.WordWrap = $false
        $richText.ScrollBars = [System.Windows.Forms.RichTextBoxScrollBars]::None
        $richText.ShortcutsEnabled = $true
        $richText.HideSelection = $false
        $richText.DetectUrls = $false
        $richText.TabStop = $true
        function ConvertTo-RtfEscapedText {
            param([string]$Value)

            if ($null -eq $Value) {
                return ""
            }

            $escaped = New-Object System.Text.StringBuilder
            foreach ($character in $Value.ToCharArray()) {
                $code = [int][char]$character
                if ($character -eq "\") {
                    [void]$escaped.Append("\\")
                } elseif ($character -eq "{") {
                    [void]$escaped.Append("\{")
                } elseif ($character -eq "}") {
                    [void]$escaped.Append("\}")
                } elseif ($code -gt 127) {
                    if ($code -gt 32767) {
                        $code = $code - 65536
                    }
                    [void]$escaped.Append("\u$($code)?")
                } else {
                    [void]$escaped.Append($character)
                }
            }
            return $escaped.ToString()
        }

        $rtf = New-Object System.Text.StringBuilder
        [void]$rtf.Append("{\rtf1\ansi\deff0")
        [void]$rtf.Append("{\fonttbl{\f0 Cascadia Mono;}{\f1 Bahnschrift;}}")
        [void]$rtf.Append("{\colortbl;")
        [void]$rtf.Append("\red196\green148\blue82;")
        [void]$rtf.Append("\red196\green219\blue211;")
        [void]$rtf.Append("\red158\green185\blue178;")
        [void]$rtf.Append("}")
        [void]$rtf.Append("\viewkind4\uc1\pard")

        for ($i = 0; $i -lt $renderedLines.Count; $i++) {
            $lineText = ConvertTo-RtfEscapedText ([string]$renderedLines[$i])
            $style = [string]$lineStyles[$i]
            if ($style -eq "heading") {
                [void]$rtf.Append("\f1\b\fs20\cf1 ")
            } elseif ($style -eq "blank") {
                [void]$rtf.Append("\f0\b0\fs12\cf3 ")
            } else {
                [void]$rtf.Append("\f0\b0\fs21\cf2 ")
            }
            [void]$rtf.Append($lineText)
            if ($i -lt ($renderedLines.Count - 1)) {
                [void]$rtf.Append("\par ")
            }
        }
        [void]$rtf.Append("}")
        $richText.Rtf = $rtf.ToString()
        $richText.Select(0, 0)
        $richText.Add_KeyDown({
            param($sender, $event)

            if ($event.Control -and $event.KeyCode -eq [System.Windows.Forms.Keys]::C) {
                try {
                    if (-not [string]::IsNullOrEmpty($sender.SelectedText)) {
                        [System.Windows.Forms.Clipboard]::SetText($sender.SelectedText)
                        $event.SuppressKeyPress = $true
                    }
                } catch {
                    Write-TaskBoardError "Copy detail selection failed" $_
                }
            }
        })
        $richText.Add_MouseWheel({ param($sender, $event) Invoke-DetailsMouseWheel $event })
        $details.Controls.Add($richText)

        $contentHeight = 28
        for ($i = 0; $i -lt $lineStyles.Count; $i++) {
            $style = [string]$lineStyles[$i]
            if ($style -eq "heading") {
                $contentHeight += 20
            } elseif ($style -eq "blank") {
                $contentHeight += 10
            } else {
                $contentHeight += 22
            }
        }
        $richText.Height = [Math]::Max($minimumHeight - 28, $contentHeight)
        $details.Height = [Math]::Max($minimumHeight, $contentHeight + 14)
        $scrollOffset = 0
        if ($script:restoreSavedDetailsScroll) {
            $scrollOffset = $script:pendingDetailsScrollOffset
            $script:restoreSavedDetailsScroll = $false
            $script:pendingDetailsScrollOffset = 0
        }
        Set-DetailsScrollOffset $scrollOffset
        $details.ResumeLayout()
    } catch {
        Write-TaskBoardError "Set-DetailsText failed" $_
    }
}

function Set-ModeButtonStyle {
    param(
        [System.Windows.Forms.Button]$Button,
        [bool]$Active
    )

    if ($Active) {
        $Button.BackColor = [System.Drawing.Color]::FromArgb(18, 70, 65)
        $Button.ForeColor = [System.Drawing.Color]::FromArgb(226, 238, 232)
        $Button.FlatAppearance.BorderColor = [System.Drawing.Color]::FromArgb(196, 148, 82)
    } else {
        $Button.BackColor = [System.Drawing.Color]::FromArgb(7, 22, 29)
        $Button.ForeColor = [System.Drawing.Color]::FromArgb(176, 204, 196)
        $Button.FlatAppearance.BorderColor = [System.Drawing.Color]::FromArgb(34, 66, 68)
    }
}

function Set-CopyButtonVisualState {
    param([bool]$HasSelection)

    if ($HasSelection) {
        $copySelectionButton.ForeColor = [System.Drawing.Color]::FromArgb(211, 228, 221)
        $copySelectionButton.FlatAppearance.BorderColor = [System.Drawing.Color]::FromArgb(164, 122, 69)
    } else {
        $copySelectionButton.ForeColor = [System.Drawing.Color]::FromArgb(142, 165, 159)
        $copySelectionButton.FlatAppearance.BorderColor = [System.Drawing.Color]::FromArgb(92, 79, 63)
    }
}

function Get-SelectedBoardReference {
    if ($list.SelectedItems.Count -eq 0) {
        return [string]$script:selectedBoardTag
    }

    return [string]$list.SelectedItems[0].Tag
}

function Get-ListVisibleCapacity {
    try {
        return [Math]::Max(1, [int][Math]::Floor(([Math]::Max(40, $list.ClientSize.Height - 24)) / 20))
    } catch {
        return 12
    }
}

function Get-ListScrollMaximum {
    return [Math]::Max(0, $script:currentItems.Count - (Get-ListVisibleCapacity))
}

function Update-ListScrollbar {
    try {
        $maxOffset = Get-ListScrollMaximum
        if ($maxOffset -le 0) {
            $script:listScrollOffset = 0
            $listScrollTrack.Visible = $false
            return
        }

        $trackHeight = [Math]::Max(40, $listScrollTrack.Height)
        $visibleCapacity = Get-ListVisibleCapacity
        $totalRows = [Math]::Max(1, $script:currentItems.Count)
        $thumbHeight = [int][Math]::Max(34, [Math]::Min($trackHeight, ($visibleCapacity / $totalRows) * $trackHeight))
        $travel = [Math]::Max(1, $trackHeight - $thumbHeight)
        $thumbTop = [int](($script:listScrollOffset / [Math]::Max(1, $maxOffset)) * $travel)

        $listScrollThumb.SetBounds(2, $thumbTop, 8, $thumbHeight)
        $listScrollTrack.Visible = $true
        $listScrollTrack.BringToFront()
    } catch {
        Write-TaskBoardError "Update-ListScrollbar failed" $_
    }
}

function Set-ListScrollOffset {
    param([int]$Offset)

    $script:listScrollOffset = [int][Math]::Max(0, [Math]::Min($Offset, (Get-ListScrollMaximum)))
    Refresh-Board
}

function Copy-SelectedBoardReference {
    try {
        $reference = Get-SelectedBoardReference
        if ([string]::IsNullOrWhiteSpace($reference)) {
            if (-not $isRenderPreview) {
                $notifyIcon.ShowBalloonTip(
                    1800,
                    "Strategic Nexus",
                    "Neni vybrany zadny ukol ani report.",
                    [System.Windows.Forms.ToolTipIcon]::Info)
            }
            return
        }

        [System.Windows.Forms.Clipboard]::SetText($reference)
        if (-not $isRenderPreview) {
            $notifyIcon.ShowBalloonTip(
                1600,
                "Strategic Nexus",
                "Zkopirovano: $reference",
                [System.Windows.Forms.ToolTipIcon]::Info)
        }
    } catch {
        Write-TaskBoardError "Copy-SelectedBoardReference failed" $_
    }
}

function Refresh-Board {
    try {
    $selectedId = $null
    if ($list.SelectedItems.Count -gt 0) {
        $selectedId = $list.SelectedItems[0].Tag
    } elseif (-not [string]::IsNullOrWhiteSpace($script:selectedBoardTag)) {
        $selectedId = $script:selectedBoardTag
    } elseif (-not [string]::IsNullOrWhiteSpace($script:pendingSelectedTag)) {
        $selectedId = $script:pendingSelectedTag
    }

    $tasks = @(Read-Tasks)
    $reports = @(Read-Reports)
    $suggestions = @(Read-Suggestions)
    $script:currentItems = if ($script:currentBoardMode -eq "reports") {
        @($reports)
    } elseif ($script:currentBoardMode -eq "suggestions") {
        @($suggestions)
    } else {
        @($tasks)
    }
    $visibleCapacity = Get-ListVisibleCapacity
    $selectedIndex = -1
    if ($selectedId) {
        for ($index = 0; $index -lt $script:currentItems.Count; ++$index) {
            $boardItem = $script:currentItems[$index]
            $kind = if ($boardItem.board_kind -eq "report") {
                "report"
            } elseif ($boardItem.board_kind -eq "suggestion") {
                "suggestion"
            } else {
                "task"
            }
            if ("$kind|$($boardItem.id)" -eq $selectedId) {
                $selectedIndex = $index
                break
            }
        }
    }

    if ($selectedIndex -ge 0) {
        if ($selectedIndex -lt $script:listScrollOffset) {
            $script:listScrollOffset = $selectedIndex
        } elseif ($selectedIndex -ge ($script:listScrollOffset + $visibleCapacity)) {
            $script:listScrollOffset = [Math]::Max(0, $selectedIndex - $visibleCapacity + 1)
        }
    }
    $script:listScrollOffset = [int][Math]::Max(0, [Math]::Min($script:listScrollOffset, (Get-ListScrollMaximum)))

    $visibleItems = @($script:currentItems | Select-Object -Skip $script:listScrollOffset -First $visibleCapacity)
    $script:suppressListSelectionChanged = $true
    $list.Items.Clear()
    $list.Columns[0].Text = if ($script:currentBoardMode -eq "tasks") {
        "Priorita"
    } elseif ($script:currentBoardMode -eq "reports") {
        "Dulezitost"
    } else {
        "Stav"
    }
    $list.Columns[1].Text = if ($script:currentBoardMode -eq "tasks") {
        "Hotovo"
    } elseif ($script:currentBoardMode -eq "suggestions") {
        "Oblast"
    } else {
        "Kategorie"
    }
    $list.Columns[2].Text = "Nazev"

    foreach ($boardItem in $visibleItems) {
        $kind = if ($boardItem.board_kind -eq "report") {
            "report"
        } elseif ($boardItem.board_kind -eq "suggestion") {
            "suggestion"
        } else {
            "task"
        }
        $severity = if ($boardItem.severity) { [string]$boardItem.severity } else { "normal" }
        $title = if ($boardItem.title) { [string]$boardItem.title } else { [string]$boardItem.id }
        $priorityText = switch ($severity.ToLowerInvariant()) {
            "critical" { "kriticka" }
            "high" { "vysoka" }
            "normal" { "normalni" }
            "low" { "nizka" }
            default { $severity }
        }
        $statusText = if ($kind -eq "report") {
            $priorityText
        } elseif ($kind -eq "suggestion") {
            if ($boardItem.status) { [string]$boardItem.status } else { "navrh" }
        } else {
            $priorityText
        }
        $infoText = if ($kind -eq "task") {
            "$(Get-ProgressPercent $boardItem)%"
        } else {
            if ($boardItem.category) { [string]$boardItem.category } else { "prace" }
        }
        $item = New-Object System.Windows.Forms.ListViewItem $statusText
        [void]$item.SubItems.Add($infoText)
        [void]$item.SubItems.Add($title)
        $item.Tag = "$kind|$($boardItem.id)"
        $item.BackColor = switch ($severity.ToLowerInvariant()) {
            "critical" { [System.Drawing.Color]::FromArgb(31, 44, 48) }
            "high" { [System.Drawing.Color]::FromArgb(24, 43, 44) }
            "normal" { [System.Drawing.Color]::FromArgb(11, 42, 50) }
            "low" { [System.Drawing.Color]::FromArgb(12, 45, 37) }
            default { [System.Drawing.Color]::FromArgb(11, 42, 50) }
        }
        $item.ForeColor = switch ($severity.ToLowerInvariant()) {
            "critical" { [System.Drawing.Color]::FromArgb(232, 199, 178) }
            "high" { [System.Drawing.Color]::FromArgb(228, 207, 171) }
            "normal" { [System.Drawing.Color]::FromArgb(203, 225, 219) }
            "low" { [System.Drawing.Color]::FromArgb(194, 222, 203) }
            default { [System.Drawing.Color]::FromArgb(203, 225, 219) }
        }
        [void]$list.Items.Add($item)
        if ($selectedId -and $selectedId -eq $item.Tag) {
            $item.Selected = $true
        }
    }
    $script:suppressListSelectionChanged = $false

    $taskCount = $tasks.Count
    $reportCount = $reports.Count
    $suggestionCount = $suggestions.Count
    $projectProgress = Read-ProjectProgressEstimate
    $count = $script:currentItems.Count
    $header.Text = if ($script:currentBoardMode -eq "reports") {
        "Strategic Nexus - reporty: $reportCount | ukoly: $taskCount | navrhy: $suggestionCount"
    } elseif ($script:currentBoardMode -eq "suggestions") {
        "Strategic Nexus - navrhy: $suggestionCount | ukoly: $taskCount | reporty: $reportCount"
    } else {
        "Strategic Nexus - ukoly pro me: $taskCount | reporty: $reportCount | navrhy: $suggestionCount"
    }
    $projectProgressLabel.Text = "Odhad projektu: $($projectProgress.percent)%`r`nZbyva: $($projectProgress.remaining)`r`nPresnost odhadu: $($projectProgress.confidence)"
    $form.Text = "Strategic Nexus ($taskCount/$reportCount/$suggestionCount)"
    Set-ModeButtonStyle -Button $tasksModeButton -Active ($script:currentBoardMode -eq "tasks")
    Set-ModeButtonStyle -Button $reportsModeButton -Active ($script:currentBoardMode -eq "reports")
    Set-ModeButtonStyle -Button $suggestionsModeButton -Active ($script:currentBoardMode -eq "suggestions")

    $hasTasks = $taskCount -gt 0
    $taskbarIconStateChanged = ($null -eq $script:lastHasTasksForTaskbarIcon) -or ([bool]$script:lastHasTasksForTaskbarIcon -ne [bool]$hasTasks)
    Set-TaskBoardIcon -HasTasks $hasTasks -TaskCount $taskCount
    if ($taskbarIconStateChanged) {
        Invoke-TaskbarIconRefresh
        $script:lastHasTasksForTaskbarIcon = [bool]$hasTasks
    }
    $header.BackColor = [System.Drawing.Color]::FromArgb(12, 58, 54)
    $header.ForeColor = if ($hasTasks) {
        [System.Drawing.Color]::FromArgb(224, 205, 166)
    } else {
        [System.Drawing.Color]::FromArgb(216, 232, 225)
    }

    if ($list.SelectedItems.Count -eq 0 -and $list.Items.Count -gt 0 -and ($selectedIndex -lt 0)) {
        $list.Items[0].Selected = $true
        $script:selectedBoardTag = [string]$list.Items[0].Tag
        Set-DetailsText (Format-TaskDetails $visibleItems[0])
        Set-CopyButtonVisualState -HasSelection $true
    } elseif ($list.SelectedItems.Count -eq 0) {
        if ($script:currentItems.Count -eq 0) {
            $script:selectedBoardTag = ""
            Set-DetailsText (Format-TaskDetails $null)
            Set-CopyButtonVisualState -HasSelection $false
        }
    } else {
        $script:pendingSelectedTag = ""
        $script:selectedBoardTag = [string]$list.SelectedItems[0].Tag
        $selectedItem = $script:currentItems | Where-Object {
            $kind = if ($_.board_kind -eq "report") {
                "report"
            } elseif ($_.board_kind -eq "suggestion") {
                "suggestion"
            } else {
                "task"
            }
            "$kind|$($_.id)" -eq $script:selectedBoardTag
        } | Select-Object -First 1
        Set-DetailsText (Format-TaskDetails $selectedItem)
        Set-CopyButtonVisualState -HasSelection $true
    }
    Update-ListScrollbar
    Save-TaskBoardState
    } catch {
        $script:suppressListSelectionChanged = $false
        Write-TaskBoardError "Refresh-Board failed" $_
    }
}

$notifyIcon = New-Object System.Windows.Forms.NotifyIcon
$notifyIcon.Text = "Strategic Nexus"
$notifyIcon.Icon = $script:currentTaskBoardIcon
$notifyIcon.Visible = -not $isRenderPreview

$trayMenu = New-Object System.Windows.Forms.ContextMenuStrip
$showMenuItem = New-Object System.Windows.Forms.ToolStripMenuItem "Zobrazit ukoly"
$restoreStartMenuShortcutMenuItem = New-Object System.Windows.Forms.ToolStripMenuItem "Obnovit zastupce v nabidce Start"
$hardExitMenuItem = New-Object System.Windows.Forms.ToolStripMenuItem "Tvrde zavrit"
[void]$trayMenu.Items.Add($showMenuItem)
[void]$trayMenu.Items.Add($restoreStartMenuShortcutMenuItem)
[void]$trayMenu.Items.Add($hardExitMenuItem)
$notifyIcon.ContextMenuStrip = $trayMenu

function Install-StartMenuShortcut {
    param([bool]$ShowNotification)

    try {
        $arguments = "-NoProfile -ExecutionPolicy RemoteSigned -File `"$shortcutInstallerScriptPath`" -InstallDesktopShortcut:`$false -InstallStartupShortcut:`$false -InstallStartMenuShortcut:`$true"
        $startInfo = New-Object System.Diagnostics.ProcessStartInfo
        $startInfo.FileName = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
        $startInfo.Arguments = $arguments
        $startInfo.WorkingDirectory = $repoRoot
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true

        $process = [System.Diagnostics.Process]::Start($startInfo)
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            throw "Shortcut installer failed with exit code $($process.ExitCode)."
        }

        Ensure-Directory $firstRunStartMenuShortcutMarkerPath
        Set-Content -LiteralPath $firstRunStartMenuShortcutMarkerPath -Value (Get-Date).ToString("o") -Encoding UTF8

        if ($ShowNotification -and -not $isRenderPreview) {
            $notifyIcon.ShowBalloonTip(
                2000,
                "Strategic Nexus",
                "Zastupce v nabidce Start byl obnoven.",
                [System.Windows.Forms.ToolTipIcon]::Info)
        }
    } catch {
        Write-TaskBoardError "Install-StartMenuShortcut failed" $_
        if ($ShowNotification -and -not $isRenderPreview) {
            $notifyIcon.ShowBalloonTip(
                3000,
                "Strategic Nexus",
                "Zastupce v nabidce Start se nepodarilo obnovit. Rekni Codexu, at zkontroluje log.",
                [System.Windows.Forms.ToolTipIcon]::Warning)
        }
    }
}

function Ensure-FirstRunStartMenuShortcut {
    if ($isRenderPreview) {
        return
    }

    if (Test-Path -LiteralPath $firstRunStartMenuShortcutMarkerPath) {
        return
    }

    Install-StartMenuShortcut -ShowNotification $false
}

function Show-TaskBoard {
    $form.Show()
    $form.WindowState = [System.Windows.Forms.FormWindowState]::Normal
    $form.Activate()
    Refresh-Board
}

function Hard-Exit {
    Ensure-Directory $stopPath
    Set-Content -LiteralPath $stopPath -Value (Get-Date).ToString("o") -Encoding UTF8
    $script:allowHardExit = $true
    Save-TaskBoardState
    $form.Close()
}

$notifyIcon.Add_DoubleClick({ Show-TaskBoard })
$showMenuItem.Add_Click({ Show-TaskBoard })
$restoreStartMenuShortcutMenuItem.Add_Click({ Install-StartMenuShortcut -ShowNotification $true })
$hardExitMenuItem.Add_Click({ Hard-Exit })
$hardExitButton.Add_Click({ Hard-Exit })
$copySelectionButton.Add_Click({ Copy-SelectedBoardReference })
$tasksModeButton.Add_Click({
    $script:currentBoardMode = "tasks"
    $script:pendingSelectedTag = ""
    $script:selectedBoardTag = ""
    $script:listScrollOffset = 0
    $script:restoreSavedDetailsScroll = $false
    Refresh-Board
    Save-TaskBoardState
})
$reportsModeButton.Add_Click({
    $script:currentBoardMode = "reports"
    $script:pendingSelectedTag = ""
    $script:selectedBoardTag = ""
    $script:listScrollOffset = 0
    $script:restoreSavedDetailsScroll = $false
    Refresh-Board
    Save-TaskBoardState
})
$suggestionsModeButton.Add_Click({
    $script:currentBoardMode = "suggestions"
    $script:pendingSelectedTag = ""
    $script:selectedBoardTag = ""
    $script:listScrollOffset = 0
    $script:restoreSavedDetailsScroll = $false
    Refresh-Board
    Save-TaskBoardState
})
$minimizeButton.Add_Click({
    Save-TaskBoardState
    $form.WindowState = [System.Windows.Forms.FormWindowState]::Minimized
})
$closeButton.Add_Click({ $form.Close() })

$listContextMenu = New-Object System.Windows.Forms.ContextMenuStrip
$copyReferenceMenuItem = New-Object System.Windows.Forms.ToolStripMenuItem "Kopirovat ID polozky"
$copyReferenceMenuItem.Enabled = $false
[void]$listContextMenu.Items.Add($copyReferenceMenuItem)
$list.ContextMenuStrip = $listContextMenu
$copyReferenceMenuItem.Add_Click({ Copy-SelectedBoardReference })

Add-TaskBoardWindowDragHandlers $titleBar
Add-TaskBoardWindowDragHandlers $titleLabel
Add-TaskBoardWindowDragHandlers $header
Add-TaskBoardWindowDragHandlers $projectProgressLabel

$list.Add_DrawColumnHeader({
    param($sender, $event)

    $brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(10, 38, 47))
    $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(43, 82, 80))
    $event.Graphics.FillRectangle($brush, $event.Bounds)
    $event.Graphics.DrawRectangle($pen, $event.Bounds)
    $headerFlags = [System.Windows.Forms.TextFormatFlags](
        [int][System.Windows.Forms.TextFormatFlags]::VerticalCenter -bor
        [int][System.Windows.Forms.TextFormatFlags]::Left)
    [System.Windows.Forms.TextRenderer]::DrawText(
        $event.Graphics,
        $event.Header.Text,
        $list.Font,
        $event.Bounds,
        [System.Drawing.Color]::FromArgb(211, 228, 221),
        $headerFlags)
    $pen.Dispose()
    $brush.Dispose()
})

$list.Add_DrawItem({
    param($sender, $event)
})

$list.Add_DrawSubItem({
    param($sender, $event)
    $selected = $event.Item.Selected
    $backgroundColor = if ($selected) {
        [System.Drawing.Color]::FromArgb(31, 82, 82)
    } else {
        $event.Item.BackColor
    }
    $textColor = if ($selected) {
        [System.Drawing.Color]::FromArgb(246, 238, 207)
    } else {
        $event.Item.ForeColor
    }

    $brush = New-Object System.Drawing.SolidBrush $backgroundColor
    try {
        $event.Graphics.FillRectangle($brush, $event.Bounds)
    } finally {
        $brush.Dispose()
    }

    if ($selected -and $event.ColumnIndex -eq 0) {
        $accentBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(226, 170, 88))
        try {
            $accentBounds = New-Object System.Drawing.Rectangle $event.Bounds.Left, $event.Bounds.Top, 4, $event.Bounds.Height
            $event.Graphics.FillRectangle($accentBrush, $accentBounds)
        } finally {
            $accentBrush.Dispose()
        }
    }

    $textBounds = New-Object System.Drawing.Rectangle (
        $event.Bounds.Left + 4),
        $event.Bounds.Top,
        ([Math]::Max(1, $event.Bounds.Width - 6)),
        $event.Bounds.Height
    $textFlags = [System.Windows.Forms.TextFormatFlags](
        [int][System.Windows.Forms.TextFormatFlags]::VerticalCenter -bor
        [int][System.Windows.Forms.TextFormatFlags]::Left -bor
        [int][System.Windows.Forms.TextFormatFlags]::EndEllipsis)
    [System.Windows.Forms.TextRenderer]::DrawText(
        $event.Graphics,
        $event.SubItem.Text,
        $list.Font,
        $textBounds,
        $textColor,
        $textFlags)
})

$list.Add_MouseWheel({
    param($sender, $event)
    $step = if ($event.Delta -lt 0) { 3 } else { -3 }
    Set-ListScrollOffset ($script:listScrollOffset + $step)
})

$listScrollTrack.Add_MouseDown({
    param($sender, $event)
    if ($event.Button -ne [System.Windows.Forms.MouseButtons]::Left) {
        return
    }

    $relativeY = $event.Y - ($listScrollThumb.Height / 2)
    $travel = [Math]::Max(1, $listScrollTrack.Height - $listScrollThumb.Height)
    $ratio = [Math]::Max(0, [Math]::Min(1, $relativeY / $travel))
    Set-ListScrollOffset ([int]($ratio * (Get-ListScrollMaximum)))
})

$listScrollThumb.Add_MouseDown({
    param($sender, $event)
    if ($event.Button -eq [System.Windows.Forms.MouseButtons]::Left) {
        $script:listScrollDragging = $true
        $script:listScrollDragStartY = [System.Windows.Forms.Control]::MousePosition.Y
        $script:listScrollDragStartOffset = $script:listScrollOffset
    }
})

$listScrollThumb.Add_MouseMove({
    if (-not $script:listScrollDragging) {
        return
    }

    $delta = [System.Windows.Forms.Control]::MousePosition.Y - $script:listScrollDragStartY
    $travel = [Math]::Max(1, $listScrollTrack.Height - $listScrollThumb.Height)
    $maxOffset = Get-ListScrollMaximum
    Set-ListScrollOffset ([int]($script:listScrollDragStartOffset + (($delta / $travel) * $maxOffset)))
})

$listScrollThumb.Add_MouseUp({
    $script:listScrollDragging = $false
})

$list.Add_SelectedIndexChanged({
    if ($script:suppressListSelectionChanged) {
        return
    }

    if ($list.SelectedItems.Count -eq 0) {
        Set-DetailsText (Format-TaskDetails $null)
        Set-CopyButtonVisualState -HasSelection $false
        $copyReferenceMenuItem.Enabled = $false
        Save-TaskBoardState
        return
    }

    $tag = [string]$list.SelectedItems[0].Tag
    $script:selectedBoardTag = $tag
    $item = $script:currentItems | Where-Object {
        $kind = if ($_.board_kind -eq "report") {
            "report"
        } elseif ($_.board_kind -eq "suggestion") {
            "suggestion"
        } else {
            "task"
        }
        "$kind|$($_.id)" -eq $tag
    } | Select-Object -First 1
    Set-DetailsText (Format-TaskDetails $item)
    Set-CopyButtonVisualState -HasSelection $true
    $copyReferenceMenuItem.Enabled = $true
    Save-TaskBoardState
})

$detailsPanel.Add_Resize({
    $script:pendingDetailsScrollOffset = $script:detailsScrollOffset
    $script:restoreSavedDetailsScroll = $script:detailsScrollOffset -gt 0
    Set-DetailsText $script:detailsText
    Save-TaskBoardState
})

$detailsPanel.Add_MouseWheel({
    param($sender, $event)
    Invoke-DetailsMouseWheel $event
})

$details.Add_MouseWheel({
    param($sender, $event)
    Invoke-DetailsMouseWheel $event
})

$detailsScrollTrack.Add_MouseDown({
    param($sender, $event)
    if ($event.Button -ne [System.Windows.Forms.MouseButtons]::Left) {
        return
    }

    $relativeY = $event.Y - ($detailsScrollThumb.Height / 2)
    $travel = [Math]::Max(1, $detailsScrollTrack.Height - $detailsScrollThumb.Height)
    $ratio = [Math]::Max(0, [Math]::Min(1, $relativeY / $travel))
    Set-DetailsScrollOffset ([int]($ratio * (Get-DetailsScrollMaximum)))
})

$detailsScrollThumb.Add_MouseDown({
    param($sender, $event)
    if ($event.Button -eq [System.Windows.Forms.MouseButtons]::Left) {
        $script:detailsScrollDragging = $true
        $script:detailsScrollDragStartY = [System.Windows.Forms.Control]::MousePosition.Y
        $script:detailsScrollDragStartOffset = $script:detailsScrollOffset
    }
})

$detailsScrollThumb.Add_MouseMove({
    if (-not $script:detailsScrollDragging) {
        return
    }

    $delta = [System.Windows.Forms.Control]::MousePosition.Y - $script:detailsScrollDragStartY
    $travel = [Math]::Max(1, $detailsScrollTrack.Height - $detailsScrollThumb.Height)
    $maxOffset = Get-DetailsScrollMaximum
    Set-DetailsScrollOffset ([int]($script:detailsScrollDragStartOffset + (($delta / $travel) * $maxOffset)))
})

$detailsScrollThumb.Add_MouseUp({
    $script:detailsScrollDragging = $false
})

$form.Add_MouseUp({
    Stop-TaskBoardWindowDrag
    $script:detailsScrollDragging = $false
    $script:listScrollDragging = $false
})

$form.Add_Resize({
    Set-TaskBoardLayout
    $script:pendingDetailsScrollOffset = $script:detailsScrollOffset
    $script:restoreSavedDetailsScroll = $script:detailsScrollOffset -gt 0
    Set-DetailsText $script:detailsText
    Save-TaskBoardState
})

$form.Add_Move({
    Save-TaskBoardState
})

function Save-TaskBoardPreview {
    param([string]$PreviewPath)

    $resolvedPreviewPath = Resolve-ProjectPath $PreviewPath
    Ensure-Directory $resolvedPreviewPath

    $form.ShowInTaskbar = $false
    $form.StartPosition = "Manual"
    $form.Left = -32000
    $form.Top = -32000
    $form.Show()
    [System.Windows.Forms.Application]::DoEvents()

    Set-TaskBoardLayout
    Refresh-Board
    $form.PerformLayout()
    $contentPanel.PerformLayout()
    $detailsPanel.PerformLayout()
    $form.Refresh()
    [System.Windows.Forms.Application]::DoEvents()

    $bitmap = New-Object System.Drawing.Bitmap $form.Width, $form.Height
    try {
        $bounds = New-Object System.Drawing.Rectangle 0, 0, $form.Width, $form.Height
        $form.DrawToBitmap($bitmap, $bounds)
        $bitmap.Save($resolvedPreviewPath, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $bitmap.Dispose()
        $form.Hide()
    }
}

if ($isRenderPreview) {
    try {
        Save-TaskBoardPreview $RenderPreviewPath
    } catch {
        Write-TaskBoardError "Save-TaskBoardPreview failed" $_
        throw
    } finally {
        $notifyIcon.Visible = $false
        $notifyIcon.Dispose()
        $form.Dispose()
        if ($singleInstanceMutex) {
            $singleInstanceMutex.ReleaseMutex()
            $singleInstanceMutex.Dispose()
        }
    }
    return
}

Ensure-FirstRunStartMenuShortcut

Ensure-Directory $taskPath
Ensure-Directory $reportPath
Ensure-Directory $suggestionPath
Ensure-Directory $projectProgressPath

function New-BoardFileWatcher {
    param([string]$Path)

    $watcher = New-Object System.IO.FileSystemWatcher
    $watcher.Path = Split-Path -Parent $Path
    $watcher.Filter = Split-Path -Leaf $Path
    $watcher.NotifyFilter = [System.IO.NotifyFilters]'FileName, LastWrite, Size, CreationTime'
    $watcher.EnableRaisingEvents = $true
    return $watcher
}

$taskWatcher = New-BoardFileWatcher $taskPath
$reportWatcher = New-BoardFileWatcher $reportPath
$suggestionWatcher = New-BoardFileWatcher $suggestionPath
$roadmapWatcher = New-BoardFileWatcher $roadmapPath

$onBoardFileChanged = {
    try {
        if (-not $form.IsDisposed) {
            $form.BeginInvoke([Action]{ Refresh-Board }) | Out-Null
        }
    } catch {
        Write-TaskBoardError "Board file change handler failed" $_
    }
}

$taskChangedRegistration = Register-ObjectEvent -InputObject $taskWatcher -EventName Changed -Action $onBoardFileChanged
$taskCreatedRegistration = Register-ObjectEvent -InputObject $taskWatcher -EventName Created -Action $onBoardFileChanged
$taskRenamedRegistration = Register-ObjectEvent -InputObject $taskWatcher -EventName Renamed -Action $onBoardFileChanged
$reportChangedRegistration = Register-ObjectEvent -InputObject $reportWatcher -EventName Changed -Action $onBoardFileChanged
$reportCreatedRegistration = Register-ObjectEvent -InputObject $reportWatcher -EventName Created -Action $onBoardFileChanged
$reportRenamedRegistration = Register-ObjectEvent -InputObject $reportWatcher -EventName Renamed -Action $onBoardFileChanged
$suggestionChangedRegistration = Register-ObjectEvent -InputObject $suggestionWatcher -EventName Changed -Action $onBoardFileChanged
$suggestionCreatedRegistration = Register-ObjectEvent -InputObject $suggestionWatcher -EventName Created -Action $onBoardFileChanged
$suggestionRenamedRegistration = Register-ObjectEvent -InputObject $suggestionWatcher -EventName Renamed -Action $onBoardFileChanged
$roadmapChangedRegistration = Register-ObjectEvent -InputObject $roadmapWatcher -EventName Changed -Action $onBoardFileChanged
$roadmapCreatedRegistration = Register-ObjectEvent -InputObject $roadmapWatcher -EventName Created -Action $onBoardFileChanged
$roadmapRenamedRegistration = Register-ObjectEvent -InputObject $roadmapWatcher -EventName Renamed -Action $onBoardFileChanged

$form.Add_Shown({
    Enable-DarkTitleBar $form
    Enable-DarkListViewTheme $list
    [StrategicNexusTaskBoardNative]::ShowWindow($form.Handle, 5) | Out-Null
    if ($script:pendingWindowState -eq "Maximized") {
        $form.WindowState = [System.Windows.Forms.FormWindowState]::Maximized
        $script:pendingWindowState = ""
    }
    Refresh-Board
})
$form.Add_FormClosing({
    param($sender, $event)

    Save-TaskBoardState
    if (-not $script:allowHardExit -and $event.CloseReason -eq [System.Windows.Forms.CloseReason]::UserClosing) {
        $event.Cancel = $true
        $form.ShowInTaskbar = $true
        $form.WindowState = [System.Windows.Forms.FormWindowState]::Minimized
        $notifyIcon.ShowBalloonTip(
            2000,
            "Strategic Nexus",
            "Task board bezi dal na taskbaru i v oznamovaci oblasti.",
            [System.Windows.Forms.ToolTipIcon]::Info)
    }
})
$form.Add_FormClosed({
    Unregister-Event -SourceIdentifier $taskChangedRegistration.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $taskCreatedRegistration.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $taskRenamedRegistration.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $reportChangedRegistration.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $reportCreatedRegistration.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $reportRenamedRegistration.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $suggestionChangedRegistration.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $suggestionCreatedRegistration.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $suggestionRenamedRegistration.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $roadmapChangedRegistration.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $roadmapCreatedRegistration.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $roadmapRenamedRegistration.Name -ErrorAction SilentlyContinue
    $taskWatcher.Dispose()
    $reportWatcher.Dispose()
    $suggestionWatcher.Dispose()
    $roadmapWatcher.Dispose()
    $notifyIcon.Visible = $false
    $notifyIcon.Dispose()
    if ($script:currentTaskBoardIcon) {
        $script:currentTaskBoardIcon.Dispose()
    }
    if ($singleInstanceMutex) {
        $singleInstanceMutex.ReleaseMutex()
        $singleInstanceMutex.Dispose()
    }
})

[void]$form.ShowDialog()
