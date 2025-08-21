##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
set(LIB_SRCS ${MODULE_PATH}/src/tal_kv.c ${MODULE_PATH}/src/kv_serialize.c)

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC 
    ${MODULE_PATH}/include
)

if (NOT DEFINED CONFIG_BOARD_CHOICE_L511C OR NOT (CONFIG_BOARD_CHOICE_L511C STREQUAL "y"))
set(LITTLEFS_SRC ${MODULE_PATH}/littlefs/lfs_util.c ${MODULE_PATH}/littlefs/lfs.c)
list(APPEND LIB_SRCS ${LITTLEFS_SRC})

set(LITTLEFS_INC ${MODULE_PATH}/littlefs)
list(APPEND LIB_PUBLIC_INC ${LITTLEFS_INC})

set(LIB_PRIVATE_INC  ${MODULE_PATH}/port)
add_definitions(-DLFS_CONFIG=lfs_config.h)
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