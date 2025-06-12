@echo off
setlocal enabledelayedexpansion

set OPEN_SDK_ROOT=%~dp0
set OPEN_SDK_ROOT=%OPEN_SDK_ROOT:~0,-1%

:: Debug information
echo OPEN_SDK_ROOT = %OPEN_SDK_ROOT%
echo Current working directory = %CD%

:: change work path
cd /d %OPEN_SDK_ROOT%

:: Function to check Python version
echo Checking Python version...
python --version >nul 2>&1
if errorlevel 1 (
    echo Error: Python is not installed or not in PATH!
    echo Please install Python 3.6.0 or higher.
    pause
    exit /b 1
)

:: Get Python version
for /f "tokens=2" %%i in ('python --version 2^>^&1') do set PYTHON_VERSION=%%i
echo Using Python %PYTHON_VERSION%

:: Check if Python version is 3.6 or higher
for /f "tokens=1,2 delims=." %%a in ("%PYTHON_VERSION%") do (
    set MAJOR=%%a
    set MINOR=%%b
)

if %MAJOR% LSS 3 (
    echo Error: Python version %PYTHON_VERSION% is too old!
    echo Please install Python 3.6.0 or higher.
    pause
    exit /b 1
)

if %MAJOR% EQU 3 if %MINOR% LSS 6 (
    echo Error: Python version %PYTHON_VERSION% is too old!
    echo Please install Python 3.6.0 or higher.
    pause
    exit /b 1
)

:: create a virtual environment
if not exist "%OPEN_SDK_ROOT%\.venv" (
    echo Creating virtual environment...
    python -m venv "%OPEN_SDK_ROOT%\.venv"
    if errorlevel 1 (
        echo Error: Failed to create virtual environment!
        echo Please check your Python installation and try again.
        pause
        exit /b 1
    )
    echo Virtual environment created successfully.
) else (
    echo Virtual environment already exists.
)

:: Verify that the virtual environment was created properly
if not exist "%OPEN_SDK_ROOT%\.venv\Scripts\python.exe" (
    echo Error: Virtual environment Python executable not found at %OPEN_SDK_ROOT%\.venv\Scripts\python.exe
    pause
    exit /b 1
)

if not exist "%OPEN_SDK_ROOT%\.venv\Scripts\pip.exe" (
    echo Error: Virtual environment pip executable not found at %OPEN_SDK_ROOT%\.venv\Scripts\pip.exe
    pause
    exit /b 1
)

:: activate (set PATH to use virtual environment)
echo DEBUG: Activating virtual environment from %OPEN_SDK_ROOT%\.venv\Scripts
set PATH=%OPEN_SDK_ROOT%\.venv\Scripts;%PATH%

:: Verify activation worked by checking if we're using the right Python
for /f %%i in ('where python') do set ACTIVE_PYTHON=%%i
echo Virtual environment activated successfully: %ACTIVE_PYTHON%

:: cmd prompt
set PROMPT=(tos) %PROMPT%

:: Environmental variable
set OPEN_SDK_PYTHON=%OPEN_SDK_ROOT%\.venv\Scripts\python.exe
set OPEN_SDK_PIP=%OPEN_SDK_ROOT%\.venv\Scripts\pip.exe
set PATH=%PATH%;%OPEN_SDK_ROOT%
DOSKEY tos.py=%OPEN_SDK_PYTHON% %OPEN_SDK_ROOT%\tos.py $*

:: install dependencies
echo Installing dependencies...
pip install -r %OPEN_SDK_ROOT%\requirements.txt
if errorlevel 1 (
    echo Warning: Some dependencies may not have been installed correctly.
)

:: remove .env.json
if exist "%OPEN_SDK_ROOT%\.env.json" del /F /Q "%OPEN_SDK_ROOT%\.env.json"

echo ****************************************
echo Exit use: exit
echo ****************************************

:: keep cmd
cmd /k
