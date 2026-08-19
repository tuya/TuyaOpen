##
# @file CMakeLists.txt
# @brief Xteink X4 board (ESP32-C3)
#/

set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

set(LIB_SRCS "")
aux_source_directory(${MODULE_PATH} LIB_SRCS)

set(LIB_PUBLIC_INC
    ${MODULE_PATH}
)

set(IDF_COMPONENTS_PATH ${CMAKE_SOURCE_DIR}/platform/ESP32/esp-idf/components)

set(LIB_PRIVATE_INC
    ${IDF_COMPONENTS_PATH}/esp_common/include
    ${IDF_COMPONENTS_PATH}/esp_driver_gpio/include
    ${IDF_COMPONENTS_PATH}/esp_driver_spi/include
    ${IDF_COMPONENTS_PATH}/esp_driver_sdspi/include
    ${IDF_COMPONENTS_PATH}/esp_hw_support/include
    ${IDF_COMPONENTS_PATH}/esp_hw_support/dma/include
    ${IDF_COMPONENTS_PATH}/esp_partition/include
    ${IDF_COMPONENTS_PATH}/esp_rom/include
    ${IDF_COMPONENTS_PATH}/esp_system/include
    ${IDF_COMPONENTS_PATH}/fatfs/src
    ${IDF_COMPONENTS_PATH}/fatfs/vfs
    ${IDF_COMPONENTS_PATH}/freertos/FreeRTOS-Kernel/include
    ${IDF_COMPONENTS_PATH}/freertos/FreeRTOS-Kernel/portable/riscv/include
    ${IDF_COMPONENTS_PATH}/freertos/FreeRTOS-Kernel/portable/riscv/include/freertos
    ${IDF_COMPONENTS_PATH}/freertos/config/include
    ${IDF_COMPONENTS_PATH}/freertos/config/include/freertos
    ${IDF_COMPONENTS_PATH}/freertos/config/riscv/include
    ${IDF_COMPONENTS_PATH}/hal/include
    ${IDF_COMPONENTS_PATH}/heap/include
    ${IDF_COMPONENTS_PATH}/log/include
    ${IDF_COMPONENTS_PATH}/newlib/platform_include
    ${IDF_COMPONENTS_PATH}/riscv/include
    ${IDF_COMPONENTS_PATH}/sdmmc/include
    ${IDF_COMPONENTS_PATH}/soc/esp32c3/include
    ${IDF_COMPONENTS_PATH}/soc/esp32c3/register
    ${IDF_COMPONENTS_PATH}/wear_levelling/include
)

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

list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
set(PLATFORM_IDF_REQUIRES "fatfs esp_driver_gpio esp_driver_spi esp_driver_sdspi sdmmc vfs" PARENT_SCOPE)
set(PLATFORM_IDF_SDKCONFIG "CONFIG_VFS_SUPPORT_IO=y CONFIG_VFS_SUPPORT_DIR=y" PARENT_SCOPE)
