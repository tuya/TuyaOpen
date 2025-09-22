#!/bin/sh

# ==============================================================================
# This script is used to update the tuyaopen platform template
# ------------------------------------------------------------------------------
# example:
# ./kernel_update.sh 
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

update_kernel_by_template()
{
    echo copy kernel $TARGET_PLATFORM_KERNEL_VERSION to $TARGET_PLATFORM_PATH ...
    echo KERNEL_CONFIG_PATH=$KERNEL_CONFIG_PATH
    [ ! -e $KERNEL_CONFIG_PATH ] && return 99 && echo "error: kernel config not existed!"
    
    if [ ! -d $KERNEL_ADAPTER_PATH ]; then
        mkdir -p $KERNEL_ADAPTER_PATH/include
    fi 

    # utilities
    rm -rf  $KERNEL_ADAPTER_PATH/include/utilities
    mkdir -p $KERNEL_ADAPTER_PATH/include/utilities
    cp -rf $TEMPLATE_PATH/utilities/include  $KERNEL_ADAPTER_PATH/include/utilities/
    cp -rf $TEMPLATE_PATH/utilities/src  $KERNEL_ADAPTER_PATH/include/utilities/

    # init
    rm -rf  $KERNEL_ADAPTER_PATH/include/init
    mkdir -p $KERNEL_ADAPTER_PATH/include/init
    cp -rf $TEMPLATE_PATH/init/include  $KERNEL_ADAPTER_PATH/include/init/
    cp -rf $TEMPLATE_PATH/init/src  $KERNEL_ADAPTER_PATH/include/init/

    # security
    rm -rf  $KERNEL_ADAPTER_PATH/include/security
    mkdir -p $KERNEL_ADAPTER_PATH/include/security
    cp $TEMPLATE_PATH/security/include/*  $KERNEL_ADAPTER_PATH/include/security/

    # network
    rm -rf  $KERNEL_ADAPTER_PATH/include/network
    mkdir -p $KERNEL_ADAPTER_PATH/include/network
    cp $TEMPLATE_PATH/network/include/*  $KERNEL_ADAPTER_PATH/include/network/

    # system
    rm -rf  $KERNEL_ADAPTER_PATH/include/system
    mkdir -p $KERNEL_ADAPTER_PATH/include/system
    cp $TEMPLATE_PATH/system/include/*  $KERNEL_ADAPTER_PATH/include/system/

    # flash
    rm -rf  $KERNEL_ADAPTER_PATH/include/flash
    mkdir -p $KERNEL_ADAPTER_PATH/include/flash
    cp $TEMPLATE_PATH/flash/include/*  $KERNEL_ADAPTER_PATH/include/flash/
    
    cat $KERNEL_CONFIG_PATH | while read line
    do 
        # null
        if [ x"$line" = x"" ] ; then
            continue
        fi

        # disable ability
        IS_DISABLE_ABILITY=$(echo "$line" | grep -b -o "#" | cut -d: -f1)
        if [ ! x"$IS_DISABLE_ABILITY" = x"" ] ; then
            ABILITY=$(echo "$line" | cut -d " " -f2)
            [ x"$ABILITY" = x"CONFIG_ENABLE_WIRED" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/wired && rm -rf $KERNEL_ADAPTER_PATH/include/init/include/tkl_init_wired.h && rm -rf $KERNEL_ADAPTER_PATH/include/init/src/tkl_init_wired.c && echo ABILITY $ABILITY is disabled, rm wired
            [ x"$ABILITY" = x"CONFIG_ENABLE_WIFI" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/wifi && rm -rf $KERNEL_ADAPTER_PATH/include/init/include/tkl_init_wifi.h && rm -rf $KERNEL_ADAPTER_PATH/include/init/src/tkl_init_wifi.c && echo ABILITY $ABILITY is disabled, rm wifi
            [ x"$ABILITY" = x"CONFIG_ENABLE_PM" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/pm && rm -rf $KERNEL_ADAPTER_PATH/include/init/include/tkl_init_pm.h && rm -rf $KERNEL_ADAPTER_PATH/include/init/src/tkl_init_pm.c && echo ABILITY $ABILITY is disabled, rm pm
            [ x"$ABILITY" = x"CONFIG_ENABLE_BLUETOOTH" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/bluetooth && echo ABILITY $ABILITY is disabled, rm bluetooth
            [ x"$ABILITY" = x"CONFIG_ENABLE_RTC" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/rtc && echo ABILITY $ABILITY is disabled, rm rtc
            [ x"$ABILITY" = x"CONFIG_ENABLE_ADC" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/adc && echo ABILITY $ABILITY is disabled, rm adc
            [ x"$ABILITY" = x"CONFIG_ENABLE_UART" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/uart && echo ABILITY $ABILITY is disabled, rm uart
            [ x"$ABILITY" = x"CONFIG_ENABLE_WATCHDOG" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/watchdog && echo ABILITY $ABILITY is disabled, rm watchdog
            # [ x"$ABILITY" = x"CONFIG_ENABLE_FLASH" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/flash && echo ABILITY $ABILITY is disabled, rm flash
            [ x"$ABILITY" = x"CONFIG_ENABLE_PWM" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/pwm && echo ABILITY $ABILITY is disabled, rm pwm
            [ x"$ABILITY" = x"CONFIG_ENABLE_GPIO" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/gpio && echo ABILITY $ABILITY is disabled, rm gpio
            [ x"$ABILITY" = x"CONFIG_ENABLE_TIMER" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/timer && echo ABILITY $ABILITY is disabled, rm timer
            [ x"$ABILITY" = x"CONFIG_ENABLE_HCI" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/hci && echo ABILITY $ABILITY is disabled, rm hci
            [ x"$ABILITY" = x"CONFIG_ENABLE_DISPLAY" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/display && echo ABILITY $ABILITY is disabled, rm display
            [ x"$ABILITY" = x"CONFIG_ENABLE_MEDIA" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/media && echo ABILITY $ABILITY is disabled, rm media
            [ x"$ABILITY" = x"CONFIG_ENABLE_STORAGE" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/storage && echo ABILITY $ABILITY is disabled, rm storage
            [ x"$ABILITY" = x"CONFIG_ENABLE_DAC" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/dac && echo ABILITY $ABILITY is disabled, rm dac
            [ x"$ABILITY" = x"CONFIG_ENABLE_I2C" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/i2c && echo ABILITY $ABILITY is disabled, rm i2c
            [ x"$ABILITY" = x"CONFIG_ENABLE_I2S" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/i2s && echo ABILITY $ABILITY is disabled, rm i2s
            [ x"$ABILITY" = x"CONFIG_ENABLE_PINMUX" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/pinmux && echo ABILITY $ABILITY is disabled, rm pinmux
            [ x"$ABILITY" = x"CONFIG_ENABLE_WAKEUP" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/wakeup && echo ABILITY $ABILITY is disabled, rm wakeup
            [ x"$ABILITY" = x"CONFIG_ENABLE_REGISTER" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/register && echo ABILITY $ABILITY is disabled, rm register
            [ x"$ABILITY" = x"CONFIG_ENABLE_SPI" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/spi && echo ABILITY $ABILITY is disabled, rm spi
            [ x"$ABILITY" = x"CONFIG_ENABLE_FILE_SYSTEM" ] && rm -rf  $KERNEL_ADAPTER_PATH/include/system/tkl_fs.h && echo ABILITY $ABILITY is disabled, rm spi

            continue
        fi

        # enable ability
        ABILITY=$(echo "$line" | cut -d "=" -f 1)
        ABILITY_STATUS=$(echo "$line" | cut -d "=" -f 2)
        echo ABILITY $ABILITY is $ABILITY_STATUS, update...
        if [ x"$ABILITY" = x"CONFIG_OPERATING_SYSTEM" ]; then
            if [ x"$ABILITY_STATUS" = x"100" ]; then
                rm -rf $KERNEL_ADAPTER_PATH/include/network
                rm -rf $KERNEL_ADAPTER_PATH/include/init/src/tkl_init_network.c
            fi
        fi

        if [ x"$ABILITY" = x"CONFIG_ENABLE_WIRED" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/wired
            mkdir -p $KERNEL_ADAPTER_PATH/include/wired
            cp $TEMPLATE_PATH/wired/include/* $KERNEL_ADAPTER_PATH/include/wired/
        fi

        if [ x"$ABILITY" = x"CONFIG_ENABLE_WIFI" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/wifi
            mkdir -p $KERNEL_ADAPTER_PATH/include/wifi
            cp $TEMPLATE_PATH/wifi/include/*  $KERNEL_ADAPTER_PATH/include/wifi/
        fi

        if [ x"$ABILITY" = x"CONFIG_ENABLE_BLUETOOTH" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/bluetooth
            mkdir -p $KERNEL_ADAPTER_PATH/include/bluetooth
            cp $TEMPLATE_PATH/bluetooth/include/*  $KERNEL_ADAPTER_PATH/include/bluetooth/
        fi


        if [ x"$ABILITY" = x"CONFIG_ENABLE_RTC" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/rtc
            mkdir -p $KERNEL_ADAPTER_PATH/include/rtc
            cp $TEMPLATE_PATH/rtc/include/*  $KERNEL_ADAPTER_PATH/include/rtc/
        fi

        if [ x"$ABILITY" = x"CONFIG_ENABLE_ADC" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/adc
            mkdir -p $KERNEL_ADAPTER_PATH/include/adc
            cp $TEMPLATE_PATH/adc/include/*  $KERNEL_ADAPTER_PATH/include/adc/
        fi

        if [ x"$ABILITY" = x"CONFIG_ENABLE_UART" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/uart
            mkdir -p $KERNEL_ADAPTER_PATH/include/uart
            cp $TEMPLATE_PATH/uart/include/*  $KERNEL_ADAPTER_PATH/include/uart/
        fi

        if [ x"$ABILITY" = x"CONFIG_ENABLE_WATCHDOG" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/watchdog
            mkdir -p $KERNEL_ADAPTER_PATH/include/watchdog
            cp $TEMPLATE_PATH/watchdog/include/*  $KERNEL_ADAPTER_PATH/include/watchdog/
        fi        

        # if [ x"$ABILITY" = x"CONFIG_ENABLE_FLASH" ]; then
        #     rm -rf  $KERNEL_ADAPTER_PATH/include/flash
        #     mkdir -p $KERNEL_ADAPTER_PATH/include/flash
        #     cp $TEMPLATE_PATH/flash/include/*  $KERNEL_ADAPTER_PATH/include/flash/
        # fi        

        if [ x"$ABILITY" = x"CONFIG_ENABLE_PWM" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/pwm
            mkdir -p $KERNEL_ADAPTER_PATH/include/pwm
            cp $TEMPLATE_PATH/pwm/include/*  $KERNEL_ADAPTER_PATH/include/pwm/
        fi 

        if [ x"$ABILITY" = x"CONFIG_ENABLE_GPIO" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/gpio
            mkdir -p $KERNEL_ADAPTER_PATH/include/gpio
            cp $TEMPLATE_PATH/gpio/include/*  $KERNEL_ADAPTER_PATH/include/gpio/
        fi 

        if [ x"$ABILITY" = x"CONFIG_ENABLE_TIMER" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/timer
            mkdir -p $KERNEL_ADAPTER_PATH/include/timer
            cp $TEMPLATE_PATH/timer/include/*  $KERNEL_ADAPTER_PATH/include/timer/
        fi 

        if [ x"$ABILITY" = x"CONFIG_ENABLE_HCI" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/bluetooth
            rm -rf  $KERNEL_ADAPTER_PATH/include/hci
            mkdir -p $KERNEL_ADAPTER_PATH/include/bluetooth
            mkdir -p $KERNEL_ADAPTER_PATH/include/hci
            cp $TEMPLATE_PATH/bluetooth/include/*  $KERNEL_ADAPTER_PATH/include/bluetooth/
            cp $TEMPLATE_PATH/hci/include/*  $KERNEL_ADAPTER_PATH/include/hci/
        fi 

        if [ x"$ABILITY" = x"CONFIG_ENABLE_DISPLAY" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/display
            mkdir -p $KERNEL_ADAPTER_PATH/include/display
            cp $TEMPLATE_PATH/display/include/*  $KERNEL_ADAPTER_PATH/include/display/
        fi 

        if [ x"$ABILITY" = x"CONFIG_ENABLE_MEDIA" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/media
            mkdir -p $KERNEL_ADAPTER_PATH/include/media
            cp $TEMPLATE_PATH/media/include/*  $KERNEL_ADAPTER_PATH/include/media/
        fi 

        if [ x"$ABILITY" = x"CONFIG_ENABLE_PM" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/pm
            mkdir -p $KERNEL_ADAPTER_PATH/include/pm
            cp $TEMPLATE_PATH/pm/include/*  $KERNEL_ADAPTER_PATH/include/pm/
        fi 

        if [ x"$ABILITY" = x"CONFIG_ENABLE_STORAGE" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/storage
            mkdir -p $KERNEL_ADAPTER_PATH/include/storage
            cp $TEMPLATE_PATH/storage/include/*  $KERNEL_ADAPTER_PATH/include/storage/
        fi 

        if [ x"$ABILITY" = x"CONFIG_ENABLE_DAC" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/dac
            mkdir -p $KERNEL_ADAPTER_PATH/include/dac
            cp $TEMPLATE_PATH/dac/include/*  $KERNEL_ADAPTER_PATH/include/dac/
        fi 

        if [ x"$ABILITY" = x"CONFIG_ENABLE_I2C" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/i2c
            mkdir -p $KERNEL_ADAPTER_PATH/include/i2c
            cp $TEMPLATE_PATH/i2c/include/*  $KERNEL_ADAPTER_PATH/include/i2c/
        fi 

        if [ x"$ABILITY" = x"CONFIG_ENABLE_I2S" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/i2s
            mkdir -p $KERNEL_ADAPTER_PATH/include/i2s
            cp $TEMPLATE_PATH/i2s/include/*  $KERNEL_ADAPTER_PATH/include/i2s/
        fi 

        if [ x"$ABILITY" = x"CONFIG_ENABLE_PINMUX" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/pinmux
            mkdir -p $KERNEL_ADAPTER_PATH/include/pinmux
            cp $TEMPLATE_PATH/pinmux/include/*  $KERNEL_ADAPTER_PATH/include/pinmux/
        fi 

        if [ x"$ABILITY" = x"CONFIG_ENABLE_WAKEUP" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/wakeup
            mkdir -p $KERNEL_ADAPTER_PATH/include/wakeup
            cp $TEMPLATE_PATH/wakeup/include/*  $KERNEL_ADAPTER_PATH/include/wakeup/
        fi 

        if [ x"$ABILITY" = x"CONFIG_ENABLE_REGISTER" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/register
            mkdir -p $KERNEL_ADAPTER_PATH/include/register
            cp $TEMPLATE_PATH/register/include/*  $KERNEL_ADAPTER_PATH/include/register/
        fi 

        if [ x"$ABILITY" = x"CONFIG_ENABLE_SPI" ]; then
            rm -rf  $KERNEL_ADAPTER_PATH/include/spi
            mkdir -p $KERNEL_ADAPTER_PATH/include/spi
            cp $TEMPLATE_PATH/spi/include/*  $KERNEL_ADAPTER_PATH/include/spi/
        fi           
    done

}

set -e
cd `dirname $0`
CURRENT_DIR=$(pwd)
ROOT_DIR=$CURRENT_DIR/..
TARGET_PLATFORM_KERNEL_VERSION=`echo latest`
TARGET_PLATFORM=`print_not_null $1 || select_sub_dir ${ROOT_DIR}/platform`
TARGET_PLATFORM_PATH=$ROOT_DIR/platform/$TARGET_PLATFORM
TEMPLATE_PATH=$ROOT_DIR/tools/porting/adapter
KERNEL_CONFIG_PATH=$TARGET_PLATFORM_PATH/default.config
KERNEL_DOCS_PATH=$TARGET_PLATFORM_PATH/tuyaos/docs
KERNEL_ADAPTER_PATH=$TARGET_PLATFORM_PATH/tuyaos/tuyaos_adapter
echo TARGET_PLATFORM=$TARGET_PLATFORM
echo TARGET_PLATFORM_PATH=$TARGET_PLATFORM_PATH
echo CURRENT_DIR=$CURRENT_DIR
echo ROOT_DIR=$ROOT_DIR
echo ABILITY=$ABILITY
[ -z $TARGET_PLATFORM ] && echo "error: no platform!" && exit 99
[ ! -d $TARGET_PLATFORM_PATH ] && echo "error: platform not exist!" && exit 99

update_kernel_by_template

echo update kernel to $TARGET_PLATFORM_KERNEL_VERSION success
cd $CURRENT_DIR
