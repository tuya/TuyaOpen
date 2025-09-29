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
