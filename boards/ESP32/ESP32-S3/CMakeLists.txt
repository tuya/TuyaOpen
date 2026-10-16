##
# @file CMakeLists.txt
# @brief Board-level sources and public headers for the generic ESP32-S3 board.
#/

set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

set(LIB_SRCS
    ${MODULE_PATH}/board_com_api.c
)

set(LIB_PUBLIC_INC
    ${MODULE_PATH}
)

add_library(ESP32-S3)

target_sources(ESP32-S3
    PRIVATE
        ${LIB_SRCS}
)

target_include_directories(ESP32-S3
    PUBLIC
        ${LIB_PUBLIC_INC}
)

list(APPEND COMPONENT_LIBS ESP32-S3)
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)

list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
