# 当前文件所在目录
LOCAL_PATH := $(call my-dir)

# 清除 LOCAL_xxx 变量
include $(CLEAR_VARS)

# 当前模块名
LOCAL_MODULE := $(notdir $(LOCAL_PATH))

# 模块对外头文件（只能是目录）
# 加载至CFLAGS中提供给其他组件使用；打包进SDK产物中；
LOCAL_TUYA_SDK_INC := $(LOCAL_PATH)/include

# 模块对外CFLAGS：其他组件编译时可感知到
LOCAL_TUYA_SDK_CFLAGS :=

# 模块源代码
# ifeq ($(CONFIG_ENABLE_ADC), y)
# LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/os/tal_adc.c)
# endif

ifeq ($(CONFIG_ENABLE_GPIO), y)
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/os/tal_gpio.c)
endif

# ifeq ($(CONFIG_ENABLE_I2C), y)
# LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/os/tal_i2c.c)
# endif

# ifeq ($(CONFIG_ENABLE_PWM), y)
# LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/os/tal_pwm.c)
# endif

# ifeq ($(CONFIG_ENABLE_SPI), y)
# LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/os/tal_spi.c)
# endif

ifeq ($(CONFIG_ENABLE_UART), y)
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/os/tal_uart.c)
endif

# 模块内部CFLAGS：仅供本组件使用
LOCAL_CFLAGS :=

# 全局变量赋值
TUYA_SDK_INC += $(LOCAL_TUYA_SDK_INC)  # 此行勿修改
TUYA_SDK_CFLAGS += $(LOCAL_TUYA_SDK_CFLAGS)  # 此行勿修改

# 模块内部的SECTIONS： 仅本组件生效
ifeq ($(CONFIG_ENABLE_MULTI_SECTION_UPGRADE), y)
LOCAL_SECTION_NAME := .ts2
endif

# 生成静态库
include $(BUILD_STATIC_LIBRARY)

# 生成动态库
include $(BUILD_SHARED_LIBRARY)

# 导出编译详情
include $(OUT_COMPILE_INFO)

#---------------------------------------

