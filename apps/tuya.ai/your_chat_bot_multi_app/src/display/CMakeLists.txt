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
        ${APP_MODULE_PATH}/sys_manager.c
    )

aux_source_directory(${APP_MODULE_PATH}/font FONT_SRCS)
aux_source_directory(${APP_MODULE_PATH}/font/emoji EMOJI_SRCS)
aux_source_directory(${APP_MODULE_PATH}/gui_app/pages/chat_ui UI_SRCS)
aux_source_directory(${APP_MODULE_PATH}/gui_app GUI_APP_SRCS)
aux_source_directory(${APP_MODULE_PATH}/gui_app/common PAGE_MANAGER_SRCS)
aux_source_directory(${APP_MODULE_PATH}/gui_app/pages PAGES_SRCS)
aux_source_directory(${APP_MODULE_PATH}/gui_app/fonts GUI_FONT_SRCS)
aux_source_directory(${APP_MODULE_PATH}/gui_app/images GUI_IMAGE_SRCS)
aux_source_directory(${APP_MODULE_PATH}/gui_app/common/animation ANIMATION_SRCS)
aux_source_directory(${APP_MODULE_PATH}/image/eyes128 IMAG_EYES_SRCS)

# 明确添加页面管理器源文件
set(PAGE_MANAGER_SRCS
    ${APP_MODULE_PATH}/gui_app/common/page_manager/lv_lib_pm.c
    ${APP_MODULE_PATH}/gui_app/common/stack/lv_lib_stack.c
)

# 明确添加页面源文件 - 包含核心页面、Game2048Page和CalculatorPage
set(PAGES_SRCS
    ${APP_MODULE_PATH}/gui_app/pages/ui_HomePage/ui_HomePage.c
    ${APP_MODULE_PATH}/gui_app/pages/ui_SettingPage/ui_SettingPage.c
    ${APP_MODULE_PATH}/gui_app/pages/ui_WeatherPage/ui_WeatherPage.c
    ${APP_MODULE_PATH}/gui_app/pages/ui_WeatherPage/app_WeatherPage.c
    ${APP_MODULE_PATH}/gui_app/pages/ui_Game2048Page/ui_Game2048Page.c
    ${APP_MODULE_PATH}/gui_app/pages/ui_CalculatorPage/ui_CalculatorPage.c
    ${APP_MODULE_PATH}/gui_app/pages/ui_CalculatorPage/app_CalculatorPage.c
    ${APP_MODULE_PATH}/gui_app/pages/ui_CalendarPage/ui_CalendarPage.c
    ${APP_MODULE_PATH}/gui_app/pages/ui_CameraPage/ui_CameraPage.c
    ${APP_MODULE_PATH}/gui_app/pages/ui_GameMemoryPage/ui_GameMemoryPage.c
    ${APP_MODULE_PATH}/gui_app/pages/ui_GameMuyuPage/ui_GameMuyuPage.c
    ${APP_MODULE_PATH}/gui_app/pages/ui_DrawPage/ui_DrawPage.c
    ${APP_MODULE_PATH}/gui_app/pages/ui_Game_dinoso_jump/ui_dino.c
)

set(IMAGE_SRCS ${APP_MODULE_PATH}/image/TuyaOpen_img_320_480.c)

list(APPEND APP_MODULE_SRCS
    ${FONT_SRCS}
    ${EMOJI_SRCS}
    ${IMAGE_SRCS}
    ${UI_SRCS}
    ${GUI_APP_SRCS}
    ${PAGE_MANAGER_SRCS}
    ${PAGES_SRCS}
    ${GUI_FONT_SRCS}
    ${GUI_IMAGE_SRCS}
    ${ANIMATION_SRCS}
    ${IMAG_EYES_SRCS}
)

set(APP_MODULE_INC 
    ${APP_MODULE_PATH}
    ${APP_MODULE_PATH}/font
    ${APP_MODULE_PATH}/gui_app/pages/chat_ui
    ${APP_MODULE_PATH}/gui_app
    ${APP_MODULE_PATH}/gui_app/common
    ${APP_MODULE_PATH}/gui_app/pages
    ${APP_MODULE_PATH}/gui_app/common/animation
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