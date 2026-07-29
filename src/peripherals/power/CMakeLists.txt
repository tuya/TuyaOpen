##
# @file CMakeLists.txt
# @brief
#/

if (CONFIG_ENABLE_POWER STREQUAL "y")

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
file(GLOB_RECURSE LIB_SRCS
    "${MODULE_PATH}/tdl_power/src/*.c")

list(APPEND LIB_SRCS ${MODULE_PATH}/tdd_power/src/tdd_power_soc.c)

# LIB_PRIVATE_INC
set(LIB_PRIVATE_INC "")

# AXP2101 backend: built when the AXP2101 chip driver (pmic component) is enabled
if (CONFIG_ENABLE_PMIC STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tdd_power/src/tdd_power_axp2101.c)
    list(APPEND LIB_PRIVATE_INC ${MODULE_PATH}/../pmic/axp2101/include)
endif()

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC
        ${MODULE_PATH}/tdl_power/include
        ${MODULE_PATH}/tdd_power/include
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
