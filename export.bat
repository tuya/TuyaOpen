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
:: Locate a usable Python: 3.9 - 3.13 are officially supported (3.11 recommended).
:: Any Python >= 3.0 is accepted with a warning (e.g. 3.14) instead of blocking.
:: Probe order prefers 3.11, then other supported minors, then generic names.
:: ---------------------------------------------------------------------------
set "PYTHON_CMD="
call :try_python_supported py -3.11
if not defined PYTHON_CMD call :try_python_supported py -3.12
if not defined PYTHON_CMD call :try_python_supported py -3.10
if not defined PYTHON_CMD call :try_python_supported py -3.13
if not defined PYTHON_CMD call :try_python_supported py -3.9
if not defined PYTHON_CMD call :try_python_supported python
if not defined PYTHON_CMD call :try_python_supported python3

if not defined PYTHON_CMD call :try_python_any py
if not defined PYTHON_CMD call :try_python_any python
if not defined PYTHON_CMD call :try_python_any python3

if not defined PYTHON_CMD (
    echo Error: No usable Python interpreter found!
    echo        Please install Python 3.9 - 3.13 ^(3.11 recommended; all in range work^).
    pause
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`%PYTHON_CMD% -c "import sys;print(sys.version_info[0])"`) do set "PY_MAJOR=%%i"
for /f "usebackq tokens=*" %%i in (`%PYTHON_CMD% -c "import sys;print(sys.version_info[1])"`) do set "PY_MINOR=%%i"
for /f "usebackq tokens=*" %%i in (`%PYTHON_CMD% -c "import sys;print('.'.join(map(str,sys.version_info[:3])))"`) do set "PY_VER=%%i"

set "PY_IN_RANGE=0"
if "!PY_MAJOR!"=="3" if !PY_MINOR! geq 9 if !PY_MINOR! leq 13 set "PY_IN_RANGE=1"

:: ---------------------------------------------------------------------------
:: Re-source detection (is our venv already active?)
:: ---------------------------------------------------------------------------
set "IS_RESOURCE=0"
if defined VIRTUAL_ENV (
    if /I "%VIRTUAL_ENV%"=="%OPEN_SDK_ROOT%\.venv" set "IS_RESOURCE=1"
)

:: ---------------------------------------------------------------------------
:: Summary banner
::   Re-source: show "already active" note here (no child cmd is spawned,
::              so the parent cmd IS the shell the user is looking at).
::   First run: DO NOT print here. The child cmd spawned via `cmd /d /k`
::              below prints its own "Microsoft Windows ..." welcome header
::              plus a fresh prompt, and `pip install` / `tos.py hello`
::              output in between easily pushes our banner off-screen.
::              Emit the first-run banner from inside TUYA_ALIAS_BAT instead
::              (see "First-run banner inside the child cmd" below), so it
::              appears right above the interactive prompt the user lands on.
:: ---------------------------------------------------------------------------
if "%IS_RESOURCE%"=="1" (
    echo [TuyaOpen] Note: Virtual environment is already active ^(%VIRTUAL_ENV%^); refreshing environment variables.
)

cd /d "%OPEN_SDK_ROOT%"

:: ---------------------------------------------------------------------------
:: Create / reuse virtualenv
:: Note: Windows "py" may return a negative ERRORLEVEL on failure; "if errorlevel 1"
:: treats that as success because it only checks (ERRORLEVEL >= 1). Use "!errorlevel! neq 0".
:: ---------------------------------------------------------------------------
if exist "%OPEN_SDK_ROOT%\.venv\Scripts\python.exe" goto :venv_ready

if exist "%OPEN_SDK_ROOT%\.venv" (
    echo Removing incomplete virtual environment...
    rd /s /q "%OPEN_SDK_ROOT%\.venv" 2>nul
)

echo Creating virtual environment...
%PYTHON_CMD% -m venv "%OPEN_SDK_ROOT%\.venv"
if !errorlevel! neq 0 (
    echo Error: Failed to create virtual environment!
    echo Please check your Python installation and try again.
    pause
    exit /b 1
)
echo Virtual environment created successfully.

