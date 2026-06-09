import os
import sys

def generate_c_header(mp3_files, suffix):
    """生成 .h 头文件内容"""
    header_content = f"""#ifndef __MEDIA_SRC_{suffix.upper()}_H__
#define __MEDIA_SRC_{suffix.upper()}_H__

#include "tuya_cloud_types.h"
"""
    
    for filename, data_length in mp3_files:
        array_name = f"media_src_{filename.replace('-', '_')}"
        header_content += f"\nextern CONST BYTE_T {array_name}[{data_length}];"
    
    header_content += f"\n\n#endif // __MEDIA_SRC_{suffix.upper()}_H__\n"
    return header_content

def generate_c_source(mp3_files, suffix):
    """生成 .c 源文件内容"""
    source_content = f"#include \"media_src_{suffix}.h\"\n\n"
    
    for filename, data in mp3_files:
        array_name = f"media_src_{filename.replace('-', '_')}"
        hex_array = ', '.join(f'0x{byte:02X}' for byte in data)
        source_content += f"// {filename}\n"
        source_content += f"CONST BYTE_T {array_name}[{len(data)}] = {{\n    {hex_array}\n}};\n\n"
    
    return source_content

def generate_main_header(directory):
    """生成 media_src.h 头文件，包含 media_src_zh.h 和 media_src_en.h"""
    header_content = """#ifndef __MEDIA_SRC_H__
#define __MEDIA_SRC_H__

#include "media_src_zh.h"
#include "media_src_en.h"

#endif // __MEDIA_SRC_H__
"""
    
    h_file = os.path.join(directory, "../media_src.h")
    with open(h_file, 'w') as h:
        h.write(header_content)
    print(f"Generated: {h_file}")

def convert_mp3_to_c(directory):
    """遍历目录下所有MP3文件并按后缀分类转换为C文件和H文件"""
    mp3_files = {"zh": [], "en": []}
    
    for filename in sorted(os.listdir(directory)):
        if filename.endswith(".mp3"):
            filepath = os.path.join(directory, filename)
            with open(filepath, 'rb') as f:
                data = f.read()
            suffix = "zh" if filename.endswith("zh.mp3") else "en" if filename.endswith("en.mp3") else None
            if suffix:
                mp3_files[suffix].append((filename[:-4], data))
    
    for suffix in ["zh", "en"]:
        if mp3_files[suffix]:
            h_content = generate_c_header([(f[0], len(f[1])) for f in mp3_files[suffix]], suffix)
            c_content = generate_c_source(mp3_files[suffix], suffix)
            
            h_file = os.path.join(directory, f"../media_src_{suffix}.h")
            c_file = os.path.join(directory, f"../media_src_{suffix}.c")
            
            with open(h_file, 'w') as h:
                h.write(h_content)
            with open(c_file, 'w') as c:
                c.write(c_content)
            
            print(f"Generated: {h_file}, {c_file}")
    
    generate_main_header(directory)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python script.py <directory>")
    else:
        convert_mp3_to_c(sys.argv[1])
