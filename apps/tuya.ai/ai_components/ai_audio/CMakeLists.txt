##
# @file CMakeLists.txt
# @brief 
#/
set(APP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

set(APP_MODULE_SRCS)

file(GLOB_RECURSE APP_MODULE_SRCS ${APP_MODULE_PATH}/src/*.c) 

set(APP_MODULE_INC 
    ${APP_MODULE_PATH}/include
    ${APP_MODULE_PATH}/include/media
    ${APP_MODULE_PATH}/minimp3
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
