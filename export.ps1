<#
    Usage:
      . .\export.ps1      - dot-source into the current session (recommended)

    Set $env:TUYAOPEN_EXPORT_VERBOSE = "1" before running for full diagnostics.
    Set $env:TUYAOPEN_EXPORT_IDE = "1" when invoked by TuyaOpen IDE (line-based progress, stderr logs).
    Set $env:TUYAOPEN_CN_DOWNLOAD = "1" or "0" to force CN / overseas uv download mirrors (default: auto via timezone).

    This script:
      * locates the TuyaOpen project root (this script's directory),
      * ensures `uv` from <root>\.tools\uv\<version>\ (uv-manifest.env),
      * installs Python 3.12.13 via uv into <root>\.tools\python\3.12.13\,
      * creates <root>\.venv and runs `uv sync --frozen` (pyproject.toml + uv.lock),
      * sets OPEN_SDK_ROOT / OPEN_SDK_UV / OPEN_SDK_PYTHON (.venv) / OPEN_SDK_PIP / OPEN_SDK_MAKE_BIN / OPEN_SDK_MAKE,
      * runs tos.py prepare (host tools; GNU Make on Windows when missing),
      * sets TUYAOPEN_ENV_ACTIVE=1 and (TuyaOpen) prompt; registers tos.py / deactivate; resets .cache each source (re-source safe),
      * on Windows registers PowerShell tab completion for tos.py (click-pwsh + Click 8).
#>

#Requires -Version 5.1

# ---------------------------------------------------------------------------
# Session state
# ---------------------------------------------------------------------------
$script:TuyaOpenVerbose = [bool]$env:TUYAOPEN_EXPORT_VERBOSE
$script:TuyaOpenIdeHost = ($env:TUYAOPEN_EXPORT_IDE -eq '1')
$script:TuyaOpenDotSourced = $false
if ($script:TuyaOpenIdeHost) {
    $env:NO_COLOR = '1'
    $env:FORCE_COLOR = '0'
    $env:CLICOLOR = '0'
}

$script:TuyaUvToolName               = 'uv'
$script:PythonVersion            = '3.12.13'
$script:VenvMarker           = '.tuyaopen-uv'
$script:PromptPrefix          = '(TuyaOpen) '
$script:UvDownloadAttempts = 2
$script:UvVersion = '0.11.18'
$script:TuyaUvDefaultBaseUrl = 'https://github.com/astral-sh/uv/releases/download'
$script:TuyaUvAstralBaseUrl   = 'https://releases.astral.sh/github/uv/releases/download'
$script:TuyaUvBinNames       = @('uv.exe', 'uvx.exe', 'uvw.exe')
$script:TuyaMakeToolName     = 'make'
$script:TuyaMakeVersion      = '4.4.1'
$script:TuyaAliyunPypiIndex  = 'https://mirrors.aliyun.com/pypi/simple/'
# CN mirror for uv-managed Python (python-build-standalone). Replaces the
# default GitHub base for `uv python install` via UV_PYTHON_INSTALL_MIRROR.
$script:TuyaPythonInstallMirrorCn = 'https://registry.npmmirror.com/-/binary/python-build-standalone'
$script:TuyaCnTzOffsetTarget     = 480
$script:TuyaCnTzOffsetTolerance  = 30
$script:TuyaUseCnDownload          = $false
$script:TuyaRegionMsg              = ''

# Windows release triple -> uv zip artifact (MSVC builds only).
$script:TuyaUvWindowsArtifacts = @{
    'i686-pc-windows-msvc'    = 'uv-i686-pc-windows-msvc.zip'
    'x86_64-pc-windows-msvc'  = 'uv-x86_64-pc-windows-msvc.zip'
    'aarch64-pc-windows-msvc' = 'uv-aarch64-pc-windows-msvc.zip'
}

# ---------------------------------------------------------------------------
# Logging, errors, IO
# ---------------------------------------------------------------------------
function Write-TuyaOpenInfo {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Message)
    $text = $Message -join ' '
    if ($script:TuyaOpenIdeHost) {
        [Console]::Error.WriteLine($text)
    } else {
        Write-Host $text
    }
}

function Write-TuyaOpenDebug {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Message)
    if ($script:TuyaOpenVerbose) {
        $text = $Message -join ' '
        if ($script:TuyaOpenIdeHost) {
            [Console]::Error.WriteLine($text)
        } else {
            Write-Host $text
        }
    }
}

function Write-TuyaOpenStage {
    param([Parameter(Mandatory)][string]$StageId)
    if (-not $script:TuyaOpenIdeHost) { return }
    Write-TuyaOpenInfo "[TuyaOpen] Stage: $StageId"
}

function Get-TuyaUtcOffsetMinutes {
    [int][System.TimeZoneInfo]::Local.GetUtcOffset([datetime]::Now).TotalMinutes
}

function Test-TuyaCnTzRange {
    param([int]$Offset)
    $min = $script:TuyaCnTzOffsetTarget - $script:TuyaCnTzOffsetTolerance
    $max = $script:TuyaCnTzOffsetTarget + $script:TuyaCnTzOffsetTolerance
    return ($Offset -ge $min -and $Offset -le $max)
}

function Test-TuyaMainlandChina {
    if ($env:TUYAOPEN_CN_DOWNLOAD -eq '1') { return $true }
    if ($env:TUYAOPEN_CN_DOWNLOAD -eq '0') { return $false }
    return (Test-TuyaCnTzRange -Offset (Get-TuyaUtcOffsetMinutes))
}

function Invoke-TuyaRegionDetect {
    Write-TuyaOpenStage -StageId 'region'
    $override = ''
    $offset = $null
    if ($env:TUYAOPEN_CN_DOWNLOAD -eq '1') {
        $script:TuyaUseCnDownload = $true
        $override = ' (override)'
    } elseif ($env:TUYAOPEN_CN_DOWNLOAD -eq '0') {
        $script:TuyaUseCnDownload = $false
        $override = ' (override)'
    } else {
        $offset = Get-TuyaUtcOffsetMinutes
        $script:TuyaUseCnDownload = (Test-TuyaCnTzRange -Offset $offset)
    }
    if ($script:TuyaUseCnDownload) {
        $msg = "[TuyaOpen] Region: mainland China (UTC+8±$($script:TuyaCnTzOffsetTolerance)min"
        if ($null -ne $offset) {
            $msg += ", offset=$offset"
        }
        $msg += ", CN download mirror)$override"
    } else {
        $msg = '[TuyaOpen] Region: overseas'
        if ($null -ne $offset) {
            $msg += " (offset=$offset)"
        }
        $msg += " (GitHub/Astral download source)$override"
    }
    # Remember the decision (reason + which source) but don't print it here:
    # it's surfaced at uv download time so a warm start (nothing downloaded)
    # stays quiet.  Write-TuyaOpenDebug still shows it under TUYAOPEN_EXPORT_VERBOSE.
    $script:TuyaRegionMsg = $msg
    Write-TuyaOpenDebug $msg
}

function Get-TuyaExportColdStartKind {
    param([string]$Root)
    $uvReady = $false
    try {
        $ctx = New-TuyaUvInstallContext -Root $Root
        $uvReady = Test-TuyaUvExecutable $ctx.UvExe
    } catch {
        $uvReady = $false
    }
    $version     = $script:PythonVersion
    $installDir  = Get-TuyaPythonInstallDir -Root $Root -Version $version
    $pythonExe   = Get-TuyaManagedPythonExe -InstallDir $installDir
    $pythonReady = $pythonExe -and (Test-TuyaPythonExecutable -ExePath $pythonExe -ExpectedVersion $version)
    $venvPath    = Join-Path $Root '.venv'
    $venvReady   = Test-TuyaUvManagedVenv -VenvPath $venvPath
    if (-not $uvReady -or -not $pythonReady) {
        return 'full'
    }
    if (-not $venvReady) {
        return 'venv_only'
    }
    return 'warm'
}

function Write-TuyaExportColdStartHint {
    param([Parameter(Mandatory)][string]$Kind)
    switch ($Kind) {
        'full' {
            Write-TuyaOpenInfo '[TuyaOpen] First-time setup: downloading uv, Python, and dependencies (may take 3-10 minutes). Please wait...'
        }
        'venv_only' {
            Write-TuyaOpenInfo '[TuyaOpen] Rebuilding virtual environment (Python already installed)...'
        }
    }
}

function Test-TuyaExportColdStart {
    param([string]$Root)
    return (Get-TuyaExportColdStartKind -Root $Root) -ne 'warm'
}

function Invoke-TuyaUvNative {
    <#
        Run uv. Default is quiet (no venv banner, package list, or hardlink warnings).
        Use -WithProgress for long installs (e.g. python install) or set TUYAOPEN_EXPORT_VERBOSE=1.
    #>
    param(
        [Parameter(Mandatory)][string]$UvExe,
        [Parameter(Mandatory)][string[]]$ArgumentList,
        [switch]$WithProgress
    )

    $useQuiet = -not $script:TuyaOpenVerbose -and -not $WithProgress
    if ($ProgressPreference -eq 'SilentlyContinue') {
        $useQuiet = $true
    }

    if ($useQuiet) {
        $savedLink = $env:UV_LINK_MODE
        if (-not $savedLink) { $env:UV_LINK_MODE = 'copy' }
        try {
            $null = & $UvExe @ArgumentList --quiet 2>&1
        } finally {
            if ($null -eq $savedLink) {
                Remove-Item Env:UV_LINK_MODE -ErrorAction SilentlyContinue
            } else {
                $env:UV_LINK_MODE = $savedLink
            }
        }
        return
    }

    $savedUvNoProgress = $env:UV_NO_PROGRESS
    Remove-Item Env:UV_NO_PROGRESS -ErrorAction SilentlyContinue
    try {
        & $UvExe @ArgumentList
    } finally {
        if ($null -ne $savedUvNoProgress) {
            $env:UV_NO_PROGRESS = $savedUvNoProgress
        } else {
            Remove-Item Env:UV_NO_PROGRESS -ErrorAction SilentlyContinue
        }
    }
}

function Write-TuyaOpenFailureHint {
    param(
        [ValidateSet('Entry', 'Uv', 'Python', 'Venv', 'Sync', 'Session', 'Io', 'Git')]
        [string]$Stage,
        [string]$Summary,
        [string]$Cause,
        [string[]]$NextSteps
    )
    Write-TuyaOpenInfo "[TuyaOpen] Error: $Stage - $Summary"
    if ($Cause) { Write-TuyaOpenInfo "Cause: $Cause" }
    if ($NextSteps -and $NextSteps.Count -gt 0) {
        Write-TuyaOpenInfo 'Next:'
        foreach ($step in $NextSteps) { Write-TuyaOpenInfo "  $step" }
    }
}

function Stop-TuyaOpenExport {
  <# Dot-sourced runs must not call exit (it closes the host). Throw instead. #>
    param([int]$Code = 1)
    if ($script:TuyaOpenDotSourced -and $env:TUYAOPEN_EXPORT_SKIP_MAIN -ne '1') {
        throw "[TuyaOpen] export aborted (exit code $Code)."
    }
    exit $Code
}

function Ensure-TuyaDirectory {
    param([string]$Path)
    if (Test-Path -LiteralPath $Path -PathType Container) { return $true }
    try {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
        return $true
    } catch {
        Write-TuyaOpenFailureHint -Stage Io -Summary 'Cannot create directory.' -Cause $_.Exception.Message -NextSteps @("Ensure the path is writable: $Path", 'Re-run with sufficient permissions.')
        return $false
    }
}

function Remove-TuyaPathSafe {
    param([string]$Path, [switch]$Recurse)
    if (-not (Test-Path -LiteralPath $Path)) { return $true }
    try {
        Remove-Item -LiteralPath $Path -Force -Recurse:$Recurse
        return $true
    } catch {
        Write-TuyaOpenDebug "[TuyaOpen] Remove failed: $Path - $($_.Exception.Message)"
        return $false
    }
}

