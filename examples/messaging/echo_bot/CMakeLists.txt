##
# @file CMakeLists.txt
# @brief Echo bot: standalone messaging app (Telegram/Discord/Feishu), no external app refs.
#/

set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(APP_NAME ${APP_PATH} NAME)

set(APP_SRCS
    ${APP_PATH}/src/tuya_app_main.c
    ${APP_PATH}/src/cli_echo.c
    ${APP_PATH}/src/wifi/wifi_manager.c
)

set(APP_INC
    ${APP_PATH}/include
    ${APP_PATH}/src
)

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
)

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
)

########################################
# Add subdirectory
########################################
add_subdirectory(${APP_PATH}/IM)
