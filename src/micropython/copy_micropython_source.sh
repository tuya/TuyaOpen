#!/bin/bash

# Script to copy MicroPython source code to TuyaOpen project
# Maintains the original directory structure under mpy/ directory

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Copying MicroPython Source to TuyaOpen${NC}"
echo -e "${GREEN}========================================${NC}"

# Define source and destination paths
MPY_SOURCE="/home/huatuo/work/open/mcpy/micropython"
TUYA_DEST="/home/huatuo/work/open/mcpy/TuyaOpen/src/micropython/mpy"

# Create destination directory
mkdir -p ${TUYA_DEST}

echo -e "${YELLOW}Source: ${MPY_SOURCE}${NC}"
echo -e "${YELLOW}Destination: ${TUYA_DEST}${NC}"
echo ""

# Function to copy directory with progress
copy_directory() {
    local src=$1
    local name=$2
    
    if [ -d "${MPY_SOURCE}/${src}" ]; then
        echo -e "${YELLOW}Copying ${name}...${NC}"
        
        # Create destination directory
        mkdir -p "${TUYA_DEST}/${src}"
        
        # Count files for progress
        local file_count=$(find "${MPY_SOURCE}/${src}" -type f | wc -l)
        
        # Copy with rsync for better progress tracking
        rsync -av --progress "${MPY_SOURCE}/${src}/" "${TUYA_DEST}/${src}/" 2>&1 | \
            grep -E '^[^/]|/$' | \
            tail -n 20
        
        echo -e "${GREEN}✓ ${name} copied (${file_count} files)${NC}"
    else
        echo -e "${RED}✗ ${name} not found at ${MPY_SOURCE}/${src}${NC}"
    fi
    echo ""
}

# Copy py/ directory (core VM + generation tools)
echo -e "${GREEN}1. Copying py/ directory (Core VM + Generation Tools)${NC}"
copy_directory "py" "Python VM Core"

# List generation tools in py/
echo -e "${YELLOW}Generation tools in py/:${NC}"
ls -la ${TUYA_DEST}/py/*.py 2>/dev/null | grep -E "make.*\.py|.*\.py$" | awk '{print "  - " $9}'
echo ""

# Copy shared/ directory
echo -e "${GREEN}2. Copying shared/ directory (Shared Components)${NC}"
copy_directory "shared" "Shared Components"

# Copy extmod/ directory
echo -e "${GREEN}3. Copying extmod/ directory (Extension Modules)${NC}"
copy_directory "extmod" "Extension Modules"

# Copy lib/ directory
echo -e "${GREEN}4. Copying lib/ directory (Third-party Libraries)${NC}"
copy_directory "lib" "Third-party Libraries"

# Copy tools/ directory
echo -e "${GREEN}5. Copying tools/ directory (Additional Tools)${NC}"
copy_directory "tools" "Additional Tools"

# Copy ports/minimal as reference
echo -e "${GREEN}6. Copying ports/minimal as reference${NC}"
if [ -d "${MPY_SOURCE}/ports/minimal" ]; then
    echo -e "${YELLOW}Copying minimal port for reference...${NC}"
    mkdir -p "${TUYA_DEST}/ports"
    rsync -av "${MPY_SOURCE}/ports/minimal/" "${TUYA_DEST}/ports/minimal/" 2>&1 | \
        grep -E '^[^/]|/$' | \
        tail -n 10
    echo -e "${GREEN}✓ Minimal port copied${NC}"
fi
echo ""

# Create a README in mpy directory
cat > ${TUYA_DEST}/README.md << 'EOF'
# MicroPython Source Code

This directory contains the complete MicroPython source code, maintaining the original directory structure.

## Directory Structure

- **py/** - Core Python VM implementation and generation tools
  - Contains makeqstrdata.py, makeversionhdr.py, etc.
- **shared/** - Shared components (runtime, libc, readline)
- **extmod/** - Extension modules
- **lib/** - Third-party libraries
- **tools/** - Additional tools and utilities
- **ports/minimal/** - Reference minimal port

## Important Notes

1. All files maintain their original relative paths from the MicroPython repository
2. Generation tools are in the py/ directory, not tools/
3. This is a complete copy to allow selective compilation via CMake

## Version Information

Copied from MicroPython repository at: $(date)
EOF

echo -e "${GREEN}✓ README.md created${NC}"
echo ""

# Summary
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Copy Complete! Summary:${NC}"
echo -e "${GREEN}========================================${NC}"

# Count total files
TOTAL_FILES=$(find ${TUYA_DEST} -type f | wc -l)
TOTAL_SIZE=$(du -sh ${TUYA_DEST} | cut -f1)

echo "Total files copied: ${TOTAL_FILES}"
echo "Total size: ${TOTAL_SIZE}"
echo ""

# Verify critical files
echo -e "${YELLOW}Verifying critical files:${NC}"
CRITICAL_FILES=(
    "py/makeqstrdata.py"
    "py/makeversionhdr.py"
    "py/makemoduledefs.py"
    "py/make_root_pointers.py"
    "py/makecompresseddata.py"
    "py/compile.c"
    "py/runtime.c"
    "py/gc.c"
    "py/vm.c"
    "py/qstr.h"
    "py/mpconfig.h"
    "shared/runtime/pyexec.c"
    "shared/readline/readline.c"
)

MISSING=0
for file in "${CRITICAL_FILES[@]}"; do
    if [ -f "${TUYA_DEST}/${file}" ]; then
        echo -e "  ${GREEN}✓${NC} ${file}"
    else
        echo -e "  ${RED}✗${NC} ${file} - MISSING!"
        MISSING=$((MISSING + 1))
    fi
done

echo ""
if [ $MISSING -eq 0 ]; then
    echo -e "${GREEN}All critical files verified successfully!${NC}"
else
    echo -e "${RED}Warning: ${MISSING} critical files missing!${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Next steps:${NC}"
echo "1. Create header generation scripts"
echo "2. Configure CMakeLists.txt for selective compilation"
echo "3. Implement platform adaptation layer"
echo -e "${GREEN}========================================${NC}"