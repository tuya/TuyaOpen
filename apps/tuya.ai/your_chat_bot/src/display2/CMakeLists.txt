##
# @file CMakeLists.txt
# @brief 
#/
if (CONFIG_ENABLE_CHAT_DISPLAY2 STREQUAL "y")

set(APP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

set(APP_MODULE_SRCS)
list(APPEND APP_MODULE_SRCS
        ${APP_MODULE_PATH}/app_ui_helper.c
        ${APP_MODULE_PATH}/app_display.c
        ${APP_MODULE_PATH}/tuya_lvgl.c
    )

set(APP_MODULE_INC 
    ${APP_MODULE_PATH}
)

# add subdirectory
if (CONFIG_ENABLE_GUI2_CHATBOT STREQUAL "y")
add_subdirectory(${APP_MODULE_PATH}/ui_chatbot)
elseif (CONFIG_ENABLE_GUI2_WECHAT STREQUAL "y")
add_subdirectory(${APP_MODULE_PATH}/ui_wechat)
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