function Write-TuyaMarkerFile {
    param([string]$Path, [string]$Content)
    $parent = Split-Path -Parent $Path
    if ($parent -and -not (Ensure-TuyaDirectory -Path $parent)) { return $false }
    try {
        [System.IO.File]::WriteAllText($Path, $Content, (New-Object System.Text.UTF8Encoding $false))
        return $true
    } catch {
        Write-TuyaOpenDebug "[TuyaOpen] Marker write failed: $($_.Exception.Message)"
        return $false
    }
}

function Test-TuyaOpenIsResource {
    param([string]$Root)
    if ($env:TUYAOPEN_ENV_ACTIVE -ne '1') { return $false }
    if (-not $env:OPEN_SDK_ROOT -or $env:OPEN_SDK_ROOT -ine $Root) { return $false }
    $venvPy = Join-Path $Root '.venv\Scripts\python.exe'
    return (Test-Path -LiteralPath $venvPy -PathType Leaf)
}

function Invoke-TuyaGuardActive {
    param([string]$Root)
    if ($env:TUYAOPEN_ENV_ACTIVE -ne '1') { return $false }
    if (-not $env:OPEN_SDK_ROOT -or $env:OPEN_SDK_ROOT -ine $Root) { return $false }
    $venvPy = Join-Path $Root '.venv\Scripts\python.exe'
    if (-not (Test-Path -LiteralPath $venvPy -PathType Leaf)) { return $false }
    Write-TuyaOpenInfo '[TuyaOpen] Environment is already active.'
    Write-TuyaOpenInfo 'To re-activate: deactivate && . .\export.ps1'
    return $true
}

function Test-TuyaProjectFiles {
    param([string]$Root)
    $missing = @()
    foreach ($name in 'tos.py', 'pyproject.toml', 'uv.lock', 'export.ps1') {
        if (-not (Test-Path -LiteralPath (Join-Path $Root $name) -PathType Leaf)) { $missing += $name }
    }
    if ($missing.Count -eq 0) { return $true }
    Write-TuyaOpenFailureHint -Stage Entry -Summary 'Required project files are missing.' -Cause ($missing -join ', ') -NextSteps @('Use a complete TuyaOpen clone.', "Missing under: $Root")
    return $false
}

function Test-TuyaGitAvailable {
    # git is a hard dependency: platform updates, submodule downloads and version
    # detection all rely on it. GitPython also fails at import time without it.
    $cmd = Get-Command git -ErrorAction SilentlyContinue
    if ($cmd) { return $true }
    Write-TuyaOpenFailureHint -Stage Git -Summary 'git not found. It may not be installed.' -NextSteps @(
        'Open a new terminal and run: winget install Git.Git',
        '(or download from https://git-scm.com/downloads)',
        'Then restart your terminal and re-run: . .\export.ps1'
    )
    return $false
}

function Add-TuyaPathEntryIfMissing {
    param([string]$Dir)
    $sep = [System.IO.Path]::PathSeparator
    if (($env:PATH -split [regex]::Escape($sep)) | Where-Object { $_ -ieq $Dir }) {
        return
    }
    $env:PATH = "$Dir$sep$env:PATH"
}

# ---------------------------------------------------------------------------
# Platform detection
# ---------------------------------------------------------------------------
function Get-TuyaUvOsArchitectureLabel {
    try {
        $asm = [System.Reflection.Assembly]::LoadWithPartialName(
            'System.Runtime.InteropServices.RuntimeInformation')
        $type = $asm.GetType('System.Runtime.InteropServices.RuntimeInformation')
        $prop = $type.GetProperty('OSArchitecture')
        return $prop.GetValue($null).ToString()
    } catch {
        return $null
    }
}

function Get-TuyaUvOsArch {
    switch (Get-TuyaUvOsArchitectureLabel) {
        'X86'   { return 'i686' }
        'X64'   { return 'x86_64' }
        'Arm'   { return 'thumbv7a' }
        'Arm64' { return 'aarch64' }
    }
    Write-TuyaOpenDebug '[TuyaOpen] OSArchitecture unavailable; using environment fallback.'
    if ([System.Environment]::Is64BitOperatingSystem) {
        return 'x86_64'
    }
    return 'i686'
}

function Get-TuyaUvTargetTriple {
    switch (Get-TuyaUvOsArch) {
        'i686'     { return 'i686-pc-windows-msvc' }
        'aarch64'  { return 'aarch64-pc-windows-msvc' }
        'thumbv7a' { return 'thumbv7a-pc-windows-msvc' }
        default    { return 'x86_64-pc-windows-msvc' }
    }
}

# thumbv7a has no official uv build; fall back to 32-bit x86.
function Resolve-TuyaUvTargetTriple {
    $triple = Get-TuyaUvTargetTriple
    if ($script:TuyaUvWindowsArtifacts.ContainsKey($triple)) {
        return $triple
    }
    if ($triple -like '*thumbv7a*') {
        Write-TuyaOpenDebug "[TuyaOpen] No uv build for $triple; falling back to i686-pc-windows-msvc."
        return 'i686-pc-windows-msvc'
    }
    return $triple
}

function Get-TuyaUvDisplayVersion {
    param(
        [string]$Root,
        [string]$UvVersion = $null
    )
    if (-not $UvVersion) {
        return (Get-TuyaUvManifest -Root $Root).Version
    }
    $v = $UvVersion.Trim()
    if ($v -match '(?i)^uv\s+(\d+\.\d+\.\d+)') {
        return $Matches[1]
    }
    if ($v -match '^(\d+\.\d+\.\d+)') {
        return $Matches[1]
    }
    return $v
}

function Write-TuyaUvPlatformBanner {
    param(
        [string]$Root,
        [string]$UvVersion = $null
    )
    $uvDisplay = Get-TuyaUvDisplayVersion -Root $Root -UvVersion $UvVersion
    $archLabel = Get-TuyaUvOsArchitectureLabel
    if (-not $archLabel) {
        $archLabel = if ($env:PROCESSOR_ARCHITECTURE) { $env:PROCESSOR_ARCHITECTURE } else { 'unknown' }
    }
    Write-TuyaOpenInfo "OPEN_SDK_ROOT = $Root"
    Write-TuyaOpenInfo "Host: Windows $archLabel | uv $uvDisplay | Python $($script:PythonVersion)"
}

function Invoke-TuyaHello {
    param([string]$Root, [string]$PythonExe)
    if (-not $PythonExe -or -not (Test-Path -LiteralPath $PythonExe -PathType Leaf)) { return }
    $tosHello = Join-Path $Root 'tos.py'
    if (-not (Test-Path -LiteralPath $tosHello -PathType Leaf)) { return }
    if ($script:TuyaOpenVerbose) {
        & $PythonExe $tosHello hello --no-version
    } else {
        & $PythonExe $tosHello hello --no-version 2>$null
    }
}

# ---------------------------------------------------------------------------
# Manifest and download URLs (uv-manifest.env + env overrides)
# ---------------------------------------------------------------------------
function Get-TuyaUvTripleManifestKey {
    param([string]$Triple)
    return ($Triple -replace '-', '_').ToUpperInvariant()
}

function Get-TuyaUvManifest {
    param([string]$Root)

    $manifest = @{
        Version        = $script:UvVersion
        BaseUrls       = @($script:TuyaUvDefaultBaseUrl)
        ArtifactChecks = @{}
    }

    $envFile = Join-Path $Root 'uv-manifest.env'
    if (-not (Test-Path -LiteralPath $envFile -PathType Leaf)) {
        return $manifest
    }

    $astralUrl = $null
    $githubUrl = $null
    foreach ($line in Get-Content -LiteralPath $envFile) {
        if ($line -match '^\s*#' -or $line -match '^\s*$') {
            continue
        }
        if ($line -match '^\s*UV_VERSION\s*=\s*(.+)\s*$') {
            $manifest.Version = $Matches[1].Trim()
        } elseif ($line -match '^\s*UV_DOWNLOAD_SOURCE_ASTRAL\s*=\s*(.+)\s*$') {
            $astralUrl = $Matches[1].Trim()
        } elseif ($line -match '^\s*UV_DOWNLOAD_SOURCE_GITHUB\s*=\s*(.+)\s*$') {
            $githubUrl = $Matches[1].Trim()
        } elseif ($line -match '^\s*UV_([A-Za-z0-9_]+)_DOWNLOAD_CN\s*=\s*(.+)\s*$') {
            $key = $Matches[1].ToUpperInvariant()
            if (-not $manifest.ArtifactChecks.ContainsKey($key)) {
                $manifest.ArtifactChecks[$key] = @{}
            }
            $manifest.ArtifactChecks[$key].DownloadCn = $Matches[2].Trim()
        } elseif ($line -match '^\s*UV_([A-Za-z0-9_]+)_SHA256\s*=\s*(.+)\s*$') {
            $key = $Matches[1].ToUpperInvariant()
            if (-not $manifest.ArtifactChecks.ContainsKey($key)) {
                $manifest.ArtifactChecks[$key] = @{}
            }
            $manifest.ArtifactChecks[$key].Sha256 = $Matches[2].Trim().ToLower()
        } elseif ($line -match '^\s*UV_([A-Za-z0-9_]+)_SIZE\s*=\s*(.+)\s*$') {
            $key = $Matches[1].ToUpperInvariant()
            if (-not $manifest.ArtifactChecks.ContainsKey($key)) {
                $manifest.ArtifactChecks[$key] = @{}
            }
            $manifest.ArtifactChecks[$key].Size = [long]$Matches[2].Trim()
        }
    }

    $configured = @($githubUrl, $astralUrl) | Where-Object { $_ }
    if ($configured.Count -gt 0) {
        $manifest.BaseUrls = $configured
    }
    return $manifest
}

function Get-TuyaUvArtifactCheck {
    param(
        $Manifest,
        [string]$Triple
    )
    $key = Get-TuyaUvTripleManifestKey -Triple $Triple
    if (-not $Manifest.ArtifactChecks.ContainsKey($key)) {
        return $null
    }
    $entry = $Manifest.ArtifactChecks[$key]
    if (-not $entry.Size -or -not $entry.Sha256) {
        return $null
    }
    return @{
        Size   = [long]$entry.Size
        Sha256 = [string]$entry.Sha256
    }
}

function Get-TuyaUvReleaseBaseUrls {
    param(
        [string]$Version,
        [string[]]$BaseUrls
    )
    if ($env:UV_DOWNLOAD_URL) {
        return @($env:UV_DOWNLOAD_URL)
    }
    if ($env:UV_INSTALLER_GHE_BASE_URL) {
        return @("$($env:UV_INSTALLER_GHE_BASE_URL)/astral-sh/uv/releases/download/$Version")
    }
    if ($env:UV_INSTALLER_GITHUB_BASE_URL) {
        return @("$($env:UV_INSTALLER_GITHUB_BASE_URL)/astral-sh/uv/releases/download/$Version")
    }

    $sources = @($BaseUrls | Where-Object { $_ })
    if ($sources.Count -eq 0) {
        $sources = @($script:TuyaUvDefaultBaseUrl)
    }
    return @($sources | ForEach-Object { "$($_.TrimEnd('/'))/$Version" })
}

function Test-TuyaUvDownloadOverride {
    return [bool]($env:UV_DOWNLOAD_URL -or $env:UV_INSTALLER_GHE_BASE_URL -or $env:UV_INSTALLER_GITHUB_BASE_URL)
}

function Get-TuyaUvCnDownloadUrl {
    param(
        $Manifest,
        [string]$Triple
    )
    $key = Get-TuyaUvTripleManifestKey -Triple $Triple
    if (-not $Manifest.ArtifactChecks.ContainsKey($key)) {
        return $null
    }
    $url = $Manifest.ArtifactChecks[$key].DownloadCn
    if ([string]::IsNullOrWhiteSpace($url)) {
        return $null
    }
    return $url.Trim()
}

