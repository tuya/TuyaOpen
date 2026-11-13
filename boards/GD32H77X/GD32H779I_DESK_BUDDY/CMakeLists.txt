##
# @file CMakeLists.txt
# @brief GD32H779I DESK BUDDY board component.
#/

set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

aux_source_directory(${MODULE_PATH} LIB_SRCS)
set(LIB_PUBLIC_INC ${MODULE_PATH})

add_library(${MODULE_NAME})

target_sources(${MODULE_NAME}
    PRIVATE
        ${LIB_SRCS}
    )

target_compile_definitions(${MODULE_NAME} PRIVATE GD32H779XW)

target_include_directories(${MODULE_NAME}
    PRIVATE
        ${LIB_PRIVATE_INC}
        ${TOP_SOURCE_DIR}/platform/GD32H77X/gd32h77x_os/gd32/CMSIS
        ${TOP_SOURCE_DIR}/platform/GD32H77X/gd32h77x_os/gd32/CMSIS/GD/GD32H77x_78x/Include
        ${TOP_SOURCE_DIR}/platform/GD32H77X/gd32h77x_os/gd32/GD32H77x_78x_standard_peripheral/Include
        ${TOP_SOURCE_DIR}/platform/GD32H77X/gd32h77x_os/gd32/include

    PUBLIC
        ${LIB_PUBLIC_INC}
    )

list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
