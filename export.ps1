<#
    Usage:
      .\export.ps1        - runs in a dedicated child PowerShell session
      . .\export.ps1      - dot-source into the current session (recommended)

    Set $env:TUYAOPEN_EXPORT_VERBOSE = "1" before running for full diagnostics.

    This script:
      * locates the TuyaOpen project root (this script's directory),
      * creates/activates a Python venv in <root>\.venv,
      * installs requirements.txt,
      * exports OPEN_SDK_ROOT / OPEN_SDK_PYTHON / OPEN_SDK_PIP,
      * appends the project root to PATH so `tos.py` is runnable,
      * installs `tos.py` / `deactivate` helper functions in the caller's
        session (global scope), and optionally opens an interactive child
        PowerShell with a `(tos)` prompt when invoked directly (not dot-sourced).
#>

# ---------------------------------------------------------------------------
# Locate project root (script's directory) and invocation style.
# `$MyInvocation.InvocationName` is '.' when dot-sourced; otherwise it is the
# name used to invoke the script (e.g. '.\export.ps1'). We skip spawning a
# child shell when dot-sourced so the user stays in their own session.
# ---------------------------------------------------------------------------
$OpenSdkRoot   = Split-Path -Parent $MyInvocation.MyCommand.Path
$Verbose       = [bool]$env:TUYAOPEN_EXPORT_VERBOSE
$isDotSourced  = ($MyInvocation.InvocationName -eq '.')

# ---------------------------------------------------------------------------
# Verify required project files (silent on success)
# ---------------------------------------------------------------------------
$missing = @()
foreach ($f in 'export.ps1', 'requirements.txt', 'tos.py') {
    if (-not (Test-Path -LiteralPath (Join-Path $OpenSdkRoot $f) -PathType Leaf)) {
        $missing += $f
    }
}
if ($missing.Count -gt 0) {
    Write-Host "Error: Missing required file(s) in ${OpenSdkRoot}: $($missing -join ' ')"
    exit 1
}

# ---------------------------------------------------------------------------
# Locate a usable Python: 3.9 - 3.13 are officially supported (3.11 recommended).
# Any Python 3.x is accepted with a warning (e.g. 3.14) instead of blocking.
# ---------------------------------------------------------------------------
function Test-TuyaPython {
    param(
        [string]$Exec,
        [string[]]$ExtraArgs,
        [string]$Probe = 'import sys;sys.exit(0 if (3,9)<=sys.version_info[:2]<=(3,13) else 1)'
    )
    try {
        # Do not name this $args — it shadows PowerShell's automatic variable.
        $probeArgs = @($ExtraArgs) + @('-c', $Probe)
        & $Exec @probeArgs 2>$null | Out-Null
        return $LASTEXITCODE -eq 0
    } catch {
        return $false
    }
}

$candidates = @(
    @{ Exec = 'py';      Args = @('-3.11') },
    @{ Exec = 'py';      Args = @('-3.12') },
    @{ Exec = 'py';      Args = @('-3.10') },
    @{ Exec = 'py';      Args = @('-3.13') },
    @{ Exec = 'py';      Args = @('-3.9')  },
    @{ Exec = 'python';  Args = @()         },
    @{ Exec = 'python3'; Args = @()         }
)

$pythonExec = $null
$pythonArgs = @()
foreach ($c in $candidates) {
    if (Test-TuyaPython -Exec $c.Exec -ExtraArgs $c.Args) {
        $pythonExec = $c.Exec
        $pythonArgs = $c.Args
        break
    }
}

if (-not $pythonExec) {
    $fallbackProbe = 'import sys;sys.exit(0 if sys.version_info[0]>=3 else 1)'
    foreach ($c in @(
            @{ Exec = 'py';      Args = @() },
            @{ Exec = 'python';  Args = @() },
            @{ Exec = 'python3'; Args = @() }
        )) {
        if (Test-TuyaPython -Exec $c.Exec -ExtraArgs $c.Args -Probe $fallbackProbe) {
            $pythonExec = $c.Exec
            $pythonArgs = $c.Args
            break
        }
    }
}

if (-not $pythonExec) {
    Write-Host "Error: No usable Python interpreter found!"
    Write-Host "       Please install Python 3.9 - 3.13 (3.11 recommended; all in range work)."
    exit 1
}

# Fetch all version fields with a single Python invocation (whitespace-separated:
# "<major> <minor> <major>.<minor>.<patch>").
$pyVerLine = (& $pythonExec @pythonArgs -c "import sys;v=sys.version_info;print('%d %d %d.%d.%d' % (v[0], v[1], v[0], v[1], v[2]))").Trim()
$pyParts   = $pyVerLine -split '\s+', 3
$pyMajor   = $pyParts[0]
$pyMinor   = $pyParts[1]
$pyVersion = $pyParts[2]
$pyInRange = ($pyMajor -eq '3' -and [int]$pyMinor -ge 9 -and [int]$pyMinor -le 13)

# ---------------------------------------------------------------------------
# Re-source detection (is our venv already active?)
# ---------------------------------------------------------------------------
$venvPath   = Join-Path $OpenSdkRoot '.venv'
$isResource = ($env:VIRTUAL_ENV -and $env:VIRTUAL_ENV -eq $venvPath)

# ---------------------------------------------------------------------------
# Summary banner
#   Re-source: show only the "already active" note.
#   First run: show OPEN_SDK_ROOT + Host/Python line + optional rec note.
# ---------------------------------------------------------------------------
if ($isResource) {
    Write-Host "[TuyaOpen] Note: Virtual environment is already active ($env:VIRTUAL_ENV); refreshing environment variables."
} else {
    $arch = if ($env:PROCESSOR_ARCHITECTURE) { $env:PROCESSOR_ARCHITECTURE } else { 'unknown' }
    $pythonDisplay = if ($pythonArgs.Count -gt 0) {
        "$pythonExec $($pythonArgs -join ' ')"
    } else {
        $pythonExec
    }
    Write-Host "OPEN_SDK_ROOT = $OpenSdkRoot"
    Write-Host "Host: Windows $arch | $pythonDisplay $pyVersion"
    if (-not $pyInRange) {
        Write-Host "[TuyaOpen] Warning: Python $pyMajor.$pyMinor is outside the tested range 3.9 - 3.13; continuing anyway (3.11 recommended)."
    } elseif ($pyMinor -ne '11') {
        Write-Host "[TuyaOpen] Note: Python 3.11 is recommended; 3.9 - 3.13 are supported (detected 3.$pyMinor)."
    }
}

Set-Location $OpenSdkRoot

# ---------------------------------------------------------------------------
# Create / reuse virtualenv
# ---------------------------------------------------------------------------
$pythonExe  = Join-Path $venvPath 'Scripts\python.exe'
$venvPythonMissing = -not (Test-Path -LiteralPath $pythonExe -PathType Leaf)
if ($venvPythonMissing -and (Test-Path -LiteralPath $venvPath -PathType Container)) {
    Write-Host "Removing incomplete virtual environment..."
    Remove-Item -LiteralPath $venvPath -Recurse -Force -ErrorAction SilentlyContinue
}

if (-not (Test-Path -LiteralPath $venvPath -PathType Container)) {
    Write-Host "Creating virtual environment..."
    & $pythonExec @pythonArgs -m venv $venvPath
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Failed to create virtual environment!"
        Write-Host "Please check your Python installation and try again."
        exit 1
    }
    Write-Host "Virtual environment created successfully."
}

