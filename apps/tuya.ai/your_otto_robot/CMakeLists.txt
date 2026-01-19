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

if (CONFIG_ENABLE_APP_CUSTOM_EYES_UI STREQUAL "y")
    aux_source_directory(${APP_PATH}/src/ui/src UI_SRCS)

    if (CONFIG_APP_UI_EYES_160_80 STREQUAL "y")
        aux_source_directory(${APP_PATH}/src/ui/image/eyes160_80 IMAG_EYES_SRCS)
    elseif (CONFIG_APP_UI_EYES_160_160 STREQUAL "y")
        aux_source_directory(${APP_PATH}/src/ui/image/eyes160_160 IMAG_EYES_SRCS)
    elseif (CONFIG_APP_UI_EYES_240_240 STREQUAL "y")
        aux_source_directory(${APP_PATH}/src/ui/image/eyes240 IMAG_EYES_SRCS)
    elseif (CONFIG_APP_UI_EYES_128_128 STREQUAL "y")
        aux_source_directory(${APP_PATH}/src/ui/image/eyes128 IMAG_EYES_SRCS)
    endif()
    
    list(APPEND APP_SRCS ${UI_SRCS} ${IMAG_EYES_SRCS})
endif()

set(APP_INC 
            ${APP_PATH}/include
)

# Add include directories based on configuration
if (CONFIG_OTTO_TYPE_OTTO STREQUAL "y")
    list(APPEND APP_INC ${APP_PATH}/src/otto)
endif()

if (CONFIG_OTTO_TYPE_NINJA_OTTO STREQUAL "y")
    list(APPEND APP_INC ${APP_PATH}/src/otto_ninja)
endif()

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
add_subdirectory(${APP_PATH}/../ai_components)
