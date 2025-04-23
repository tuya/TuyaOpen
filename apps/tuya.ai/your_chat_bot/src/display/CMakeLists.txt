##
# @file CMakeLists.txt
# @brief 
#/
if (CONFIG_ENABLE_CHAT_DISPLAY STREQUAL "y")

set(APP_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

aux_source_directory(${APP_MODULE_PATH} APP_MODULE_SCRS)

set(APP_MODULE_INC ${APP_MODULE_PATH})

list(APPEND APP_MODULE_INC ${APP_MODULE_PATH}/material)
    
if (CONFIG_ENABLE_GUI_WECHAT STREQUAL "y")
    set(GUI_INC ${APP_MODULE_PATH}/gui_wechat)

    aux_source_directory(${APP_MODULE_PATH}/gui_wechat GUI_SRC)
else()
    set(GUI_INC ${APP_MODULE_PATH}/gui_chatbot)

    aux_source_directory(${APP_MODULE_PATH}/gui_chatbot GUI_SRC)
endif()

list(APPEND APP_MODULE_INC ${GUI_INC})
list(APPEND APP_MODULE_SCRS ${GUI_SRC})

list(APPEND APP_MODULE_SCRS
    ${APP_MODULE_PATH}/material/font_awesome_30_4.c
    ${APP_MODULE_PATH}/material/font_puhui_18_2.c
    ${APP_MODULE_PATH}/material/icon_listen.c
)

if (CONFIG_DISPLAY_LCD_WIDTH STREQUAL "320" AND CONFIG_DISPLAY_LCD_HEIGHT STREQUAL "480")
    list(APPEND APP_MODULE_SCRS ${APP_MODULE_PATH}/material/TuyaOpen_img_320_480.c)
elseif(CONFIG_DISPLAY_LCD_WIDTH STREQUAL "240" AND CONFIG_DISPLAY_LCD_HEIGHT STREQUAL "240")
    list(APPEND APP_MODULE_SCRS ${APP_MODULE_PATH}/material/TuyaOpen_img_240_240.c)
else()
    message(FATAL_ERROR "Please select display gui size")
endif()

########################################
# Target Configure
########################################
target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_MODULE_SCRS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_MODULE_INC}
    )

endif()