function Get-TuyaUvDownloadUrls {
    param(
        [string]$Version,
        [string[]]$BaseUrls,
        [string]$Triple,
        [string]$ArtifactName,
        $Manifest
    )
    $urls = [System.Collections.Generic.List[string]]::new()
    if (Test-TuyaUvDownloadOverride) {
        foreach ($base in (Get-TuyaUvReleaseBaseUrls -Version $Version -BaseUrls $BaseUrls)) {
            $urls.Add("$($base.TrimEnd('/'))/$ArtifactName")
        }
        return @($urls)
    }
    if ($script:TuyaUseCnDownload) {
        $cnUrl = Get-TuyaUvCnDownloadUrl -Manifest $Manifest -Triple $Triple
        if ($cnUrl) {
            $urls.Add($cnUrl)
        }
    }
    foreach ($base in (Get-TuyaUvReleaseBaseUrls -Version $Version -BaseUrls $BaseUrls)) {
        $urls.Add("$($base.TrimEnd('/'))/$ArtifactName")
    }
    return @($urls)
}

# Map a download URL to a short, friendly source name (github/astral/tuyacn).
function Get-TuyaUvSourceLabel {
    param([string]$Url)
    switch -Wildcard ($Url) {
        '*github.com*' { return 'github' }
        '*astral.sh*'  { return 'astral' }
        '*tuyacn.com*' { return 'tuyacn' }
    }
    try { return ([Uri]$Url).Host } catch { return 'unknown' }
}

# ---------------------------------------------------------------------------
# Install context (paths + URLs derived once per install attempt)
# ---------------------------------------------------------------------------
function New-TuyaUvInstallContext {
    param([string]$Root)

    $manifest        = Get-TuyaUvManifest -Root $Root
    $candidateTriple = Get-TuyaUvTargetTriple
    $triple          = Resolve-TuyaUvTargetTriple

    if (-not $script:TuyaUvWindowsArtifacts.ContainsKey($triple)) {
        throw "No uv binaries for this platform ($triple)."
    }

    $artifactName = $script:TuyaUvWindowsArtifacts[$triple] 
    $uvVersionDir = Join-Path $Root ".tools\uv\$($manifest.Version)"
    $artifactCheck = Get-TuyaUvArtifactCheck -Manifest $manifest -Triple $triple
    if (-not $artifactCheck) {
        $manifestKey = Get-TuyaUvTripleManifestKey -Triple $triple
        throw "Missing UV_${manifestKey}_SIZE / UV_${manifestKey}_SHA256 in uv-manifest.env"
    }
    $releaseBaseUrls = Get-TuyaUvReleaseBaseUrls -Version $manifest.Version -BaseUrls $manifest.BaseUrls
    $downloadUrls    = Get-TuyaUvDownloadUrls -Version $manifest.Version -BaseUrls $manifest.BaseUrls `
        -Triple $triple -ArtifactName $artifactName -Manifest $manifest
    return @{
        Version         = $manifest.Version
        TargetTriple    = $triple
        ArtifactName    = $artifactName
        ExpectedSize    = $artifactCheck.Size
        ExpectedSha256  = $artifactCheck.Sha256
        ReleaseBaseUrls = $releaseBaseUrls
        DownloadUrls    = $downloadUrls
        UvToolsDir      = $uvVersionDir
        ArchivePath     = Join-Path $Root ".tools\archives\$($script:TuyaUvToolName)\$($manifest.Version)\$artifactName"
        UvExe           = Join-Path $uvVersionDir 'uv.exe'
    }
}

function Get-TuyaUvDownloadUrl {
    param($Context)
    if ($Context.DownloadUrls -and $Context.DownloadUrls.Count -gt 0) {
        return $Context.DownloadUrls[0]
    }
    if ($Context.ReleaseBaseUrls.Count -eq 0) {
        return $null
    }
    $base = $Context.ReleaseBaseUrls[0].TrimEnd('/')
    return "$base/$($Context.ArtifactName)"
}

# ---------------------------------------------------------------------------
# Download, extract, verify
# ---------------------------------------------------------------------------
function Test-TuyaUvExecutable {
    param([string]$ExePath)
    if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
        return $false
    }
    try {
        & $ExePath --version 2>$null | Out-Null
        return $LASTEXITCODE -eq 0
    } catch {
        return $false
    }
}

function Remove-TuyaUvArchive {
    param($Context)
    Remove-TuyaPathSafe -Path $Context.ArchivePath | Out-Null
}

function Test-TuyaUvArchiveIntegrity {
    param($Context)
    if (-not (Test-Path -LiteralPath $Context.ArchivePath -PathType Leaf)) {
        return $false
    }

    $file = Get-Item -LiteralPath $Context.ArchivePath
    if ($file.Length -ne $Context.ExpectedSize) {
        Write-TuyaOpenDebug "[TuyaOpen] Size mismatch: got $($file.Length), expected $($Context.ExpectedSize)"
        return $false
    }

    $hash = (Get-FileHash -LiteralPath $Context.ArchivePath -Algorithm SHA256).Hash.ToLower()
    if ($hash -ne $Context.ExpectedSha256) {
        Write-TuyaOpenDebug "[TuyaOpen] SHA256 mismatch: got $hash"
        return $false
    }
    return $true
}

function Write-TuyaUvManualDownloadHint {
    param($Context)
    $manualUrl = Get-TuyaUvDownloadUrl -Context $Context
    Write-TuyaOpenInfo '[TuyaOpen] Manual install:'
    if ($manualUrl) {
        Write-TuyaOpenInfo "  Download: $manualUrl"
    }
    Write-TuyaOpenInfo "  Save zip to: $($Context.ArchivePath)"
    Write-TuyaOpenInfo "  Or extract uv.exe, uvx.exe, uvw.exe to: $($Context.UvToolsDir)"
    Write-TuyaOpenInfo '  Then re-run: . .\export.ps1'
}

function Import-TuyaUvLegacyArchive {
    param($Context)
    if (Test-Path -LiteralPath $Context.ArchivePath -PathType Leaf) {
        return $true
    }

    # Previous layout: .tools\archives\<version>\<artifact>.zip
    $versionDir   = Split-Path -Parent $Context.ArchivePath
    $version      = Split-Path -Leaf $versionDir
    $archivesRoot = Split-Path -Parent (Split-Path -Parent $versionDir)
    $legacyPath   = Join-Path $archivesRoot "$version\$($Context.ArtifactName)"
    if (-not (Test-Path -LiteralPath $legacyPath -PathType Leaf)) {
        return $false
    }

    Write-TuyaOpenDebug "[TuyaOpen] Migrating cached archive to $($Context.ArchivePath)"
    New-Item -ItemType Directory -Path $versionDir -Force | Out-Null
    Move-Item -LiteralPath $legacyPath -Destination $Context.ArchivePath
    return $true
}

function Receive-TuyaUvFileDownload {
    <#
        HTTPS download. IWR progress title ("Web request status") cannot be renamed, so when a
        size is known we use Write-Progress with ProgressLabel on the main thread (worker job
        runs WebClient.DownloadFile — avoids async progress-handler crashes).
    #>
    param(
        [string]$Url,
        [string]$DestinationPath,
        [string]$AuthToken = $null,
        [string]$ProgressLabel = '[TuyaOpen] Downloading',
        [long]$ExpectedBytes = 0
    )

    $tmpPath = "$DestinationPath.part"
    Remove-TuyaPathSafe -Path $tmpPath | Out-Null
    Remove-TuyaPathSafe -Path $DestinationPath | Out-Null

    $downloadBlock = {
        param($DownloadUrl, $PartPath, $Token)
        try {
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 -bor [Net.SecurityProtocolType]::Tls13
        } catch {
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        }
        $wc = New-Object System.Net.WebClient
        try {
            if ($Token) { $wc.Headers['Authorization'] = "Bearer $Token" }
            $wc.DownloadFile($DownloadUrl, $PartPath)
        } finally {
            $wc.Dispose()
        }
    }

    $useProgressBar = ($ProgressPreference -ne 'SilentlyContinue') -and ($ExpectedBytes -gt 0) -and (-not $script:TuyaOpenIdeHost)
    $useIdeLineProgress = $script:TuyaOpenIdeHost -and ($ExpectedBytes -gt 0)
    $job = $null

    try {
        if ($useIdeLineProgress) {
            $job = Start-Job -ScriptBlock $downloadBlock -ArgumentList $Url, $tmpPath, $AuthToken
            $lastReportAt = [datetime]::MinValue
            $lastStatusText = ''
            try {
                while ($job.State -eq 'Running') {
                    $received = 0L
                    if (Test-Path -LiteralPath $tmpPath -PathType Leaf) {
                        $received = (Get-Item -LiteralPath $tmpPath).Length
                    }
                    $now = [datetime]::UtcNow
                    if (($now - $lastReportAt).TotalMilliseconds -ge 1000) {
                        $status = '{0:N1} / {1:N1} MB' -f ($received / 1MB), ($ExpectedBytes / 1MB)
                        $line = "$ProgressLabel`: $status"
                        if ($line -ne $lastStatusText) {
                            Write-TuyaOpenInfo $line
                            $lastStatusText = $line
                        }
                        $lastReportAt = $now
                    }
                    Start-Sleep -Milliseconds 100
                }
                Receive-Job -Job $job -Wait -ErrorAction Stop | Out-Null
            } finally {
                $received = 0L
                if (Test-Path -LiteralPath $tmpPath -PathType Leaf) {
                    $received = (Get-Item -LiteralPath $tmpPath).Length
                }
                $status = '{0:N1} / {1:N1} MB' -f ($received / 1MB), ($ExpectedBytes / 1MB)
                $line = "$ProgressLabel`: $status"
                if ($line -ne $lastStatusText) {
                    Write-TuyaOpenInfo $line
                }
            }
        } elseif ($useProgressBar) {
            $job = Start-Job -ScriptBlock $downloadBlock -ArgumentList $Url, $tmpPath, $AuthToken
            try {
                while ($job.State -eq 'Running') {
                    $received = 0L
                    if (Test-Path -LiteralPath $tmpPath -PathType Leaf) {
                        $received = (Get-Item -LiteralPath $tmpPath).Length
                    }
                    $pct = [math]::Min(99, [int](100 * $received / $ExpectedBytes))
                    $status = 'Downloaded: {0:N1} MB of {1:N1} MB' -f ($received / 1MB), ($ExpectedBytes / 1MB)
                    Write-Progress -Activity $ProgressLabel -Status $status -PercentComplete $pct
                    Start-Sleep -Milliseconds 100
                }
                Receive-Job -Job $job -Wait -ErrorAction Stop | Out-Null
            } finally {
                Write-Progress -Activity $ProgressLabel -Completed
            }
        } elseif ($ProgressPreference -eq 'SilentlyContinue') {
            & $downloadBlock $Url $tmpPath $AuthToken
        } else {
            Write-TuyaOpenInfo $ProgressLabel
            & $downloadBlock $Url $tmpPath $AuthToken
        }
        Move-Item -LiteralPath $tmpPath -Destination $DestinationPath -Force
    } catch {
        Remove-TuyaPathSafe -Path $tmpPath | Out-Null
        throw
    } finally {
        if ($job) {
            Remove-Job -Job $job -Force -ErrorAction SilentlyContinue
        }
    }
}

