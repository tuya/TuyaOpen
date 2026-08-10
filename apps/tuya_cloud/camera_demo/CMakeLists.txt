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

set(APP_INC ${APP_PATH}/include)

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
        ${CMAKE_CURRENT_LIST_DIR}/../../../src/audio_player/src/resample/resample_fixed.c
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
        ${CMAKE_CURRENT_LIST_DIR}/../../../src/audio_player/src/resample
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/../../../platform/T5AI/tuyaos/tuyaos_adapter/include/vad
    )
