##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
if (CONFIG_ENABLE_DISPLAY_DRIVER STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
if (CONFIG_ENABLE_DISPLAY_LCD_RGB STREQUAL "y")

    list(APPEND LIB_SRCS ${MODULE_PATH}/tft_rgb/disp_sw_spi_driver.c)

if (CONFIG_ENABLE_LCD_RGB_ILI9488 STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tft_rgb/disp_rgb_ili9488.c)
else()
    message(FATAL_ERROR "Please select a display chip")
endif()

endif()


if (CONFIG_ENABLE_DISPLAY_LCD_SPI STREQUAL "y")

if (CONFIG_ENABLE_LCD_SPI_ST7789 STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tft_spi/disp_spi_st7789.c)
elseif(CONFIG_ENABLE_LCD_SPI_GC9A01 STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tft_spi/disp_spi_gc9a01.c)
elseif(CONFIG_ENABLE_LCD_SPI_ILI9341 STREQUAL "y")
    list(APPEND LIB_SRCS ${MODULE_PATH}/tft_spi/disp_spi_ili9341.c)
else()
    message(FATAL_ERROR "Please select a display chip")
endif()

endif()

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH}/include)

########################################
# Target Configure
########################################
add_library(${MODULE_NAME})

target_sources(${MODULE_NAME}
    PRIVATE
        ${LIB_SRCS}
    )

target_include_directories(${MODULE_NAME}
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

endif()