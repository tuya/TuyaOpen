#!/bin/bash

# GIF Batch Conversion Script
# Converts all GIF files in assets/gif to C arrays in gif/

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Paths
PYTHON_SCRIPT="$PROJECT_ROOT/script/gif2Carray.py"
INPUT_DIR="$PROJECT_ROOT/assets/gif"
OUTPUT_DIR="$PROJECT_ROOT/gif"

echo -e "${BLUE}=== GIF to C Array Batch Converter ===${NC}"
echo ""

# Check if Python script exists
if [ ! -f "$PYTHON_SCRIPT" ]; then
    echo -e "${RED}Error: Python script not found at $PYTHON_SCRIPT${NC}"
    exit 1
fi

# Check if input directory exists
if [ ! -d "$INPUT_DIR" ]; then
    echo -e "${RED}Error: Input directory not found at $INPUT_DIR${NC}"
    exit 1
fi

# Create output directory if it doesn't exist
if [ ! -d "$OUTPUT_DIR" ]; then
    echo -e "${YELLOW}Creating output directory: $OUTPUT_DIR${NC}"
    mkdir -p "$OUTPUT_DIR"
fi

# Step 1: Remove all existing .c files in output directory
echo -e "${YELLOW}Step 1: Cleaning up old C files...${NC}"
C_FILES_COUNT=$(find "$OUTPUT_DIR" -maxdepth 1 -name "*.c" -type f | wc -l)
if [ $C_FILES_COUNT -gt 0 ]; then
    echo -e "  Removing $C_FILES_COUNT .c file(s) from $OUTPUT_DIR"
    find "$OUTPUT_DIR" -maxdepth 1 -name "*.c" -type f -delete
    echo -e "${GREEN}  Cleanup complete${NC}"
else
    echo -e "  No .c files to remove"
fi
echo ""

# Step 2: Convert all GIF files
echo -e "${YELLOW}Step 2: Converting GIF files...${NC}"
GIF_COUNT=$(find "$INPUT_DIR" -maxdepth 1 -name "*.gif" -type f | wc -l)

if [ $GIF_COUNT -eq 0 ]; then
    echo -e "${YELLOW}  No GIF files found in $INPUT_DIR${NC}"
    exit 0
fi

echo -e "  Found $GIF_COUNT GIF file(s) to convert"
echo ""

SUCCESS_COUNT=0
FAIL_COUNT=0

# Loop through all GIF files
for gif_file in "$INPUT_DIR"/*.gif; do
    # Get filename without path
    filename=$(basename "$gif_file")
    # Get filename without extension
    basename_no_ext="${filename%.gif}"
    # Output C file path
    output_file="$OUTPUT_DIR/${basename_no_ext}.c"

    echo -e "${BLUE}Converting: $filename${NC}"

    # Run Python script
    if python3 "$PYTHON_SCRIPT" "$gif_file" -o "$output_file"; then
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
        echo -e "${RED}  Failed to convert $filename${NC}"
    fi
    echo ""
done

# Summary
echo -e "${BLUE}=== Conversion Complete ===${NC}"
echo -e "${GREEN}Success: $SUCCESS_COUNT${NC}"
if [ $FAIL_COUNT -gt 0 ]; then
    echo -e "${RED}Failed:  $FAIL_COUNT${NC}"
fi
echo -e "Output directory: $OUTPUT_DIR"

exit 0
