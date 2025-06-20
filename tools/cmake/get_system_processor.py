#!/usr/bin/env python3
"""
Get system processor information for CMake
"""

import platform
import sys


def get_system_processor():
    """Get the system processor architecture"""
    processor = platform.machine()

    # Handle different naming conventions
    if processor == "x86_64":
        return "x86_64"
    elif processor == "AMD64":
        return "x86_64"
    elif processor == "aarch64":
        return "arm64"
    elif processor == "arm64":
        return "arm64"
    elif processor == "i386":
        return "x86"
    elif processor == "i686":
        return "x86"
    else:
        return processor


if __name__ == "__main__":
    processor = get_system_processor()
    print(processor)
    sys.exit(0)
