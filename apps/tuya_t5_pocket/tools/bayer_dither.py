# pip install -U Pillow
from PIL import Image, ImageEnhance, ImageFilter
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


def edge_atkinson_dither(input_path: str, output_path: str, target_size: tuple = (384, 168),
                                edge_threshold: int = 105, black_thresh: float = None,
                                gamma: float = 2.0) -> None:
    """
    边缘保护+阿特金森抖动，黑白双色版本。
    边缘像素直接判黑以锁定轮廓，非边缘像素走 Atkinson 误差扩散（6 邻居，各 1/8 权重，保留 2/8 误差）。

    black_thresh 默认为 None：这时用当前图像增强后的平均亮度做自适应阈值，而不是固定值,
    这样黑白分界会跟着每张图实际的亮度走。但光有自适应阈值还不够：error-diffusion 抖动
    本质上是保留图像的平均亮度，如果原图本身偏暗（比如 mean luma 只有 70/255），单纯用
    均值当阈值算出来的黑色占比就会真的接近 (255-70)/255 ≈ 73%——这不是阈值算错了，是这张
    图本来就暗，抖动"忠实"地还原了这一点，但在这块小黑白屏上看就是一大片黑、细节被吃掉。
    加一个 gamma（默认2.0，和 gamma_serpentine_dither 用同一种手段）在抖动前把中间调整体提亮，
    用可控的失真换取暗部细节的可读性——实测在偏暗的图上能把黑色占比从 70%+ 降到 50%~60%，
    细节（比如毛发、齿轮纹理）明显更容易看出来。gamma 只作用在参与抖动的亮度通道，
    不影响边缘检测（依然基于原始灰度图，见下方 edges）。
    """
    img = Image.open(input_path).convert("RGB")
    img = img.resize(target_size, Image.LANCZOS)
    w, h = img.size

    edges = img.convert("L").filter(ImageFilter.FIND_EDGES)
    edges = ImageEnhance.Contrast(edges).enhance(2.2)
    edge_px = edges.load()

    enh = ImageEnhance.Contrast(img).enhance(1.4)
    enh = ImageEnhance.Sharpness(enh).enhance(1.8)
    enh_gray = enh.convert("L")
    if gamma != 1.0:
        lut = [int(pow(i / 255.0, 1.0 / gamma) * 255.0) for i in range(256)]
        enh_gray = enh_gray.point(lut)

    pixels = [[float(enh_gray.getpixel((x, y))) for x in range(w)] for y in range(h)]

    if black_thresh is None:
        black_thresh = sum(sum(row) for row in pixels) / (w * h)

    out = Image.new("L", (w, h))
    out_px = out.load()

    for y in range(h):
        for x in range(w):
            lum = pixels[y][x]
            is_edge = edge_px[x, y] > edge_threshold and (lum / 255.0) < 0.65

            best = 0.0 if is_edge else (255.0 if lum >= black_thresh else 0.0)
            out_px[x, y] = int(best)

            if not is_edge:
                err = lum - best
                for dx, dy in ((1, 0), (2, 0), (-1, 1), (0, 1), (1, 1), (0, 2)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h:
                        pixels[ny][nx] += err * (1.0 / 8.0)

    out.save(output_path)
    print(f"边缘保护+Atkinson抖动完成 → {output_path}  尺寸 {w}×{h}  black_thresh={black_thresh:.1f}")


def gamma_serpentine_dither(input_path: str, output_path: str, target_size: tuple = (384, 168),
                         gamma: float = 1.45, threshold: float = 128.0) -> None:
    """
    Gamma校正+蛇形抖动，黑白双色版本。
    Gamma 校正 + 反锐化蒙版提升毛发/细节，再走蛇形（serpentine）Floyd-Steinberg 误差扩散。
    """
    img = Image.open(input_path).convert("RGB")
    img = img.resize(target_size, Image.LANCZOS)
    w, h = img.size

    gray = img.convert("L")
    lut = [int(pow(i / 255.0, 1.0 / gamma) * 255.0) for i in range(256)]
    gray = gray.point(lut)
    gray = gray.filter(ImageFilter.UnsharpMask(radius=2, percent=180, threshold=2))
    gray = ImageEnhance.Contrast(gray).enhance(1.25)

    pixels = [[float(gray.getpixel((x, y))) for x in range(w)] for y in range(h)]

    out = Image.new("L", (w, h))
    out_px = out.load()

    for y in range(h):
        x_range = range(w) if y % 2 == 0 else range(w - 1, -1, -1)
        direction = 1 if y % 2 == 0 else -1

        for x in x_range:
            old_val = max(0.0, min(255.0, pixels[y][x]))
            new_val = 255.0 if old_val >= threshold else 0.0
            out_px[x, y] = int(new_val)

            err = old_val - new_val
            if 0 <= x + direction < w:
                pixels[y][x + direction] += err * (7.0 / 16.0)
            if 0 <= y + 1 < h:
                if 0 <= x - direction < w:
                    pixels[y + 1][x - direction] += err * (3.0 / 16.0)
                pixels[y + 1][x] += err * (5.0 / 16.0)
                if 0 <= x + direction < w:
                    pixels[y + 1][x + direction] += err * (1.0 / 16.0)

    out.save(output_path)
    print(f"Gamma+蛇形抖动完成 → {output_path}  尺寸 {w}×{h}")


# ------------------ CLI 示例 ------------------
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("使用方法:")
        print("  python bayer_dither.py <level> [input_image] [output_image] [width] [height]")
        print("")
        print("参数说明:")
        print("  level         - 抖动级别: 4, 8, 16, edge_atkinson, 或 gamma_serpentine")
        print("  input_image   - 输入图像路径 (默认: background.jpg)")
        print("  output_image  - 输出图像路径 (默认: output_<level>level_dither.png)")
        print("  width         - 目标宽度 (默认: 384)")
        print("  height        - 目标高度 (默认: 168)")
        print("")
        print("示例:")
        print("  python bayer_dither.py 4")
        print("  python bayer_dither.py 8 input.jpg output.png")
        print("  python bayer_dither.py 16 input.jpg output.png 384 168")
        print("  python bayer_dither.py edge_atkinson input.jpg output.png")
        print("  python bayer_dither.py gamma_serpentine input.jpg output.png")
        sys.exit(1)

    # 解析参数
    level_arg = sys.argv[1]
    input_image = sys.argv[2] if len(sys.argv) > 2 else "background.jpg"
    output_image = sys.argv[3] if len(sys.argv) > 3 else f"output_{level_arg}level_dither.png"
    width = int(sys.argv[4]) if len(sys.argv) > 4 else 384
    height = int(sys.argv[5]) if len(sys.argv) > 5 else 168

    target_size = (width, height)

    # 根据级别调用对应函数
    if level_arg == "edge_atkinson":
        edge_atkinson_dither(input_image, output_image, target_size)
    elif level_arg == "gamma_serpentine":
        gamma_serpentine_dither(input_image, output_image, target_size)
    else:
        level = int(level_arg)
        if level == 4:
            four_level_dither(input_image, output_image, target_size)
        elif level == 8:
            eight_gray_dither(input_image, output_image, target_size)
        elif level == 16:
            sixteen_gray_dither(input_image, output_image, target_size)
        else:
            print(f"错误: 不支持的抖动级别 {level_arg}，仅支持 4, 8, 16, edge_atkinson, gamma_serpentine")
            sys.exit(1)
