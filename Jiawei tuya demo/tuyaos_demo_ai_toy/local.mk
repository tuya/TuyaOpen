# 当前文件所在目录
LOCAL_PATH := $(call my-dir)

#---------------------------------------

# 清除 LOCAL_xxx 变量
include $(CLEAR_VARS)
include $(LOCAL_PATH)/build/tuya_app.config

# 当前模块名
LOCAL_MODULE := $(notdir $(LOCAL_PATH))

# 模块对外头文件（只能是目录）
# 加载至CFLAGS中提供给其他组件使用；打包进SDK产物中；
LOCAL_TUYA_SDK_INC := $(LOCAL_PATH)/include
LOCAL_TUYA_SDK_INC += $(LOCAL_PATH)/src/app_tuya_driver/include
LOCAL_TUYA_SDK_INC += $(LOCAL_PATH)/src/display $(LOCAL_PATH)/src/app_tuya_led/include
LOCAL_TUYA_SDK_INC += $(LOCAL_PATH)/src/base_cli/include
LOCAL_TUYA_SDK_INC += $(LOCAL_PATH)/src/app_tuya_key/include
LOCAL_TUYA_SDK_INC += $(LOCAL_PATH)/src/media/
ifeq ($(T5AI_CELLULAR_BOARD), y)
LOCAL_TUYA_SDK_INC += $(LOCAL_PATH)/src/libqrencode/
endif


ifneq ($(APP_PACK_FLAG), 1)  
LOCAL_TUYA_SDK_INC += $(LOCAL_PATH)/application_components
LOCAL_TUYA_SDK_INC += $(LOCAL_PATH)/application_drivers
endif

# 模块对外CFLAGS：其他组件编译时可感知到
VER = $(shell echo $(APP_VER) | grep -oP '\d*\.\d*\.\d*')
LOCAL_TUYA_SDK_CFLAGS := -DUSER_SW_VER=\"$(VER)\" -DAPP_BIN_NAME=\"$(APP_NAME)\"

# 模块源代码
LOCAL_SRC_FILES := $(shell find $(LOCAL_PATH)/src/  -maxdepth 1 -name "*.c" -o -name "*.cpp" -o -name "*.cc")
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/app_tuya_driver -name "*.c" -o -name "*.cpp" -o -name "*.cc")
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/app_tuya_key -name "*.c" -o -name "*.cpp" -o -name "*.cc")
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/app_tuya_led -name "*.c" -o -name "*.cpp" -o -name "*.cc")
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/media/ -name "*.c" -o -name "*.cpp" -o -name "*.cc")
ifeq ($(CONFIG_T5AI_CELLULAR_BOARD), y)
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/libqrencode/ -name "*.c" -o -name "*.cpp" -o -name "*.cc")
endif

ifeq ($(CONFIG_ENABLE_TUYA_UI), y)
LOCAL_SRC_FILES += $(LOCAL_PATH)/src/display/tuya_ai_display.c 
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/display/ui -maxdepth 1 -name "*.c" -o -name "*.cpp" -o -name "*.cc")
endif

LOCAL_SRC_FILES += $(LOCAL_PATH)/src/display/gui_common.c 
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/display/ui/status  -name "*.c" -o -name "*.cpp" -o -name "*.cc")

ifeq ($(CONFIG_T5AI_BOARD), y)
LOCAL_SRC_FILES += $(LOCAL_PATH)/src/display/wechat_app.c
endif

ifeq ($(CONFIG_T5AI_BOARD_EYES), y)
LOCAL_SRC_FILES += $(LOCAL_PATH)/src/display/eyes_app.c
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/display/ui/eyes  -name "*.c" -o -name "*.cpp" -o -name "*.cc")
endif

ifeq ($(CONFIG_T5AI_BOARD_EVB), y)
LOCAL_SRC_FILES += $(LOCAL_PATH)/src/display/xiaozhi_app.c
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/display/ui/emoji  -name "*.c" -o -name "*.cpp" -o -name "*.cc")
endif

ifeq ($(CONFIG_T5AI_BOARD_ROBOT), y)
LOCAL_TUYA_SDK_INC += $(LOCAL_PATH)/src/servo_ctrl/
LOCAL_TUYA_SDK_INC += $(LOCAL_PATH)/src/
LOCAL_SRC_FILES += $(LOCAL_PATH)/src/display/robot_app.c
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/servo_ctrl/ -name "*.c" -o -name "*.cpp" -o -name "*.cc")
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/display/ui/robot  -name "*.c" -o -name "*.cpp" -o -name "*.cc")
endif

ifeq ($(CONFIG_T5AI_BOARD_CELLULAR), y)
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/libqrencode/ -name "*.c" -o -name "*.cpp" -o -name "*.cc")
endif

# ifeq ($(CONFIG_ENABLE_AUDIO_ANALYSIS), y)
LOCAL_TUYA_SDK_INC += $(LOCAL_PATH)/src/audio_analysis/
LOCAL_SRC_FILES += $(shell find $(LOCAL_PATH)/src/audio_analysis  -name "*.c" -o -name "*.cpp" -o -name "*.cc")
# endif

# 模块内部CFLAGS：仅供本组件使用
LOCAL_CFLAGS :=

# 全局变量赋值
TUYA_SDK_INC += $(LOCAL_TUYA_SDK_INC)  # 此行勿修改
TUYA_SDK_CFLAGS += $(LOCAL_TUYA_SDK_CFLAGS)  # 此行勿修改

# 生成静态库
include $(BUILD_STATIC_LIBRARY)

# 生成动态库
include $(BUILD_SHARED_LIBRARY)

# 导出编译详情
include $(OUT_COMPILE_INFO)

#---------------------------------------


