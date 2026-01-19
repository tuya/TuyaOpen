##
# @file CMakeLists.txt
# @brief 
#/

# MODULE PATH
set(APP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP MODULE SRC
set(APP_MODULE_SRCS)

# APP MODULE INC
set(APP_MODULE_INC)

# APP MODULE SRC FILE (Recursively search for all .c/.cc/.cpp files in subdirectories)
# Use CONFIGURE_DEPENDS to automatically trigger reconfiguration when source files are added
file(GLOB_RECURSE APP_MODULE_SRC_FILES
    CONFIGURE_DEPENDS
    "${APP_MODULE_PATH}/*.c"
    "${APP_MODULE_PATH}/*.cc"
    "${APP_MODULE_PATH}/*.cpp"
)
list(APPEND APP_MODULE_SRCS ${APP_MODULE_SRC_FILES})

# 输出调试信息（不终止配置）
message(STATUS "[tflite-sine] APP_MODULE_SRCS: ${APP_MODULE_SRCS}")

list(APPEND APP_MODULE_INC ${APP_MODULE_PATH})
list(APPEND APP_MODULE_INC ${APP_MODULE_PATH}/third_party/flatbuffers/include)
list(APPEND APP_MODULE_INC ${APP_MODULE_PATH}/third_party/gemmlowp)
list(APPEND APP_MODULE_INC ${APP_MODULE_PATH}/third_party/kissfft)
list(APPEND APP_MODULE_INC ${APP_MODULE_PATH}/third_party/ruy)
list(APPEND APP_MODULE_INC ${APP_MODULE_PATH}/examples/hello_world)

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