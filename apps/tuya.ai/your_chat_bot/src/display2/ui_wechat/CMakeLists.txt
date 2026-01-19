set(APP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

set(APP_MODULE_SRCS)
file(GLOB_RECURSE APP_MODULE_SRCS "${APP_MODULE_PATH}/*.c")

set(APP_MODULE_INC
    ${APP_MODULE_PATH}
)

# 将当前目录的源码与头文件添加到已有的 ${EXAMPLE_LIB}
target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_MODULE_SRCS}
)
target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_MODULE_INC}
)