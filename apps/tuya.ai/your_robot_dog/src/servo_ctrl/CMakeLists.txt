# src/servo_ctrl/CMakeLists.txt

set(SERVO_CTRL_PATH ${CMAKE_CURRENT_LIST_DIR})

set(SERVO_CTRL_SRCS)
list(APPEND SERVO_CTRL_SRCS
    ${SERVO_CTRL_PATH}/servo_ctrl.c
    ${SERVO_CTRL_PATH}/tuya_robot_actions.c
)

########################################
# Target Configure
########################################
target_sources(${EXAMPLE_LIB} PRIVATE
    ${SERVO_CTRL_SRCS}
)

target_include_directories(${EXAMPLE_LIB} PRIVATE
    ${SERVO_CTRL_PATH}
)
