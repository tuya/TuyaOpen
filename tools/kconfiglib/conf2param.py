#!/usr/bin/env python3
# -*- coding: utf-8 -*-
##
# @file conf2param.py
# @brief append parameters list to .config -> (.cmake .config .json)
# @author Tuya
# @version 1.0.0
# @date 2025-05-08
#


import os
import json
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


def param2config(params_data, conf_list, config_out):
    context = ""
    for key in params_data.keys():
        value = params_data[key]
        ans = f'{key}=\"{value}\"'
        context += f'{ans}\n'

    conf_lines = []
    for f in conf_list:
        conf_f = open(f, 'r', encoding="utf-8")
        conf_lines += conf_f.readlines()
        conf_f.close()

    for cl in conf_lines:
        cl = cl.strip()
        if not cl.startswith("CONFIG_"):
            continue
        context += f'{cl}\n'

    config_f = open(config_file, 'w', encoding="utf-8")
    config_f.write(context+'\n')
    config_f.close()
    pass


def conf2param(conf_list, params_data):
    conf_lines = []
    for f in conf_list:
        conf_f = open(f, 'r', encoding="utf-8")
        conf_lines += conf_f.readlines()
        conf_f.close()

    for cl in conf_lines:
        cl = cl.strip()
        if not cl.startswith("CONFIG_"):
            continue
        ori_key = cl.split('=', 1)[0]
        ori_value = cl.split('=', 1)[1]
        if ori_value.startswith('\"'):  # str
            params_data[ori_key] = ori_value.strip("\"")
        elif ori_value == "y":  # bool
            params_data[ori_key] = True
        elif ori_value.isdigit():  # int
            params_data[ori_key] = int(ori_value)
        else:  # hex
            params_data[ori_key] = f"{ori_value}"
    pass


def param2cmake(params_data, cmake_file):
    context = ""
    for key in params_data.keys():
        value = params_data[key]
        if type(value) is str:
            # str
            ans = f'set({key} \"{value}\")'
        elif (type(value) is bool) and (value is True):
            # bool true
            ans = f'set({key} \"y\")'
        else:
            # other
            ans = f'set({key} \"{value}\")'
        context += f'{ans}\n'

    cmake_f = open(cmake_file, 'w', encoding="utf-8")
    cmake_f.write(context+'\n')
    cmake_f.close()
    pass


def param2json(params_data, json_file):
    json_str = json.dumps(params_data, indent=4, ensure_ascii=False)
    with open(json_file, 'w') as f:
        f.write(json_str)
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
                       default="params_file",
                       help="Output header file. [config.cmake]",
                       metavar="")
    parse.add_argument('-p', '--params', type=str,
                       default="",
                       help="Parameters list",
                       metavar="")
    args = parse.parse_args()

    conf_file = args.config
    cmake_file = args.output + ".cmake"
    json_file = args.output + ".json"
    config_file = args.output + ".config"
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

    param2config(params_data, conf_file_list, config_file)

    conf2param(conf_file_list, params_data)
    param2cmake(params_data, cmake_file)
    param2json(params_data, json_file)
    pass
