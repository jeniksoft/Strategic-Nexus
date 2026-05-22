param(
    [string]$Title = "Strategic Nexus - user task",
    [string]$Severity = "normal",
    [string]$Task = "No task provided.",
    [string]$Why = "No reason provided.",
    [string]$Instructions = "No instructions provided.",
    [string]$AckLogPath = "dist/private_reports/user_task_ack.log"
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$ackPath = if ([System.IO.Path]::IsPathRooted($AckLogPath)) {
    $AckLogPath
} else {
    Join-Path $repoRoot $AckLogPath
}

$ackDirectory = Split-Path -Parent $ackPath
if ($ackDirectory -and -not (Test-Path -LiteralPath $ackDirectory)) {
    New-Item -ItemType Directory -Force -Path $ackDirectory | Out-Null
}

$form = New-Object System.Windows.Forms.Form
$form.Text = $Title
$form.Width = 760
$form.Height = 520
$form.StartPosition = "CenterScreen"
$form.TopMost = $true
$form.ShowInTaskbar = $true

$severityColor = switch ($Severity.ToLowerInvariant()) {
    "critical" { [System.Drawing.Color]::FromArgb(120, 20, 20) }
    "high" { [System.Drawing.Color]::FromArgb(120, 60, 0) }
    "normal" { [System.Drawing.Color]::FromArgb(25, 70, 120) }
    "low" { [System.Drawing.Color]::FromArgb(60, 80, 60) }
    default { [System.Drawing.Color]::FromArgb(25, 70, 120) }
}

$header = New-Object System.Windows.Forms.Label
$header.Dock = "Top"
$header.Height = 52
$header.TextAlign = "MiddleLeft"
$header.Padding = New-Object System.Windows.Forms.Padding(14, 0, 14, 0)
$header.Font = New-Object System.Drawing.Font("Segoe UI", 13, [System.Drawing.FontStyle]::Bold)
$header.ForeColor = [System.Drawing.Color]::White
$header.BackColor = $severityColor
$header.Text = "$Title  [$Severity]"
$form.Controls.Add($header)

$textBox = New-Object System.Windows.Forms.TextBox
$textBox.Multiline = $true
$textBox.ReadOnly = $true
$textBox.ScrollBars = "Vertical"
$textBox.Dock = "Fill"
$textBox.Font = New-Object System.Drawing.Font("Consolas", 11)
$textBox.Text = @"
TASK
$Task

WHY
$Why

INSTRUCTIONS
$Instructions

Close this window when you have noticed the task.
Codex will see the acknowledgement in:
$ackPath
"@
$form.Controls.Add($textBox)

$buttonPanel = New-Object System.Windows.Forms.Panel
$buttonPanel.Dock = "Bottom"
$buttonPanel.Height = 54
$form.Controls.Add($buttonPanel)

$closeButton = New-Object System.Windows.Forms.Button
$closeButton.Text = "Acknowledged"
$closeButton.Width = 140
$closeButton.Height = 32
$closeButton.Left = $form.ClientSize.Width - 160
$closeButton.Top = 11
$closeButton.Anchor = "Right,Top"
$closeButton.Add_Click({ $form.Close() })
$buttonPanel.Controls.Add($closeButton)

$form.Add_FormClosed({
    $timestamp = (Get-Date).ToString("o")
    $entry = "$timestamp`t$Severity`t$Title`t$Task"
    Add-Content -LiteralPath $ackPath -Value $entry
})

[void]$form.ShowDialog()
