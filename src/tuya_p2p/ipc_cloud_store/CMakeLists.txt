##
# @file CMakeLists.txt
# @brief IPC cloud store skeleton
#/

if (CONFIG_ENABLE_IPC_CLOUD_STORE STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)
file(GLOB LIB_SRCS "${MODULE_PATH}/src/*.c")
set(LIB_PUBLIC_INC ${MODULE_PATH}/include)

add_library(${MODULE_NAME})
target_sources(${MODULE_NAME} PRIVATE ${LIB_SRCS})
target_include_directories(${MODULE_NAME} PUBLIC ${LIB_PUBLIC_INC})
target_link_libraries(${MODULE_NAME} PUBLIC tal_system common)

list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()
