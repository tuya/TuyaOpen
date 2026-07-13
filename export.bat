@echo off
setlocal enabledelayedexpansion

:: ===========================================================================
:: Usage: export.bat
:: Set TUYAOPEN_EXPORT_VERBOSE=1 before running for full diagnostic output.
:: Set TUYAOPEN_EXPORT_IDE=1 when invoked by TuyaOpen IDE (passed through to export.ps1).
:: Set TUYAOPEN_CN_DOWNLOAD=1 or 0 to force CN / overseas uv download mirrors (region detect is in export.ps1).
::
:: This script:
::   * locates the TuyaOpen project root (this script's directory),
::   * delegates uv / Python / .venv setup to export.ps1 (pyproject.toml + uv.lock),
::   * exports OPEN_SDK_ROOT / OPEN_SDK_UV / OPEN_SDK_PYTHON / OPEN_SDK_PIP / OPEN_SDK_MAKE_BIN / OPEN_SDK_MAKE,
::   * runs tos.py prepare after the venv is ready,
::   * appends project root and tool paths to PATH so `tos.py` is runnable,
::   * opens an interactive cmd with `tos.py` and `deactivate` aliases.
:: ===========================================================================

:: ---------------------------------------------------------------------------
:: Locate project root (script's directory, no trailing separator)
:: ---------------------------------------------------------------------------
set "OPEN_SDK_ROOT=%~dp0"
set "OPEN_SDK_ROOT=%OPEN_SDK_ROOT:~0,-1%"

:: ---------------------------------------------------------------------------
:: Verify required project files (silent on success)
:: ---------------------------------------------------------------------------
set "MISSING="
if not exist "%OPEN_SDK_ROOT%\export.bat"     set "MISSING=!MISSING! export.bat"
if not exist "%OPEN_SDK_ROOT%\export.ps1"     set "MISSING=!MISSING! export.ps1"
if not exist "%OPEN_SDK_ROOT%\pyproject.toml" set "MISSING=!MISSING! pyproject.toml"
if not exist "%OPEN_SDK_ROOT%\uv.lock"        set "MISSING=!MISSING! uv.lock"
if not exist "%OPEN_SDK_ROOT%\tos.py"         set "MISSING=!MISSING! tos.py"
if defined MISSING (
    echo [TuyaOpen] Error: Entry - Required project files are missing.
    echo Cause: !MISSING!
    echo Next:
    echo   Use a complete TuyaOpen clone.
    echo   Missing under: %OPEN_SDK_ROOT%
    pause
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: PowerShell is required (export.ps1 performs uv / Python / sync setup)
:: ---------------------------------------------------------------------------
where powershell >nul 2>&1
if !errorlevel! neq 0 (
    echo [TuyaOpen] Error: Entry - PowerShell is required to initialize the environment.
    echo Cause: powershell.exe not found on PATH.
    echo Next:
    echo   Install PowerShell 5.1+ or use: . .\export.ps1
    pause
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: git is a hard dependency (platform updates, submodules, version detection)
:: ---------------------------------------------------------------------------
where git >nul 2>&1
if !errorlevel! neq 0 (
    echo [TuyaOpen] Error: Git - git not found. It may not be installed.
    echo Next:
    echo   Open a new terminal and run: winget install Git.Git
    echo   ^(or download from https://git-scm.com/downloads^)
    echo   Then restart this terminal and re-run: export.bat
    pause
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: Guard active (Stage 2): already active → print hint and exit
:: ---------------------------------------------------------------------------
if "%TUYAOPEN_ENV_ACTIVE%"=="1" (
    if /I "%VIRTUAL_ENV%"=="%OPEN_SDK_ROOT%\.venv" (
        if exist "%OPEN_SDK_ROOT%\.venv\Scripts\python.exe" (
            echo [TuyaOpen] Environment is already active.
            echo To re-activate: deactivate ^&^& export.bat
            exit /b 0
        )
    )
)

cd /d "%OPEN_SDK_ROOT%"

:: ---------------------------------------------------------------------------
:: Bootstrap via export.ps1 (SKIP_MAIN=1: call init functions, skip PS prompt)
:: ---------------------------------------------------------------------------
set "TUYA_SID=%RANDOM%%RANDOM%%TIME:~9,2%"
set "TUYA_ENV_BAT=%TEMP%\tuya_env_%TUYA_SID%.bat"
set "TUYA_BOOTSTRAP_PS1=%TEMP%\tuya_bootstrap_%TUYA_SID%.ps1"

> "%TUYA_BOOTSTRAP_PS1%" echo $ErrorActionPreference = 'Stop'
>>"%TUYA_BOOTSTRAP_PS1%" echo $env:TUYAOPEN_EXPORT_SKIP_MAIN = '1'
>>"%TUYA_BOOTSTRAP_PS1%" echo . (Join-Path $env:OPEN_SDK_ROOT 'export.ps1'^)
>>"%TUYA_BOOTSTRAP_PS1%" echo $openRoot = $env:OPEN_SDK_ROOT
>>"%TUYA_BOOTSTRAP_PS1%" echo if (-not (Test-TuyaProjectFiles -Root $openRoot^)^) { exit 1 }
>>"%TUYA_BOOTSTRAP_PS1%" echo Set-Location -LiteralPath $openRoot
>>"%TUYA_BOOTSTRAP_PS1%" echo Invoke-TuyaExportSetupCore -Root $openRoot ^| Out-Null
>>"%TUYA_BOOTSTRAP_PS1%" echo Reset-TuyaSessionCache -Root $openRoot
>>"%TUYA_BOOTSTRAP_PS1%" echo if (-not $env:TUYA_ENV_BAT^) { throw 'TUYA_ENV_BAT not set' }
>>"%TUYA_BOOTSTRAP_PS1%" echo Write-TuyaCmdEnvBat -OutputPath $env:TUYA_ENV_BAT

powershell -NoProfile -NoLogo -ExecutionPolicy Bypass -File "%TUYA_BOOTSTRAP_PS1%"
set "BOOT_EXIT=!errorlevel!"
if exist "%TUYA_BOOTSTRAP_PS1%" del /F /Q "%TUYA_BOOTSTRAP_PS1%" 2>nul

if !BOOT_EXIT! neq 0 (
    echo [TuyaOpen] Error: Setup failed. See messages above.
    if exist "%TUYA_ENV_BAT%" del /F /Q "%TUYA_ENV_BAT%" 2>nul
    pause
    exit /b 1
)

if not exist "%TUYA_ENV_BAT%" (
    echo [TuyaOpen] Error: Session - Environment bootstrap did not produce expected output.
    pause
    exit /b 1
)

call "%TUYA_ENV_BAT%"
del /F /Q "%TUYA_ENV_BAT%" 2>nul

if not exist "%OPEN_SDK_PYTHON%" (
    echo [TuyaOpen] Error: Sync - .venv Python missing after setup.
    echo Cause: %OPEN_SDK_PYTHON%
    pause
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: Host tools (prepare only; hello/ready stay in child cmd alias bat)
:: ---------------------------------------------------------------------------
set "TUYA_PREPARE_PS1=%TEMP%\tuya_prepare_%TUYA_SID%.ps1"
> "%TUYA_PREPARE_PS1%" echo $ErrorActionPreference = 'Stop'
>>"%TUYA_PREPARE_PS1%" echo $env:TUYAOPEN_EXPORT_SKIP_MAIN = '1'
>>"%TUYA_PREPARE_PS1%" echo . (Join-Path $env:OPEN_SDK_ROOT 'export.ps1'^)
>>"%TUYA_PREPARE_PS1%" echo Invoke-TuyaExportFinalize -Root $env:OPEN_SDK_ROOT -SkipHello -SkipReady
powershell -NoProfile -NoLogo -ExecutionPolicy Bypass -File "%TUYA_PREPARE_PS1%"
if exist "%TUYA_PREPARE_PS1%" del /F /Q "%TUYA_PREPARE_PS1%" 2>nul

:: ---------------------------------------------------------------------------
:: PATH: uv tools dir, .venv\Scripts, make bin, project root (idempotent)
:: ---------------------------------------------------------------------------
for %%I in ("%OPEN_SDK_UV%") do set "UV_TOOLS_DIR=%%~dpI"
if defined UV_TOOLS_DIR set "UV_TOOLS_DIR=!UV_TOOLS_DIR:~0,-1!"
if defined UV_TOOLS_DIR call :add_path_if_missing "!UV_TOOLS_DIR!"
call :add_path_if_missing "%OPEN_SDK_ROOT%\.venv\Scripts"
if defined OPEN_SDK_MAKE_BIN if exist "%OPEN_SDK_MAKE_BIN%\make.exe" call :add_path_if_missing "%OPEN_SDK_MAKE_BIN%"
call :add_path_if_missing "%OPEN_SDK_ROOT%"

:: ---------------------------------------------------------------------------
:: Shell bootstrap: defer `tos.py hello` to alias bat so banner appears
:: just above the interactive prompt.
:: ---------------------------------------------------------------------------
:spawn_child

set "TUYA_ALIAS_BAT=%TEMP%\tuya_aliases_%TUYA_SID%.bat"
set "TUYA_DEACTIVATE_BAT=%TEMP%\tuya_deactivate_%TUYA_SID%.bat"

> "%TUYA_DEACTIVATE_BAT%" echo @echo off
>>"%TUYA_DEACTIVATE_BAT%" echo echo Exiting TuyaOpen environment...
>>"%TUYA_DEACTIVATE_BAT%" echo if exist "!OPEN_SDK_ROOT!\.venv\Scripts\deactivate.bat" call "!OPEN_SDK_ROOT!\.venv\Scripts\deactivate.bat"
>>"%TUYA_DEACTIVATE_BAT%" echo set "OPEN_SDK_PYTHON="
>>"%TUYA_DEACTIVATE_BAT%" echo set "OPEN_SDK_PIP="
>>"%TUYA_DEACTIVATE_BAT%" echo set "OPEN_SDK_UV="
>>"%TUYA_DEACTIVATE_BAT%" echo set "OPEN_SDK_MAKE_BIN="
>>"%TUYA_DEACTIVATE_BAT%" echo set "OPEN_SDK_MAKE="
>>"%TUYA_DEACTIVATE_BAT%" echo set "OPEN_SDK_ROOT="
>>"%TUYA_DEACTIVATE_BAT%" echo set "TUYAOPEN_ENV_ACTIVE="
>>"%TUYA_DEACTIVATE_BAT%" echo if defined _OLD_TUYA_CMD_PROMPT prompt %%_OLD_TUYA_CMD_PROMPT%%
>>"%TUYA_DEACTIVATE_BAT%" echo set "_OLD_TUYA_CMD_PROMPT="
>>"%TUYA_DEACTIVATE_BAT%" echo echo TuyaOpen environment deactivated. Re-enter: export.bat
>>"%TUYA_DEACTIVATE_BAT%" echo exit

> "%TUYA_ALIAS_BAT%" echo @echo off
>>"%TUYA_ALIAS_BAT%" echo doskey tos.py^="!OPEN_SDK_PYTHON!" "!OPEN_SDK_ROOT!\tos.py" $*
>>"%TUYA_ALIAS_BAT%" echo doskey deactivate=call "%TUYA_DEACTIVATE_BAT%"
>>"%TUYA_ALIAS_BAT%" echo if not defined _OLD_TUYA_CMD_PROMPT set "_OLD_TUYA_CMD_PROMPT=%%PROMPT%%"
>>"%TUYA_ALIAS_BAT%" echo prompt (TuyaOpen) $P$G

:: Banner already printed by bootstrap (Write-TuyaUvPlatformBanner); only hello in child cmd.
>>"%TUYA_ALIAS_BAT%" echo "%OPEN_SDK_PYTHON%" "%OPEN_SDK_ROOT%\tos.py" hello
>>"%TUYA_ALIAS_BAT%" echo echo [TuyaOpen] Ready - tos.py available. Exit: deactivate

cmd /d /k "%TUYA_ALIAS_BAT%"

if exist "%TUYA_ALIAS_BAT%" del /F /Q "%TUYA_ALIAS_BAT%" 2>nul
if exist "%TUYA_DEACTIVATE_BAT%" del /F /Q "%TUYA_DEACTIVATE_BAT%" 2>nul
goto :eof

:: ===========================================================================
:: Helpers
:: ===========================================================================

:add_path_if_missing
set "ADD_PATH=%~1"
if not defined ADD_PATH exit /b 0
echo ;%PATH%; | findstr /I /C:";%ADD_PATH%;" >nul
if errorlevel 1 set "PATH=%ADD_PATH%;%PATH%"
exit /b 0
