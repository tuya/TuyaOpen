##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/button)

########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
