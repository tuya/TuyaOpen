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
aux_source_directory(${APP_PATH}/src/media media_alert)

list(APPEND APP_SRCS ${media_srcs} ${media_alert})

set(APP_INC 
            ${APP_PATH}/include
            ${APP_PATH}/include/media
)

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

add_subdirectory(${APP_PATH}/src/display)
add_subdirectory(${APP_PATH}/src/expand)


add_subdirectory(${APP_PATH}/../../tuya.ai/ai_components)
target_include_directories(${EXAMPLE_LIB} PRIVATE ${APP_PATH}/../../tuya.ai/ai_components)