:venv_ready
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
if !errorlevel! neq 0 (
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
:: Greeting / shell bootstrap.
::
:: Re-source (venv already active): no child cmd is spawned, so the user is
::   looking at THIS parent cmd. Print `tos.py hello` here and bail out.
::
:: First run: SKIP `tos.py hello` in the parent and defer it to the alias bat
::   we hand to the child cmd below. That way the order the user actually
::   sees inside the fresh child cmd is:
::     OPEN_SDK_ROOT / Host / [TuyaOpen] Note banner
::     [INFO]: Running tos.py ...  (from tos.py hello)
::     ASCII art, then "ready" / exit lines from this script
::   i.e. environment info FIRST, greeting SECOND. Running hello in the
::   parent here would print it above the child's "Microsoft Windows ..."
::   header and scroll away before the user could read the banner.
::
:: Aliases: DOSKEY macros live only inside the child cmd, so we stage them
:: in a tiny temp bat executed by cmd /d /k on startup. Do NOT define "exit"
:: as a long doskey with $T chains - echo-to-file parsing can corrupt the
:: macro (broken quotes / swallowed chars) and leak text as commands. Use a
:: small helper .bat and: doskey exit=call "helper.bat".
:: cmd /d skips HKCU\...\Command Processor\AutoRun (often contains cls).
:: ---------------------------------------------------------------------------
if not "%IS_RESOURCE%"=="1" goto :spawn_child
"%OPEN_SDK_PYTHON%" "%OPEN_SDK_ROOT%\tos.py" hello
echo tos.py Tool and TuyaOpen SDK is now ready.
echo Exit environment: `exit` or `deactivate`.
goto :eof

:spawn_child

:: ---------------------------------------------------------------------------
:: Session id: 2x %RANDOM% alone only gives ~10^9 combinations and both draws
:: share the same PRNG tick, so add the current time's hundredths-of-seconds
:: slot (%TIME:~9,2%) for extra entropy. Still not cryptographic, but enough
:: to avoid collisions when multiple shells source this script concurrently.
:: ---------------------------------------------------------------------------
set "TUYA_SID=%RANDOM%%RANDOM%%TIME:~9,2%"
set "TUYA_ALIAS_BAT=%TEMP%\tuya_aliases_%TUYA_SID%.bat"
set "TUYA_EXIT_BAT=%TEMP%\tuya_open_exit_%TUYA_SID%.bat"

:: ---------------------------------------------------------------------------
:: Write helper batch files line by line (not inside a `( ... )` block).
:: A parenthesised block terminates early if %OPEN_SDK_ROOT% contains a `)`
:: (e.g. `C:\Program Files (x86)\TuyaOpen`), which corrupts the helper.
:: Outside such a block `)` is a harmless literal character for `echo`,
:: so per-line redirects with delayed expansion are safe.
:: ---------------------------------------------------------------------------
> "%TUYA_EXIT_BAT%" echo @echo off
>>"%TUYA_EXIT_BAT%" echo echo Exiting TuyaOpen environment...
>>"%TUYA_EXIT_BAT%" echo call "!OPEN_SDK_ROOT!\.venv\Scripts\deactivate.bat"
>>"%TUYA_EXIT_BAT%" echo set "OPEN_SDK_PYTHON="
>>"%TUYA_EXIT_BAT%" echo set "OPEN_SDK_PIP="
>>"%TUYA_EXIT_BAT%" echo set "OPEN_SDK_ROOT="
>>"%TUYA_EXIT_BAT%" echo echo TuyaOpen environment deactivated.
>>"%TUYA_EXIT_BAT%" echo exit

> "%TUYA_ALIAS_BAT%" echo @echo off
>>"%TUYA_ALIAS_BAT%" echo doskey tos.py^="!OPEN_SDK_PYTHON!" "!OPEN_SDK_ROOT!\tos.py" $*
>>"%TUYA_ALIAS_BAT%" echo doskey exit=call "%TUYA_EXIT_BAT%"

:: ---------------------------------------------------------------------------
:: First-run banner inside the child cmd.
:: The child cmd's own "Microsoft Windows ..." header is always printed when
:: `cmd /d /k` spawns it, which would otherwise hide banner lines echoed from
:: the parent cmd. Emitting them via the alias bat makes them appear just
:: above the interactive prompt so the user actually sees them.
::
:: IMPORTANT: use %VAR% (not !VAR!) on these lines. Delayed-expansion parsing
:: re-scans the line a second time when it contains `!...!`, which eats one
:: more layer of `^` and turns our escaped pipe back into a real pipe:
::   with !VAR!:  parent `^^^|` -> pass1 `^|` -> pass2 `|`  (pipe! -> py 3.12.10)
::   with %VAR%:  parent `^^^|` -> pass1 `^|`  (written verbatim, child parses)
:: Child bat has no delayed expansion, so its single parse turns `^|`/`^(`/`^)`
:: into literal `|`/`(`/`)`. These vars (OPEN_SDK_ROOT, PY_* ...) are set
:: outside any `( ... )` block and are not mutated after being set, so `%VAR%`
:: and `!VAR!` are equivalent here.
:: ---------------------------------------------------------------------------
>>"%TUYA_ALIAS_BAT%" echo echo OPEN_SDK_ROOT = %OPEN_SDK_ROOT%
>>"%TUYA_ALIAS_BAT%" echo echo Host: Windows %PROCESSOR_ARCHITECTURE% ^^^| %PYTHON_CMD% %PY_VER%
if "%PY_IN_RANGE%"=="0" >>"%TUYA_ALIAS_BAT%" echo echo [TuyaOpen] Warning: Python %PY_MAJOR%.%PY_MINOR% is outside the tested range 3.9 - 3.13; continuing anyway ^^^(3.11 recommended^^^).
if "%PY_IN_RANGE%"=="1" if not "%PY_MINOR%"=="11" >>"%TUYA_ALIAS_BAT%" echo echo [TuyaOpen] Note: Python 3.11 is recommended; 3.9 - 3.13 are supported ^^^(detected 3.%PY_MINOR%^^^).

:: Run tos.py hello AFTER the banner so the visible order in the child cmd is
:: "environment info -> [INFO]: Running tos.py ... -> ASCII art -> ready/exit".
:: Invoked in the child (not the parent) so it appears below the banner
:: instead of scrolling off behind the child cmd's own header.
>>"%TUYA_ALIAS_BAT%" echo "%OPEN_SDK_PYTHON%" "%OPEN_SDK_ROOT%\tos.py" hello
>>"%TUYA_ALIAS_BAT%" echo echo tos.py Tool and TuyaOpen SDK is now ready.
>>"%TUYA_ALIAS_BAT%" echo echo Exit environment: `exit` or `deactivate`.

cmd /d /k "%TUYA_ALIAS_BAT%"

if exist "%TUYA_ALIAS_BAT%" del /F /Q "%TUYA_ALIAS_BAT%" 2>nul
if exist "%TUYA_EXIT_BAT%" del /F /Q "%TUYA_EXIT_BAT%" 2>nul
goto :eof

:: ===========================================================================
:: Helpers
:: ===========================================================================

:try_python_supported
:: Usage: call :try_python_supported <python-command-tokens>
:: Sets PYTHON_CMD=<command> when the command satisfies 3.9 <= ver <= 3.13.
%* -c "import sys;sys.exit(0 if (3,9)<=sys.version_info[:2]<=(3,13) else 1)" >nul 2>&1
if !errorlevel! neq 0 exit /b 0
set "PYTHON_CMD=%*"
exit /b 0

:try_python_any
:: Usage: call :try_python_any <python-command-tokens>
:: Sets PYTHON_CMD=<command> for any Python 3.x interpreter that runs.
%* -c "import sys;sys.exit(0 if sys.version_info[0]>=3 else 1)" >nul 2>&1
if !errorlevel! neq 0 exit /b 0
set "PYTHON_CMD=%*"
exit /b 0
