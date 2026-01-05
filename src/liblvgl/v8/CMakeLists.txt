##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
if (CONFIG_LVGL_VERSION_8 STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
file(GLOB_RECURSE  LIB_SRCS
    "${MODULE_PATH}/lvgl/src/*.c" "${MODULE_PATH}/port/*.c")

set(LVGL_DEMO_SRCS "")
if (CONFIG_ENABLE_LVGL_DEMO STREQUAL "y")
    file(GLOB_RECURSE  LVGL_DEMO_SRCS "${MODULE_PATH}/lvgl/demos/*.c" "${MODULE_PATH}/lvgl/examples/*.c" )
endif()

list(APPEND LIB_SRCS
    ${LVGL_DEMO_SRCS})

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC 
    ${MODULE_PATH}
    ${MODULE_PATH}/lvgl
    ${MODULE_PATH}/port
    ${MODULE_PATH}/conf
)

set(LIB_OPTIONS "-O3")

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

target_compile_options(${MODULE_NAME}
    PRIVATE
        ${LIB_OPTIONS}
    )
########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()