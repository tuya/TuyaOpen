##
# @file CMakeLists.txt
# @brief 
#/

if (CONFIG_ENABLE_IR STREQUAL "y")
# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
file(GLOB_RECURSE LIB_SRCS 
    "${MODULE_PATH}/tdl_ir_device/src/*.c"
    )

if (CONFIG_PLATFORM_T2 STREQUAL "y")
    list(APPEND LIB_SRCS "${MODULE_PATH}/tdd_ir_driver/src/tdd_ir_driver_T2.c")
elseif (CONFIG_PLATFORM_T5 STREQUAL "y")
    list(APPEND LIB_SRCS "${MODULE_PATH}/tdd_ir_driver/src/tdd_ir_driver_T5.c")
elseif (CONFIG_PLATFORM_BK7231X STREQUAL "y")
    list(APPEND LIB_SRCS "${MODULE_PATH}/tdd_ir_driver/src/tdd_ir_driver_bk7231n.c")
elseif (CONFIG_PLATFORM_T1 STREQUAL "y")
    list(APPEND LIB_SRCS "${MODULE_PATH}/tdd_ir_driver/src/tdd_ir_driver_T1.c")
else()
    message(FATAL_ERROR "IR driver not supported for this platform")
endif()

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC 
        ${MODULE_PATH}/tdl_ir_device/include
        ${MODULE_PATH}/tdd_ir_driver/include
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