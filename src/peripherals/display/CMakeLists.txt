##
# @file CMakeLists.txt
# @brief 
#/

if (CONFIG_ENABLE_DISPLAY STREQUAL "y")
# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
file(GLOB_RECURSE disp_srcs "${MODULE_PATH}/tdl_display/src/*.c")

if (CONFIG_ENABLE_SPI STREQUAL "y")   
    file(GLOB_RECURSE tdd_disp_spi_srcs "${MODULE_PATH}/tdd_display/src/spi/*.c")
endif()

if (CONFIG_ENABLE_I2C STREQUAL "y")  
    file(GLOB_RECURSE tdd_disp_i2c_srcs "${MODULE_PATH}/tdd_display/src/i2c/*.c")
endif()

if (CONFIG_ENABLE_QSPI STREQUAL "y")
    file(GLOB_RECURSE tdd_disp_qspi_srcs "${MODULE_PATH}/tdd_display/src/qspi/*.c")
endif()

if (CONFIG_ENABLE_RGB STREQUAL "y")   
    file(GLOB_RECURSE tdd_disp_rgb_srcs "${MODULE_PATH}/tdd_display/src/rgb/*.c")
endif()

if (CONFIG_ENABLE_MCU8080 STREQUAL "y")
    file(GLOB_RECURSE tdd_disp_8080_srcs "${MODULE_PATH}/tdd_display/src/mcu8080/*.c")
endif()

if (CONFIG_ENABLE_MIPI_DSI STREQUAL "y")
    file(GLOB_RECURSE tdd_disp_mipi_srcs "${MODULE_PATH}/tdd_display/src/mipi/*.c")
endif()


set(LIB_SRCS ${disp_srcs} ${tdd_disp_spi_srcs} ${tdd_disp_i2c_srcs} ${tdd_disp_qspi_srcs} ${tdd_disp_rgb_srcs} ${tdd_disp_8080_srcs} ${tdd_disp_mipi_srcs})

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC
    ${MODULE_PATH}/tdl_display/include
    ${MODULE_PATH}/tdd_display/include
    )

# The MIPI-DSI TDD reaches the platform through the TKL MIPI-DSI contract, and
# TDL's power-pin handling uses the GPIO contract. Other bus families get these
# from their own platform headers, which is why this was not needed before.
list(APPEND LIB_PRIVATE_INC
    ${TOP_SOURCE_DIR}/tools/porting/adapter/gpio
    )

# Unlike the other display contracts, tkl_mipi_dsi.h ships with the platform
# that implements it rather than with the shared adapters, so the TDD needs the
# platform's own include directory. This is the one place a module has to know
# where a platform keeps its headers; the GPIO contract above does not, because
# it is shared. The audio module conditions on the platform the same way.
if(CONFIG_PLATFORM_CHOICE STREQUAL "JIELI")
    list(APPEND LIB_PRIVATE_INC
        ${PLATFORM_PATH}/tuyaos/tuyaos_adapter/include/mipi_dsi
        )
endif()

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


########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()