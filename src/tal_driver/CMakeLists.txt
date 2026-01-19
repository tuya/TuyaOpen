##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# Set library sources, public includes and private includes
set(LIB_SRCS)
set(LIB_PUBLIC_INC)
set(LIB_PRIVATE_INC)

# Add uart source files
file(GLOB_RECURSE UART_SRCS "${MODULE_PATH}/uart/*.c")
list(APPEND LIB_SRCS ${UART_SRCS})
list(APPEND LIB_PUBLIC_INC ${MODULE_PATH}/uart)

# Add dma2d source files
if (CONFIG_ENABLE_DMA2D STREQUAL "y")
    file(GLOB_RECURSE DMA2D_SRCS "${MODULE_PATH}/dma2d/*.c")
    list(APPEND LIB_SRCS ${DMA2D_SRCS})
    list(APPEND LIB_PUBLIC_INC ${MODULE_PATH}/dma2d)
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
