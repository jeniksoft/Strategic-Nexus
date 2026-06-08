param(
    [ValidateSet("set", "clear", "status")]
    [string]$Mode = "status",
    [ValidateSet("desktop", "mobile")]
    [string]$Chat = "desktop",
    [string]$Reason = "foreground work",
    [string]$RunId = "",
    [int]$TtlMinutes = 120,
    [string]$RepoRoot = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
} else {
    $RepoRoot = Resolve-Path $RepoRoot
}

function Resolve-ProjectPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $RepoRoot $Path
}

function Ensure-ParentDirectory {
    param([string]$Path)

    $parent = Split-Path -Parent $Path
    if ($parent -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
}

function Read-JsonFile {
    param([string]$Path)

    try {
        if (Test-Path -LiteralPath $Path) {
            return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -ErrorAction Stop
        }
    } catch {
    }

    return $null
}

$lockPath = Resolve-ProjectPath -Path (".codex_local/foreground_{0}_active.json" -f $Chat)

if ($Mode -eq "clear") {
    if (Test-Path -LiteralPath $lockPath) {
        Remove-Item -LiteralPath $lockPath -Force
    }

    Write-Host ("foreground_lock_active=false")
    Write-Host ("foreground_lock_chat=" + $Chat)
    Write-Host ("foreground_lock_path=" + $lockPath)
    return
}

if ($Mode -eq "set") {
    $now = [DateTimeOffset]::Now
    $existing = Read-JsonFile -Path $lockPath
    $startedAt = $now.ToString("o")
    if ($null -ne $existing -and $existing.active -eq $true -and $existing.started_at) {
        $startedAt = [string]$existing.started_at
    }

    $payload = [ordered]@{
        schema_version = 1
        active = $true
        chat = $Chat
        reason = $Reason
        run_id = $RunId
        started_at = $startedAt
        updated_at = $now.ToString("o")
        expires_at = $now.AddMinutes($TtlMinutes).ToString("o")
        ttl_minutes = $TtlMinutes
    }

    Ensure-ParentDirectory -Path $lockPath
    $payload | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $lockPath -Encoding UTF8
}

$current = Read-JsonFile -Path $lockPath
$active = ($null -ne $current -and $current.active -eq $true)

Write-Host ("foreground_lock_active=" + $active.ToString().ToLowerInvariant())
Write-Host ("foreground_lock_chat=" + $Chat)
Write-Host ("foreground_lock_path=" + $lockPath)
if ($null -ne $current) {
    Write-Host ("foreground_lock_reason=" + [string]$current.reason)
    Write-Host ("foreground_lock_expires_at=" + [string]$current.expires_at)
}
