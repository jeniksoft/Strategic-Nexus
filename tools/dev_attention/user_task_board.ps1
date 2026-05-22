param(
    [string]$TaskFilePath = "dist/private_reports/user_tasks.json",
    [string]$StopFilePath = "dist/private_reports/user_task_board.stop",
    [string]$RenderPreviewPath = ""
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
}
"@

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
$stopPath = Resolve-ProjectPath $StopFilePath
$errorLogPath = Resolve-ProjectPath "dist/private_reports/user_task_board_error.log"
$calmIconPath = Join-Path $repoRoot "tools/dev_attention/assets/task_board_calm.ico"
$alertIconPath = Join-Path $repoRoot "tools/dev_attention/assets/task_board_alert.ico"
$taskPanelTexturePath = Join-Path $repoRoot "tools/dev_attention/assets/task_panel_texture_v3.png"
$detailPanelTexturePath = Join-Path $repoRoot "tools/dev_attention/assets/detail_panel_texture_v3.png"
$isRenderPreview = -not [string]::IsNullOrWhiteSpace($RenderPreviewPath)
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

[System.Windows.Forms.Application]::SetUnhandledExceptionMode([System.Windows.Forms.UnhandledExceptionMode]::CatchException)
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

function New-PanelTexture {
    param(
        [string]$Path,
        [System.Drawing.Color]$BaseColor,
        [System.Drawing.Color]$AccentColor
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

New-PanelTexture `
    -Path $taskPanelTexturePath `
    -BaseColor ([System.Drawing.Color]::FromArgb(6, 24, 31)) `
    -AccentColor ([System.Drawing.Color]::FromArgb(39, 137, 130))
New-PanelTexture `
    -Path $detailPanelTexturePath `
    -BaseColor ([System.Drawing.Color]::FromArgb(7, 11, 18)) `
    -AccentColor ([System.Drawing.Color]::FromArgb(43, 91, 123))

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

        return @($json.tasks | Where-Object { $_.status -ne "done" })
    } catch {
        Write-TaskBoardError "Read-Tasks failed" $_
        return @([pscustomobject]@{
            id = "task_file_error"
            title = "Task file read error"
            severity = "high"
            task = "Codex task board could not parse the task file."
            why = $_.Exception.Message
            instructions = "Tell Codex to inspect dist/private_reports/user_tasks.json."
            status = "open"
        })
    }
}

$form = New-Object System.Windows.Forms.Form
$form.Text = "Strategic Nexus - tvoje aktualni ukoly"
$form.Width = 860
$form.Height = 620
$form.StartPosition = "CenterScreen"
$form.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::None
$form.ShowInTaskbar = $true
$form.Icon = New-Object System.Drawing.Icon $calmIconPath
$form.BackColor = [System.Drawing.Color]::FromArgb(164, 122, 69)
$form.ForeColor = [System.Drawing.Color]::FromArgb(211, 228, 221)
$script:allowHardExit = $false
$script:draggingWindow = $false
$script:dragOffset = [System.Drawing.Point]::Empty

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

$contentPanel = New-Object System.Windows.Forms.Panel
$contentPanel.Left = 0
$contentPanel.Top = $titleBar.Height + $header.Height
$contentPanel.Width = $form.ClientSize.Width
$contentPanel.Height = $form.ClientSize.Height - $titleBar.Height - $header.Height - 46
$contentPanel.Anchor = [System.Windows.Forms.AnchorStyles](
    [int][System.Windows.Forms.AnchorStyles]::Top -bor
    [int][System.Windows.Forms.AnchorStyles]::Bottom -bor
    [int][System.Windows.Forms.AnchorStyles]::Left -bor
    [int][System.Windows.Forms.AnchorStyles]::Right)
$contentPanel.BackColor = [System.Drawing.Color]::FromArgb(5, 10, 15)
$form.Controls.Add($contentPanel)

$list = New-Object System.Windows.Forms.ListView
$list.Dock = "Left"
$list.Width = 300
$list.View = "Details"
$list.FullRowSelect = $true
$list.MultiSelect = $false
$list.HideSelection = $false
$list.BorderStyle = "None"
$list.BackColor = [System.Drawing.Color]::FromArgb(7, 22, 29)
$list.BackgroundImage = [System.Drawing.Image]::FromFile($taskPanelTexturePath)
$list.BackgroundImageTiled = $true
$list.ForeColor = [System.Drawing.Color]::FromArgb(211, 232, 226)
$list.Font = New-Object System.Drawing.Font("Bahnschrift", 9.5)
$list.OwnerDraw = $true
$list.Columns.Add("Priorita", 80) | Out-Null
$list.Columns.Add("Ukol", 220) | Out-Null
$contentPanel.Controls.Add($list)