$python3Exe = Join-Path $venvPath 'Scripts\python3.exe'
$pipExe     = Join-Path $venvPath 'Scripts\pip.exe'
$scriptsDir = Join-Path $venvPath 'Scripts'

if (-not (Test-Path -LiteralPath $pythonExe)) {
    Write-Host "Error: Virtual environment Python executable not found: $pythonExe"
    exit 1
}
if (-not (Test-Path -LiteralPath $python3Exe)) {
    Copy-Item -LiteralPath $pythonExe -Destination $python3Exe
}

# ---------------------------------------------------------------------------
# Activate venv: set env vars and idempotently update PATH
# ---------------------------------------------------------------------------
$env:VIRTUAL_ENV     = $venvPath
$env:OPEN_SDK_ROOT   = $OpenSdkRoot
$env:OPEN_SDK_PYTHON = $pythonExe
$env:OPEN_SDK_PIP    = $pipExe

# PowerShell's -notcontains is case-sensitive by default; Windows paths are not.
# Use case-insensitive comparison so re-sourcing does not append duplicates.
if (-not (($env:PATH -split ';') | Where-Object { $_ -ieq $scriptsDir })) {
    $env:PATH = "$scriptsDir;$env:PATH"
}
if (-not (($env:PATH -split ';') | Where-Object { $_ -ieq $OpenSdkRoot })) {
    $env:PATH = "$env:PATH;$OpenSdkRoot"
}

