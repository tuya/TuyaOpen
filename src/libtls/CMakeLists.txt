##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

set(MBEDTLS mbedtls-3.1.0)

# LIB_SRCS
file(GLOB_RECURSE 
    LIB_SRCS 
    "${MODULE_PATH}/src/*.c")

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC 
    ${MODULE_PATH}/include
    ${MODULE_PATH}/port)

if (CONFIG_ENABLE_PLATFORM_MBEDTLS STREQUAL "y")
    message(STATUS "Using mbedtls from platform")
else()
    file(GLOB_RECURSE MBEDTLS_SRCS "${MODULE_PATH}/${MBEDTLS}/library/*.c")

    list(APPEND LIB_SRCS "${MBEDTLS_SRCS}")
    list(APPEND LIB_PUBLIC_INC "${MODULE_PATH}/${MBEDTLS}/include")
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

