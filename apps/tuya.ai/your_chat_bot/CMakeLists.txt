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

file(GLOB_RECURSE ai_audio_srcs "${APP_PATH}/src/ai_audio/*.c")
file(GLOB_RECURSE media_srcs "${APP_PATH}/src/ai_audio/media/*.c")
list(APPEND APP_SRCS ${ai_audio_srcs} ${media_srcs})

set(APP_INC 
            ${APP_PATH}/include
            ${APP_PATH}/include/ai_audio
            ${APP_PATH}/include/ai_audio/media
)

# ESP32 cannot use app header files
if (NOT CONFIG_PLATFORM_CHOICE STREQUAL "ESP32")
    list(APPEND APP_INC "${APP_PATH}/assets")
endif()

list(APPEND APP_INC "${APP_PATH}/src/minimp3")
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

if (NOT CONFIG_PLATFORM_CHOICE STREQUAL "ESP32")
    # esp32 use idf lvgl
    if (CONFIG_ENABLE_CHAT_DISPLAY STREQUAL "y")
            add_subdirectory(${APP_PATH}/src/display)
    endif()
endif()