# ---------------------------------------------------------------------------
# Install dependencies
# ---------------------------------------------------------------------------
$reqFile = Join-Path $OpenSdkRoot 'requirements.txt'
if ($Verbose) {
    & $pipExe install -r $reqFile
} else {
    & $pipExe install -q -r $reqFile
}
if ($LASTEXITCODE -ne 0) {
    Write-Host "Warning: Some dependencies may not have been installed correctly."
}

# ---------------------------------------------------------------------------
# Clean stale cache files
# ---------------------------------------------------------------------------
$cachePath = Join-Path $OpenSdkRoot '.cache'
New-Item -ItemType Directory -Path $cachePath -Force -ErrorAction SilentlyContinue | Out-Null
foreach ($name in '.env.json', '.dont_prompt_update_platform') {
    $p = Join-Path $cachePath $name
    if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Force }
}

# ---------------------------------------------------------------------------
# Install tos.py / deactivate helpers into the caller's session (global scope).
#
# `function global:*` registers the function at session (process) scope so it
# survives whether this script was dot-sourced or invoked directly. This is
# what makes `tos.py <subcommand>` work in the user's own PowerShell after a
# dot-source or a re-source (isResource), not only inside the spawned child.
#
# The helpers read $env:OPEN_SDK_PYTHON / $env:OPEN_SDK_ROOT at call time
# rather than the $pythonExe / $OpenSdkRoot captured at source time, so they
# continue to work correctly across re-sources.
# ---------------------------------------------------------------------------
function global:tos.py {
    if (-not $env:OPEN_SDK_PYTHON -or -not $env:OPEN_SDK_ROOT) {
        Write-Host 'TuyaOpen environment is not active. Source export.ps1 first.'
        return
    }
    & $env:OPEN_SDK_PYTHON (Join-Path $env:OPEN_SDK_ROOT 'tos.py') @args
}

function global:__tuyaTeardown {
    param([switch]$Silent)
    if (-not $Silent) { Write-Host 'Exiting TuyaOpen environment...' }
    if ($env:OPEN_SDK_ROOT) {
        $sdkRoot    = $env:OPEN_SDK_ROOT
        $sdkScripts = Join-Path $sdkRoot '.venv\Scripts'
        # Remove exactly the two entries we appended, case-insensitively.
        $env:PATH = (($env:PATH -split ';') |
            Where-Object { $_ -and ($_ -ine $sdkRoot) -and ($_ -ine $sdkScripts) }
        ) -join ';'
    }
    Remove-Item Env:VIRTUAL_ENV     -ErrorAction SilentlyContinue
    Remove-Item Env:OPEN_SDK_ROOT   -ErrorAction SilentlyContinue
    Remove-Item Env:OPEN_SDK_PYTHON -ErrorAction SilentlyContinue
    Remove-Item Env:OPEN_SDK_PIP    -ErrorAction SilentlyContinue
    # Restore the original prompt (saved on activation by the block below).
    if (Test-Path function:_OLD_TUYA_PROMPT) {
        Copy-Item -Path function:_OLD_TUYA_PROMPT -Destination function:prompt -Force -ErrorAction SilentlyContinue
        Remove-Item function:_OLD_TUYA_PROMPT -Force -ErrorAction SilentlyContinue
    }
    Remove-Item 'function:\tos.py'     -Force -ErrorAction SilentlyContinue
    Remove-Item 'function:\deactivate' -Force -ErrorAction SilentlyContinue
    Remove-Item 'function:\deactive'   -Force -ErrorAction SilentlyContinue
    if (-not $Silent) { Write-Host 'TuyaOpen environment deactivated.' }
}

