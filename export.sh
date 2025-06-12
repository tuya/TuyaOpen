#!/usr/bin/env bash

# Usage: . ./export.sh
#

OPEN_SDK_ROOT=$(cd "$(dirname "$0")" && pwd)

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

# create a virtual environment
if [ ! -d "$OPEN_SDK_ROOT/.venv" ]; then
    echo "Creating virtual environment..."
    $PYTHON_CMD -m venv .venv
    if [ $? -ne 0 ]; then
        echo "Error: Failed to create virtual environment!"
        echo "Please check your Python installation and try again."
        return 1
    fi
    echo "Virtual environment created successfully."
else
    echo "Virtual environment already exists."
fi
export OPEN_SDK_PYTHON=${OPEN_SDK_ROOT}/.venv/bin/python
export OPEN_SDK_PIP=${OPEN_SDK_ROOT}/.venv/bin/pip

# activate
. ${OPEN_SDK_ROOT}/.venv/bin/activate

# install dependencies
pip install -r ./requirements.txt

export PATH=$PATH:${OPEN_SDK_ROOT}

# remove .env.json
rm -f ${OPEN_SDK_ROOT}/.env.json

echo "****************************************"
echo "Exit use: deactivate"
echo "****************************************"
