<#
    Usage: .\export.ps1
    Set $env:TUYAOPEN_EXPORT_VERBOSE = "1" before running for full diagnostics.

    This script:
      * locates the TuyaOpen project root (this script's directory),
      * creates/activates a Python venv in <root>\.venv,
      * installs requirements.txt,
      * exports OPEN_SDK_ROOT / OPEN_SDK_PYTHON / OPEN_SDK_PIP,
      * appends the project root to PATH so `tos.py` is runnable,
      * opens an interactive PowerShell session with tos.py / exit / deactivate
        functions wired up.
#>

# ---------------------------------------------------------------------------
# Locate project root (script's directory)
# ---------------------------------------------------------------------------
$OpenSdkRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Verbose = [bool]$env:TUYAOPEN_EXPORT_VERBOSE

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
    Write-Host "Error: Missing required file(s) in $OpenSdkRoot: $($missing -join ' ')"
    exit 1
}

# ---------------------------------------------------------------------------
# Locate a usable Python: supported range 3.9 - 3.13 (recommended: 3.11)
# ---------------------------------------------------------------------------
function Test-TuyaPython {
    param([string]$Exec, [string[]]$ExtraArgs)
    try {
        $probe = @($ExtraArgs) + @(
            '-c',
            'import sys;sys.exit(0 if (3,9)<=sys.version_info[:2]<=(3,13) else 1)'
        )
        & $Exec @probe 2>$null | Out-Null
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
    Write-Host "Error: No suitable Python version found!"
    Write-Host "       Please install Python 3.9 - 3.13 (recommended: 3.11)."
    exit 1
}

$pyVersion = (& $pythonExec @pythonArgs -c "import sys;print('.'.join(map(str,sys.version_info[:3])))").Trim()
$pyMinor   = (& $pythonExec @pythonArgs -c "import sys;print(sys.version_info[1])").Trim()

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
    if ($pyMinor -ne '11') {
        Write-Host "[TuyaOpen] Note: Python 3.11 is recommended (detected 3.$pyMinor)."
    }
}

Set-Location $OpenSdkRoot

# ---------------------------------------------------------------------------
# Create / reuse virtualenv
# ---------------------------------------------------------------------------
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

$pythonExe  = Join-Path $venvPath 'Scripts\python.exe'
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

$pathParts = $env:PATH -split ';'
if ($pathParts -notcontains $scriptsDir)  { $env:PATH = "$scriptsDir;$env:PATH" }
if (($env:PATH -split ';') -notcontains $OpenSdkRoot) { $env:PATH = "$env:PATH;$OpenSdkRoot" }

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
# Greeting banner (via tos.py hello; prints TuyaOpen version inside the art)
# ---------------------------------------------------------------------------
& $pythonExe (Join-Path $OpenSdkRoot 'tos.py') hello

# ---------------------------------------------------------------------------
# If already inside the activated shell, just return to caller.
# Otherwise spawn an interactive PowerShell with tos.py / exit / deactivate
# helper functions wired up.
# ---------------------------------------------------------------------------
if ($isResource) { return }

$tempScript = [System.IO.Path]::GetTempFileName() + '.ps1'
$scriptContent = @"
`$env:VIRTUAL_ENV     = '$venvPath'
`$env:OPEN_SDK_ROOT   = '$OpenSdkRoot'
`$env:OPEN_SDK_PYTHON = '$pythonExe'
`$env:OPEN_SDK_PIP    = '$pipExe'
`$env:PATH            = '$scriptsDir;' + `$env:PATH + ';$OpenSdkRoot'

Set-Location '$OpenSdkRoot'

function global:prompt {
    '(tos) ' + `$(`$ExecutionContext.SessionState.Path.CurrentLocation) + (' >' * (`$nestedPromptLevel + 1)) + ' '
}

function global:tos.py {
    & '$pythonExe' '$OpenSdkRoot\tos.py' @args
}

function global:deactivate {
    Write-Host 'Exiting TuyaOpen environment...'
    Remove-Item Env:VIRTUAL_ENV     -ErrorAction SilentlyContinue
    Remove-Item Env:OPEN_SDK_ROOT   -ErrorAction SilentlyContinue
    Remove-Item Env:OPEN_SDK_PYTHON -ErrorAction SilentlyContinue
    Remove-Item Env:OPEN_SDK_PIP    -ErrorAction SilentlyContinue
    Write-Host 'TuyaOpen environment deactivated.'
}

# `exit` is a PowerShell keyword and cannot be reliably overridden by a
# function, so hook session teardown via the engine Exiting event instead.
# This fires for `exit`, window close, or any other way the host is torn down.
Register-EngineEvent PowerShell.Exiting -SupportEvent -Action {
    Remove-Item Env:VIRTUAL_ENV     -ErrorAction SilentlyContinue
    Remove-Item Env:OPEN_SDK_ROOT   -ErrorAction SilentlyContinue
    Remove-Item Env:OPEN_SDK_PYTHON -ErrorAction SilentlyContinue
    Remove-Item Env:OPEN_SDK_PIP    -ErrorAction SilentlyContinue
} | Out-Null
"@

$scriptContent | Out-File -FilePath $tempScript -Encoding UTF8
try {
    & powershell -NoExit -File $tempScript
} finally {
    Remove-Item -LiteralPath $tempScript -Force -ErrorAction SilentlyContinue
}
