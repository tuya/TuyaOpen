#!/usr/bin/env python3
"""
Embed PNG files as raw-byte lv_img_dsc_t C arrays, in the same format already
used by src/display/ducky/tuya_floyd.c and src/display/logo/tuyaopen_logo-384-168.c
in tuya_t5_pocket_ai. LVGL's built-in PNG decoder (LV_USE_PNG) decodes the
embedded bytes at runtime, so no pixel-format conversion is needed here -
this just dumps the PNG file bytes into a C byte array.

Usage:
  python3 png2lvgl_raw.py <input_dir> <output_dir> [--prefix photo_]
"""
import argparse
import os

from PIL import Image

TEMPLATE_HEAD = """#ifdef __has_include
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

#ifndef LV_ATTRIBUTE_IMG_{upper_name}
#define LV_ATTRIBUTE_IMG_{upper_name}
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_{upper_name} uint8_t {name}_map[] = {{
"""


def convert_one(png_path: str, output_path: str, name: str) -> None:
    with open(png_path, "rb") as f:
        data = f.read()

    img = Image.open(png_path)
    width, height = img.size

    lines = [TEMPLATE_HEAD.format(upper_name=name.upper(), name=name)]
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")
    lines.append("};\n\n")
    lines.append(f"const lv_img_dsc_t {name} = {{\n")
    lines.append(f"    .header.w = {width},\n")
    lines.append(f"    .header.h = {height},\n")
    lines.append(f"    .data_size = {len(data)},\n")
    lines.append(f"    .data = {name}_map,\n")
    lines.append("};\n")

    with open(output_path, "w") as f:
        f.writelines(lines)

    print(f"{png_path} -> {output_path}  {width}x{height}  {len(data)} bytes")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input_dir")
    parser.add_argument("output_dir")
    parser.add_argument("--prefix", default="photo_")
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)
    png_files = sorted(f for f in os.listdir(args.input_dir) if f.lower().endswith(".png"))

    for png_file in png_files:
        base_name = os.path.splitext(png_file)[0]
        name = f"{args.prefix}{base_name}"
        convert_one(os.path.join(args.input_dir, png_file), os.path.join(args.output_dir, f"{name}.c"), name)


if __name__ == "__main__":
    main()
