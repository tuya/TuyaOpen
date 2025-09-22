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

list(APPEND media_srcs "${APP_PATH}/src/media")

list(APPEND APP_SRCS ${media_srcs})

set(APP_INC 
            ${APP_PATH}/assets
            ${APP_PATH}/include
            ${APP_PATH}/include/media
)

list(APPEND APP_INC "${APP_PATH}/assets")

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

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        "-DLV_LVGL_H_INCLUDE_SIMPLE"
    )

########################################
# Add subdirectory
########################################

if (CONFIG_ENABLE_CHAT_DISPLAY STREQUAL "y")
        add_subdirectory(${APP_PATH}/src/display)
endif()

add_subdirectory(${APP_PATH}/../ai_components/ai_audio)
target_include_directories(${EXAMPLE_LIB} PRIVATE ${APP_PATH}/../ai_components/ai_audio)