$separator = New-Object System.Windows.Forms.Panel
$separator.Dock = "Left"
$separator.Width = 1
$separator.BackColor = [System.Drawing.Color]::FromArgb(17, 40, 43)
$contentPanel.Controls.Add($separator)

$detailsPanel = New-Object System.Windows.Forms.Panel
$detailsPanel.Left = $list.Width + $separator.Width
$detailsPanel.Top = 0
$detailsPanel.Width = $form.ClientSize.Width - $list.Width - $separator.Width
$detailsPanel.Height = $contentPanel.ClientSize.Height
$detailsPanel.Anchor = [System.Windows.Forms.AnchorStyles](
    [int][System.Windows.Forms.AnchorStyles]::Top -bor
    [int][System.Windows.Forms.AnchorStyles]::Bottom -bor
    [int][System.Windows.Forms.AnchorStyles]::Left -bor
    [int][System.Windows.Forms.AnchorStyles]::Right)
$detailsPanel.AutoScroll = $true
$detailsPanel.BackgroundImage = [System.Drawing.Image]::FromFile($detailPanelTexturePath)
$detailsPanel.BackgroundImageLayout = "Tile"
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

$script:detailsText = ""

function Set-TaskBoardLayout {
    $frame = 2
    $titleBar.SetBounds(0, 0, $form.ClientSize.Width, 30)
    $closeButton.SetBounds($titleBar.ClientSize.Width - $closeButton.Width, 0, $closeButton.Width, $titleBar.Height)
    $minimizeButton.SetBounds($closeButton.Left - $minimizeButton.Width, 0, $minimizeButton.Width, $titleBar.Height)
    $titleLabel.Width = [Math]::Max(80, $minimizeButton.Left - $titleLabel.Left - 8)
    $header.SetBounds($frame, $titleBar.Bottom + $frame, $form.ClientSize.Width - ($frame * 2), 54)
    $buttonPanel.SetBounds($frame, $form.ClientSize.Height - $buttonPanel.Height - $frame, $form.ClientSize.Width - ($frame * 2), $buttonPanel.Height)
    $contentPanel.SetBounds($frame, $header.Bottom, $form.ClientSize.Width - ($frame * 2), $buttonPanel.Top - $header.Bottom)
    $detailsPanel.SetBounds(
        $list.Width + $separator.Width,
        0,
        [Math]::Max(260, $contentPanel.ClientSize.Width - $list.Width - $separator.Width),
        $contentPanel.ClientSize.Height)
    $titleBar.BringToFront()
    $header.BringToFront()
    $buttonPanel.BringToFront()
}

$script:currentTasks = @()

function Format-TaskDetails {
    param($Task)

    if (-not $Task) {
        return "Zadne aktualni ukoly od Codexu.`r`n`r`nKdyz budu potrebovat test nebo tvoji akci, objevi se tady."
    }

    return @"
NAZEV
$($Task.title)

PRIORITA
$($Task.severity)

CO POTREBUJU
$($Task.task)

PROC
$($Task.why)

POSTUP
$($Task.instructions)

ID
$($Task.id)
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
        $availableWidth = [int][Math]::Max(260, $panelWidth - 42)
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

        $headingNames = @("NAZEV", "PRIORITA", "CO POTREBUJU", "PROC", "POSTUP", "ID")
        $lines = $Text -split "`r?`n", -1
        $y = 14
        foreach ($line in $lines) {
            $isHeading = $headingNames -contains $line.Trim()
            $label = New-Object System.Windows.Forms.Label
            $label.AutoSize = $false
            $label.Left = 14
            $label.Top = $y
            $label.Width = [Math]::Max(120, $details.Width - 28)
            $label.BackColor = [System.Drawing.Color]::Transparent
            if ($isHeading) {
                $label.Font = $headingFont
                $label.ForeColor = $headingColor
                $label.Height = 20
            } elseif ([string]::IsNullOrWhiteSpace($line)) {
                $label.Font = $bodyFont
                $label.ForeColor = $mutedColor
                $label.Height = 10
            } else {
                $label.Font = $bodyFont
                $label.ForeColor = $bodyColor
                $measureSize = New-Object System.Drawing.Size($label.Width, 4000)
                $measureFlags = [System.Windows.Forms.TextFormatFlags](
                    [int][System.Windows.Forms.TextFormatFlags]::WordBreak -bor
                    [int][System.Windows.Forms.TextFormatFlags]::NoPadding)
                $preferred = [System.Windows.Forms.TextRenderer]::MeasureText(
                    $line,
                    $label.Font,
                    $measureSize,
                    $measureFlags)
                $label.Height = [Math]::Max(22, $preferred.Height + 4)
            }
            $label.Text = $line
            $details.Controls.Add($label)
            $y += $label.Height
        }
        $details.Height = [Math]::Max($minimumHeight, $y + 14)
        $details.ResumeLayout()
    } catch {
        Write-TaskBoardError "Set-DetailsText failed" $_
    }
}

