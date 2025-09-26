##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
if (CONFIG_ENABLE_IMU STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
set(LIB_SRCS "")
set(LIB_PUBLIC_INC "")

if (CONFIG_ENABLE_IMU_BMI270 STREQUAL "y")
    # Use glob pattern to include all .c files in bmi270 directory
    file(GLOB BMI270_SOURCES "${MODULE_PATH}/bmi270/*.c")
    list(APPEND LIB_SRCS ${BMI270_SOURCES})
    list(APPEND LIB_PUBLIC_INC ${MODULE_PATH}/bmi270)
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
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_LIBS ${COMPONENT_LIBS} PARENT_SCOPE)
set(COMPONENT_PUBINC ${COMPONENT_PUBINC} PARENT_SCOPE)
endif()