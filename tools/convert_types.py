#!/usr/bin/env python3
"""Convert Tuya legacy type aliases to standard C types."""

import re
import sys
import argparse
from pathlib import Path

# Order matters: longer/more-specific patterns before shorter ones to avoid partial matches
REPLACEMENTS = [
    # 64-bit (before 32/16/8 variants)
    ("UINT64_T", "uint64_t"),
    ("INT64_T",  "int64_t"),
    # 32-bit
    ("UINT32_T", "uint32_t"),
    ("INT32_T",  "int32_t"),
    # 16-bit
    ("UINT16_T", "uint16_t"),
    ("INT16_T",  "int16_t"),
    ("SHORT_T",  "int16_t"),
    ("USHORT_T", "uint16_t"),
    ("PWORD_T",  "uint16_t *"),
    ("WORD_T",   "uint16_t"),
    # 32-bit pointer aliases (before base types)
    ("PDWORD_T", "uint32_t *"),
    ("DWORD_T",  "uint32_t"),
    # 8-bit
    ("UINT8_T",  "uint8_t"),
    ("INT8_T",   "int8_t"),
    ("SCHAR_T",  "int8_t"),
    ("UCHAR_T",  "uint8_t"),
    ("BYTE_T",   "uint8_t"),
    # other sized aliases (after the explicit-width ones)
    ("UINT_T",   "uint32_t"),
    ("INT_T",    "int32_t"),
    ("ULONG_T",  "unsigned long"),
    # void/char/bool (VOID_T before VOID)
    ("PVOID_T",  "void *"),
    ("VOID_T",   "void"),
    ("VOID",     "void"),
    ("CHAR_T",   "char"),
    # float/size
    ("FLOAT_T",  "float"),
    ("SIZE_T",   "size_t"),
    # storage class / qualifier
    ("INLINE",   "inline"),
    ("CONST",    "const"),
    ("STATIC",   "static"),
    # parameter direction annotations (empty macros — remove them)
    # INOUT must come before IN/OUT to avoid partial match
    ("INOUT",    ""),
    ("IN",       ""),
    ("OUT",      ""),
]

# Pre-compile patterns with word boundaries
PATTERNS = [(re.compile(r"\b" + old + r"\b"), new) for old, new in REPLACEMENTS]


def convert_text(text):
    for pattern, new in PATTERNS:
        text = pattern.sub(new, text)
    return text


def convert_file(path, dry_run=False):
    original = path.read_text(encoding="utf-8", errors="replace")
    converted = convert_text(original)
    if converted == original:
        return 0
    if dry_run:
        print(f"[dry-run] {path}")
    else:
        path.write_text(converted, encoding="utf-8")
        print(f"converted: {path}")
    return 1


def collect_files(targets):
    c_exts = {".c", ".h", ".cpp", ".hpp", ".cc", ".cxx"}
    for t in targets:
        p = Path(t)
        if p.is_file():
            yield p
        elif p.is_dir():
            for ext in c_exts:
                yield from p.rglob(f"*{ext}")
        else:
            # glob pattern
            from glob import glob
            for match in glob(t, recursive=True):
                mp = Path(match)
                if mp.is_file() and mp.suffix in c_exts:
                    yield mp


def main():
    parser = argparse.ArgumentParser(description="Convert Tuya legacy types to standard C types")
    parser.add_argument("targets", nargs="+", help="Files, directories, or glob patterns")
    parser.add_argument("-n", "--dry-run", action="store_true", help="Show files that would change without modifying them")
    args = parser.parse_args()

    changed = sum(convert_file(f, dry_run=args.dry_run) for f in collect_files(args.targets))
    print(f"\n{'Would change' if args.dry_run else 'Changed'} {changed} file(s).")


if __name__ == "__main__":
    main()