function global:deactivate { __tuyaTeardown }

# Tolerate the common misspelling `deactive`.
function global:deactive   { deactivate }

# ---------------------------------------------------------------------------
# Prompt decoration, modeled after Python's venv `Activate.ps1`.
# Back up the caller's existing `prompt` once (so re-sourcing does not
# double-wrap and lose the real original), then install a wrapper that
# prefixes `(.venv) ` and delegates to the original prompt. `deactivate`
# (via __tuyaTeardown above) restores it and deletes the backup.
# ---------------------------------------------------------------------------
if (-not (Test-Path function:_OLD_TUYA_PROMPT)) {
    if (Test-Path function:prompt) {
        Copy-Item -Path function:prompt -Destination function:_OLD_TUYA_PROMPT
    }
}
function global:prompt {
    Write-Host -NoNewline -ForegroundColor Green '(.venv) '
    if (Test-Path function:_OLD_TUYA_PROMPT) {
        & $function:_OLD_TUYA_PROMPT
    } else {
        'PS ' + $ExecutionContext.SessionState.Path.CurrentLocation + '> '
    }
}

# ---------------------------------------------------------------------------
# Greeting banner (via tos.py hello; ASCII/version only). "Ready" + exit hint
# are printed here so PowerShell can recommend `deactivate` only — `exit` is
# a language keyword and cannot be wrapped like in cmd/bash.
# ---------------------------------------------------------------------------
& $pythonExe (Join-Path $OpenSdkRoot 'tos.py') hello
Write-Host 'tos.py Tool and TuyaOpen SDK is now ready.'
Write-Host 'Exit environment: `deactivate`.'

# ---------------------------------------------------------------------------
# Decide whether to spawn an interactive child shell with a `(.venv)` prompt:
#   - isResource (venv already active): skip - the user is refreshing an
#     environment that is already set up in the current shell.
#   - isDotSourced: skip - the user explicitly asked to integrate the env
#     into their current session, so dumping them into a child would be
#     surprising (and the helpers above already cover their session).
#   - otherwise (plain `.\export.ps1` invocation): spawn a child so the
#     user gets the dedicated venv prompt.
# ---------------------------------------------------------------------------
if ($isResource -or $isDotSourced) { return }

# ---------------------------------------------------------------------------
# Pick the same PowerShell host as the parent so pwsh (PS 7) users are not
# silently downgraded to Windows PowerShell 5.1. Fall back to the edition
# default if the current process path cannot be resolved.
# ---------------------------------------------------------------------------
$hostExe = $null
try { $hostExe = (Get-Process -Id $PID).Path } catch { $hostExe = $null }
if (-not $hostExe -or -not (Test-Path -LiteralPath $hostExe -PathType Leaf)) {
    $hostExe = if ($PSVersionTable.PSEdition -eq 'Core') { 'pwsh' } else { 'powershell' }
}

# ---------------------------------------------------------------------------
# Create the child bootstrap script with a non-colliding, self-cleaning name.
# [IO.Path]::GetTempFileName() creates an extra *.tmp file we would have to
# clean up; [guid] gives us a unique name without the stray file.
# ---------------------------------------------------------------------------
$tempScript = Join-Path ([IO.Path]::GetTempPath()) ("tuya_export_{0}.ps1" -f [guid]::NewGuid().ToString('N'))

$scriptContent = @"
`$__tuyaOrigPath = `$env:PATH

`$env:VIRTUAL_ENV     = '$venvPath'
`$env:OPEN_SDK_ROOT   = '$OpenSdkRoot'
`$env:OPEN_SDK_PYTHON = '$pythonExe'
`$env:OPEN_SDK_PIP    = '$pipExe'

