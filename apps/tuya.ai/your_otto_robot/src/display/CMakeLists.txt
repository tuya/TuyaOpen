##
# @file CMakeLists.txt
# @brief 
#/
if (CONFIG_ENABLE_CHAT_DISPLAY STREQUAL "y")

set(APP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

set(APP_MODULE_SRCS)
list(APPEND APP_MODULE_SRCS
        ${APP_MODULE_PATH}/app_display.c
        ${APP_MODULE_PATH}/tuya_lvgl.c
    )

aux_source_directory(${APP_MODULE_PATH}/font FONT_SRCS)
aux_source_directory(${APP_MODULE_PATH}/font/emoji EMOJI_SRCS)
aux_source_directory(${APP_MODULE_PATH}/ui UI_SRCS)
aux_source_directory(${APP_MODULE_PATH}/image/eyes128 IMAG_EYES_SRCS)

set(IMAGE_SRCS ${APP_MODULE_PATH}/image/TuyaOpen_img_320_480.c)

list(APPEND APP_MODULE_SRCS
    ${FONT_SRCS}
    ${EMOJI_SRCS}
    ${IMAGE_SRCS}
    ${UI_SRCS}
    ${IMAG_EYES_SRCS}
)

set(APP_MODULE_INC 
    ${APP_MODULE_PATH}
    ${APP_MODULE_PATH}/font
    ${APP_MODULE_PATH}/ui
)

########################################
# Target Configure
########################################
target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_MODULE_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_MODULE_INC}
    )

endif()