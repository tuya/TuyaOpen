# Check if already in virtual environment by checking if current Python is in .venv
try {
    $currentPython = (Get-Command python -ErrorAction SilentlyContinue).Source
    if ($currentPython -and $currentPython.Contains(".venv")) {
        Write-Host "Virtual environment is already activated."
        exit 0
    }
} catch {
    # Python not found, continue with setup
}

# Set script root directory
$OPEN_SDK_ROOT = Split-Path -Parent $MyInvocation.MyCommand.Path

# Debug information
Write-Host "OPEN_SDK_ROOT = $OPEN_SDK_ROOT"
Write-Host "Current working directory = $(Get-Location)"

# Change working directory
Set-Location $OPEN_SDK_ROOT

# Check Python version
Write-Host "Checking Python version..."
try {
    $pythonVersion = python --version 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Python is not installed or not in PATH!"
        Write-Host "Please install Python 3.6.0 or higher."
        Read-Host "Press any key to continue"
        exit 1
    }
    Write-Host "Using Python $pythonVersion"
} catch {
    Write-Host "Error: Python is not installed or not in PATH!"
    Write-Host "Please install Python 3.6.0 or higher."
    Read-Host "Press any key to continue"
    exit 1
}

# Get Python version number
$versionMatch = $pythonVersion -match "Python (\d+)\.(\d+)"
if ($versionMatch) {
    $major = [int]$matches[1]
    $minor = [int]$matches[2]

    if ($major -lt 3) {
        Write-Host "Error: Python version $pythonVersion is too old!"
        Write-Host "Please install Python 3.6.0 or higher."
        Read-Host "Press any key to continue"
        exit 1
    }

    if ($major -eq 3 -and $minor -lt 6) {
        Write-Host "Error: Python version $pythonVersion is too old!"
        Write-Host "Please install Python 3.6.0 or higher."
        Read-Host "Press any key to continue"
        exit 1
    }
}

# Create virtual environment
$venvPath = Join-Path $OPEN_SDK_ROOT ".venv"
if (-not (Test-Path $venvPath)) {
    Write-Host "Creating virtual environment..."
    try {
        python -m venv $venvPath
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Error: Failed to create virtual environment!"
            Write-Host "Please check your Python installation and try again."
            Read-Host "Press any key to continue"
            exit 1
        }
        Write-Host "Virtual environment created successfully."
    } catch {
        Write-Host "Error: Failed to create virtual environment!"
        Write-Host "Please check your Python installation and try again."
        Read-Host "Press any key to continue"
        exit 1
    }
} else {
    Write-Host "Virtual environment already exists."
}

# Verify virtual environment was created properly
$pythonExe = Join-Path $venvPath "Scripts\python.exe"
$python3Exe = Join-Path $venvPath "Scripts\python3.exe"
$pipExe = Join-Path $venvPath "Scripts\pip.exe"

if (-not (Test-Path $pythonExe)) {
    Write-Host "Error: Virtual environment Python executable not found: $pythonExe"
    Read-Host "Press any key to continue"
    exit 1
}

if (-not (Test-Path $pipExe)) {
    Write-Host "Error: Virtual environment pip executable not found: $pipExe"
    Read-Host "Press any key to continue"
    exit 1
}

# Copy python.exe to python3.exe for compatibility
if (-not (Test-Path $python3Exe)) {
    Copy-Item $pythonExe $python3Exe
}

# Activate virtual environment (set PATH to use virtual environment)
Write-Host "DEBUG: Activating virtual environment from $venvPath\Scripts"
$env:PATH = "$venvPath\Scripts;$env:PATH"

# Verify activation worked by checking if we're using the right Python
$activePython = (Get-Command python).Source
Write-Host "Virtual environment activated successfully: $activePython"

# Environmental variables
$env:OPEN_SDK_PYTHON = $pythonExe
$env:OPEN_SDK_PIP = $pipExe
$env:PATH = "$env:PATH;$OPEN_SDK_ROOT"

# Install dependencies
Write-Host "Installing dependencies..."
$requirementsPath = Join-Path $OPEN_SDK_ROOT "requirements.txt"
if (Test-Path $requirementsPath) {
    try {
        & $env:OPEN_SDK_PIP install -r $requirementsPath
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Warning: Some dependencies may not have been installed correctly."
        }
    } catch {
        Write-Host "Warning: Error occurred while installing dependencies."
    }
} else {
    Write-Host "Warning: requirements.txt file not found."
}

# Remove cache files
$cachePath = Join-Path $OPEN_SDK_ROOT ".cache"
New-Item -ItemType Directory -Path $cachePath -Force -ErrorAction SilentlyContinue | Out-Null
$envJsonPath = Join-Path $cachePath ".env.json"
if (Test-Path $envJsonPath) {
    Remove-Item $envJsonPath -Force
}
$dontUpdatePlatform = Join-Path $cachePath ".dont_prompt_update_platform"
if (Test-Path $dontUpdatePlatform) {
    Remove-Item $dontUpdatePlatform -Force
}

# hello tuya
$HELLO_TUYA = '
 ______                 ____
/_  __/_ ____ _____ _  / __ \___  ___ ___
 / / / // / // / _ `/ / /_/ / _ \/ -_) _ \
/_/  \_,_/\_, /\_,_/  \____/ .__/\__/_//_/
         /___/            /_/
'
Write-Host "****************************************"
Write-Host $HELLO_TUYA
Write-Host "Exit use: exit"
Write-Host "****************************************"

# Create a temporary script file for the new session
$tempScript = [System.IO.Path]::GetTempFileName() + ".ps1"
$scriptContent = @"
# Set environment variables
`$env:OPEN_SDK_PYTHON = '$pythonExe'
`$env:OPEN_SDK_PIP = '$pipExe'
`$env:PATH = '$venvPath\Scripts;' + `$env:PATH
`$env:PATH = `$env:PATH + ';$OPEN_SDK_ROOT'

# Set working directory
Set-Location '$OPEN_SDK_ROOT'

# Create custom prompt function (equivalent to set PROMPT=(tos) %PROMPT% in batch)
function prompt {
    "(tos) `$(`$executionContext.SessionState.Path.CurrentLocation)`$('>' * (`$nestedPromptLevel + 1)) "
}

# Create tos.py function
function tos.py {
    & `$env:OPEN_SDK_PYTHON '$OPEN_SDK_ROOT\tos.py' `$args
}

# Create exit function to clean up environment
function exit {
    Write-Host "Exiting TuyaOpen environment..."

    # Clean up environment variables
    Remove-Item Env:OPEN_SDK_PYTHON -ErrorAction SilentlyContinue
    Remove-Item Env:OPEN_SDK_PIP -ErrorAction SilentlyContinue
    Remove-Item Env:OPEN_SDK_ROOT -ErrorAction SilentlyContinue

    Write-Host "TuyaOpen environment deactivated."

    # Exit PowerShell
    [Environment]::Exit(0)
}

"@

# Write the script content to temporary file
$scriptContent | Out-File -FilePath $tempScript -Encoding UTF8

# Start interactive PowerShell session with the temporary script
Write-Host "Starting interactive session with virtual environment..."
Write-Host "tmpScript $tempScript"
powershell -NoExit -File $tempScript

# Clean up temporary file
Remove-Item $tempScript -Force -ErrorAction SilentlyContinue