function Invoke-TuyaUvArchiveDownload {
    param($Context)

    if (-not (Ensure-TuyaDirectory -Path (Split-Path -Parent $Context.ArchivePath))) { Stop-TuyaOpenExport 1 }
    $authToken = $env:UV_GITHUB_TOKEN
    $mirror    = 0
    $lastError = $null

    $downloadUrls = @($Context.DownloadUrls)
    if ($downloadUrls.Count -eq 0) {
        foreach ($releaseBase in $Context.ReleaseBaseUrls) {
            $downloadUrls += "$($releaseBase.TrimEnd('/'))/$($Context.ArtifactName)"
        }
    }
    foreach ($downloadUrl in $downloadUrls) {
        $mirror++
        $src = Get-TuyaUvSourceLabel -Url $downloadUrl
        if ($downloadUrls.Count -gt 1) {
            Write-TuyaOpenInfo "[TuyaOpen] Downloading $($Context.ArtifactName) from $src (source $mirror/$($downloadUrls.Count))"
        } else {
            Write-TuyaOpenInfo "[TuyaOpen] Downloading $($Context.ArtifactName) from $src"
        }
        Write-TuyaOpenDebug "[TuyaOpen] URL: $downloadUrl"

        for ($attempt = 1; $attempt -le $script:UvDownloadAttempts; $attempt++) {
            if ($attempt -gt 1) {
                Write-TuyaOpenInfo "[TuyaOpen] Retry $attempt/$($script:UvDownloadAttempts) from $src..."
            }
            try {
                Remove-TuyaPathSafe -Path $Context.ArchivePath | Out-Null
                Receive-TuyaUvFileDownload -Url $downloadUrl -DestinationPath $Context.ArchivePath -AuthToken $authToken `
                    -ProgressLabel "[TuyaOpen] Downloading $($Context.ArtifactName)" -ExpectedBytes $Context.ExpectedSize
                Write-TuyaOpenDebug '[TuyaOpen] Download complete.'
                return $true
            } catch {
                $lastError = $_.Exception.Message
                Write-TuyaOpenDebug "[TuyaOpen] Download failed: $lastError"
            }
        }
        if ($mirror -lt $downloadUrls.Count) {
            Write-TuyaOpenInfo "[TuyaOpen] Download from $src failed ($lastError); trying next source..."
        } else {
            Write-TuyaOpenInfo "[TuyaOpen] Download from $src failed ($lastError)."
        }
    }
    return $false
}

function Resolve-TuyaUvArchive {
    param($Context)

    Import-TuyaUvLegacyArchive -Context $Context | Out-Null

    if (Test-Path -LiteralPath $Context.ArchivePath -PathType Leaf) {
        if (Test-TuyaUvArchiveIntegrity -Context $Context) {
            Write-TuyaOpenDebug '[TuyaOpen] Using cached uv package.'
            return $true
        }
        Write-TuyaOpenDebug '[TuyaOpen] Removing invalid cache.'
        Remove-TuyaUvArchive -Context $Context
    }

    if ($script:TuyaRegionMsg) { Write-TuyaOpenInfo $script:TuyaRegionMsg }
    if (-not (Invoke-TuyaUvArchiveDownload -Context $Context)) {
        Write-TuyaOpenFailureHint -Stage Uv -Summary 'uv download failed.' -Cause 'All mirrors and retries exhausted.' -NextSteps @('Check network or proxy.', 'See manual install below.')
        Write-TuyaUvManualDownloadHint -Context $Context
        Stop-TuyaOpenExport 1
    }

    if (-not (Test-TuyaUvArchiveIntegrity -Context $Context)) {
        Remove-TuyaUvArchive -Context $Context
        Write-TuyaOpenFailureHint -Stage Uv -Summary 'Downloaded package failed verification.' -Cause 'Size or SHA256 mismatch.' -NextSteps @('Delete the zip and re-run: . .\export.ps1')
        Write-TuyaUvManualDownloadHint -Context $Context
        Stop-TuyaOpenExport 1
    }
    return $true
}

function Import-TuyaUvFromLegacyDir {
    param($Context)
    if (Test-TuyaUvExecutable $Context.UvExe) {
        return $true
    }

    # Previous layout: binaries lived directly under .tools\uv\
    $uvRoot = Split-Path -Parent $Context.UvToolsDir
    $legacyExe = Join-Path $uvRoot 'uv.exe'
    if (-not (Test-TuyaUvExecutable $legacyExe)) {
        return $false
    }

    Write-TuyaOpenDebug "[TuyaOpen] Migrating uv from legacy path $uvRoot to $($Context.UvToolsDir)"
    New-Item -ItemType Directory -Path $Context.UvToolsDir -Force | Out-Null
    foreach ($binName in $script:TuyaUvBinNames) {
        $src = Join-Path $uvRoot $binName
        if (-not (Test-Path -LiteralPath $src -PathType Leaf)) {
            continue
        }
        Copy-Item -LiteralPath $src -Destination (Join-Path $Context.UvToolsDir $binName) -Force
    }
    return (Test-TuyaUvExecutable $Context.UvExe)
}

function Expand-TuyaUvArchive {
    param($Context)

    Write-TuyaOpenDebug '[TuyaOpen] Extracting uv...'
    $extractDir = Join-Path ([IO.Path]::GetTempPath()) ("tuya_uv_{0}" -f [guid]::NewGuid().ToString('N'))
    if (-not (Ensure-TuyaDirectory -Path $Context.UvToolsDir)) { Stop-TuyaOpenExport 1 }

    try {
        if (-not (Ensure-TuyaDirectory -Path $extractDir)) { Stop-TuyaOpenExport 1 }
        Expand-Archive -LiteralPath $Context.ArchivePath -DestinationPath $extractDir -Force

        $uvInstalled = $false
        foreach ($binName in $script:TuyaUvBinNames) {
            $src = Join-Path $extractDir $binName
            if (-not (Test-Path -LiteralPath $src -PathType Leaf)) { continue }
            Copy-Item -LiteralPath $src -Destination (Join-Path $Context.UvToolsDir $binName) -Force
            if ($binName -eq 'uv.exe') { $uvInstalled = $true }
        }
        if (-not $uvInstalled) {
            Write-TuyaOpenFailureHint -Stage Uv -Summary 'uv.exe not found in archive.' -Cause $Context.ArchivePath -NextSteps @('Remove cached zip and re-run: . .\export.ps1')
            Stop-TuyaOpenExport 1
        }
    } catch {
        Write-TuyaOpenFailureHint -Stage Io -Summary 'Failed to extract uv archive.' -Cause $_.Exception.Message -NextSteps @('Close programs locking the zip.', 'Remove cached zip and re-run: . .\export.ps1')
        Stop-TuyaOpenExport 1
    } finally {
        Remove-TuyaPathSafe -Path $extractDir -Recurse | Out-Null
    }
}

function Install-TuyaUv {
    param($Context)

    if (Import-TuyaUvFromLegacyDir -Context $Context) {
        Write-TuyaOpenDebug "[TuyaOpen] uv ready: $($Context.UvExe)"
        return
    }

    Resolve-TuyaUvArchive -Context $Context | Out-Null
    Expand-TuyaUvArchive -Context $Context
    Write-TuyaOpenDebug "[TuyaOpen] uv ready: $($Context.UvExe)"
}

function Invoke-TuyaSetupUv {
    param([string]$Root)

    Write-TuyaOpenStage -StageId 'uv'
    $ctx = New-TuyaUvInstallContext -Root $Root

    if (Test-TuyaUvExecutable $ctx.UvExe) {
        Write-TuyaOpenDebug "[TuyaOpen] uv already installed: $($ctx.UvExe)"
    } else {
        Install-TuyaUv -Context $ctx
    }

    if (-not (Test-TuyaUvExecutable $ctx.UvExe)) {
        Write-TuyaOpenFailureHint -Stage Uv -Summary 'uv installation failed.' -Cause 'Executable missing or not runnable.' -NextSteps @('See manual install below.')
        Write-TuyaUvManualDownloadHint -Context $ctx
        Stop-TuyaOpenExport 1
    }

    Add-TuyaPathEntryIfMissing -Dir $ctx.UvToolsDir
    return $ctx.UvExe
}

# ---------------------------------------------------------------------------
# Python (uv-managed, project-local)
# ---------------------------------------------------------------------------
function Get-TuyaPythonInstallDir {
    param(
        [string]$Root,
        [string]$Version = $script:PythonVersion
    )
    return Join-Path $Root ".tools\python\$Version"
}

function Get-TuyaManagedPythonExe {
    param([string]$InstallDir)
    if (-not (Test-Path -LiteralPath $InstallDir -PathType Container)) {
        return $null
    }
    $candidates = Get-ChildItem -LiteralPath $InstallDir -Recurse -Filter 'python.exe' -File -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '\\Lib\\venv\\' }
    if (-not $candidates) {
        return $null
    }
    return ($candidates | Select-Object -First 1).FullName
}

function Test-TuyaPythonExecutable {
    param(
        [string]$ExePath,
        [string]$ExpectedVersion = $script:PythonVersion
    )
    if (-not $ExePath -or -not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
        return $false
    }
    try {
        $versionLine = (& $ExePath --version 2>&1 | Select-Object -First 1).ToString()
        return $versionLine -match [regex]::Escape("Python $ExpectedVersion")
    } catch {
        return $false
    }
}

function Convert-TuyaSizeToMiB {
    param(
        [double]$Value,
        [string]$Unit
    )
    switch ($Unit.ToUpperInvariant()) {
        'KIB' { return $Value / 1024 }
        'MIB' { return $Value }
        'GIB' { return $Value * 1024 }
        default { return $Value }
    }
}

function Measure-TuyaPythonInstallDirBytes {
    param(
        [Parameter(Mandatory)][string]$InstallDir
    )
    if (-not (Test-Path -LiteralPath $InstallDir)) {
        return 0L
    }
    $sum = 0L
    Get-ChildItem -LiteralPath $InstallDir -Recurse -File -Force -ErrorAction SilentlyContinue |
        ForEach-Object { $sum += $_.Length }
    return $sum
}

function Write-TuyaPythonInstallProgress {
    param(
        [Parameter(Mandatory)][string]$Version,
        [string]$Artifact = '',
        [double]$ReceivedMiB = -1,
        [double]$TotalMiB = -1
    )
    $text = "[TuyaOpen] Installing Python ${Version}"
    if ($Artifact) {
        $text += ": $Artifact"
    }
    if ($TotalMiB -gt 0 -and $ReceivedMiB -ge 0) {
        $pct = [Math]::Min(99, [int](100 * $ReceivedMiB / $TotalMiB))
        $text += ": {0:N1} / {1:N1} MB ({2}%)" -f $ReceivedMiB, $TotalMiB, $pct
    } elseif ($TotalMiB -gt 0) {
        $text += ": {0:N1} MB total" -f $TotalMiB
    }
    Write-TuyaOpenInfo $text
}

function Write-TuyaPythonInstallProgressIfChanged {
    param(
        [Parameter(Mandatory)][string]$Version,
        [Parameter(Mandatory)][hashtable]$State,
        [switch]$Force
    )
    $recv = $State.ReceivedMiB
    $recvForPct = if ($recv -ge 0 -and $State.TotalMiB -gt 0) {
        [Math]::Min($recv, $State.TotalMiB)
    } else {
        -1.0
    }
    $pct = if ($recvForPct -ge 0 -and $State.TotalMiB -gt 0) {
        [Math]::Min(99, [int](100 * $recvForPct / $State.TotalMiB))
    } else {
        -1
    }
    $text = "[TuyaOpen] Installing Python ${Version}"
    if ($State.Artifact) {
        $text += ": $($State.Artifact)"
    }
    if ($recv -ge 0 -and $State.TotalMiB -gt 0 -and $recv -gt $State.TotalMiB) {
        $text += ": extracting ({0:N1} MB written)" -f $recv
    } elseif ($State.TotalMiB -gt 0 -and $recv -ge 0) {
        $text += ": {0:N1} / {1:N1} MB ({2}%)" -f $recvForPct, $State.TotalMiB, $pct
    } elseif ($State.TotalMiB -gt 0) {
        $text += ": {0:N1} MB total" -f $State.TotalMiB
    }
    $now = [datetime]::UtcNow
    $elapsedMs = ($now - $State.LastEmitAt).TotalMilliseconds
    $pctDelta = if ($pct -ge 0 -and $State.LastPct -ge 0) {
        [Math]::Abs($pct - $State.LastPct)
    } else {
        100
    }
    if (-not $Force) {
        if ($text -eq $State.LastText -and $elapsedMs -lt 5000) {
            return
        }
        if ($elapsedMs -lt 2000 -and $pctDelta -lt 2) {
            return
        }
    }
    $State.LastEmitAt = $now
    $State.LastPct = $pct
    $State.LastText = $text
    Write-TuyaOpenInfo $text
}

function Update-TuyaPythonInstallFromUvLine {
    param(
        [Parameter(Mandatory)][string]$Line,
        [Parameter(Mandatory)][string]$Version,
        [Parameter(Mandatory)][hashtable]$State
    )
    if ([string]::IsNullOrWhiteSpace($Line)) {
        return
    }
    $changed = $false
    if ($Line -match '(?i)Downloading\s+(cpython[^\s(]+)') {
        $State.Artifact = $Matches[1].Trim()
        $changed = $true
    }
    if ($Line -match '(?i)([\d.]+)\s*(MiB|KiB|GiB)\s*/\s*([\d.]+)\s*(MiB|KiB|GiB)') {
        $State.ReceivedMiB = Convert-TuyaSizeToMiB -Value ([double]$Matches[1]) -Unit $Matches[2]
        $State.TotalMiB = Convert-TuyaSizeToMiB -Value ([double]$Matches[3]) -Unit $Matches[4]
        $changed = $true
    } else {
        $sizeMatches = [regex]::Matches($Line, '(?i)\(([\d.]+)\s*(MiB|KiB|GiB)\)')
        if ($sizeMatches.Count -gt 0) {
            $last = $sizeMatches[$sizeMatches.Count - 1]
            $State.TotalMiB = Convert-TuyaSizeToMiB -Value ([double]$last.Groups[1].Value) -Unit $last.Groups[2].Value
            $changed = $true
        }
    }
    if (-not $changed) {
        return
    }
    Write-TuyaPythonInstallProgressIfChanged -Version $Version -State $State
}

# uv diagnostics: keep error/cause lines from a streamed uv run so a failure
# can explain the real reason (network vs. other) instead of a bare exit code.
$script:TuyaUvDiag = [System.Collections.Generic.List[string]]::new()

function Reset-TuyaUvDiag {
    $script:TuyaUvDiag = [System.Collections.Generic.List[string]]::new()
}

function Add-TuyaUvDiagLine {
    param([string]$Line)
    if ([string]::IsNullOrWhiteSpace($Line)) { return }
    if ($Line -match '(?i)(error:|caused by:|failed to |timed out|warning:)') {
        [void]$script:TuyaUvDiag.Add($Line.Trim())
    }
}

# Best-effort: does the captured uv output look like a network/connectivity issue?
function Test-TuyaUvDiagIsNetwork {
    if ($script:TuyaUvDiag.Count -eq 0) { return $false }
    $text = ($script:TuyaUvDiag -join "`n")
    return ($text -match '(?i)(dns|lookup|name resolution|connection refused|connection reset|connect error|tcp connect|could not connect|timed out|timeout|failed to fetch|failed to download|error sending request|request failed|retries|unreachable|certificate|ssl|tls|proxy|network)')
}

function Write-TuyaUvDiag {
    if ($script:TuyaUvDiag.Count -eq 0) { return }
    Write-TuyaOpenInfo 'uv output:'
    foreach ($diagLine in $script:TuyaUvDiag) { Write-TuyaOpenInfo "  $diagLine" }
}

function Invoke-TuyaUvPythonInstallWithIdeProgress {
    param(
        [Parameter(Mandatory)][string]$UvExe,
        [Parameter(Mandatory)][string]$Version,
        [Parameter(Mandatory)][string]$InstallDir
    )
    $installArgs = @(
        'python', 'install', $Version,
        '--install-dir', $InstallDir,
        '--no-registry', '--no-bin'
    )
    $state = @{
        Artifact        = 'cpython'
        TotalMiB        = 0.0
        ReceivedMiB     = -1.0
        LastEmitAt      = [datetime]::MinValue
        LastHeartbeatAt = [datetime]::MinValue
        LastPollAt      = [datetime]::MinValue
        LastPct         = -1
        LastText        = ''
    }
    $savedNoProgress = $env:UV_NO_PROGRESS
    $savedLink = $env:UV_LINK_MODE
    $env:UV_NO_PROGRESS = '1'
    if (-not $savedLink) { $env:UV_LINK_MODE = 'copy' }
    $onLine = {
        param($line)
        Add-TuyaUvDiagLine -Line $line
        Update-TuyaPythonInstallFromUvLine -Line $line -Version $Version -State $state
    }
    $onPoll = {
        $now = [datetime]::UtcNow
        if (($now - $state.LastPollAt).TotalMilliseconds -lt 500) {
            return
        }
        $state.LastPollAt = $now
        if ($state.TotalMiB -le 0) {
            return
        }
        $recvForExtract = if ($state.ReceivedMiB -ge 0) { $state.ReceivedMiB } else { 0.0 }
        $isExtractPhase = $state.TotalMiB -gt 0 -and $recvForExtract -ge $state.TotalMiB
        if (-not $isExtractPhase -and $state.ReceivedMiB -lt 0) {
            return
        }
        $bytes = Measure-TuyaPythonInstallDirBytes -InstallDir $InstallDir
        if ($bytes -gt 0) {
            $recvMiB = $bytes / 1048576.0
            $isExtract = $state.TotalMiB -gt 0 -and $recvMiB -gt $state.TotalMiB
            if (-not $isExtract -and $recvMiB -lt 0.2 -and $state.ReceivedMiB -lt 0) {
                return
            }
            if ($isExtract) {
                $now = [datetime]::UtcNow
                $elapsed = ($now - $state.LastHeartbeatAt).TotalSeconds
                $delta = if ($state.ReceivedMiB -ge 0) { $recvMiB - $state.ReceivedMiB } else { 999 }
                if ($state.ReceivedMiB -ge 0 -and $elapsed -lt 5 -and $delta -lt 2) {
                    return
                }
                $state.LastHeartbeatAt = $now
            } elseif ($recvMiB -le $state.ReceivedMiB + 0.05 -and $state.ReceivedMiB -ge 0) {
                return
            }
            $state.ReceivedMiB = $recvMiB
            Write-TuyaPythonInstallProgressIfChanged -Version $Version -State $state
            return
        }
        $now = [datetime]::UtcNow
        if (($now - $state.LastHeartbeatAt).TotalSeconds -lt 5) {
            return
        }
        $state.LastHeartbeatAt = $now
        Write-TuyaPythonInstallProgressIfChanged -Version $Version -State $state
    }
    try {
        return Invoke-TuyaProcessStreamLines -Exe $UvExe -ArgumentList $installArgs `
            -OnLine $onLine -OnPoll $onPoll
    } finally {
        if ($null -eq $savedNoProgress) {
            Remove-Item Env:UV_NO_PROGRESS -ErrorAction SilentlyContinue
        } else {
            $env:UV_NO_PROGRESS = $savedNoProgress
        }
        if ($null -eq $savedLink) {
            Remove-Item Env:UV_LINK_MODE -ErrorAction SilentlyContinue
        } else {
            $env:UV_LINK_MODE = $savedLink
        }
    }
}

# Run one `uv python install` attempt, announcing the source it downloads from
# (so the origin is visible in logs for later diagnosis).
function Invoke-TuyaPythonInstallAttempt {
    param(
        [Parameter(Mandatory)][string]$UvExe,
        [Parameter(Mandatory)][string]$Version,
        [Parameter(Mandatory)][string]$InstallDir,
        [Parameter(Mandatory)][string]$Source
    )
    Reset-TuyaUvDiag
    Write-TuyaOpenInfo "[TuyaOpen] Installing Python $Version from $Source..."
    if ($script:TuyaOpenIdeHost) {
        return (Invoke-TuyaUvPythonInstallWithIdeProgress -UvExe $UvExe -Version $Version -InstallDir $InstallDir)
    }
    Invoke-TuyaUvNative -UvExe $UvExe -WithProgress -ArgumentList @(
        'python', 'install', $Version,
        '--install-dir', $InstallDir,
        '--no-registry', '--no-bin'
    )
    return $LASTEXITCODE
}

function Install-TuyaPython {
    param(
        [string]$Root,
        [string]$UvExe
    )
    $version    = $script:PythonVersion
    $installDir = Get-TuyaPythonInstallDir -Root $Root -Version $version

    # If the user pinned their own mirror, honor it (no managed fallback).
    # Otherwise in mainland China try the CN mirror first and fall back to the
    # default (GitHub) source — uv does not fall back automatically.
    $savedMirror = $env:UV_PYTHON_INSTALL_MIRROR
    $exitCode = 0
    if ($savedMirror) {
        Write-TuyaOpenDebug "[TuyaOpen] Python mirror URL: $savedMirror"
        $exitCode = Invoke-TuyaPythonInstallAttempt -UvExe $UvExe -Version $version -InstallDir $installDir -Source 'custom mirror'
    } elseif ($script:TuyaUseCnDownload -and $script:TuyaPythonInstallMirrorCn) {
        $env:UV_PYTHON_INSTALL_MIRROR = $script:TuyaPythonInstallMirrorCn
        Write-TuyaOpenDebug "[TuyaOpen] Python mirror URL: $($script:TuyaPythonInstallMirrorCn)"
        try {
            $exitCode = Invoke-TuyaPythonInstallAttempt -UvExe $UvExe -Version $version -InstallDir $installDir -Source 'npmmirror (CN mirror)'
        } finally {
            Remove-Item Env:UV_PYTHON_INSTALL_MIRROR -ErrorAction SilentlyContinue
        }
        if ($exitCode -ne 0) {
            Write-TuyaOpenInfo "[TuyaOpen] CN Python mirror failed (exit $exitCode); falling back to default source (GitHub)..."
            $exitCode = Invoke-TuyaPythonInstallAttempt -UvExe $UvExe -Version $version -InstallDir $installDir -Source 'GitHub (default)'
        }
    } else {
        $exitCode = Invoke-TuyaPythonInstallAttempt -UvExe $UvExe -Version $version -InstallDir $installDir -Source 'GitHub (default)'
    }
    if ($exitCode -ne 0) {
        $cause = "uv python install exited with code $exitCode"
        if ($script:TuyaUvDiag.Count -gt 0) {
            if (Test-TuyaUvDiagIsNetwork) {
                $cause = 'network error while downloading Python (check connection/proxy; see uv output below)'
            } else {
                $cause = 'uv python install failed (see uv output below)'
            }
        }
        Write-TuyaOpenFailureHint -Stage Python -Summary "Python $version installation failed." -Cause $cause -NextSteps @("Run: `"$UvExe`" python install $version --install-dir `"$installDir`"", 'Re-run: . .\export.ps1')
        Write-TuyaUvDiag
        Stop-TuyaOpenExport 1
    }
}

function Invoke-TuyaSetupPython {
    param(
        [string]$Root,
        [string]$UvExe
    )
    Write-TuyaOpenStage -StageId 'python'
    $version    = $script:PythonVersion
    $installDir = Get-TuyaPythonInstallDir -Root $Root -Version $version
    $pythonExe  = Get-TuyaManagedPythonExe -InstallDir $installDir

    if (Test-TuyaPythonExecutable -ExePath $pythonExe -ExpectedVersion $version) {
        Write-TuyaOpenDebug "[TuyaOpen] Python ${version}: $pythonExe"
    } else {
        if ($pythonExe -and -not (Test-TuyaPythonExecutable -ExePath $pythonExe)) {
            Write-TuyaOpenDebug '[TuyaOpen] Existing Python install is invalid; reinstalling.'
            if (-not (Remove-TuyaPathSafe -Path $installDir -Recurse)) {
                Write-TuyaOpenFailureHint -Stage Python -Summary 'Cannot remove invalid Python install.' -Cause $installDir -NextSteps @('Close processes using .tools\python', 'Delete folder manually, then re-run.')
                Stop-TuyaOpenExport 1
            }
            $pythonExe = $null
        }
        if (-not $pythonExe) {
            Install-TuyaPython -Root $Root -UvExe $UvExe
            $pythonExe = Get-TuyaManagedPythonExe -InstallDir $installDir
        }
        if (-not (Test-TuyaPythonExecutable -ExePath $pythonExe -ExpectedVersion $version)) {
            Write-TuyaOpenFailureHint -Stage Python -Summary 'Python installation incomplete.' -Cause "Expected Python $version under $installDir" -NextSteps @('Re-run: . .\export.ps1')
            Stop-TuyaOpenExport 1
        }
        Write-TuyaOpenDebug "[TuyaOpen] Python $version ready: $pythonExe"
    }

    return $pythonExe
}


# ---------------------------------------------------------------------------
# Project .venv (uv sync)
# ---------------------------------------------------------------------------
function Enable-TuyaUvAliyunPypiMirrorSession {
    <#
        Temporarily set uv default index to Aliyun PyPI for this process.
        Returns saved env values to pass to Restore-TuyaUvPypiMirrorEnv.
    #>
    Write-TuyaOpenDebug "[TuyaOpen] uv index -> $($script:TuyaAliyunPypiIndex)"
    $saved = @{}
    foreach ($key in @('UV_DEFAULT_INDEX', 'UV_INDEX_URL')) {
        $item = Get-Item -Path "Env:$key" -ErrorAction SilentlyContinue
        $saved[$key] = if ($item) { $item.Value } else { $null }
        Set-Item -Path "Env:$key" -Value $script:TuyaAliyunPypiIndex
    }
    return $saved
}

function Get-TuyaUvSyncPlan {
    <#
        Both plans install strictly from uv.lock (--frozen); 'mirror' only
        changes the index URL.  Explicit TUYAOPEN_PYPI_MIRROR wins (1=on, 0=off);
        otherwise mainland China auto-uses the Aliyun mirror.
    #>
    $override = $env:TUYAOPEN_PYPI_MIRROR
    if ($override -eq '1') {
        return @{ ArgumentList = @('sync', '--frozen'); UseAliyunMirror = $true; Reason = 'override-on' }
    }
    if ($override -eq '0') {
        return @{ ArgumentList = @('sync', '--frozen'); UseAliyunMirror = $false; Reason = 'override-off' }
    }
    if ($script:TuyaUseCnDownload) {
        return @{ ArgumentList = @('sync', '--frozen'); UseAliyunMirror = $true; Reason = 'cn-auto' }
    }
    return @{ ArgumentList = @('sync', '--frozen'); UseAliyunMirror = $false; Reason = 'default' }
}

function Restore-TuyaUvPypiMirrorEnv {
    param($Saved)
    if (-not $Saved) { return }
    foreach ($key in $Saved.Keys) {
        if ($null -eq $Saved[$key]) {
            Remove-Item -Path "Env:$key" -ErrorAction SilentlyContinue
        } else {
            Set-Item -Path "Env:$key" -Value $Saved[$key]
        }
    }
}

function Get-TuyaUvLockPackageCount {
    param([Parameter(Mandatory)][string]$LockPath)
    if (-not (Test-Path -LiteralPath $LockPath -PathType Leaf)) {
        return 1
    }
    $matches = Select-String -LiteralPath $LockPath -Pattern '^\[\[package\]\]' -AllMatches
    $count = @($matches).Count
    if ($count -lt 1) { return 1 }
    return $count
}

function New-TuyaUvSyncProgressState {
    return @{
        LastPct     = -1
        Started     = $false
        NewlineDone = $false
        LastWidth   = 0
        UseInline   = (-not $script:TuyaOpenIdeHost) -and (-not [Console]::IsOutputRedirected)
        LastText    = ''
        LastEmitAt  = [datetime]::MinValue
        Current     = 0
        LastName    = ''
    }
}

function Write-TuyaUvSyncProgressUpdate {
    <#
        Single-line progress bar (\r in-place refresh) or throttled lines when redirected.
        Inline mode redraws on every caller update (no 10% throttle) so output feels streamed.
    #>
    param(
        [Parameter(Mandatory)][int]$Current,
        [Parameter(Mandatory)][int]$Total,
        [string]$PackageName = '',
        [Parameter(Mandatory)][hashtable]$State
    )

    if ($Total -lt 1) { $Total = 1 }
    $pct = if ($Current -ge $Total) {
        100
    } else {
        [Math]::Max(0, [int](100 * $Current / $Total))
    }

    if (-not $State.UseInline) {
        if ($pct -gt 0 -and $pct -lt 100 -and ($pct -lt ($State.LastPct + 10))) {
            return
        }
        if ($pct -ge 100 -and $State.LastPct -ge 100) {
            return
        }
    }

    $barWidth = 28
    $filled = [Math]::Min($barWidth, [int]($barWidth * $Current / $Total))
    $empty = $barWidth - $filled
    $bar = ('#' * $filled) + ('-' * $empty)
    $text = "[TuyaOpen] Syncing dependencies [$bar] $Current/$Total ($pct%)"
    if ($PackageName) {
        $text += " - $PackageName"
    }

    if (-not $State.UseInline) {
        $now = [datetime]::UtcNow
        $elapsedMs = ($now - $State.LastEmitAt).TotalMilliseconds
        if ($text -eq $State.LastText -and $elapsedMs -lt 5000) {
            return
        }
        if ($elapsedMs -lt 1500 -and $State.LastPct -ge 0 -and [Math]::Abs($pct - $State.LastPct) -lt 3) {
            return
        }
        $State.LastText = $text
        $State.LastEmitAt = $now
    }

    if ($State.UseInline) {
        if ($State.LastWidth -gt $text.Length) {
            $text += (' ' * ($State.LastWidth - $text.Length))
        }
        $State.LastWidth = $text.Length
        if (-not $State.Started) {
            Write-Host $text -NoNewline
            $State.Started = $true
        } else {
            Write-Host "`r$text" -NoNewline
        }
    } else {
        Write-TuyaOpenInfo $text
        $State.Started = $true
    }
    $State.LastPct = $pct
}

function Update-TuyaUvSyncFromUvLine {
    param(
        [Parameter(Mandatory)][string]$Line,
        [Parameter(Mandatory)][int]$TotalPackages,
        [Parameter(Mandatory)][hashtable]$State
    )

    $changed = $false
    $plusRe = [regex]'^\+\s+([^\s=]+)'
    $downloadingRe = [regex]'(?i)^Downloading\s+(\S+)'
    $installedRe = [regex]'(?i)Installed\s+(\d+)\s+packages'
    $auditedRe = [regex]'(?i)Audited\s+(\d+)\s+packages'

    $m = $plusRe.Match($Line)
    if ($m.Success) {
        $State.Current = [Math]::Min($TotalPackages, $State.Current + 1)
        $State.LastName = $m.Groups[1].Value
        $changed = $true
    }
    $m = $downloadingRe.Match($Line)
    if ($m.Success) {
        $next = [Math]::Min($TotalPackages, $State.Current + 1)
        if ($next -gt $State.Current) {
            $State.Current = $next
            $State.LastName = $m.Groups[1].Value
            $changed = $true
        }
    }
    $m = $installedRe.Match($Line)
    if ($m.Success) {
        $n = [int]$m.Groups[1].Value
        if ($n -gt $State.Current) {
            $State.Current = [Math]::Min($TotalPackages, $n)
            $changed = $true
        }
    }
    $m = $auditedRe.Match($Line)
    if ($m.Success) {
        $n = [int]$m.Groups[1].Value
        if ($n -gt $State.Current) {
            $State.Current = [Math]::Min($TotalPackages, $n)
            $changed = $true
        }
    }

    if ($changed) {
        Write-TuyaUvSyncProgressUpdate -Current $State.Current -Total $TotalPackages `
            -PackageName $State.LastName -State $State
    }
}

function Invoke-TuyaProcessStreamLines {
    <#
        Run a process and invoke OnLine per stdout/stderr line as it arrives (no pipeline buffering).
        Optional OnPoll runs every ~500ms while the child is alive (IDE heartbeat progress).
    #>
    param(
        [Parameter(Mandatory)][string]$Exe,
        [Parameter(Mandatory)][string[]]$ArgumentList,
        [string]$WorkingDirectory = $null,
        [Parameter(Mandatory)][scriptblock]$OnLine,
        [scriptblock]$OnPoll = $null
    )

    $argText = ($ArgumentList | ForEach-Object {
            if ($_ -match '[\s"]') {
                '"' + ($_ -replace '"', '\"') + '"'
            } else {
                $_
            }
        }) -join ' '

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.Arguments = $argText
    if ($WorkingDirectory) {
        $psi.WorkingDirectory = $WorkingDirectory
    }
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $psi.StandardOutputEncoding = [System.Text.UTF8Encoding]::new($false)
    $psi.StandardErrorEncoding = [System.Text.UTF8Encoding]::new($false)

    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    $queue = New-Object System.Collections.Concurrent.ConcurrentQueue[string]
    $handler = {
        if ($Event.SourceEventArgs.Data) {
            $Event.MessageData.Queue.Enqueue($Event.SourceEventArgs.Data)
        }
    }
    $msg = @{ Queue = $queue }
    $subs = @(
        Register-ObjectEvent -InputObject $proc -EventName OutputDataReceived -Action $handler -MessageData $msg
        Register-ObjectEvent -InputObject $proc -EventName ErrorDataReceived -Action $handler -MessageData $msg
    )

    try {
        [void]$proc.Start()
        $proc.BeginOutputReadLine()
        $proc.BeginErrorReadLine()
        while (-not $proc.HasExited) {
            while ($queue.Count -gt 0) {
                $line = $null
                if ($queue.TryDequeue([ref]$line)) {
                    & $OnLine $line.Trim()
                }
            }
            if ($OnPoll) {
                & $OnPoll
            }
            Start-Sleep -Milliseconds 25
        }
        $proc.WaitForExit()
        while ($queue.Count -gt 0) {
            $line = $null
            if ($queue.TryDequeue([ref]$line)) {
                & $OnLine $line.Trim()
            }
        }
        return $proc.ExitCode
    } finally {
        foreach ($sub in $subs) {
            Unregister-Event -SubscriptionId $sub.Id -ErrorAction SilentlyContinue
            Remove-Job -Id $sub.Id -Force -ErrorAction SilentlyContinue
        }
    }
}

function Complete-TuyaUvSyncProgress {
    param([Parameter(Mandatory)][hashtable]$State)
    if ($State.Started -and -not $State.NewlineDone) {
        if ($State.UseInline) {
            Write-Host ''
        }
        $State.NewlineDone = $true
    }
}

function Write-TuyaUvLockContentionHint {
    <#
        uv serializes .venv access via an OS-level lock on .venv\.lock and waits
        for a busy lock silently (nothing is printed without -v), which looks
        like a hang. A leftover .lock FILE after a crash is normal and harmless;
        only a live uv process holding the lock blocks us.
    #>
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string]$Reason
    )
    Write-TuyaOpenInfo "[TuyaOpen] $Reason"
    Write-TuyaOpenInfo "           uv waits silently for the venv lock: $(Join-Path $Root '.venv\.lock')"
    Write-TuyaOpenInfo '           Likely another TuyaOpen/uv session holds it. Check: Get-Process uv'
    Write-TuyaOpenInfo '           Close other sessions or stop stray uv processes, then re-run: . .\export.ps1'
}

function Invoke-TuyaUvSyncWithProgress {
    param(
        [Parameter(Mandatory)][string]$UvExe,
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][int]$TotalPackages
    )

    Reset-TuyaUvDiag
    $priorUv = @(Get-Process -Name uv -ErrorAction SilentlyContinue)
    if ($priorUv.Count -gt 0) {
        Write-TuyaUvLockContentionHint -Root $Root -Reason "Another uv process is already running (PID: $($priorUv.Id -join ', ')); dependency sync may pause until it finishes."
    }
    $syncPlan = Get-TuyaUvSyncPlan
    $syncSrc = if ($syncPlan.UseAliyunMirror) { 'Aliyun PyPI mirror (CN)' } else { 'PyPI (default)' }
    Write-TuyaOpenInfo "[TuyaOpen] Syncing $TotalPackages Python dependencies from $syncSrc..."
    Write-TuyaOpenDebug "[TuyaOpen] Dependency sync plan: $($syncPlan.Reason)."

    $savedPypiMirror = if ($syncPlan.UseAliyunMirror) {
        Enable-TuyaUvAliyunPypiMirrorSession
    } else {
        $null
    }
    $syncArgs = $syncPlan.ArgumentList
    try {
        if ($ProgressPreference -eq 'SilentlyContinue') {
            Invoke-TuyaUvNative -UvExe $UvExe -ArgumentList $syncArgs
            return $LASTEXITCODE
        }

        $progressState = New-TuyaUvSyncProgressState

        $savedLink = $env:UV_LINK_MODE
        if (-not $savedLink) { $env:UV_LINK_MODE = 'copy' }
        $savedNoProgress = $env:UV_NO_PROGRESS
        $env:UV_NO_PROGRESS = '1'

        $uvLines = [System.Collections.Generic.List[string]]::new()
        # A sync blocked on the venv lock produces no output at all: watch for
        # total silence after start and explain the wait once (see hint above).
        $lockWatch = @{ StartedAt = [datetime]::UtcNow; GotOutput = $false; Warned = $false }
        $onUvLine = {
            param($line)
            $lockWatch.GotOutput = $true
            if (-not $line) { return }
            $uvLines.Add($line)
            Add-TuyaUvDiagLine -Line $line
            Update-TuyaUvSyncFromUvLine -Line $line -TotalPackages $TotalPackages -State $progressState
        }
        $onLockPoll = {
            if ($lockWatch.Warned -or $lockWatch.GotOutput) { return }
            if (([datetime]::UtcNow - $lockWatch.StartedAt).TotalSeconds -lt 10) { return }
            $lockWatch.Warned = $true
            if ($progressState.UseInline -and $progressState.Started -and -not $progressState.NewlineDone) {
                Write-Host ''
            }
            Write-TuyaUvLockContentionHint -Root $Root -Reason 'uv sync has produced no output for 10+ seconds; it may be waiting to acquire the venv lock.'
        }

        try {
            Write-TuyaUvSyncProgressUpdate -Current 0 -Total $TotalPackages -State $progressState
            $exitCode = Invoke-TuyaProcessStreamLines -Exe $UvExe -ArgumentList $syncArgs `
                -WorkingDirectory $Root -OnLine $onUvLine -OnPoll $onLockPoll
            if ($exitCode -ne 0 -and $uvLines.Count -gt 0) {
                Write-TuyaOpenInfo '[TuyaOpen] uv sync output:'
                foreach ($uvLine in $uvLines) {
                    Write-TuyaOpenInfo "  $uvLine"
                }
            }
            if ($exitCode -eq 0 -and $progressState.LastPct -lt 100) {
                Write-TuyaUvSyncProgressUpdate -Current $TotalPackages -Total $TotalPackages `
                    -State $progressState
            }
        } finally {
            Complete-TuyaUvSyncProgress -State $progressState
            if ($null -eq $savedLink) {
                Remove-Item Env:UV_LINK_MODE -ErrorAction SilentlyContinue
            } else {
                $env:UV_LINK_MODE = $savedLink
            }
            if ($null -eq $savedNoProgress) {
                Remove-Item Env:UV_NO_PROGRESS -ErrorAction SilentlyContinue
            } else {
                $env:UV_NO_PROGRESS = $savedNoProgress
            }
        }
        return $exitCode
    } finally {
        Restore-TuyaUvPypiMirrorEnv -Saved $savedPypiMirror
    }
}

function Get-TuyaWindowsMakeBinDir {
    param([Parameter(Mandatory)][string]$Root)
    return Join-Path $Root ".tools\$($script:TuyaMakeToolName)\$($script:TuyaMakeVersion)"
}

function Get-TuyaVenvMarkerPath {
    param([string]$VenvPath)
    return Join-Path $VenvPath $script:VenvMarker
}

function Test-TuyaUvManagedVenv {
    param([string]$VenvPath)
    $marker = Get-TuyaVenvMarkerPath -VenvPath $VenvPath
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) { return $false }
    $venvPy = Join-Path $VenvPath 'Scripts\python.exe'
    return (Test-Path -LiteralPath $venvPy -PathType Leaf)
}

function Remove-TuyaLegacyVenvIfNeeded {
    param([string]$Root)
    $venvPath = Join-Path $Root '.venv'
    $venvPy   = Join-Path $venvPath 'Scripts\python.exe'

    if (Test-Path -LiteralPath $venvPath -PathType Leaf) {
        Write-TuyaOpenDebug '[TuyaOpen] Removing invalid .venv (not a directory)...'
        if (-not (Remove-TuyaPathSafe -Path $venvPath -Recurse)) {
            Write-TuyaOpenFailureHint -Stage Venv -Summary 'Cannot remove .venv.' -Cause 'Path is a file or locked.' -NextSteps @('Delete .venv manually', 'Re-run: . .\export.ps1')
            Stop-TuyaOpenExport 1
        }
        return
    }
    if (-not (Test-Path -LiteralPath $venvPath -PathType Container)) { return }
    if ((Test-TuyaUvManagedVenv -VenvPath $venvPath) -and (Test-Path -LiteralPath $venvPy -PathType Leaf)) { return }

    Write-TuyaOpenInfo '[TuyaOpen] Detected legacy Python venv (.venv). Migrating to uv-managed environment...'
    Write-TuyaOpenInfo '           Old .venv removed. A new environment will be created.'
    if (-not (Remove-TuyaPathSafe -Path $venvPath -Recurse)) {
        Write-TuyaOpenFailureHint -Stage Venv -Summary 'Cannot remove .venv.' -Cause 'Directory may be in use.' -NextSteps @('Close IDE/terminals using .venv', 'Delete folder manually', 'Re-run: . .\export.ps1')
        Stop-TuyaOpenExport 1
    }
}

function Invoke-TuyaSetupVenv {
    param(
        [string]$Root,
        [string]$UvExe,
        [string]$ManagedPythonExe
    )
    Write-TuyaOpenStage -StageId 'venv'
    Remove-TuyaLegacyVenvIfNeeded -Root $Root
    $venvPath = Join-Path $Root '.venv'
    $venvPy   = Join-Path $venvPath 'Scripts\python.exe'
    $marker   = Get-TuyaVenvMarkerPath -VenvPath $venvPath
    $createdVenv = $false

    if (-not (Test-TuyaUvManagedVenv -VenvPath $venvPath) -or -not (Test-Path -LiteralPath $venvPy -PathType Leaf)) {
        Write-TuyaOpenInfo '[TuyaOpen] Creating .venv...'
        Push-Location $Root
        try {
            Invoke-TuyaUvNative -UvExe $UvExe -ArgumentList @('venv', $venvPath, '--python', $ManagedPythonExe)
            if ($LASTEXITCODE -ne 0) {
                Write-TuyaOpenFailureHint -Stage Venv -Summary 'Failed to create .venv.' -Cause "uv venv exited with code $LASTEXITCODE" -NextSteps @("Run: `"$UvExe`" venv .venv --python `"$ManagedPythonExe`"", 'Re-run: . .\export.ps1')
                Stop-TuyaOpenExport 1
            }
        } finally { Pop-Location }
        $markerContent = "managed-by=export.ps1`npython=$($script:PythonVersion)"
        if (-not (Write-TuyaMarkerFile -Path $marker -Content $markerContent)) {
            Write-TuyaOpenFailureHint -Stage Venv -Summary 'Cannot write venv marker.' -Cause $marker -NextSteps @('Check .venv permissions', 'Re-run: . .\export.ps1')
            Stop-TuyaOpenExport 1
        }
        $createdVenv = $true
        Write-TuyaOpenDebug '[TuyaOpen] .venv created.'
    }

    # Warm start: skip sync when uv.lock has not changed since the last
    # successful sync (the marker mtime is refreshed after each sync). This
    # keeps re-sourcing fast and avoids re-acquiring the venv lock every time.
    # TUYAOPEN_EXPORT_VERBOSE=1 forces a full sync (self-repair escape hatch).
    $needSync = $true
    if (-not $createdVenv -and -not $script:TuyaOpenVerbose) {
        $lockStamp = $null
        $markerStamp = $null
        try { $lockStamp   = (Get-Item -LiteralPath (Join-Path $Root 'uv.lock') -ErrorAction Stop).LastWriteTimeUtc } catch {}
        try { $markerStamp = (Get-Item -LiteralPath $marker -ErrorAction Stop).LastWriteTimeUtc } catch {}
        if ($null -ne $lockStamp -and $null -ne $markerStamp -and $lockStamp -le $markerStamp) {
            $needSync = $false
        }
    }

    if ($needSync) {
        Push-Location $Root
        try {
            Write-TuyaOpenStage -StageId 'sync'
            $pkgCount = Get-TuyaUvLockPackageCount -LockPath (Join-Path $Root 'uv.lock')
            $syncRc = Invoke-TuyaUvSyncWithProgress -UvExe $UvExe -Root $Root -TotalPackages $pkgCount
            if ($syncRc -ne 0) {
                $cause = 'uv sync --frozen failed.'
                if ($script:TuyaUvDiag.Count -gt 0) {
                    if (Test-TuyaUvDiagIsNetwork) {
                        $cause = 'network error while syncing dependencies (check connection/proxy; see uv sync output above)'
                    } else {
                        $cause = 'dependency resolution or uv sync failed (see uv sync output above)'
                    }
                }
                Write-TuyaOpenFailureHint -Stage Sync -Summary 'Dependency sync failed.' -Cause $cause -NextSteps @('Ensure uv.lock matches pyproject.toml', 'Check network, then re-run: . .\export.ps1')
                Stop-TuyaOpenExport 1
            }
            Write-TuyaOpenDebug '[TuyaOpen] Dependencies synced.'
        } finally { Pop-Location }
        try { (Get-Item -LiteralPath $marker -ErrorAction Stop).LastWriteTimeUtc = [datetime]::UtcNow } catch {}
    } else {
        Write-TuyaOpenStage -StageId 'sync'
        Write-TuyaOpenInfo '[TuyaOpen] Python dependencies up to date (uv.lock unchanged); skipping sync.'
    }

    if (-not (Test-Path -LiteralPath $venvPy -PathType Leaf)) {
        Write-TuyaOpenFailureHint -Stage Sync -Summary '.venv Python missing after sync.' -Cause $venvPy -NextSteps @('Remove .venv and re-run: . .\export.ps1')
        Stop-TuyaOpenExport 1
    }
    return $venvPy
}

