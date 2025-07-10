# 当前文件所在目录
LOCAL_PATH := $(call my-dir)

#---------------------------------------

# 清除 LOCAL_xxx 变量
include $(CLEAR_VARS)

ifeq ($(CONFIG_ENABLE_WIRED), y)
# 当前模块名
LOCAL_MODULE := $(notdir $(LOCAL_PATH))

# 模块对外头文件（只能是目录）
# 加载至CFLAGS中提供给其他组件使用；打包进SDK产物中；
TUYA_SDK_INC += $(LOCAL_PATH)/include

# 模块对外CFLAGS：其他组件编译时可感知到
TUYA_SDK_CFLAGS +=

# 模块对外DOCS：组件对外输出文档
TUYA_SDK_DOCS +=  $(LOCAL_PATH)/docs/*

# 模块源代码
LOCAL_SRC_FILES := $(shell find $(LOCAL_PATH)/src -name "*.c" -o -name "*.cpp" -o -name "*.cc")

# 模块内部CFLAGS：仅供本组件使用
LOCAL_CFLAGS :=  

# 生成静态库
include $(BUILD_STATIC_LIBRARY)

# 生成动态库
include $(BUILD_SHARED_LIBRARY)

endif
#---------------------------------------

