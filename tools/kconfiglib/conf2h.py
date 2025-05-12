#!/usr/bin/env python3
# -*- coding: utf-8 -*-
##
# @file cong2h.py
# @brief .config to .h
# @author Tuya
# @version 1.0.0
# @date 2023-04-10
#


import os
import argparse


##
# @brief need parse [***] [:::] [""]
#        KEY:::"VALUE"***
#
# @param params
#
# @return key-value
def _parse_params(params):
    params_data = {}
    key_value_list = params.split("***")
    for key_value in key_value_list:
        key_value = key_value.strip()
        kv = key_value.split(":::", 1)
        if len(kv) < 2:
            continue
        key = kv[0]
        value = kv[1].strip("\"")
        params_data[key] = value
    return params_data


def conf2h(conf, header, header_in, params_data={}):
    context = ""
    for key in params_data.keys():
        value = params_data[key]
        ans = f'#define {key} \"{value}\"'
        context += f'{ans}\n'

    conf_lines = []
    for f in conf:
        conf_f = open(f, 'r', encoding="utf-8")
        conf_lines += conf_f.readlines()
        conf_f.close()

    # context
    for cl in conf_lines:
        cl = cl.strip()
        ans = ""
        if cl.startswith("CONFIG_"):
            ori_key = cl.split('=', 1)[0]
            ori_value = cl.split('=', 1)[1]

            def_head = "#define "
            def_key = ori_key.replace("CONFIG_", '', 1) + ' '
            def_value = ori_value if ori_value != 'y' else "1"

            ans = def_head + def_key + def_value
        elif cl.startswith("#"):
            ans = cl.replace('#', "//", 1)
        else:
            ans = cl
        context += f'{ans}\n'

    # config.h.in
    sub_flag = "@CONFIG_HEADER_CONTEXT@"
    in_context = sub_flag
    if os.path.exists(header_in):
        f = open(header_in, 'r', encoding="utf-8")
        in_context = f.read()
    context = in_context.replace(sub_flag, context)

    header_f = open(header, 'w', encoding="utf-8")
    header_f.write(context+'\n')
    header_f.close()
    pass


if __name__ == "__main__":
    parse = argparse.ArgumentParser(
        usage="-c \"a.config b.config\" -o config.h",
        description="Translate .config to .h")
    parse.add_argument('-c', '--config', type=str,
                       default=".config",
                       help="Input config file. [.config]",
                       metavar="")
    parse.add_argument('-o', '--output', type=str,
                       default="config.h",
                       help="Output header file. [config.h]",
                       metavar="")
    parse.add_argument('-i', '--input', type=str,
                       default="config.h.in",
                       help="Input header file. [config.h.in]",
                       metavar="", dest="header_in")
    parse.add_argument('-p', '--params', type=str,
                       default="",
                       help="Parameters list",
                       metavar="")
    args = parse.parse_args()

    conf_file = args.config
    h_file = args.output
    h_file_in = args.header_in
    params = args.params
    params_data = _parse_params(params)

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

    conf2h(conf_file_list, h_file, h_file_in, params_data)
    pass
