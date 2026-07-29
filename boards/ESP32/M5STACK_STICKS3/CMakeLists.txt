##
# @file CMakeLists.txt
# @brief M5Stack StickS3 board build configuration
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
aux_source_directory(${MODULE_PATH} LIB_SRCS)

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH})

# BMI270 IMU headers (required by board_bmi270_api.h)
if(CONFIG_ENABLE_IMU_BMI270 STREQUAL "y")
    list(APPEND LIB_PUBLIC_INC ${CMAKE_SOURCE_DIR}/src/peripherals/imu/bmi270)
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
