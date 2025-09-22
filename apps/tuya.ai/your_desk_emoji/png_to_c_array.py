#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PNG to C Array Tool
Convert PNG images to C language array format for embedded system display
"""

import os
import sys
from PIL import Image
import argparse

def png_to_c_array(png_path, output_path, array_name, target_size=None):
    """
    Convert PNG image to C array
    
    Args:
        png_path: PNG image path
        output_path: Output C file path
        array_name: C array name
        target_size: Target size (width, height), if None then keep original size
    """
    try:
        # Open PNG image
        img = Image.open(png_path)
        
        # Convert to RGB mode (if not already)
        if img.mode != 'RGB':
            img = img.convert('RGB')
        
        # If target size is specified, resize the image
        if target_size:
            img = img.resize(target_size, Image.Resampling.LANCZOS)
        
        # Get image dimensions
        width, height = img.size
        
        # Get pixel data
        pixels = list(img.getdata())
        
        # Generate C array
        c_code = f"""#ifdef __has_include
#if __has_include("lvgl.h")
#ifndef LV_LVGL_H_INCLUDE_SIMPLE
#define LV_LVGL_H_INCLUDE_SIMPLE
#endif
#endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMAGE_{array_name.upper()}
#define LV_ATTRIBUTE_IMAGE_{array_name.upper()}
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_{array_name.upper()} uint8_t
    {array_name}_map[] = {{
"""
        
        # Convert pixels to RAW_CHROMA_KEYED format (RGB565)
        raw_data = []
        for pixel in pixels:
            r, g, b = pixel
            
            # Convert to RGB565 format (5-6-5)
            r = (r >> 3) & 0x1F  # 5 bits
            g = (g >> 2) & 0x3F  # 6 bits
            b = (b >> 3) & 0x1F  # 5 bits
            
            # Combine into 16-bit RGB565
            rgb565 = (r << 11) | (g << 5) | b
            
            # Store as 2 bytes: RGB565 (little-endian)
            raw_data.extend([rgb565 & 0xFF, (rgb565 >> 8) & 0xFF])
        
        # 16 bytes per line (8 pixels * 2 bytes)
        for i in range(0, len(raw_data), 16):
            line_data = raw_data[i:i+16]
            c_code += "    " + ", ".join([f"0x{x:02X}" for x in line_data]) + ",\n"
        
        c_code += "};\n\n"
        
        # Add image descriptor
        c_code += f"""const lv_image_dsc_t {array_name} = {{
    .header.cf = LV_COLOR_FORMAT_RGB565,
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.w = {width},
    .header.h = {height},
    .data_size = {len(raw_data)},
    .data = {array_name}_map,
}};\n"""
        
        # Write to file
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(c_code)
        
        print(f"Conversion successful: {png_path} -> {output_path}")
        print(f"Image size: {width}x{height}")
        print(f"Data size: {len(raw_data)} bytes ({len(raw_data)//2} pixels)")
        
    except Exception as e:
        print(f"Conversion failed: {e}")
        return False
    
    return True

def main():
    parser = argparse.ArgumentParser(description='PNG to C Array Tool')
    parser.add_argument('input_dir', help='Input PNG image directory')
    parser.add_argument('output_dir', help='Output C file directory')
    parser.add_argument('--size', help='Target size, format WxH, e.g. 64x64')
    
    args = parser.parse_args()
    
    # Parse size parameter
    target_size = None
    if args.size:
        try:
            width, height = map(int, args.size.split('x'))
            target_size = (width, height)
        except ValueError:
            print("Error: Size format should be WxH, e.g. 64x64")
            return
    
    # Ensure output directory exists
    os.makedirs(args.output_dir, exist_ok=True)
    
    # Process all PNG files in directory
    png_files = [f for f in os.listdir(args.input_dir) if f.lower().endswith('.png')]
    
    if not png_files:
        print("No PNG files found")
        return
    
    print(f"Found {len(png_files)} PNG files")
    
    for png_file in png_files:
        png_path = os.path.join(args.input_dir, png_file)
        
        # Generate array name (remove extension, convert to underscore format)
        base_name = os.path.splitext(png_file)[0]
        array_name = f"img_{base_name}"
        
        # Generate output file path
        output_file = f"{base_name}.c"
        output_path = os.path.join(args.output_dir, output_file)
        
        # Convert image
        png_to_c_array(png_path, output_path, array_name, target_size)

if __name__ == "__main__":
    main() 