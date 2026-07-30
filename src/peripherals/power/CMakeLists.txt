##
# @file CMakeLists.txt
# @brief
#/

if (CONFIG_ENABLE_POWER STREQUAL "y")

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS: TDL layer + every TDD backend and its chip driver. The chip drivers
# (axp2101/m5pm1) are implementation detail of this component, not a standalone one.
file(GLOB_RECURSE LIB_SRCS
    "${MODULE_PATH}/tdl_power/src/*.c")

list(APPEND LIB_SRCS
        ${MODULE_PATH}/tdd_power/src/tdd_power_soc.c
        ${MODULE_PATH}/tdd_power/src/tdd_power_axp2101.c
        ${MODULE_PATH}/tdd_power/src/tdd_power_m5pm1.c
        ${MODULE_PATH}/tdd_power/src/axp2101/axp2101_driver.c
        ${MODULE_PATH}/tdd_power/src/m5pm1/m5pm1_driver.c
    )

# LIB_PRIVATE_INC
set(LIB_PRIVATE_INC "")

# LIB_PUBLIC_INC: each chip driver keeps its header next to its source under src/<chip>/.
# Those dirs are public so both the backends and board code that drives the PMIC directly
# (GPIO/wake, e.g. M5STACK) can reach the chip headers.
set(LIB_PUBLIC_INC
        ${MODULE_PATH}/tdl_power/include
        ${MODULE_PATH}/tdd_power/include
        ${MODULE_PATH}/tdd_power/src/axp2101
        ${MODULE_PATH}/tdd_power/src/m5pm1
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
