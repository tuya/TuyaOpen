#!/usr/bin/env bash
##
# @file kconfiglib/save_config.sh
# @brief 
# @author Tuya
# @version 1.0.0
# @date 2023-04-12
#/

# $1: DOT_CONFIG_PATH
# $2: PRE_CONFIG_DIR

cd `dirname $0`

read -p "Enter new config name: " NEW_CONFIG_NAME
if [ x"" = x"$NEW_CONFIG_NAME" ]; then
    echo "[KCONFIG] New config name is [Empty]"
    exit 0
fi

if [ "config" != ${NEW_CONFIG_NAME##*.} ]; then
    NEW_CONFIG_NAME=$NEW_CONFIG_NAME.config
fi

if [ ! -d "$2" ]; then
    mkdir -p "$2"
fi

cp ${1} ${2}/${NEW_CONFIG_NAME}
echo "[KCONFIG] Save new config [${NEW_CONFIG_NAME}]"

