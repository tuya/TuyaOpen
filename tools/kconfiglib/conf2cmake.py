#!/usr/bin/env python3
# -*- coding: utf-8 -*-
##
# @file cong2h.py
# @brief .config to .cmake
# @author Tuya
# @version 1.0.0
# @date 2023-04-11
#


import os
import argparse


def conf2cmake(conf, cmake):
    context = ""
    conf_lines = []
    for f in conf:
        conf_f = open(f, 'r', encoding="utf-8")
        conf_lines += conf_f.readlines()
        conf_f.close()

    for cl in conf_lines:
        cl = cl.strip()
        ans = ""
        if cl.startswith("CONFIG_"):
            ori_key = cl.split('=', 1)[0]
            ori_value = cl.split('=', 1)[1].strip("\"")
            ans = f'set({ori_key} \"{ori_value}\")'
        else:
            ans = cl
        context += f'{ans}\n'

    cmake_f = open(cmake, 'w', encoding="utf-8")
    cmake_f.write(context+'\n')
    cmake_f.close()
    pass


if __name__ == "__main__":
    parse = argparse.ArgumentParser(
        usage="-c \"a.config b.config\" -o config.cmake",
        description="Translate .config to .h")
    parse.add_argument('-c', '--config', type=str,
                       default=".config",
                       help="Input config file. [.config]",
                       metavar="")
    parse.add_argument('-o', '--output', type=str,
                       default="config.h",
                       help="Output header file. [config.cmake]",
                       metavar="")
    args = parse.parse_args()

    conf_file = args.config
    cmake_file = args.output

    conf_file_sub = conf_file.split()
    conf_file_list = []
    for f in conf_file_sub:
        if os.path.exists(f):
            conf_file_list.append(f)
        else:
            print("can't find file: ", f)

    if not conf_file_list:
        print("No files !", conf_file)
        exit(1)

    conf2cmake(conf_file_list, cmake_file)
    pass
