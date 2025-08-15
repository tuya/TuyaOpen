##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
# if (CONFIG_ENABLE_BLUETOOTH STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
file(GLOB_RECURSE  LIB_SRCS  "${MODULE_PATH}/src/*.c")

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC 
    ${MODULE_PATH}/pjlib/include
    ${MODULE_PATH}/pjlib-util/include
    ${MODULE_PATH}/pjmedia/include
    ${MODULE_PATH}/pjnath/include)

# set(NIMBLE nimble)
file(GLOB_RECURSE  PJPROJECT_SRCS  
    "${MODULE_PATH}/pjlib/src/*.c"
    "${MODULE_PATH}/pjlib-util/src/*.c"
    "${MODULE_PATH}/pjmedia/src/*.c"
    "${MODULE_PATH}/pjnath/src/*.c")
list(REMOVE_ITEM PJPROJECT_SRCS "${MODULE_PATH}/pjlib/src/pj/os_rwmutex.c" "${MODULE_PATH}/pjlib/src/pj/ioqueue_common_abs.c")
list(REMOVE_ITEM PJPROJECT_SRCS "${MODULE_PATH}/pjlib-util/src/pjlib-util/scanner_cis_bitwise.c" "${MODULE_PATH}/pjlib-util/src/pjlib-util/scanner_cis_uint.c")
message("PJPROJECT_SRCS: ${PJPROJECT_SRCS}")
list(APPEND LIB_SRCS ${PJPROJECT_SRCS})

########################################
# Target Configure
########################################
message("LIB_SRCS: ${LIB_SRCS}")
add_library(${MODULE_NAME} STATIC ${LIB_SRCS})

target_sources(${MODULE_NAME}
    PRIVATE
        ${LIB_SRCS}
    )

target_include_directories(${MODULE_NAME}
    PRIVATE
        ${LIB_PRIVATE_INC}

    INTERFACE
        ${LIB_INTERFACE_INC}

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

# endif()

