#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PNG图片转C数组工具
将PNG图片转换为C语言数组格式，用于嵌入式系统显示
"""

import os
import sys
from PIL import Image
import argparse

def png_to_c_array(png_path, output_path, array_name, target_size=None):
    """
    将PNG图片转换为C数组
    
    Args:
        png_path: PNG图片路径
        output_path: 输出的C文件路径
        array_name: C数组名称
        target_size: 目标尺寸 (width, height)，如果为None则保持原尺寸
    """
    try:
        # 打开PNG图片
        img = Image.open(png_path)
        
        # 转换为RGB模式（如果不是的话）
        if img.mode != 'RGB':
            img = img.convert('RGB')
        
        # 如果指定了目标尺寸，则调整图片大小
        if target_size:
            img = img.resize(target_size, Image.Resampling.LANCZOS)
        
        # 获取图片尺寸
        width, height = img.size
        
        # 获取像素数据
        pixels = list(img.getdata())
        
        # 生成C数组
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
        
        # 转换像素为RAW_CHROMA_KEYED格式 (RGB565)
        raw_data = []
        for pixel in pixels:
            r, g, b = pixel
            
            # 转换为RGB565格式 (5-6-5)
            r = (r >> 3) & 0x1F  # 5位
            g = (g >> 2) & 0x3F  # 6位
            b = (b >> 3) & 0x1F  # 5位
            
            # 组合为16位RGB565
            rgb565 = (r << 11) | (g << 5) | b
            
            # 存储为2字节: RGB565 (小端序)
            raw_data.extend([rgb565 & 0xFF, (rgb565 >> 8) & 0xFF])
        
        # 每行16个字节 (8个像素 * 2字节)
        for i in range(0, len(raw_data), 16):
            line_data = raw_data[i:i+16]
            c_code += "    " + ", ".join([f"0x{x:02X}" for x in line_data]) + ",\n"
        
        c_code += "};\n\n"
        
        # 添加图片描述符
        c_code += f"""const lv_image_dsc_t {array_name} = {{
    .header.cf = LV_COLOR_FORMAT_RGB565,
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.w = {width},
    .header.h = {height},
    .data_size = {len(raw_data)},
    .data = {array_name}_map,
}};\n"""
        
        # 写入文件
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(c_code)
        
        print(f"转换成功: {png_path} -> {output_path}")
        print(f"图片尺寸: {width}x{height}")
        print(f"数据大小: {len(raw_data)} 字节 ({len(raw_data)//2} 个像素)")
        
    except Exception as e:
        print(f"转换失败: {e}")
        return False
    
    return True

def main():
    parser = argparse.ArgumentParser(description='PNG图片转C数组工具')
    parser.add_argument('input_dir', help='输入PNG图片目录')
    parser.add_argument('output_dir', help='输出C文件目录')
    parser.add_argument('--size', help='目标尺寸，格式为 WxH，例如 64x64')
    
    args = parser.parse_args()
    
    # 解析尺寸参数
    target_size = None
    if args.size:
        try:
            width, height = map(int, args.size.split('x'))
            target_size = (width, height)
        except ValueError:
            print("错误：尺寸格式应为 WxH，例如 64x64")
            return
    
    # 确保输出目录存在
    os.makedirs(args.output_dir, exist_ok=True)
    
    # 处理目录中的所有PNG文件
    png_files = [f for f in os.listdir(args.input_dir) if f.lower().endswith('.png')]
    
    if not png_files:
        print("未找到PNG文件")
        return
    
    print(f"找到 {len(png_files)} 个PNG文件")
    
    for png_file in png_files:
        png_path = os.path.join(args.input_dir, png_file)
        
        # 生成数组名称（去掉扩展名，转换为下划线格式）
        base_name = os.path.splitext(png_file)[0]
        array_name = f"img_{base_name}"
        
        # 生成输出文件路径
        output_file = f"{base_name}.c"
        output_path = os.path.join(args.output_dir, output_file)
        
        # 转换图片
        png_to_c_array(png_path, output_path, array_name, target_size)

if __name__ == "__main__":
    main() 