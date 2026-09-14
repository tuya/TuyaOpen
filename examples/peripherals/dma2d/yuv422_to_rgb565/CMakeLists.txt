##
# @file CMakeLists.txt
# @brief DMA2D YUV422 to RGB565 hardware test example.
#/

set(CONFIG_ENABLE_DMA2D "y")
set(CONFIG_ENABLE_EXT_RAM "y")

set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(APP_NAME ${APP_PATH} NAME)
aux_source_directory(${APP_PATH}/src APP_SRCS)

add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/tal_driver/dma2d
        ${PROJECT_SOURCE_DIR}/platform/ESP32/esp-idf/components/heap/include
    )
