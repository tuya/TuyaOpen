##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
if (CONFIG_ENABLE_LIBU8G2 STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
file(GLOB_RECURSE  LIB_SRCS
    "${MODULE_PATH}/u8g2/csrc/*.c"
    "${MODULE_PATH}/u8g2/csrc/driver/*.c"
    "${MODULE_PATH}/port/*.c")

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC 
    ${MODULE_PATH}
    ${MODULE_PATH}/u8g2/csrc
    ${MODULE_PATH}/u8g2/csrc/driver
    ${MODULE_PATH}/port
)

########################################
# Target Configure
########################################
add_library(${MODULE_NAME})

target_sources(${MODULE_NAME}
    PRIVATE
        ${LIB_SRCS}
    )

target_include_directories(${MODULE_NAME}
    PRIVATE
        ${LIB_PRIVATE_INC}

    PUBLIC
        ${LIB_PUBLIC_INC}
    )

########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()
