#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os, codecs, struct

OUT = 'u2g_tbl.inc'

# 强制让 Python 在 Windows 也能找到 gbk codec
os.environ['PYTHONIOENCODING'] = 'gbk:backslashreplace'

# 定义要包含的 Unicode 范围
RANGES = [
    # ASCII 可打印字符（虽然是单字节，但为了统一处理也包含）
    (0x0020, 0x007E),   # 空格到~，包含英文字母、数字、标点
    # 中文标点符号
    (0x3000, 0x303F),   # CJK 符号和标点
    (0xFF00, 0xFFEF),   # 全角ASCII、全角标点
    # 常用汉字
    (0x4E00, 0x9FA5),   # CJK统一表意文字（基本汉字）
    # 其他常用符号
    (0x2000, 0x206F),   # 通用标点
    (0x2100, 0x214F),   # 字母式符号
    (0x2190, 0x21FF),   # 箭头
    (0x2460, 0x24FF),   # 带圈字符
    (0x2500, 0x257F),   # 制表符
    (0x25A0, 0x25FF),   # 几何图形
]

tbl = []

# 遍历所有定义的范围
for start, end in RANGES:
    for u in range(start, end + 1):
        try:
            gb = chr(u).encode('gbk')          # Unicode → GBK 字节
            if len(gb) == 1:
                # 单字节字符（ASCII），也添加进去
                tbl.append((u, 0x00, gb[0]))   # 用 0x00 作为高字节
            elif len(gb) == 2:
                tbl.append((u, gb[0], gb[1]))
        except UnicodeEncodeError:
            continue

if not tbl:
    print('❌ 0 条记录，请用管理员 PowerShell 执行：')
    print('   chcp 936 & python gen_u2g_win.py')
    exit(1)

# 按 Unicode 码点排序（确保二分查找正确）
tbl.sort(key=lambda x: x[0])

with codecs.open(OUT, 'w', 'utf-8') as f:
    f.write('/* UTF-8 → GBK 映射表，包含汉字和标点符号 */\n')
    f.write('/* 格式：Unicode大端3字节 + GBK 2字节，共5字节/条 */\n')
    f.write('static const uint8_t u2g_tbl[] = {\n')
    for u, h, l in tbl:
        # Unicode 大端序：高字节在前，低字节在后，第三字节为0
        f.write(f'    0x{(u>>16)&0xFF:02X},0x{(u>>8)&0xFF:02X},0x{u&0xFF:02X}, 0x{h:02X},0x{l:02X},\n')
    f.write('};\n')

print(f'✅ 已生成 {OUT}，共 {len(tbl)} 项，{len(tbl)*5} 字节')
print(f'   包含范围：')
print(f'   - ASCII 可打印字符 (0x0020-0x007E)')
print(f'   - CJK 符号和标点 (0x3000-0x303F)')
print(f'   - 全角字符 (0xFF00-0xFFEF)')
print(f'   - 常用汉字 (0x4E00-0x9FA5)')
print(f'   - 其他常用符号')