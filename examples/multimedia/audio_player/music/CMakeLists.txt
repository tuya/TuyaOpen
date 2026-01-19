##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRCS
file(GLOB_RECURSE audio_spk_src "${APP_PATH}/src/*.c")

set(APP_SRCS ${audio_spk_src})

set(APP_MODULE_INC 
    ${APP_PATH}/src
    ${APP_PATH}/src/minimp3
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
        ${APP_MODULE_INC}
    )
