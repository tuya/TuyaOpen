##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/audio_codecs)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/display)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tp)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/encoder)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/button)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/led)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/joystick)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/pmic)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/camera)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ir)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/leds_pixel)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/imu)

########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
