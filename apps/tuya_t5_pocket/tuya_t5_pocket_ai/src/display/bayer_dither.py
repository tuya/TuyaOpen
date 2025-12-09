# pip install -U Pillow
from PIL import Image
import sys


def four_level_dither(input_path: str, output_path: str, target_size: tuple = (384, 168)) -> None:
    """
    单色屏幕模拟4色灰阶的有序抖动算法（2x2 Bayer矩阵）
    
    Args:
        input_path: 输入图像路径（支持任意格式）
        output_path: 输出二值图像路径（建议保存为.png/.bmp）
        target_size: 目标尺寸 tuple (width, height)，默认(384, 168)
    """
    # 1. 定义2x2 Bayer抖动矩阵（适配4色灰阶，元素值0-3）
    bayer_matrix = [
        [0, 2],
        [3, 1]
    ]
    matrix_size = 2  # 矩阵尺寸（2x2）

    # 2. 读取图像 → 转为灰度图 → 强制重采样到目标尺寸
    img = Image.open(input_path).convert("L")
    img_resized = img.resize(target_size, Image.LANCZOS)
    width, height = img_resized.size
    pixels = img_resized.load()

    # 3. 创建输出二值图像
    output_img = Image.new("1", (width, height))
    output_pixels = output_img.load()

    # 4. 遍历每个像素，执行抖动算法
    for y in range(height):
        for x in range(width):
            gray_value = pixels[x, y]
            normalized_gray = (gray_value / 255.0) * 3  # 映射后范围：0.0 ~ 3.0

            matrix_x = x % matrix_size
            matrix_y = y % matrix_size
            threshold = bayer_matrix[matrix_y][matrix_x]

            output_pixels[x, y] = 255 if normalized_gray >= threshold else 0

    # 5. 保存结果
    output_img.save(output_path)
    print(f"4阶灰度抖动完成 → {output_path}  尺寸 {width}×{height}")


def eight_gray_dither(input_path: str, output_path: str, target_size: tuple = (384, 168)) -> None:
    """
    8 阶灰度 Bayer 有序抖动（3×3 矩阵，阈值 0–8）
    输出二值图
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
            norm = gray * 8 / 255.0
            threshold = bayer[y % n][x % n]
            out_px[x, y] = 255 if norm >= threshold else 0

    # 5. 保存
    out.save(output_path)
    print(f"8阶灰度抖动完成 → {output_path}  尺寸 {w}×{h}")


def sixteen_gray_dither(input_path: str, output_path: str, target_size: tuple = (384, 168)) -> None:
    """
    16 阶灰度 Bayer 有序抖动（4×4 矩阵，阈值 0–15）
    输出二值图
    """
    # 1. 4×4 Bayer 矩阵（阈值 0–15）
    bayer = [
        [ 0,  8,  2, 10],
        [12,  4, 14,  6],
        [ 3, 11,  1,  9],
        [15,  7, 13,  5]
    ]
    n = 4

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
            norm = gray * 16 / 255.0
            threshold = bayer[y % n][x % n]
            out_px[x, y] = 255 if norm >= threshold else 0

    # 5. 保存
    out.save(output_path)
    print(f"16阶灰度抖动完成 → {output_path}  尺寸 {w}×{h}")


# ------------------ CLI 示例 ------------------
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("使用方法:")
        print("  python bayer_dither.py <level> [input_image] [output_image] [width] [height]")
        print("")
        print("参数说明:")
        print("  level         - 抖动级别: 4, 8, 或 16")
        print("  input_image   - 输入图像路径 (默认: background.jpg)")
        print("  output_image  - 输出图像路径 (默认: output_<level>level_dither.png)")
        print("  width         - 目标宽度 (默认: 384)")
        print("  height        - 目标高度 (默认: 168)")
        print("")
        print("示例:")
        print("  python bayer_dither.py 4")
        print("  python bayer_dither.py 8 input.jpg output.png")
        print("  python bayer_dither.py 16 input.jpg output.png 384 168")
        sys.exit(1)

    # 解析参数
    level = int(sys.argv[1])
    input_image = sys.argv[2] if len(sys.argv) > 2 else "background.jpg"
    output_image = sys.argv[3] if len(sys.argv) > 3 else f"output_{level}level_dither.png"
    width = int(sys.argv[4]) if len(sys.argv) > 4 else 384
    height = int(sys.argv[5]) if len(sys.argv) > 5 else 168
    
    target_size = (width, height)
    
    # 根据级别调用对应函数
    if level == 4:
        four_level_dither(input_image, output_image, target_size)
    elif level == 8:
        eight_gray_dither(input_image, output_image, target_size)
    elif level == 16:
        sixteen_gray_dither(input_image, output_image, target_size)
    else:
        print(f"错误: 不支持的抖动级别 {level}，仅支持 4, 8, 16")
        sys.exit(1)
