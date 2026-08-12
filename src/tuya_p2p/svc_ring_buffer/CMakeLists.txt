##
# @file CMakeLists.txt
# @brief IPC ring buffer
#/

if (NOT CONFIG_ENABLE_IPC_RING_BUFFER STREQUAL "y")
    return()
endif()

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

file(GLOB_RECURSE LIB_SRCS "${MODULE_PATH}/src/*.c")
set(LIB_PUBLIC_INC ${MODULE_PATH}/include)

add_library(${MODULE_NAME} STATIC ${LIB_SRCS})
target_sources(${MODULE_NAME} PRIVATE ${LIB_SRCS})
target_include_directories(${MODULE_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../svc_streaming_p2p/include
    PUBLIC
        ${LIB_PUBLIC_INC}
)

list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