function Set-TuyaSessionEnv {
    param([string]$Root, [string]$VenvPythonExe)
    $venvPath   = Join-Path $Root '.venv'
    $scriptsDir = Join-Path $venvPath 'Scripts'
    $pipExe     = Join-Path $scriptsDir 'pip.exe'
    $python3Exe = Join-Path $scriptsDir 'python3.exe'
    if (-not (Test-Path -LiteralPath $python3Exe -PathType Leaf)) {
        try { Copy-Item -LiteralPath $VenvPythonExe -Destination $python3Exe -Force } catch { }
    }
    $makeBinDir = Get-TuyaWindowsMakeBinDir -Root $Root
    $env:VIRTUAL_ENV        = $venvPath
    $env:OPEN_SDK_ROOT      = $Root
    $env:OPEN_SDK_PYTHON    = $VenvPythonExe
    $env:OPEN_SDK_PIP       = $pipExe
    $env:OPEN_SDK_MAKE_BIN  = $makeBinDir
    $env:OPEN_SDK_MAKE      = Join-Path $makeBinDir 'make.exe'
    $env:TUYAOPEN_ENV_ACTIVE = '1'
    Add-TuyaPathEntryIfMissing -Dir $scriptsDir
    Add-TuyaPathEntryIfMissing -Dir $Root
}

