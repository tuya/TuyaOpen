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
    ${APP_MODULE_PATH}/robot_app.c
    ${APP_MODULE_PATH}/tuya_ai_display.c
    ${APP_MODULE_PATH}/gui_common.c
    )

aux_source_directory(${APP_MODULE_PATH}/font FONT_SRCS)
aux_source_directory(${APP_MODULE_PATH}/font/emoji EMOJI_SRCS)
aux_source_directory(${APP_MODULE_PATH}/ui UI_SRCS)
aux_source_directory(${APP_MODULE_PATH}/image/eyes128 IMAG_EYES_SRCS)
aux_source_directory(${APP_MODULE_PATH}/status STATUS_SRCS)
aux_source_directory(${APP_MODULE_PATH}/emotion EMOTION_SRCS)

set(IMAGE_SRCS ${APP_MODULE_PATH}/image/TuyaOpen_img_320_480.c)

list(APPEND APP_MODULE_SRCS
    ${FONT_SRCS}
    ${EMOJI_SRCS}
    ${IMAGE_SRCS}
    ${UI_SRCS}
    ${IMAG_EYES_SRCS}
    ${STATUS_SRCS}
    ${EMOTION_SRCS}
)

set(APP_MODULE_INC 
    ${APP_MODULE_PATH}
    ${APP_MODULE_PATH}/font
    ${APP_MODULE_PATH}/ui
    ${APP_MODULE_PATH}/status
    ${APP_MODULE_PATH}/emotion
    ${PROJECT_SOURCE_DIR}/tools/porting/adapter/display
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