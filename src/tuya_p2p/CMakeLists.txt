##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
if (CONFIG_ENABLE_TUYA_P2P STREQUAL "y")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/base_ice)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib_rtp)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/pjproject)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/svc_ipc_core)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/svc_streaming_p2p)

########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()