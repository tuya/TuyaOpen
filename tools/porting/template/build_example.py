#!/usr/bin/env python3
# coding=utf-8
# 参数说明：
# $1 - params path: $1/build_param.[cmake/config/json]
# $2 - user cmd: build/clean/...

import os
import sys
import json
import subprocess
import shutil


def clean(root):
    shutil.rmtree("build", ignore_errors=True)
    pass


def parser_para_file(json_file):
    if not os.path.isfile(json_file):
        print(f"Error: Not found [{json_file}].")
        return {}
    try:
        with open(json_file, 'r', encoding='utf-8') as f:
            json_data = json.load(f)
    except Exception as e:
        print(f"Parser json error:  [{str(e)}].")
        return {}
    return json_data


def main():
    if len(sys.argv) < 2:
        print(f"Error: At least 2 parameters are needed {sys.argv}.")
    build_param_path = sys.argv[1]
    user_cmd = sys.argv[2]
    root = os.path.dirname(os.path.abspath(__file__))
    if "clean" == user_cmd:
        clean(root)
        sys.exit(0)

    build_param_file = os.path.join(build_param_path, "build_param.json")
    param_data = parser_para_file(build_param_file)
    if not len(param_data):
        sys.exit(1)

    if not os.path.isfile("CMakeLists.txt"):
        print("Error: CMakeLists.txt not found in the current directory.")
        sys.exit(1)

    try:
        os.makedirs("build", exist_ok=True)

        cmake_command = [
            "cmake", "-S", ".", "-B", "build", "-G", "Ninja",
            f"-DBUILD_PARAM_PATH={build_param_path}"
        ]

        subprocess.run(
            cmake_command,
            check=True,
        )
        subprocess.run(["cmake", "--build", "build"], check=True)
    except subprocess.CalledProcessError as e:
        print(f"CMake process failed: {e}")
        sys.exit(1)

    source_file = os.path.join(os.getcwd(), "build", param_data['CONFIG_PROJECT_NAME'])
    dst_dir = param_data["BIN_OUTPUT_DIR"]

    if not os.path.exists(dst_dir):
        os.makedirs(dst_dir, exist_ok=True)

    destination_path = os.path.join(dst_dir, param_data['CONFIG_PROJECT_NAME'] + "_QIO_" + param_data['CONFIG_PROJECT_VERSION'] + ".bin")
    shutil.copy(source_file, destination_path)

    destination_path = os.path.join(dst_dir, param_data['CONFIG_PROJECT_NAME'] + "_" + param_data['CONFIG_PROJECT_VERSION'] + ".elf")
    shutil.copy(source_file, destination_path)

    sys.exit(0)


if __name__ == "__main__":
    main()
