@echo off
setlocal

set OPEN_SDK_ROOT=%~dp0
set OPEN_SDK_ROOT=%OPEN_SDK_ROOT:~0,-1%

:: change work path
cd /d %OPEN_SDK_ROOT%

:: create a virtual environment
if not exist .venv (
    echo Creating virtual environment...
    python -m venv .venv
) else (
    echo Virtual environment already exists.
)

:: activate
::call %OPEN_SDK_ROOT%\.venv\Scripts\activate.bat
set PATH=%OPEN_SDK_ROOT%\.venv\Scripts;%PATH%

:: cmd prompt
set PROMPT=(tos) %PROMPT%

:: Environmental variable
set OPEN_SDK_PYTHON=%OPEN_SDK_ROOT%\.venv\Scripts\python.exe
set OPEN_SDK_PIP=%OPEN_SDK_ROOT%\.venv\Scripts\pip.exe
set PATH=%PATH%;%OPEN_SDK_ROOT%
DOSKEY tos.py=%OPEN_SDK_PYTHON% %OPEN_SDK_ROOT%\tos.py $*

:: install dependencies
pip install -r %OPEN_SDK_ROOT%\requirements.txt

:: remove .env.json
if exist "%OPEN_SDK_ROOT%\.env.json" del /F /Q "%OPEN_SDK_ROOT%\.env.json"

echo ****************************************
echo Exit use: exit
echo ****************************************

:: keep cmd
cmd /k
