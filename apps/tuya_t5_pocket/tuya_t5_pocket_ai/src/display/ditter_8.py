from PIL import Image

def eight_gray_dither(input_path: str,
                      output_path: str,
                      target_size: tuple = (168, 168)) -> None:
    """
    8 阶灰度 Bayer 有序抖动（3×3 矩阵，阈值 0–8）
    输出 8 级灰度图（0,36,73,109,146,182,219,255）或可选二值图
    """
    # 1. 3×3 Bayer 矩阵（阈值 0–8）
    bayer = [
        [0, 7, 3],
        [6, 4, 2],
        [1, 5, 8]
    ]
    n = 3

    # 2. 读取 → 灰度 → 重采样
    img = Image.open(input_path).convert("L")
    img = img.resize(target_size, Image.LANCZOS)
    w, h = img.size
    pixels = img.load()

    # 3. 准备输出
    out = Image.new("L", (w, h))
    out_px = out.load()

    # 4. 抖动
    for y in range(h):
        for x in range(w):
            gray = pixels[x, y]
            norm = gray * 8 / 255.0          # 映射到 0–8
            threshold = bayer[y % n][x % n]  # 0–8
            # level = int(norm + threshold / 9.0)  # 0–7
            # level = max(0, min(7, level))
            # out_px[x, y] = int(level * 255 / 7)  # 8 级灰度

            # 如果想输出“二值”而非 8 级灰度，把上面两行换成：
            out_px[x, y] = 255 if norm >= threshold else 0

    # 5. 保存
    out.save(output_path)
    print(f"8 阶灰度抖动完成 → {output_path}  尺寸 {w}×{h}")

# ------------------ CLI 示例 ------------------
if __name__ == "__main__":
    # 替换为你的输入/输出路径
    INPUT_IMAGE = "background.jpg"   # 输入灰度图（或彩色图，会自动转灰度）
    OUTPUT_IMAGE = "output_8level_dither_384x168.png"  # 输出168x168二值图
    
    # 调用函数（默认自动重采样到168x168）
    eight_gray_dither(INPUT_IMAGE, OUTPUT_IMAGE, target_size=(384, 168))
    
    # 若需自定义尺寸（可选），例如：four_level_dither(INPUT_IMAGE, OUTPUT_IMAGE, target_size=(200, 200))