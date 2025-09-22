#!/usr/bin/env bash
##
# @file build_example.sh
# @brief When compiled for windows, gcc may generate .d file with a colon ":" in the path.
#        Causes a second compilation error (the colon in the makefile is treated as the target).
#        This script generates the compiler_depend.internal file before compiling.
#        You can prevent compiler_depend.make from being built
# @author Tuya
# @version 1.0.0
# @date 2024-01-30
#/

BUILD_DIR=$1
FLAG=$2
BUILD_CMD="example_all"

if [ x"$BUILD_DIR" = x"" ]; then
    echo "Error: Need build path."
    exit 1
fi

cd $BUILD_DIR

if [ x"$FLAG" = x"1" ]; then
    DEPEND_PATHS=`find ${BUILD_DIR} -name 'compiler_depend.make'`
    for p in $DEPEND_PATHS;
    do
    echo "" > ${p:0:-5}.internal;
    done
fi

make $BUILD_CMD
