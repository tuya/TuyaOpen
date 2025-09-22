#!/bin/sh

# ==============================================================================
# This script is used to generate the tuyaopen platform template
# ------------------------------------------------------------------------------
# example:
# ./kernel_porting.sh <your-platform-name>
# ------------------------------------------------------------------------------

print_not_null()
{
    # If $1 is empty, return error
    if [ x"$1" = x"" ]; then
        return 1
    fi

    echo "$1"
}

select_sub_dir()
{
    ls $1 -1F | grep -oP '[\w-.]*(?=/)' | bash ./listmenu.sh "select new platform" 20 60 13
}

set -e
cd `dirname $0`
CURRENT_DIR=$(pwd)
ROOT_DIR=$CURRENT_DIR/..
TARGET_PLATFORM_KERNEL_VERSION=`echo latest`
TARGET_PLATFORM=`print_not_null $1 || select_sub_dir ${ROOT_DIR}/platform`
TARGET_PLATFORM_PATH=$ROOT_DIR/platform/$TARGET_PLATFORM
TEMPLATE_PATH=$TARGET_PLATFORM_PATH/default.config
KERNEL_PATH=$TARGET_PLATFORM_PATH/tuyaos/tuyaos_adapter
echo TARGET_PLATFORM=$TARGET_PLATFORM
echo TARGET_PLATFORM_PATH=$TARGET_PLATFORM_PATH
echo CURRENT_DIR=$CURRENT_DIR
echo ROOT_DIR=$ROOT_DIR

check_kernel_env()
{
    if [ -d "$TARGET_PLATFORM_PATH" ]; then
        echo Kernel existed at $TARGET_PLATFORM_PATH...
        read -p  "Do you want to update [yes/no]?" user_input
        if [ x"$user_input" = x"yes" ]; then   
            echo Update the kernel...
        else
            echo Please backup and try again...            
            exit 1
        fi        
    else
        echo Kernel not existed at $TARGET_PLATFORM_PATH...
        mkdir -p $TARGET_PLATFORM_PATH
        mkdir -p $TARGET_PLATFORM_PATH/tuyaos
        cp $ROOT_DIR/tools/porting/template/Kconfig $TARGET_PLATFORM_PATH
    fi
}

generate_default_config()
{
    python3 ./kconfiglib/set_catalog_config.py -b $TARGET_PLATFORM_PATH -s $ROOT_DIR/src -o $TARGET_PLATFORM_PATH/allconfig
    ./kconfiglib/run_menuconfig.sh $TARGET_PLATFORM_PATH/allconfig $TARGET_PLATFORM_PATH/default.config
    rm -rf $TARGET_PLATFORM_PATH/allconfig
}


check_kernel_env
generate_default_config
./kernel_update.sh $TARGET_PLATFORM
python3 ./porting/kernel_porting.py $TARGET_PLATFORM_PATH $TARGET_PLATFORM