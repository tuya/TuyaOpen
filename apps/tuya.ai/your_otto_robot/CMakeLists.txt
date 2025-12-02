##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRCS
aux_source_directory(${APP_PATH}/src APP_SRCS)

list(APPEND media_srcs "${APP_PATH}/src/media")

# Add otto source files based on configuration
if (CONFIG_OTTO_TYPE_OTTO STREQUAL "y")
    # Add otto source files explicitly
    aux_source_directory(${APP_PATH}/src/otto OTTO_SRCS)
    list(APPEND APP_SRCS ${OTTO_SRCS})
endif()

# Add otto_ninja source files based on configuration
if (CONFIG_OTTO_TYPE_NINJA_OTTO STREQUAL "y")
    # Add otto_ninja source files explicitly
    aux_source_directory(${APP_PATH}/src/otto_ninja OTTO_NINJA_SRCS)
    list(APPEND APP_SRCS ${OTTO_NINJA_SRCS})
endif()

list(APPEND APP_SRCS ${media_srcs})

set(APP_INC 
            ${APP_PATH}/assets
            ${APP_PATH}/include
            ${APP_PATH}/include/media
)

# Add include directories based on configuration
if (CONFIG_OTTO_TYPE_OTTO STREQUAL "y")
    list(APPEND APP_INC ${APP_PATH}/src/otto)
endif()

if (CONFIG_OTTO_TYPE_NINJA_OTTO STREQUAL "y")
    list(APPEND APP_INC ${APP_PATH}/src/otto_ninja)
endif()

list(APPEND APP_INC "${APP_PATH}/assets")

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        "-DLV_LVGL_H_INCLUDE_SIMPLE"
    )

########################################
# Add subdirectory
########################################

if (CONFIG_ENABLE_CHAT_DISPLAY STREQUAL "y")
        add_subdirectory(${APP_PATH}/src/display)
endif()

add_subdirectory(${APP_PATH}/../ai_components/ai_audio)
target_include_directories(${EXAMPLE_LIB} PRIVATE ${APP_PATH}/../ai_components/ai_audio)
