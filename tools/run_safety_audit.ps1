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

    $rg = Get-Command rg -ErrorAction SilentlyContinue
    if ($rg) {
        $output = & rg -n -i --glob "!tools/run_safety_audit.ps1" $Pattern @Paths 2>$null
        if ($LASTEXITCODE -eq 0) {
            Add-FindingBlock $Title $output
        } elseif ($LASTEXITCODE -ne 1) {
            throw "Safety audit search failed for $Title."
        }
        return
    }

    $pathPrefixes = $Paths | ForEach-Object { $_ -replace '\\', '/' }
    $candidateFiles = git ls-files --cached --others --exclude-standard
    if ($LASTEXITCODE -ne 0) {
        throw "Safety audit fallback file discovery failed for $Title."
    }

    $resolvedFiles = foreach ($file in $candidateFiles) {
        $normalized = $file -replace '\\', '/'
        $inScope = $false
        foreach ($prefix in $pathPrefixes) {
            if ($normalized -eq $prefix -or $normalized.StartsWith("$prefix/")) {
                $inScope = $true
                break
            }
        }

        $binaryExtensions = @(".7z", ".dll", ".exe", ".gif", ".ico", ".jpeg", ".jpg", ".lib", ".obj", ".pdb", ".png", ".pyc", ".zip")
        if ($inScope -and ($binaryExtensions -notcontains [System.IO.Path]::GetExtension($file).ToLowerInvariant())) {
            Get-Item -LiteralPath $file -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -ne $PSCommandPath }
        }
    }

    $output = $resolvedFiles |
        Select-String -Pattern $Pattern -CaseSensitive:$false |
        ForEach-Object {
            $relativePath = (Resolve-Path -Path $_.Path -Relative) -replace '^\.[\\/]', ''
            "$($relativePath):$($_.LineNumber):$($_.Line)"
        }

    Add-FindingBlock $Title $output
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
        try {
            $visibility = gh repo view jeniksoft/Strategic-Nexus --json visibility --jq ".visibility" 2>$null
            if (-not [string]::IsNullOrWhiteSpace($visibility) -and $visibility -ne "PRIVATE" -and $visibility -ne "PUBLIC") {
                Add-FindingBlock "repository visibility" @("Unexpected repository visibility: $visibility")
            }
        } catch {
            # In sandboxed/offline environments, gh may be present but unusable (config path ACLs, no network, etc.).
            # Treat this check as best-effort so the rest of the audit can still run.
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
