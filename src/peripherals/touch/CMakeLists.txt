##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
if (CONFIG_ENABLE_TOUCH STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
file(GLOB_RECURSE touch_srcs "${MODULE_PATH}/tdl_touch/src/*.c")

if (CONFIG_ENABLE_I2C STREQUAL "y")   
    file(GLOB_RECURSE tdd_touch_i2c_srcs "${MODULE_PATH}/tdd_touch/src/i2c/*.c")
endif()

set(LIB_SRCS ${touch_srcs} ${tdd_touch_i2c_srcs})

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC 
    ${MODULE_PATH}/tdl_touch/include
    ${MODULE_PATH}/tdd_touch/include
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