function Reset-TuyaSessionCache {
    param([string]$Root)
    $cachePath = Join-Path $Root '.cache'
    if (-not (Ensure-TuyaDirectory -Path $cachePath)) { return }
    foreach ($name in '.env.json', '.dont_prompt_update_platform') {
        Remove-TuyaPathSafe -Path (Join-Path $cachePath $name) | Out-Null
    }
}


function Restore-TuyaOpenPrompt {
    if (Test-Path function:_OLD_TUYA_PROMPT) {
        Copy-Item -Path function:_OLD_TUYA_PROMPT -Destination function:prompt -Force -ErrorAction SilentlyContinue
        Remove-Item function:_OLD_TUYA_PROMPT -Force -ErrorAction SilentlyContinue
        return
    }
    function global:prompt {
        "PS $($executionContext.SessionState.Path.CurrentLocation)> "
    }
}

function Install-TuyaOpenPromptIndicator {
    if (-not (Test-Path function:_OLD_TUYA_PROMPT)) {
        if (Test-Path function:prompt) {
            $current = (Get-Command prompt -CommandType Function).ScriptBlock.ToString()
            if ($current -notmatch 'TuyaOpenPromptPrefix' -and $current -notmatch '\(TuyaOpen\) ') {
                Copy-Item -Path function:prompt -Destination function:_OLD_TUYA_PROMPT
            }
        }
    }
    function global:prompt {
        Write-Host -NoNewline -ForegroundColor Green $script:PromptPrefix
        if (Test-Path function:_OLD_TUYA_PROMPT) {
            & $function:_OLD_TUYA_PROMPT
        } else {
            'PS ' + $ExecutionContext.SessionState.Path.CurrentLocation + '> '
        }
    }
}

