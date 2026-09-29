##
# @file CMakeLists.txt
# @brief 
#/

if (CONFIG_ENABLE_AUDIO_CODECS STREQUAL "y")
# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
file(GLOB_RECURSE LIB_SRCS 
    "${MODULE_PATH}/tdl_audio/src/*.c")

if (CONFIG_ENABLE_MEDIA STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tdd_audio/src/tdd_audio.c)
    if (CONFIG_PLATFORM_CHOICE STREQUAL "JIELI")
        # TKL media APIs are supplied by the platform adapter, not this module.
        # Keep these headers scoped to Jieli media builds so other platforms
        # continue to resolve their own TKL audio interfaces.
        list(APPEND LIB_PRIVATE_INC
            ${CMAKE_SOURCE_DIR}/tools/porting/adapter/media
            ${CMAKE_SOURCE_DIR}/tools/porting/adapter/vad
        )
    endif()
endif()

# Add ALSA support for Ubuntu platform
if (CONFIG_ENABLE_AUDIO_ALSA STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tdd_audio/src/tdd_audio_alsa.c)
    message(STATUS "ALSA audio support enabled")
endif()

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC
    ${MODULE_PATH}/tdl_audio/include
    ${MODULE_PATH}/tdd_audio/include
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

# Link ALSA library if ALSA support is enabled
if (CONFIG_ENABLE_AUDIO_ALSA STREQUAL "y")
    target_link_libraries(${MODULE_NAME} PUBLIC asound pthread)
    target_compile_definitions(${MODULE_NAME} PUBLIC ENABLE_AUDIO_ALSA=1)
endif()


########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
list(APPEND COMPONENT_PUBINC ${LIB_PRIVATE_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()
