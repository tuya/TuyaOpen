##
# @file CMakeLists.txt
# @brief 
#/
set(COMP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(${COMP_MODULE_PATH}/assets)
add_subdirectory(${COMP_MODULE_PATH}/ai_agent)
add_subdirectory(${COMP_MODULE_PATH}/ai_audio)
add_subdirectory(${COMP_MODULE_PATH}/ai_main)
add_subdirectory(${COMP_MODULE_PATH}/ai_mcp) 
add_subdirectory(${COMP_MODULE_PATH}/ai_mode)
add_subdirectory(${COMP_MODULE_PATH}/ai_skills)
add_subdirectory(${COMP_MODULE_PATH}/ai_ui)
add_subdirectory(${COMP_MODULE_PATH}/ai_video)
add_subdirectory(${COMP_MODULE_PATH}/utility) 