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


set(LIB_SRCS ${disp_srcs} ${tdd_disp_spi_srcs} ${tdd_disp_i2c_srcs} ${tdd_disp_qspi_srcs} ${tdd_disp_rgb_srcs} ${tdd_disp_8080_srcs})

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC 
    ${MODULE_PATH}/tdl_display/include
    ${MODULE_PATH}/tdd_display/include
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


########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()