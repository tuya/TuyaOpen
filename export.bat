@echo off
setlocal enabledelayedexpansion

:: ===========================================================================
:: Usage: export.bat
:: Set TUYAOPEN_EXPORT_VERBOSE=1 before running for full diagnostic output.
::
:: This script:
::   * locates the TuyaOpen project root (this script's directory),
::   * creates/activates a Python venv in <root>\.venv,
::   * installs requirements.txt,
::   * exports OPEN_SDK_ROOT / OPEN_SDK_PYTHON / OPEN_SDK_PIP,
::   * appends the project root to PATH so `tos.py` is runnable,
::   * opens an interactive cmd with `tos.py`, `exit`, `deactivate` aliases.
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
if not exist "%OPEN_SDK_ROOT%\export.bat"       set "MISSING=!MISSING! export.bat"
if not exist "%OPEN_SDK_ROOT%\requirements.txt" set "MISSING=!MISSING! requirements.txt"
if not exist "%OPEN_SDK_ROOT%\tos.py"           set "MISSING=!MISSING! tos.py"
if defined MISSING (
    echo Error: Missing required file^(s^) in %OPEN_SDK_ROOT%:!MISSING!
    pause
    exit /b 1
)

:: ---------------------------------------------------------------------------
:: Locate a usable Python: supported range 3.9 - 3.13 (recommended: 3.11)
:: Probe order prefers 3.11, then other supported minors, then generic names.
:: ---------------------------------------------------------------------------
set "PYTHON_CMD="
call :try_python py -3.11
if not defined PYTHON_CMD call :try_python py -3.12
if not defined PYTHON_CMD call :try_python py -3.10
if not defined PYTHON_CMD call :try_python py -3.13
if not defined PYTHON_CMD call :try_python py -3.9
if not defined PYTHON_CMD call :try_python python
if not defined PYTHON_CMD call :try_python python3
if not defined PYTHON_CMD (
    echo Error: No suitable Python version found!
    echo        Please install Python 3.9 - 3.13 ^(recommended: 3.11^).
    pause
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`%PYTHON_CMD% -c "import sys;print(sys.version_info[1])"`) do set "PY_MINOR=%%i"
for /f "usebackq tokens=*" %%i in (`%PYTHON_CMD% -c "import sys;print('.'.join(map(str,sys.version_info[:3])))"`) do set "PY_VER=%%i"

:: ---------------------------------------------------------------------------
:: Re-source detection (is our venv already active?)
:: ---------------------------------------------------------------------------
set "IS_RESOURCE=0"
if defined VIRTUAL_ENV (
    if /I "%VIRTUAL_ENV%"=="%OPEN_SDK_ROOT%\.venv" set "IS_RESOURCE=1"
)

:: ---------------------------------------------------------------------------
:: Summary banner
::   Re-source: show only the "already active" note.
::   First run: show OPEN_SDK_ROOT + Host/Python line + optional rec note.
:: ---------------------------------------------------------------------------
if "%IS_RESOURCE%"=="1" (
    echo [TuyaOpen] Note: Virtual environment is already active ^(%VIRTUAL_ENV%^); refreshing environment variables.
) else (
    echo OPEN_SDK_ROOT = %OPEN_SDK_ROOT%
    echo Host: Windows %PROCESSOR_ARCHITECTURE% ^| %PYTHON_CMD% %PY_VER%
    if not "!PY_MINOR!"=="11" (
        echo [TuyaOpen] Note: Python 3.11 is recommended ^(detected 3.!PY_MINOR!^).
    )
)

cd /d "%OPEN_SDK_ROOT%"

:: ---------------------------------------------------------------------------
:: Create / reuse virtualenv
:: ---------------------------------------------------------------------------
if exist "%OPEN_SDK_ROOT%\.venv" goto :venv_exists
echo Creating virtual environment...
%PYTHON_CMD% -m venv "%OPEN_SDK_ROOT%\.venv"
if errorlevel 1 (
    echo Error: Failed to create virtual environment!
    echo Please check your Python installation and try again.
    pause
    exit /b 1
)
echo Virtual environment created successfully.
:venv_exists

if not exist "%OPEN_SDK_ROOT%\.venv\Scripts\python.exe" (
    echo Error: Virtual environment Python executable not found at %OPEN_SDK_ROOT%\.venv\Scripts\python.exe
    pause
    exit /b 1
)

:: python3.exe alias for tool compatibility
if not exist "%OPEN_SDK_ROOT%\.venv\Scripts\python3.exe" (
    copy /Y "%OPEN_SDK_ROOT%\.venv\Scripts\python.exe" "%OPEN_SDK_ROOT%\.venv\Scripts\python3.exe" >nul
)

:: ---------------------------------------------------------------------------
:: Activate venv (use the venv's native activate.bat so deactivate.bat works)
:: ---------------------------------------------------------------------------
call "%OPEN_SDK_ROOT%\.venv\Scripts\activate.bat"

:: Append project root to PATH only if not already present (idempotent).
echo ;%PATH%; | findstr /I /C:";%OPEN_SDK_ROOT%;" >nul
if errorlevel 1 set "PATH=%PATH%;%OPEN_SDK_ROOT%"

set "OPEN_SDK_PYTHON=%OPEN_SDK_ROOT%\.venv\Scripts\python.exe"
set "OPEN_SDK_PIP=%OPEN_SDK_ROOT%\.venv\Scripts\pip.exe"

:: ---------------------------------------------------------------------------
:: Install dependencies
:: ---------------------------------------------------------------------------
if defined TUYAOPEN_EXPORT_VERBOSE (
    "%OPEN_SDK_PIP%" install -r "%OPEN_SDK_ROOT%\requirements.txt"
) else (
    "%OPEN_SDK_PIP%" install -q -r "%OPEN_SDK_ROOT%\requirements.txt"
)
if errorlevel 1 (
    echo Warning: Some dependencies may not have been installed correctly.
)

:: ---------------------------------------------------------------------------
:: Clean stale cache files
:: ---------------------------------------------------------------------------
set "CACHE_PATH=%OPEN_SDK_ROOT%\.cache"
if not exist "%CACHE_PATH%" mkdir "%CACHE_PATH%" 2>nul
if exist "%CACHE_PATH%\.env.json" del /F /Q "%CACHE_PATH%\.env.json"
if exist "%CACHE_PATH%\.dont_prompt_update_platform" del /F /Q "%CACHE_PATH%\.dont_prompt_update_platform"

:: ---------------------------------------------------------------------------
:: Greeting banner (via tos.py hello; prints TuyaOpen version inside the art)
:: ---------------------------------------------------------------------------
"%OPEN_SDK_PYTHON%" "%OPEN_SDK_ROOT%\tos.py" hello

:: ---------------------------------------------------------------------------
:: If already inside the activated shell, just return.
:: Otherwise spawn an interactive cmd with `tos.py`, `exit` aliases wired up.
:: The venv's activate.bat has already made `deactivate` available natively.
:: DOSKEY macros live only inside the child cmd process, so we stage them in
:: a tiny temporary batch file that cmd /k executes on startup.
:: ---------------------------------------------------------------------------
if "%IS_RESOURCE%"=="1" goto :eof

set "TUYA_ALIAS_BAT=%TEMP%\tuya_aliases_%RANDOM%.bat"
> "%TUYA_ALIAS_BAT%" echo @echo off
>>"%TUYA_ALIAS_BAT%" echo doskey tos.py^="%OPEN_SDK_PYTHON%" "%OPEN_SDK_ROOT%\tos.py" $*
>>"%TUYA_ALIAS_BAT%" echo doskey exit=echo Exiting TuyaOpen environment... $T call deactivate $T set OPEN_SDK_PYTHON^= $T set OPEN_SDK_PIP^= $T set OPEN_SDK_ROOT^= $T echo TuyaOpen environment deactivated. $T exit

cmd /k "%TUYA_ALIAS_BAT%"

if exist "%TUYA_ALIAS_BAT%" del /F /Q "%TUYA_ALIAS_BAT%" 2>nul
goto :eof

:: ===========================================================================
:: Helpers
:: ===========================================================================

:try_python
:: Usage: call :try_python <python-command-tokens>
:: Sets PYTHON_CMD=<command> when the command satisfies 3.9 <= ver <= 3.13.
%* -c "import sys;sys.exit(0 if (3,9)<=sys.version_info[:2]<=(3,13) else 1)" >nul 2>&1
if errorlevel 1 exit /b 0
set "PYTHON_CMD=%*"
exit /b 0
