#!/usr/bin/env bash

# Usage: . ./export.sh
#

OPEN_SDK_ROOT=$(cd "$(dirname "$0")" && pwd)

# Debug information
echo "OPEN_SDK_ROOT = $OPEN_SDK_ROOT"
echo "Current working directory = $(pwd)"

# Function to check Python version
check_python_version() {
    local python_cmd="$1"
    if command -v "$python_cmd" >/dev/null 2>&1; then
        local version=$($python_cmd -c "import sys; print('.'.join(map(str, sys.version_info[:3])))" 2>/dev/null)
        if [ $? -eq 0 ]; then
            local major=$(echo "$version" | cut -d. -f1)
            local minor=$(echo "$version" | cut -d. -f2)
            local patch=$(echo "$version" | cut -d. -f3)
            # Check if version >= 3.6.0
            if [ "$major" -eq 3 ] && [ "$minor" -ge 6 ]; then
                echo "$python_cmd"
                return 0
            elif [ "$major" -gt 3 ]; then
                echo "$python_cmd"
                return 0
            fi
        fi
    fi
    return 1
}

# Determine which Python command to use
PYTHON_CMD=""
if check_python_version "python3" >/dev/null 2>&1; then
    PYTHON_CMD=$(check_python_version "python3")
    echo "Using python3 ($(python3 --version))"
elif check_python_version "python" >/dev/null 2>&1; then
    PYTHON_CMD=$(check_python_version "python")
    echo "Using python ($(python --version))"
else
    echo "Error: No suitable Python version found!"
    echo "Please install Python 3.6.0 or higher."
    return 1
fi

# Change to the script directory to ensure relative paths work correctly
cd "$OPEN_SDK_ROOT"

# create a virtual environment
if [ ! -d "$OPEN_SDK_ROOT/.venv" ]; then
    echo "Creating virtual environment..."
    $PYTHON_CMD -m venv "$OPEN_SDK_ROOT/.venv"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to create virtual environment!"
        echo "Please check your Python installation and try again."
        return 1
    fi
    echo "Virtual environment created successfully."
else
    echo "Virtual environment already exists."
fi

# Verify that the virtual environment was created properly
if [ ! -f "$OPEN_SDK_ROOT/.venv/bin/activate" ]; then
    echo "Error: Virtual environment activation script not found at $OPEN_SDK_ROOT/.venv/bin/activate"
    return 1
fi


# activate
echo "DEBUG: Activating virtual environment from $OPEN_SDK_ROOT/.venv/bin/activate"
. ${OPEN_SDK_ROOT}/.venv/bin/activate
export OPEN_SDK_PYTHON=${OPEN_SDK_ROOT}/.venv/bin/python
export OPEN_SDK_PIP=${OPEN_SDK_ROOT}/.venv/bin/pip

# Verify activation worked
if [ -z "$VIRTUAL_ENV" ]; then
    echo "Error: Failed to activate virtual environment"
    return 1
fi
echo "Virtual environment activated successfully: $VIRTUAL_ENV"

# install dependencies
pip install -r ./requirements.txt

# remove .env.json
rm -f ${OPEN_SDK_ROOT}/.env.json

echo "****************************************"
echo "Exit use: deactivate"
echo "****************************************"
