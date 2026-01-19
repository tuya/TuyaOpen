##
# @file CMakeLists.txt
# @brief 
#/
if (CONFIG_ENABLE_COMP_AI_DISPLAY STREQUAL "y")

set(APP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

set(APP_MODULE_SRCS)
list(APPEND APP_MODULE_SRCS
    ${APP_MODULE_PATH}/robot_app.c
    ${APP_MODULE_PATH}/gui_common.c
    )

# Fonts are provided by ai_components/ai_ui/font. Do not compile local copies to avoid duplication.
aux_source_directory(${APP_MODULE_PATH}/status STATUS_SRCS)
aux_source_directory(${APP_MODULE_PATH}/emotion EMOTION_SRCS)

list(APPEND APP_MODULE_SRCS
    ${STATUS_SRCS}
    ${EMOTION_SRCS}
)

set(AI_UI_FONT_PATH ${APP_MODULE_PATH}/../../../ai_components/ai_ui/font)

set(APP_MODULE_INC 
    ${APP_MODULE_PATH}
    ${APP_MODULE_PATH}/status
    ${APP_MODULE_PATH}/emotion
    ${PROJECT_SOURCE_DIR}/tools/porting/adapter/display
)

if (EXISTS ${AI_UI_FONT_PATH})
    list(APPEND APP_MODULE_INC ${AI_UI_FONT_PATH})
endif()

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