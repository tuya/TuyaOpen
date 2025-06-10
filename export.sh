#!/usr/bin/env bash

# Usage: . ./export.sh
#

OPEN_SDK_ROOT=$(cd "$(dirname "$0")" && pwd)

# create a virtual environment
if [ ! -d "$OPEN_SDK_ROOT/.venv" ]; then
    echo "Creating virtual environment..."
    python -m venv .venv
else
    echo "Virtual environment already exists."
fi

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
