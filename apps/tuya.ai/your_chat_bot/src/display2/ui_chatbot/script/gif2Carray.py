#!/usr/bin/env python3
"""
GIF to C Array Converter for LVGL
Converts GIF files to C source files compatible with LVGL's GIF decoder.
"""

import os
import sys
import argparse
import logging
from pathlib import Path

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(levelname)s: %(message)s'
)


def gif_to_c_array(gif_path, output_path=None, var_name=None):
    """
    Convert a GIF file to a C source file with LVGL format.

    Args:
        gif_path: Path to input GIF file
        output_path: Path to output C file (optional, defaults to same name with .c extension)
        var_name: Variable name for the array (optional, defaults to filename without extension)
    """
    gif_path = Path(gif_path)

    if not gif_path.exists():
        logging.error(f"File '{gif_path}' not found")
        return False

    if not gif_path.suffix.lower() == '.gif':
        logging.warning(f"File '{gif_path}' does not have .gif extension")

    # Generate output path if not provided
    if output_path is None:
        output_path = gif_path.with_suffix('.c')
    else:
        output_path = Path(output_path)

    # Generate variable name if not provided
    if var_name is None:
        var_name = gif_path.stem.lower()
        # Replace invalid characters with underscores
        var_name = ''.join(c if c.isalnum() or c == '_' else '_' for c in var_name)
        # Ensure it doesn't start with a number
        if var_name[0].isdigit():
            var_name = '_' + var_name

    # Read the GIF file as binary
    try:
        with open(gif_path, 'rb') as f:
            gif_data = f.read()
    except Exception as e:
        logging.error(f"Error reading file: {e}")
        return False

    data_len = len(gif_data)

    # Generate C source code
    try:
        with open(output_path, 'w', encoding='utf-8') as f:
            # Write header
            f.write('#include "lvgl/lvgl.h"\n')
            f.write('#include <stdint.h>\n')
            f.write('\n')

            # Write data array
            f.write(f'const uint8_t gif_{var_name}_map[] = {{\n')

            # Write data in rows of 19 bytes (as in the original format)
            bytes_per_line = 19
            for i in range(0, data_len, bytes_per_line):
                # Get the chunk of bytes for this line
                chunk = gif_data[i:i + bytes_per_line]

                # Format as hex values
                hex_values = ', '.join(f'0x{byte:02x}' for byte in chunk)

                # Add comma at the end if not the last line
                if i + bytes_per_line < data_len:
                    f.write(f'    {hex_values},\n')
                else:
                    f.write(f'    {hex_values}\n')

            f.write('};\n')
            f.write('\n')

            # Write length variable
            f.write(f'const uint32_t gif_{var_name}_map_len = {data_len};\n')
            f.write('\n')

            # Write LVGL image descriptor
            f.write(f'const lv_image_dsc_t gif_{var_name} = {{\n')
            f.write('    .header.cf    = LV_COLOR_FORMAT_RAW, // GIF raw stream\n')
            f.write('    .header.magic = LV_IMAGE_HEADER_MAGIC,\n')
            f.write('    .header.w     = 0, // Can be set to 0, GIF header will be read during decoding\n')
            f.write('    .header.h     = 0,\n')
            f.write(f'    .data_size    = gif_{var_name}_map_len,\n')
            f.write(f'    .data         = gif_{var_name}_map,\n')
            f.write('};\n')

        logging.info(f"Successfully converted '{gif_path}' to '{output_path}'")
        logging.info(f"  Variable name: gif_{var_name}")
        logging.info(f"  Data size: {data_len} bytes")
        return True

    except Exception as e:
        logging.error(f"Error writing output file: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description='Convert GIF files to C arrays for LVGL',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  python gif2Carray.py input.gif
  python gif2Carray.py input.gif -o output.c
  python gif2Carray.py input.gif -n my_animation
  python gif2Carray.py *.gif
        '''
    )

    parser.add_argument('input', nargs='+', help='Input GIF file(s)')
    parser.add_argument('-o', '--output', help='Output C file path (for single file only)')
    parser.add_argument('-n', '--name', help='Variable name (for single file only)')
    parser.add_argument('-v', '--verbose', action='store_true', help='Enable verbose output (DEBUG level)')
    parser.add_argument('-q', '--quiet', action='store_true', help='Quiet mode (ERROR level only)')

    args = parser.parse_args()

    # Configure logging level based on arguments
    if args.quiet:
        logging.getLogger().setLevel(logging.ERROR)
    elif args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    # If multiple files, ignore output and name arguments
    if len(args.input) > 1:
        if args.output or args.name:
            logging.warning("--output and --name are ignored when processing multiple files")

        success_count = 0
        for gif_file in args.input:
            if gif_to_c_array(gif_file):
                success_count += 1

        logging.info(f"Processed {success_count}/{len(args.input)} files successfully")
    else:
        # Single file
        gif_to_c_array(args.input[0], args.output, args.name)


if __name__ == '__main__':
    main()
