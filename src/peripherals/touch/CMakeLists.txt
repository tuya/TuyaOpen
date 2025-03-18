##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
if (CONFIG_ENABLE_TOUCH_DRIVER STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
set(LIB_SRCS 
        "${MODULE_PATH}/tkl_touch.c"
)

if (CONFIG_ENABLE_TOUCH_GT911 STREQUAL "y")
    list(APPEND LIB_SRCS "${MODULE_PATH}/gt911_i2c.c")
elseif(CONFIG_ENABLE_TOUCH_CST816X STREQUAL "y")
    list(APPEND LIB_SRCS "${MODULE_PATH}/cst816x.c")
else()
    message(FATAL_ERROR "Please select a touch driver")
endif()

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH})

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