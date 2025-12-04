from PIL import Image

def four_level_dither(input_path: str, output_path: str, target_size: tuple = (168, 168)) -> None:
    """
    单色屏幕模拟4色灰阶的有序抖动算法（2x2 Bayer矩阵），强制重采样到目标尺寸（默认168x168）
    
    Args:
        input_path: 输入图像路径（支持任意格式）
        output_path: 输出二值图像路径（建议保存为.png/.bmp）
        target_size: 目标尺寸 tuple (width, height)，默认(168, 168)
    """
    # 1. 定义2x2 Bayer抖动矩阵（适配4色灰阶，元素值0-3）
    bayer_matrix = [
        [0, 2],
        [3, 1]
    ]
    matrix_size = 2  # 矩阵尺寸（2x2）

    # 2. 读取图像 → 转为灰度图 → 强制重采样到目标尺寸（168x168）
    img = Image.open(input_path).convert("L")  # "L"表示单通道灰度图（0-255）
    img_resized = img.resize(target_size, Image.LANCZOS)  # 高质量缩放（保留细节）
    width, height = img_resized.size  # 此时尺寸已为168x168
    pixels = img_resized.load()  # 读取缩放后的像素数组

    # 3. 创建输出二值图像（模式"1"表示单色二值图，0=黑，255=白）
    output_img = Image.new("1", (width, height))
    output_pixels = output_img.load()

    # 4. 遍历每个像素，执行抖动算法
    for y in range(height):
        for x in range(width):
            # 4.1 获取原始灰度值（0-255），归一化到0-3区间（对应4色灰阶）
            gray_value = pixels[x, y]
            normalized_gray = (gray_value / 255.0) * 3  # 映射后范围：0.0 ~ 3.0

            # 4.2 根据像素坐标获取抖动矩阵中的阈值（循环复用2x2矩阵）
            matrix_x = x % matrix_size
            matrix_y = y % matrix_size
            threshold = bayer_matrix[matrix_y][matrix_x]  # 阈值：0/1/2/3

            # 4.3 阈值比较：归一化灰度 ≥ 阈值 → 白（255），否则黑（0）
            output_pixels[x, y] = 255 if normalized_gray >= threshold else 0

    # 5. 保存结果
    output_img.save(output_path)
    print(f"4色灰阶抖动处理完成！")
    print(f"输出尺寸：{width}x{height}")
    print(f"输出保存至：{output_path}")

# ------------------- 测试示例 -------------------
if __name__ == "__main__":
    # 替换为你的输入/输出路径
    INPUT_IMAGE = "background.jpg"   # 输入灰度图（或彩色图，会自动转灰度）
    OUTPUT_IMAGE = "output_4level_dither_168x168.png"  # 输出168x168二值图
    
    # 调用函数（默认自动重采样到168x168）
    four_level_dither(INPUT_IMAGE, OUTPUT_IMAGE, target_size=(384, 168))
    
    # 若需自定义尺寸（可选），例如：four_level_dither(INPUT_IMAGE, OUTPUT_IMAGE, target_size=(200, 200))