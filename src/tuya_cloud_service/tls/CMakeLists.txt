##
# @file CMakeLists.txt
# @brief TLS session helpers over the platform TLS backend.
#/

set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(MODULE_DIR_NAME ${MODULE_PATH} NAME)
# Prefixed rather than bare: src/peripherals/transport already builds a target
# called `transport` from its own directory name, and CMake target names are
# global. One collision is enough to make the whole set carry the prefix -
# picking it per directory would leave the next reader guessing which is which.
set(MODULE_NAME tcs_${MODULE_DIR_NAME})

file(GLOB_RECURSE LIB_SRCS "${MODULE_PATH}/*.c")
set(LIB_PUBLIC_INC ${MODULE_PATH})

add_library(${MODULE_NAME})
target_sources(${MODULE_NAME} PRIVATE ${LIB_SRCS})
target_include_directories(${MODULE_NAME}
    PRIVATE
        ${LIB_PRIVATE_INC}
    PUBLIC
        ${LIB_PUBLIC_INC}
    )

list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