# The parent already appended scriptsDir / OpenSdkRoot to PATH before spawning
# this child, so the child inherits an updated PATH. Guard against duplicates
# with the same case-insensitive dedupe logic the parent uses.
if (-not ((`$env:PATH -split ';') | Where-Object { `$_ -ieq '$scriptsDir' })) {
    `$env:PATH = '$scriptsDir;' + `$env:PATH
}
if (-not ((`$env:PATH -split ';') | Where-Object { `$_ -ieq '$OpenSdkRoot' })) {
    `$env:PATH = `$env:PATH + ';$OpenSdkRoot'
}

Set-Location '$OpenSdkRoot'

# Same prompt decoration scheme as the dot-sourced path (mirrors Python's
# venv Activate.ps1): back up the host's default prompt, install a wrapper
# that prefixes `(.venv) `, delegate to the original for the rest.
if (-not (Test-Path function:_OLD_TUYA_PROMPT)) {
    if (Test-Path function:prompt) {
        Copy-Item -Path function:prompt -Destination function:_OLD_TUYA_PROMPT
    }
}
function global:prompt {
    Write-Host -NoNewline -ForegroundColor Green '(.venv) '
    if (Test-Path function:_OLD_TUYA_PROMPT) {
        & `$function:_OLD_TUYA_PROMPT
    } else {
        'PS ' + `$ExecutionContext.SessionState.Path.CurrentLocation + '> '
    }
}

# Read env vars at call time (matches the parent-session helper so both
# stay in sync even if the user reassigns OPEN_SDK_PYTHON / OPEN_SDK_ROOT).
function global:tos.py {
    if (-not `$env:OPEN_SDK_PYTHON -or -not `$env:OPEN_SDK_ROOT) {
        Write-Host 'TuyaOpen environment is not active.'
        return
    }
    & `$env:OPEN_SDK_PYTHON (Join-Path `$env:OPEN_SDK_ROOT 'tos.py') @args
}

# Centralised teardown so `deactivate` and the engine Exiting event stay in
# sync (both restore PATH, prompt, and clear every env var we added).
function global:__tuyaTeardown {
    param([switch]`$Silent)
    if (-not `$Silent) { Write-Host 'Exiting TuyaOpen environment...' }
    if (Test-Path function:_OLD_TUYA_PROMPT) {
        Copy-Item -Path function:_OLD_TUYA_PROMPT -Destination function:prompt -Force -ErrorAction SilentlyContinue
        Remove-Item function:_OLD_TUYA_PROMPT -Force -ErrorAction SilentlyContinue
    }
    if (`$__tuyaOrigPath) { `$env:PATH = `$__tuyaOrigPath }
    Remove-Item Env:VIRTUAL_ENV     -ErrorAction SilentlyContinue
    Remove-Item Env:OPEN_SDK_ROOT   -ErrorAction SilentlyContinue
    Remove-Item Env:OPEN_SDK_PYTHON -ErrorAction SilentlyContinue
    Remove-Item Env:OPEN_SDK_PIP    -ErrorAction SilentlyContinue
    Remove-Item 'function:\tos.py'     -Force -ErrorAction SilentlyContinue
    Remove-Item 'function:\deactivate' -Force -ErrorAction SilentlyContinue
    Remove-Item 'function:\deactive'   -Force -ErrorAction SilentlyContinue
    if (-not `$Silent) { Write-Host 'TuyaOpen environment deactivated.' }
}

function global:deactivate { __tuyaTeardown }

# Tolerate the common misspelling `deactive`.
function global:deactive   { deactivate }

# `exit` is a PowerShell keyword and cannot be reliably overridden by a
# function, so hook session teardown via the engine Exiting event instead.
# This fires for `exit`, window close, or any other way the host is torn down.
Register-EngineEvent PowerShell.Exiting -SupportEvent -Action {
    try { __tuyaTeardown -Silent } catch { }
} | Out-Null
"@

# -Encoding UTF8 writes a BOM on Windows PowerShell 5.1 which PS 5.1 itself
# reads fine; OEM/ASCII avoids the BOM but may mangle non-ASCII paths. UTF8
# is the correct default for the content we emit here.
$scriptContent | Out-File -FilePath $tempScript -Encoding UTF8
try {
    & $hostExe -NoExit -NoProfile -File $tempScript
} finally {
    Remove-Item -LiteralPath $tempScript -Force -ErrorAction SilentlyContinue
}