function Refresh-Board {
    try {
    $selectedId = $null
    if ($list.SelectedItems.Count -gt 0) {
        $selectedId = $list.SelectedItems[0].Tag
    }

    $script:currentTasks = Read-Tasks
    $list.Items.Clear()

    foreach ($task in $script:currentTasks) {
        $severity = if ($task.severity) { [string]$task.severity } else { "normal" }
        $title = if ($task.title) { [string]$task.title } else { [string]$task.id }
        $item = New-Object System.Windows.Forms.ListViewItem $severity
        [void]$item.SubItems.Add($title)
        $item.Tag = [string]$task.id
        $item.BackColor = switch ($severity.ToLowerInvariant()) {
            "critical" { [System.Drawing.Color]::FromArgb(78, 48, 34) }
            "high" { [System.Drawing.Color]::FromArgb(64, 49, 31) }
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

    $count = $script:currentTasks.Count
    $header.Text = if ($count -eq 0) {
        "Strategic Nexus - zadne aktualni ukoly"
    } else {
        "Strategic Nexus - aktualni ukoly: $count"
    }
    $form.Text = if ($count -eq 0) {
        "Strategic Nexus"
    } else {
        "Strategic Nexus ($count)"
    }

    $hasTasks = $count -gt 0
    $currentIconPath = if ($hasTasks) { $alertIconPath } else { $calmIconPath }
    $currentIcon = New-Object System.Drawing.Icon $currentIconPath
    $form.Icon = $currentIcon
    $notifyIcon.Icon = $currentIcon
    $notifyIcon.Text = if ($count -eq 0) { "Strategic Nexus - zadne ukoly" } else { "Strategic Nexus - ukoly: $count" }
    $header.BackColor = [System.Drawing.Color]::FromArgb(12, 58, 54)
    $header.ForeColor = if ($hasTasks) {
        [System.Drawing.Color]::FromArgb(224, 205, 166)
    } else {
        [System.Drawing.Color]::FromArgb(216, 232, 225)
    }

    if ($list.SelectedItems.Count -eq 0 -and $list.Items.Count -gt 0) {
        $list.Items[0].Selected = $true
        Set-DetailsText (Format-TaskDetails $script:currentTasks[0])
    } elseif ($list.SelectedItems.Count -eq 0) {
        Set-DetailsText (Format-TaskDetails $null)
    }
    } catch {
        Write-TaskBoardError "Refresh-Board failed" $_
    }
}

$notifyIcon = New-Object System.Windows.Forms.NotifyIcon
$notifyIcon.Text = "Strategic Nexus"
$notifyIcon.Icon = New-Object System.Drawing.Icon $calmIconPath
$notifyIcon.Visible = -not $isRenderPreview

$trayMenu = New-Object System.Windows.Forms.ContextMenuStrip
$showMenuItem = New-Object System.Windows.Forms.ToolStripMenuItem "Zobrazit ukoly"
$hardExitMenuItem = New-Object System.Windows.Forms.ToolStripMenuItem "Tvrde zavrit"
[void]$trayMenu.Items.Add($showMenuItem)
[void]$trayMenu.Items.Add($hardExitMenuItem)
$notifyIcon.ContextMenuStrip = $trayMenu

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
    $form.Close()
}

$notifyIcon.Add_DoubleClick({ Show-TaskBoard })
$showMenuItem.Add_Click({ Show-TaskBoard })
$hardExitMenuItem.Add_Click({ Hard-Exit })
$hardExitButton.Add_Click({ Hard-Exit })
$minimizeButton.Add_Click({ $form.WindowState = [System.Windows.Forms.FormWindowState]::Minimized })
$closeButton.Add_Click({ $form.Close() })

$titleBar.Add_MouseDown({
    param($sender, $event)

    if ($event.Button -eq [System.Windows.Forms.MouseButtons]::Left) {
        $script:draggingWindow = $true
        $script:dragOffset = New-Object System.Drawing.Point $event.X, $event.Y
    }
})

$titleBar.Add_MouseMove({
    if ($script:draggingWindow) {
        $mouse = [System.Windows.Forms.Control]::MousePosition
        $form.Left = $mouse.X - $script:dragOffset.X
        $form.Top = $mouse.Y - $script:dragOffset.Y
    }
})

$titleBar.Add_MouseUp({
    $script:draggingWindow = $false
})

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
    $selected = ($event.ItemState -band [System.Windows.Forms.ListViewItemStates]::Selected) -ne 0
    $backgroundColor = if ($selected) {
        [System.Drawing.Color]::FromArgb(22, 55, 60)
    } else {
        $event.Item.BackColor
    }
    $textColor = if ($selected) {
        [System.Drawing.Color]::FromArgb(218, 233, 226)
    } else {
        $event.Item.ForeColor
    }

    $brush = New-Object System.Drawing.SolidBrush $backgroundColor
    try {
        $event.Graphics.FillRectangle($brush, $event.Bounds)
    } finally {
        $brush.Dispose()
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

$list.Add_SelectedIndexChanged({
    if ($list.SelectedItems.Count -eq 0) {
        Set-DetailsText (Format-TaskDetails $null)
        return
    }

    $id = [string]$list.SelectedItems[0].Tag
    $task = $script:currentTasks | Where-Object { $_.id -eq $id } | Select-Object -First 1
    Set-DetailsText (Format-TaskDetails $task)
})

$detailsPanel.Add_Resize({
    Set-DetailsText $script:detailsText
})

$form.Add_Resize({
    Set-TaskBoardLayout
    Set-DetailsText $script:detailsText
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

Ensure-Directory $taskPath
$watcher = New-Object System.IO.FileSystemWatcher
$watcher.Path = Split-Path -Parent $taskPath
$watcher.Filter = Split-Path -Leaf $taskPath
$watcher.NotifyFilter = [System.IO.NotifyFilters]'FileName, LastWrite, Size, CreationTime'
$watcher.EnableRaisingEvents = $true

$onTaskFileChanged = {
    try {
        if (-not $form.IsDisposed) {
            $form.BeginInvoke([Action]{ Refresh-Board }) | Out-Null
        }
    } catch {
        Write-TaskBoardError "Task file change handler failed" $_
    }
}

$changedRegistration = Register-ObjectEvent -InputObject $watcher -EventName Changed -Action $onTaskFileChanged
$createdRegistration = Register-ObjectEvent -InputObject $watcher -EventName Created -Action $onTaskFileChanged
$renamedRegistration = Register-ObjectEvent -InputObject $watcher -EventName Renamed -Action $onTaskFileChanged

$form.Add_Shown({
    Enable-DarkTitleBar $form
    [StrategicNexusTaskBoardNative]::ShowWindow($form.Handle, 5) | Out-Null
    Refresh-Board
})
$form.Add_FormClosing({
    param($sender, $event)

    if (-not $script:allowHardExit -and $event.CloseReason -eq [System.Windows.Forms.CloseReason]::UserClosing) {
        $event.Cancel = $true
        $form.Hide()
        $notifyIcon.ShowBalloonTip(
            2000,
            "Strategic Nexus",
            "Task board bezi dal v oznamovaci oblasti.",
            [System.Windows.Forms.ToolTipIcon]::Info)
    }
})
$form.Add_FormClosed({
    Unregister-Event -SourceIdentifier $changedRegistration.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $createdRegistration.Name -ErrorAction SilentlyContinue
    Unregister-Event -SourceIdentifier $renamedRegistration.Name -ErrorAction SilentlyContinue
    $watcher.Dispose()
    $notifyIcon.Visible = $false
    $notifyIcon.Dispose()
    if ($singleInstanceMutex) {
        $singleInstanceMutex.ReleaseMutex()
        $singleInstanceMutex.Dispose()
    }
})

[void]$form.ShowDialog()
