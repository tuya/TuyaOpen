# pip install -U Pillow
"""
本地验证脚本：对比 bayer_dither.py 中各抖动算法在黑白双色屏 (384x168) 上的效果。
读取 image/ 目录下的素材，逐个跑完所有算法，拼成一张网格图输出，方便肉眼对比。
"""
import os
import tempfile
from PIL import Image, ImageDraw, ImageFont

from bayer_dither import (
    four_level_dither,
    eight_gray_dither,
    sixteen_gray_dither,
    edge_atkinson_dither,
    gamma_serpentine_dither,
)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
IMAGE_DIR = os.path.join(SCRIPT_DIR, "image")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "dither_compare_output")
TARGET_SIZE = (384, 168)

ALGORITHMS = [
    ("4阶灰度 Bayer", four_level_dither),
    ("8阶灰度 Bayer", eight_gray_dither),
    ("16阶灰度 Bayer", sixteen_gray_dither),
    ("边缘+Atkinson", edge_atkinson_dither),
    ("Gamma+蛇形抖动", gamma_serpentine_dither),
]


def build_grid(images_labels, cell_size, cols, label_h=22, pad=8, font=None):
    rows = (len(images_labels) + cols - 1) // cols
    cell_w, cell_h = cell_size
    grid_w = cols * (cell_w + pad) + pad
    grid_h = rows * (cell_h + label_h + pad) + pad

    grid = Image.new("RGB", (grid_w, grid_h), (60, 60, 60))
    draw = ImageDraw.Draw(grid)

    for i, (label, img) in enumerate(images_labels):
        r, c = divmod(i, cols)
        x = pad + c * (cell_w + pad)
        y = pad + r * (cell_h + label_h + pad)
        draw.rectangle([x - 1, y - 1, x + cell_w, y + cell_h], outline=(120, 120, 120))
        grid.paste(img.convert("RGB"), (x, y))
        draw.text((x, y + cell_h + 4), label, fill=(255, 255, 255), font=font)

    return grid


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    input_images = sorted(
        f for f in os.listdir(IMAGE_DIR) if f.lower().endswith((".png", ".jpg", ".jpeg"))
    )
    if not input_images:
        print(f"未在 {IMAGE_DIR} 找到测试图像")
        return

    try:
        font = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14
        )
    except OSError:
        font = ImageFont.load_default()

    cells = []
    with tempfile.TemporaryDirectory(prefix="dither_compare_") as tmp_dir:
        for image_name in input_images:
            input_path = os.path.join(IMAGE_DIR, image_name)
            base_name = os.path.splitext(image_name)[0]

            original = Image.open(input_path).convert("RGB").resize(TARGET_SIZE, Image.LANCZOS)
            cells.append((f"{base_name} 原图", original))

            for label, fn in ALGORITHMS:
                # Per-algorithm PNGs are only needed transiently to build the grid below;
                # only the final grid image is kept in OUTPUT_DIR.
                tmp_path = os.path.join(tmp_dir, f"{base_name}_{fn.__name__}.png")
                fn(input_path, tmp_path, TARGET_SIZE)
                cells.append((f"{base_name} {label}", Image.open(tmp_path).copy()))

    cols = len(ALGORITHMS) + 1  # 原图 + 各算法
    grid = build_grid(cells, TARGET_SIZE, cols, font=font)

    grid_path = os.path.join(OUTPUT_DIR, "dither_comparison_grid.png")
    grid.save(grid_path)
    print(f"对比网格图已生成 → {grid_path}  尺寸 {grid.size}")


if __name__ == "__main__":
    main()
