##
# @file CMakeLists.txt
# @brief
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

if (CONFIG_LVGL_PC_SIMULATOR STREQUAL "y")
########################################
# Simulator mode: display only
########################################
add_library(${EXAMPLE_LIB})

target_compile_definitions(${EXAMPLE_LIB}
    PRIVATE
        LVGL_PC_SIMULATOR
        ENABLE_CHAT_DISPLAY2=1
)

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_PATH}/include
        ${APP_PATH}/../ai_components/assets/include
)

########################################
# Generate language config header
########################################
add_subdirectory(${APP_PATH}/src/display2)

add_subdirectory(${APP_PATH}/../ai_components)

else()
########################################
# Embedded mode: full build (original)
########################################

# APP_SRCS
aux_source_directory(${APP_PATH}/src APP_SRCS)

set(APP_INC ${APP_PATH}/include)

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

########################################
# Add subdirectory
########################################

if (CONFIG_ENABLE_BATTERY STREQUAL "y")
    add_subdirectory(${APP_PATH}/src/battery)
endif()

add_subdirectory(${APP_PATH}/../ai_components)

endif()
