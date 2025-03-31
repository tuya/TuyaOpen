##
# @file CMakeLists.txt
# @brief 
#/
if (CONFIG_ENABLE_CHAT_DISPLAY STREQUAL "y")

set(APP_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

aux_source_directory(${APP_MODULE_PATH} APP_MODULE_SCRS)

set(APP_MODULE_INC ${APP_MODULE_PATH})

if (CONFIG_ENABLE_GUI_WECHAT STREQUAL "y")
    set(GUI_INC ${APP_MODULE_PATH}/gui_wechat)
    set(GUI_MATERIALS_INC ${APP_MODULE_PATH}/gui_wechat/material)
    list(APPEND APP_MODULE_INC ${GUI_INC} ${GUI_MATERIALS_INC})

    aux_source_directory(${APP_MODULE_PATH}/gui_wechat GUI_SRC)
    list(APPEND APP_MODULE_SCRS ${GUI_SRC})
    list(APPEND APP_MODULE_SCRS
        ${APP_MODULE_PATH}/gui_wechat/material/font_awesome_30_4.c
        ${APP_MODULE_PATH}/gui_wechat/material/FONT_SY_20.c
        ${APP_MODULE_PATH}/gui_wechat/material/icon_listen.c
    )

    if (CONFIG_ENABLE_GUI_WECHAT_SZ_320_480 STREQUAL "y")
        list(APPEND APP_MODULE_SCRS ${APP_MODULE_PATH}/gui_wechat/material/TuyaOpen_img_320_480.c)
    elseif(CONFIG_ENABLE_GUI_WECHAT_SZ_240_240 STREQUAL "y")
        list(APPEND APP_MODULE_SCRS ${APP_MODULE_PATH}/gui_wechat/material/TuyaOpen_img_240_240.c)
    else()
        message(FATAL_ERROR "Please select display gui size")
    endif()
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