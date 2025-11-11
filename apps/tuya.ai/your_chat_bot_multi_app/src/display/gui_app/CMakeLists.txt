
file(GLOB_RECURSE GUI_APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"  # 查找当前目录及子目录中的所有 .c 文件
)

# 定义 gui_app 模块为一个静态库
add_library(gui_app STATIC ${GUI_APP_SOURCES})

# 设置 gui_app 的头文件路径
target_include_directories(gui_app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/common/stack
    ${CMAKE_CURRENT_SOURCE_DIR}/common/page_manager
    ${PROJECT_SOURCE_DIR}/src/liblvgl
    ${PROJECT_SOURCE_DIR}/src/liblvgl/lvgl
    ${PROJECT_SOURCE_DIR}/src/liblvgl/port
    ${PROJECT_SOURCE_DIR}/src/liblvgl/conf
)

# 让 gui_app 也使用简单的 LVGL 头文件包含方式
target_compile_definitions(gui_app PUBLIC LV_LVGL_H_INCLUDE_SIMPLE)

# 注: 嵌入式目标不依赖主机 json-c/curl 库