function Install-TuyaOpenPwshCompletion {
    <#
        Register tos.py tab completion in the current PowerShell session (Click 8 + click-pwsh).
        Failures are ignored so sourcing export.ps1 never aborts.
    #>
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string]$PythonExe
    )

    if ($PSVersionTable.PSEdition -ne 'Core') {
        return
    }

    $tosScript = Join-Path $Root 'tos.py'
    if (-not (Test-Path -LiteralPath $tosScript)) {
        return
    }

    $prevComplete = $env:_TOS_PY_COMPLETE
    try {
        $env:_TOS_PY_COMPLETE = 'pwsh_source'
        $sourceScript = & $PythonExe $tosScript 2>$null
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($sourceScript)) {
            return
        }
        Invoke-Expression $sourceScript
    } catch {
        if ($script:TuyaOpenVerbose) {
            Write-TuyaOpenDebug "PowerShell completion setup skipped: $($_.Exception.Message)"
        }
    } finally {
        if ($null -eq $prevComplete) {
            Remove-Item Env:_TOS_PY_COMPLETE -ErrorAction SilentlyContinue
        } else {
            $env:_TOS_PY_COMPLETE = $prevComplete
        }
    }
}

function Register-TuyaOpenCommandHelpers {
    function global:tos.py {
        if (-not $env:OPEN_SDK_PYTHON -or -not $env:OPEN_SDK_ROOT) {
            Write-TuyaOpenInfo 'TuyaOpen environment is not active. Source export.ps1 first: . .\export.ps1'
            return
        }
        & $env:OPEN_SDK_PYTHON (Join-Path $env:OPEN_SDK_ROOT 'tos.py') @args
    }

    function global:__tuyaTeardown {
        param([switch]$Silent)
        if ($env:OPEN_SDK_ROOT) {
            $sdkRoot    = $env:OPEN_SDK_ROOT
            $sdkScripts = Join-Path $sdkRoot '.venv\Scripts'
            $makeBin    = $env:OPEN_SDK_MAKE_BIN
            if (-not $makeBin) {
                $makeBin = Get-TuyaWindowsMakeBinDir -Root $sdkRoot
            }
            $uvDir      = Join-Path $sdkRoot ".tools\uv\$((Get-TuyaUvManifest -Root $sdkRoot).Version)"
            $sep        = [System.IO.Path]::PathSeparator
            $env:PATH = (($env:PATH -split [regex]::Escape($sep)) | Where-Object {
                $_ -and ($_ -ine $sdkRoot) -and ($_ -ine $sdkScripts) -and ($_ -ine $makeBin) -and ($_ -ine $uvDir)
            }) -join $sep
        }
        Remove-Item Env:VIRTUAL_ENV        -ErrorAction SilentlyContinue
        Remove-Item Env:OPEN_SDK_ROOT      -ErrorAction SilentlyContinue
        Remove-Item Env:OPEN_SDK_PYTHON    -ErrorAction SilentlyContinue
        Remove-Item Env:OPEN_SDK_PIP       -ErrorAction SilentlyContinue
        Remove-Item Env:OPEN_SDK_MAKE_BIN  -ErrorAction SilentlyContinue
        Remove-Item Env:OPEN_SDK_MAKE      -ErrorAction SilentlyContinue
        Remove-Item Env:OPEN_SDK_UV        -ErrorAction SilentlyContinue
        Remove-Item Env:TUYAOPEN_ENV_ACTIVE -ErrorAction SilentlyContinue
        if ($env:_OLD_TUYA_PYTHONHOME) {
            $env:PYTHONHOME = $env:_OLD_TUYA_PYTHONHOME
            Remove-Item Env:_OLD_TUYA_PYTHONHOME -ErrorAction SilentlyContinue
        }
        Restore-TuyaOpenPrompt
        Remove-Item 'function:\tos.py'     -Force -ErrorAction SilentlyContinue
        Remove-Item 'function:\deactivate' -Force -ErrorAction SilentlyContinue
        Remove-Item 'function:\__tuyaTeardown' -Force -ErrorAction SilentlyContinue
        if (-not $Silent) {
            Write-TuyaOpenInfo 'TuyaOpen environment deactivated. Re-enter: . .\export.ps1'
        }
    }

    function global:deactivate { __tuyaTeardown }
}

