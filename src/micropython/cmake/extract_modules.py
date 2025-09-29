#!/usr/bin/env python3
"""Extract MP_REGISTER_MODULE statements from source files."""

import sys
import re
import os

def extract_modules(source_files, output_file, pattern):
    """Extract module registration statements from source files."""

    # Regular expression to match MP_REGISTER_MODULE or MP_REGISTER_ROOT_POINTER
    if pattern == "MP_REGISTER_MODULE":
        regex = re.compile(r'MP_REGISTER_MODULE\([^)]+\);', re.MULTILINE | re.DOTALL)
    elif pattern == "MP_REGISTER_ROOT_POINTER":
        regex = re.compile(r'MP_REGISTER_ROOT_POINTER\([^)]+\);', re.MULTILINE | re.DOTALL)
    else:
        # Generic pattern
        regex = re.compile(pattern)

    found_items = []

    # Process each source file
    for src_file in source_files:
        if not os.path.exists(src_file):
            continue

        try:
            with open(src_file, 'r', encoding='utf-8') as f:
                content = f.read()

            # Find all matches
            matches = regex.findall(content)
            for match in matches:
                # Clean up whitespace
                clean_match = ' '.join(match.split())
                found_items.append(clean_match)
                print(f"Found in {os.path.basename(src_file)}: {clean_match[:-1]}")  # Print without semicolon

        except Exception as e:
            print(f"Error processing {src_file}: {e}", file=sys.stderr)

    # Write results to output file
    with open(output_file, 'w') as f:
        for item in found_items:
            f.write(item + '\n')

    print(f"Extracted {len(found_items)} items to {output_file}")

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("Usage: extract_modules.py <output_file> <pattern> <source_file1> [source_file2 ...]", file=sys.stderr)
        sys.exit(1)

    output_file = sys.argv[1]
    pattern = sys.argv[2]
    source_files = sys.argv[3:]

    extract_modules(source_files, output_file, pattern)