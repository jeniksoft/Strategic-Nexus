$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$findings = New-Object System.Collections.Generic.List[string]

function Add-FindingBlock {
    param(
        [string]$Title,
        [string[]]$Lines
    )

    if ($Lines.Count -gt 0) {
        $findings.Add("[$Title]") | Out-Null
        foreach ($line in $Lines) {
            $findings.Add($line) | Out-Null
        }
    }
}

function Search-Pattern {
    param(
        [string]$Title,
        [string]$Pattern,
        [string[]]$Paths
    )

    $output = & rg -n -i --glob "!tools/run_safety_audit.ps1" $Pattern @Paths 2>$null
    if ($LASTEXITCODE -eq 0) {
        Add-FindingBlock $Title $output
    } elseif ($LASTEXITCODE -ne 1) {
        throw "Safety audit search failed for $Title."
    }
}

Push-Location $repoRoot
try {
    if (-not (Test-Path (Join-Path $repoRoot ".git"))) {
        Add-FindingBlock "local backup" @("Local Git working copy is missing .git metadata.")
    }

    $remoteUrl = git remote get-url origin 2>$null
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($remoteUrl)) {
        Add-FindingBlock "local backup" @("Local working copy has no origin remote.")
    }

    $trackedForbidden = git ls-files |
        Select-String -Pattern '(^|/)tools/runtime_research/|docs/(RUNTIME_|NATIVE_BRIDGE|STELLARIS_EXE_ARCHITECTURE|OBSERVE_COMMAND_RESEARCH|POC_A_TEST_INSTRUCTIONS|POC_A_RESULTS)|resources/strategic_nexus_console' |
        ForEach-Object { $_.Line }
    Add-FindingBlock "forbidden tracked files" $trackedForbidden

    Search-Pattern "legacy runtime integration terms" '\b(native bridge|native hook|runtime injection|reverse engineering|process memory|rva|frida|x64dbg|disassembl|callsite|dll injection|process injection|stellaris exe|runtime_research)\b' @("docs", "src", "tools", "resources", "mod", ".github")

    Search-Pattern "legacy daemon delivery terms" '\b(host-side injection|host-only daemon injection|daemon injection|bridge injection|native injection|runtime state injection|injection systems|failed bridge injection|inject payload|inject daemon state|after injection|before native bridge|native bridge work|automatic host-side injection|host-injected)\b' @("docs", "src", "tools", "resources", "mod", ".github")

    Search-Pattern "legacy console transport artifacts" '\b(strategic_nexus_console|run strategic_nexus|legacy console|console observe)\b' @("docs", "src", "tools", "resources", "mod", ".github")

    $requiredSafetyRules = @(
        @{ Path = "docs/SAFETY_SCOPE.md"; Pattern = "Only legal, GitHub-rule-compliant, OpenAI-rule-compliant, and safe-use-compliant material may be committed or pushed to GitHub." },
        @{ Path = "docs/SAFETY_SCOPE.md"; Pattern = "Treat every GitHub commit as potentially public" },
        @{ Path = "docs/SAFETY_SCOPE.md"; Pattern = "Codex must maintain a local working copy of the GitHub repository as a backup" },
        @{ Path = "docs/MASTER_ARCHITECTURE_INDEX.md"; Pattern = "Treat GitHub content as potentially public" },
        @{ Path = "docs/ENGINEERING_RULES.md"; Pattern = "Only legal, GitHub-rule-compliant, OpenAI-rule-compliant, and safe-use-compliant material may be committed or pushed to GitHub." }
    )

    foreach ($rule in $requiredSafetyRules) {
        $matched = Select-String -Path $rule.Path -Pattern ([regex]::Escape($rule.Pattern)) -Quiet
        if (-not $matched) {
            Add-FindingBlock "required safety rule missing" @("$($rule.Path): $($rule.Pattern)")
        }
    }

    $gh = Get-Command gh -ErrorAction SilentlyContinue
    if ($gh) {
        $visibility = gh repo view jeniksoft/Strategic-Nexus --json visibility --jq ".visibility"
        if ($visibility -ne "PRIVATE") {
            Add-FindingBlock "repository visibility" @("Expected PRIVATE, got $visibility")
        }
    }

    if ($findings.Count -gt 0) {
        foreach ($finding in $findings) {
            Write-Host $finding
        }
        exit 1
    }

    Write-Host "Safety audit passed with no findings."
}
finally {
    Pop-Location
}
