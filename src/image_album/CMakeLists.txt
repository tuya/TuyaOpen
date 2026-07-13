##
# @file CMakeLists.txt
# @brief Image Album Component
#/

if (CONFIG_ENABLE_IMAGE_ALBUM STREQUAL "y")

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
file(GLOB LIB_SRCS "${MODULE_PATH}/src/*.c")

if (CONFIG_ENABLE_IMAGE_ALBUM_STORAGE_MEM STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/src/storage/image_album_storage_mem.c)
endif()

if (CONFIG_ENABLE_IMAGE_ALBUM_STORAGE_SD STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/src/storage/image_album_storage_sd.c)
endif()

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH}/include)

# LIB_PRIVATE_INC
set(LIB_PRIVATE_INC ${MODULE_PATH}/src)

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

target_link_libraries(${MODULE_NAME}
    PUBLIC
        tal_system
        tal_image
        common
    )

########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()
