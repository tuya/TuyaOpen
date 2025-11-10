#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse
import json

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
    # Read the JSON file
    with open(input_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    # Check if the required keys exist
    if 'language' not in data or 'type' not in data['language']:
        raise KeyError("The JSON file is missing the 'language' or 'type' key.")
    if 'strings' not in data:
        raise KeyError("The JSON file is missing the 'strings' key.")

    lang_code = data['language']['type']

    lang_code_define = []
    for key, value in data['strings'].items():
        lang_string = f'#define {key} "{value}"'
        lang_code_define.append(lang_string)
    
    # Create the header content
    lang_code_define = "\n".join(lang_code_define)

    print(f"LANG_CODE: {lang_code}")
    print(f"lang_code_define: {lang_code_define}")

    # Generate the header file content
    header_content = HEADER_TEMPLATE.format(
        lang_code_for_font=lang_code.replace("-", "_").lower(),
        lang_code=lang_code,
        lang_code_define=lang_code_define
    )

    # Write to the header file
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(header_content)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, help="Path to the input JSON file")
    parser.add_argument("--output", required=True, help="Path to the output header file")
    args = parser.parse_args()

    generate_header(args.input, args.output)
