#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse
import json
import logging

HEADER_TEMPLATE = """// Auto-generated language config
#ifndef __LANGUAGE_CONFIG_H__
#define __LANGUAGE_CONFIG_H__

#include <stdio.h>

#ifndef {lang_code_for_font}
    #define {lang_code_for_font}
#endif

#ifdef __cplusplus
extern "C" {{
#endif

#define LANG_CODE "{lang_code}"

{lang_code_define}

#ifdef __cplusplus
}}
#endif

#endif // __LANGUAGE_CONFIG_H
"""

def generate_header(input_path, output_path):
    logging.info(f"Reading input file: {input_path}")
    # Read the JSON file
    try:
        with open(input_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except FileNotFoundError:
        logging.error(f"Input file not found: {input_path}")
        raise
    except json.JSONDecodeError as e:
        logging.error(f"Failed to parse JSON file: {e}")
        raise

    # Check if the required keys exist
    if 'language' not in data or 'type' not in data['language']:
        logging.error("The JSON file is missing the 'language' or 'type' key.")
        raise KeyError("The JSON file is missing the 'language' or 'type' key.")
    if 'strings' not in data:
        logging.error("The JSON file is missing the 'strings' key.")
        raise KeyError("The JSON file is missing the 'strings' key.")

    lang_code = data['language']['type']
    logging.info(f"Language code: {lang_code}")

    lang_code_define = []
    for key, value in data['strings'].items():
        lang_string = f'#define {key} "{value}"'
        lang_code_define.append(lang_string)
    
    # Create the header content
    lang_code_define = "\n".join(lang_code_define)
    logging.debug(f"Generated {len(data['strings'])} language string definitions")

    # Generate the header file content
    header_content = HEADER_TEMPLATE.format(
        lang_code_for_font=lang_code.replace("-", "_").lower(),
        lang_code=lang_code,
        lang_code_define=lang_code_define
    )

    # Write to the header file
    logging.info(f"Writing output file: {output_path}")
    try:
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(header_content)
        logging.info("Header file generated successfully")
    except IOError as e:
        logging.error(f"Failed to write output file: {e}")
        raise

if __name__ == "__main__":
    # Configure logging
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(levelname)s - %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S'
    )
    
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, help="Path to the input JSON file")
    parser.add_argument("--output", required=True, help="Path to the output header file")
    parser.add_argument("--verbose", "-v", action="store_true", help="Enable verbose logging")
    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    try:
        generate_header(args.input, args.output)
    except Exception as e:
        logging.error(f"Failed to generate header file: {e}")
        raise
