##
# @file CMakeLists.txt
# @brief 
#/
set(APP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

# Automatically find all source files recursively in the module directory
file(GLOB_RECURSE APP_MODULE_SRCS "${APP_MODULE_PATH}/*.c")

########################################
# Target Configure
########################################
target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_MODULE_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_MODULE_PATH}
    )