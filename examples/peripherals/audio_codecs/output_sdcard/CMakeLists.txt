##
# @file CMakeLists.txt
# @brief 
#/

if (CONFIG_ENABLE_AUDIO_CODECS STREQUAL "y")
# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRCS
aux_source_directory(${APP_PATH}/src APP_SRCS)

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )
else()
message(FATAL_ERROR "audio_recorder cannot work when ENABLE_AUDIO_CODECS is not set")
endif()