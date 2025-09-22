#!/usr/bin/env bash
##
# @file kconfiglib/run_menuconfig.sh
# @brief 
# @author Tuya
# @version 1.0.0
# @date 2023-04-08
#/

KCONFIG_CATALOG=$1
DOT_KCONFIG=$2

KCONFIG_TOOLS=`dirname $0`

export KCONFIG_CONFIG="$DOT_KCONFIG"
python3 -u $KCONFIG_TOOLS/menuconfig.py $KCONFIG_CATALOG
