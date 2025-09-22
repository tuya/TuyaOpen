##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
if (CONFIG_ENABLE_PMIC STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
set(LIB_SRCS "")
set(LIB_PUBLIC_INC "")

if (CONFIG_ENABLE_PMIC_AXP2101 STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/axp2101/src/axp2101_driver.c)
    list(APPEND LIB_PUBLIC_INC ${MODULE_PATH}/axp2101/include)
endif()

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