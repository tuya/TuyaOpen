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

list(APPEND APP_SRCS 
    ${APP_PATH}/libqrencode/qrencode.c
    ${APP_PATH}/libqrencode/qrinput.c
    ${APP_PATH}/libqrencode/bitstream.c
    ${APP_PATH}/libqrencode/qrspec.c
    ${APP_PATH}/libqrencode/rsecc.c
    ${APP_PATH}/libqrencode/split.c
    ${APP_PATH}/libqrencode/mask.c
    ${APP_PATH}/libqrencode/mqrspec.c
    ${APP_PATH}/libqrencode/mmask.c
)

add_definitions(-DSTATIC_IN_RELEASE=static)
add_definitions(-DMAJOR_VERSION=4 -DMINOR_VERSION=1 -DMICRO_VERSION=1 -DVERSION=\"4.1.1\")

# APP_INC
set(APP_INC ${APP_PATH}/libqrencode)

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
