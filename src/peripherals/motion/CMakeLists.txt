##
# @file CMakeLists.txt
# @brief Motion sensor component.
#/

if (CONFIG_ENABLE_MOTION STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

file(GLOB_RECURSE LIB_SRCS
    "${MODULE_PATH}/tdl_motion/src/*.c")

list(APPEND LIB_SRCS
    "${MODULE_PATH}/tdd_motion/src/tdd_motion_i2c.c")

set(LIB_PUBLIC_INC
    ${MODULE_PATH}/tdl_motion/include
    ${MODULE_PATH}/tdd_motion/include
)

if (CONFIG_ENABLE_MOTION_SC7A20 STREQUAL "y")
    list(APPEND LIB_SRCS
        "${MODULE_PATH}/tdd_motion/src/tdd_motion_sc7a20.c"
        "${MODULE_PATH}/../imu/sc7a20/sc7a20.c"
    )
    list(APPEND LIB_PRIVATE_INC ${MODULE_PATH}/../imu/sc7a20)
endif()

if (CONFIG_ENABLE_MOTION_BMI270 STREQUAL "y")
    file(GLOB BMI270_SOURCES "${MODULE_PATH}/../imu/bmi270/*.c")
    list(APPEND LIB_SRCS ${BMI270_SOURCES})
    list(APPEND LIB_SRCS "${MODULE_PATH}/tdd_motion/src/tdd_motion_bmi270.c")
    list(APPEND LIB_PRIVATE_INC ${MODULE_PATH}/../imu/bmi270)
endif()

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

endif()