function Invoke-TuyaExportSetupCore {
    param([Parameter(Mandatory)][string]$Root)
    # An inherited PYTHONHOME (conda or another Python distribution active in
    # the launching shell) breaks startup of every python this script and the
    # venv run. Clear it like a standard venv activate does; deactivate
    # restores it.
    if ($env:PYTHONHOME) {
        $env:_OLD_TUYA_PYTHONHOME = $env:PYTHONHOME
        Remove-Item Env:PYTHONHOME -ErrorAction SilentlyContinue
        Write-TuyaOpenDebug '[TuyaOpen] Cleared inherited PYTHONHOME (deactivate restores it).'
    }
    $coldKind = Get-TuyaExportColdStartKind -Root $Root
    Write-TuyaExportColdStartHint -Kind $coldKind
    Invoke-TuyaRegionDetect
    $env:OPEN_SDK_UV = Invoke-TuyaSetupUv -Root $Root
    Write-TuyaUvPlatformBanner -Root $Root
    $managedPython = Invoke-TuyaSetupPython -Root $Root -UvExe $env:OPEN_SDK_UV
    $venvPython    = Invoke-TuyaSetupVenv -Root $Root -UvExe $env:OPEN_SDK_UV -ManagedPythonExe $managedPython
    Set-TuyaSessionEnv -Root $Root -VenvPythonExe $venvPython
    return $venvPython
}

function Invoke-TuyaExportFinalize {
    param(
        [Parameter(Mandatory)][string]$Root,
        [switch]$SkipHello,
        [switch]$SkipReady
    )
    $pythonExe = $env:OPEN_SDK_PYTHON
    if (-not $pythonExe) {
        Write-TuyaOpenFailureHint -Stage Entry -Summary 'OPEN_SDK_PYTHON is not set.' -Cause 'Session environment incomplete.' -NextSteps @('Re-run: . .\export.ps1')
        Stop-TuyaOpenExport 1
    }
    Write-TuyaOpenStage -StageId 'prepare'
    $tosPy = Join-Path $Root 'tos.py'
    & $pythonExe $tosPy prepare
    if ($LASTEXITCODE -ne 0) {
        Write-TuyaOpenInfo '[TuyaOpen] Warning: tos.py prepare failed. Retry: tos.py prepare'
    }
    $makeBinDir = Get-TuyaWindowsMakeBinDir -Root $Root
    $makeExe    = Join-Path $makeBinDir 'make.exe'
    if (Test-Path -LiteralPath $makeExe -PathType Leaf) {
        Add-TuyaPathEntryIfMissing -Dir $makeBinDir
    }
    if (-not $SkipHello) {
        Invoke-TuyaHello -Root $Root -PythonExe $pythonExe
    }
    if (-not $SkipReady) {
        Write-TuyaOpenStage -StageId 'ready'
        Write-TuyaOpenInfo '[TuyaOpen] Ready - tos.py available. Exit: deactivate'
    }
}

function Write-TuyaCmdEnvBat {
    param([Parameter(Mandatory)][string]$OutputPath)
    @(
        '@echo off',
        "set `"OPEN_SDK_ROOT=$($env:OPEN_SDK_ROOT)`"",
        "set `"OPEN_SDK_PYTHON=$($env:OPEN_SDK_PYTHON)`"",
        "set `"OPEN_SDK_PIP=$($env:OPEN_SDK_PIP)`"",
        "set `"OPEN_SDK_UV=$($env:OPEN_SDK_UV)`"",
        "set `"OPEN_SDK_MAKE_BIN=$($env:OPEN_SDK_MAKE_BIN)`"",
        "set `"OPEN_SDK_MAKE=$($env:OPEN_SDK_MAKE)`"",
        "set `"VIRTUAL_ENV=$($env:VIRTUAL_ENV)`"",
        'set "PYTHONHOME="',
        'set "TUYAOPEN_ENV_ACTIVE=1"'
    ) | Set-Content -LiteralPath $OutputPath -Encoding ASCII
}

# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------
if ($env:TUYAOPEN_EXPORT_SKIP_MAIN -eq '1') { return }

$script:TuyaOpenDotSourced = ($MyInvocation.InvocationName -eq '.')
if (-not $script:TuyaOpenDotSourced) {
    Write-TuyaOpenInfo '[TuyaOpen] Tip: dot-source this script: . .\export.ps1'
}

if (-not $env:OPEN_SDK_ROOT) {
    $env:OPEN_SDK_ROOT = Split-Path -Parent $MyInvocation.MyCommand.Path
}
$openRoot = $env:OPEN_SDK_ROOT

try {
    if (-not (Test-TuyaProjectFiles -Root $openRoot)) { Stop-TuyaOpenExport 1 }
    if (-not (Test-TuyaGitAvailable)) { Stop-TuyaOpenExport 1 }
    Set-Location $openRoot

    if (Invoke-TuyaGuardActive -Root $openRoot) {
        # Env vars may be inherited from a parent process, but functions are
        # session-local: re-register tos.py / deactivate so this shell works.
        Register-TuyaOpenCommandHelpers
        Install-TuyaOpenPromptIndicator
        return
    }

    $venvPython = Invoke-TuyaExportSetupCore -Root $openRoot
    Register-TuyaOpenCommandHelpers
    Install-TuyaOpenPwshCompletion -Root $openRoot -PythonExe $venvPython
    Install-TuyaOpenPromptIndicator
    Reset-TuyaSessionCache -Root $openRoot
    Invoke-TuyaExportFinalize -Root $openRoot
} catch {
    if ($_.Exception.Message -match '^\[TuyaOpen\] export aborted \(exit code (\d+)\)\.$') {
        return
    }
    throw
}
