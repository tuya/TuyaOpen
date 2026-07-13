##
# @file CMakeLists.txt
# @brief
#/
if (CONFIG_ENABLE_AI_COMPONENTS STREQUAL "y")

set(COMP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(${COMP_MODULE_PATH}/assets)
add_subdirectory(${COMP_MODULE_PATH}/ai_agent)
add_subdirectory(${COMP_MODULE_PATH}/ai_audio)
add_subdirectory(${COMP_MODULE_PATH}/ai_main)
add_subdirectory(${COMP_MODULE_PATH}/ai_mcp)
add_subdirectory(${COMP_MODULE_PATH}/ai_mode)
add_subdirectory(${COMP_MODULE_PATH}/ai_picture)
add_subdirectory(${COMP_MODULE_PATH}/ai_skills)
add_subdirectory(${COMP_MODULE_PATH}/ai_ui)
add_subdirectory(${COMP_MODULE_PATH}/ai_video)
add_subdirectory(${COMP_MODULE_PATH}/utility)

########################################
# Layer Configure
########################################
# Re-propagate the libs/includes the subdirectories appended (via PARENT_SCOPE)
# up to the top-level scope.
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()
