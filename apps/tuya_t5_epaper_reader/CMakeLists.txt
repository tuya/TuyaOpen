##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRCS
aux_source_directory(${APP_PATH}/src APP_SRCS)

# Add utf8_to_gbk source
list(APPEND APP_SRCS 
    ${CMAKE_SOURCE_DIR}/apps/tuya_t5_pocket/tuya_t5_pocket_ai/src/expand/src/utf8_to_gbk.c
)

# Add E-Paper Library sources
aux_source_directory(${APP_PATH}/lib/Config LIB_CONFIG_SRCS)
aux_source_directory(${APP_PATH}/lib/e-Paper LIB_EPAPER_SRCS)
aux_source_directory(${APP_PATH}/lib/GUI LIB_GUI_SRCS)
aux_source_directory(${APP_PATH}/lib/Fonts LIB_FONTS_SRCS)

list(APPEND APP_SRCS 
    ${LIB_CONFIG_SRCS}
    ${LIB_EPAPER_SRCS}
    ${LIB_GUI_SRCS}
    ${LIB_FONTS_SRCS}
    ${CMAKE_SOURCE_DIR}/src/liblvgl/v9/lvgl/src/libs/tjpgd/tjpgd.c
    ${APP_PATH}/src/third_party/lodepng/lodepng_wrap.c
    ${CMAKE_SOURCE_DIR}/src/common/qrcode/qrcodegen.c
)

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_PATH}/src
        ${APP_PATH}/src/third_party/lodepng
        ${CMAKE_SOURCE_DIR}/apps/tuya_t5_pocket/tuya_t5_pocket_ai/src/expand/inc
        ${CMAKE_SOURCE_DIR}/examples/e-Paper/4.26inch_network_novel_reader/lib/Fonts
        ${CMAKE_SOURCE_DIR}/src/common/qrcode
        ${APP_PATH}/lib/Config
        ${APP_PATH}/lib/e-Paper
        ${APP_PATH}/lib/GUI
        ${APP_PATH}/lib/Fonts
        ${CMAKE_SOURCE_DIR}/src/liblvgl/v9/lvgl/src/libs/tjpgd
)

target_compile_definitions(${EXAMPLE_LIB}
    PRIVATE
)

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )
