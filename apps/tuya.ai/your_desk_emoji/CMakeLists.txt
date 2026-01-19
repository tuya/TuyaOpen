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

aux_source_directory(${APP_PATH}/src/ui/src UI_SRCS)
aux_source_directory(${APP_PATH}/src/ui/image/emmo IMG_EMO_SRCS)
aux_source_directory(${APP_PATH}/src/ui/image/weather IMG_WEATHER_SRCS)

list(APPEND APP_SRCS ${UI_SRCS} ${IMG_EMO_SRCS} ${IMG_WEATHER_SRCS})

set(APP_INC 
    ${APP_PATH}/include
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
add_subdirectory(${APP_PATH}/../ai_components)
