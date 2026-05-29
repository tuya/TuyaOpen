#!/usr/bin/env python3
"""
Convert PNG files to LVGL C sources.

- v8: embeds raw PNG bytes (LV_IMG_CF_RAW_ALPHA), relies on v8's lodepng decoder.
- v9: pre-decodes PNG to ARGB8888 raw pixels (no runtime decoder needed).

Output layout:
    <out_dir>/v8/<stem>.c
    <out_dir>/v9/<stem>.c

Example (cwd = this script's directory, i.e. assets/):
    python3 png_to_lvgl_icon_c.py picture ../icon
    python3 png_to_lvgl_icon_c.py picture ../icon --version v9
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from PIL import Image


def c_ident_from_stem(stem: str) -> str:
    """Prefix with icon_ to avoid clashes with libc (e.g. clock) and C keywords."""
    s = re.sub(r"[^0-9a-zA-Z_]", "_", stem)
    if s and s[0].isdigit():
        s = "_" + s
    base = s or "img"
    if base.startswith("icon_"):
        return base
    return f"icon_{base}"


def format_rows(data: bytes, per_line: int = 13) -> str:
    lines: list[str] = []
    for i in range(0, len(data), per_line):
        chunk = data[i : i + per_line]
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ", ")
    return "\n".join(lines)


LVGL_HEADER = """\
#ifdef __has_include
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
"""


def emit_c_file_v8(
    out_path: Path,
    symbol: str,
    w: int,
    h: int,
    png_data: bytes,
    has_alpha: bool,
) -> None:
    """v8: embed raw PNG bytes. v8 lodepng/png decoder handles runtime decode."""
    map_name = f"{symbol}_map"
    attr_macro = f"LV_ATTRIBUTE_IMG_{symbol.upper()}"
    cf = "LV_IMG_CF_RAW_ALPHA" if has_alpha else "LV_IMG_CF_RAW"
    body = f"""\
{LVGL_HEADER}
#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef {attr_macro}
#define {attr_macro}
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST {attr_macro} uint8_t {map_name}[] = {{
{format_rows(png_data)}
}};

const lv_img_dsc_t {symbol} = {{
  .header.cf = {cf},
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = {w},
  .header.h = {h},
  .data_size = {len(png_data)},
  .data = {map_name},
}};
"""
    out_path.write_text(body, encoding="utf-8")


def emit_c_file_v9(
    out_path: Path,
    symbol: str,
    w: int,
    h: int,
    img: Image.Image,
) -> None:
    """v9: pre-decode to ARGB8888 raw pixels. No runtime PNG decoder needed.

    LVGL v9 ARGB8888 pixel layout in memory (little-endian): [B, G, R, A].
    """
    rgba = img.convert("RGBA")
    raw = rgba.tobytes()  # [R, G, B, A, R, G, B, A, ...]

    # Rearrange each pixel from PIL RGBA → LVGL ARGB8888 (BGRA in memory)
    pixel_data = bytearray()
    for i in range(0, len(raw), 4):
        r, g, b, a = raw[i], raw[i + 1], raw[i + 2], raw[i + 3]
        pixel_data.extend([b, g, r, a])

    map_name = f"{symbol}_map"
    attr_macro = f"LV_ATTRIBUTE_IMG_{symbol.upper()}"
    body = f"""\
{LVGL_HEADER}
#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef {attr_macro}
#define {attr_macro}
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST {attr_macro} uint8_t {map_name}[] = {{
{format_rows(bytes(pixel_data))}
}};

const lv_img_dsc_t {symbol} = {{
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.cf = LV_COLOR_FORMAT_ARGB8888,
  .header.w = {w},
  .header.h = {h},
  .data_size = {len(pixel_data)},
  .data = {map_name},
}};
"""
    out_path.write_text(body, encoding="utf-8")


def convert_png(png_path: Path, out_dir: Path, versions: list[str]) -> None:
    stem = png_path.stem
    symbol = c_ident_from_stem(stem)
    img = Image.open(png_path)
    w, h = img.size
    has_alpha = img.mode in ("RGBA", "LA", "PA") or "transparency" in img.info
    png_data = png_path.read_bytes()

    for ver in versions:
        ver_dir = out_dir / ver
        ver_dir.mkdir(parents=True, exist_ok=True)
        out_c = ver_dir / f"{stem}.c"
        if ver == "v8":
            emit_c_file_v8(out_c, symbol, w, h, png_data, has_alpha)
        else:
            emit_c_file_v9(out_c, symbol, w, h, img)
        print(f"  {ver}/{stem}.c")


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Convert PNGs to LVGL C image sources (v8: raw PNG, v9: ARGB8888 pixels).",
        epilog="Example: python3 png_to_lvgl_icon_c.py picture ../icon",
    )
    ap.add_argument("picture_dir", type=Path, help="Directory containing PNG files")
    ap.add_argument("out_dir", type=Path, help="Output directory for .c files")
    ap.add_argument(
        "--version", "-v",
        choices=["v8", "v9", "all"],
        default="all",
        help="Which LVGL version(s) to generate (default: all)",
    )
    args = ap.parse_args()
    pic = args.picture_dir.resolve()
    out = args.out_dir.resolve()
    if not pic.is_dir():
        print(f"Not a directory: {pic}", file=sys.stderr)
        return 1
    out.mkdir(parents=True, exist_ok=True)
    pngs = sorted(pic.glob("*.png"))
    if not pngs:
        print(f"No PNG in {pic}", file=sys.stderr)
        return 1

    versions = ["v8", "v9"] if args.version == "all" else [args.version]

    for p in pngs:
        print(f"{p.name} ->")
        convert_png(p, out, versions)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
