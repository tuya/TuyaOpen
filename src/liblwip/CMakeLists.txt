##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
if (CONFIG_ENABLE_LIBLWIP STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

set(LWIP lwip-2.1.2)

# LIB_SRCS
file(GLOB_RECURSE  LWIP_SRCS  
"${MODULE_PATH}/${LWIP}/src/core/*.c"
"${MODULE_PATH}/${LWIP}/src/api/*.c")

list(APPEND LWIP_SRCS ${MODULE_PATH}/${LWIP}/src/netif/ethernet.c)

file(GLOB_RECURSE  LIB_SRCS  "${MODULE_PATH}/port/*.c")
list(APPEND LIB_SRCS ${LWIP_SRCS})

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC 
    ${MODULE_PATH}/${LWIP}/src/include
    ${MODULE_PATH}/${LWIP}/src/include/lwip
    ${MODULE_PATH}/${LWIP}/src/include/lwip/apps
    ${MODULE_PATH}/${LWIP}/src/include/compat
    )

set(LIB_OPTIONS "-w")

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