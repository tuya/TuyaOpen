##
# @file CMakeLists.txt
# @brief LVGL PC simulator component
#/

if (CONFIG_LVGL_PC_SIMULATOR STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(MODULE_NAME "lvgl_simulator")

set(LIB_SRCS ${MODULE_PATH}/src/lvgl_sim.c)
set(LIB_PUBLIC_INC ${MODULE_PATH}/include)

add_library(${MODULE_NAME})

target_sources(${MODULE_NAME} PRIVATE ${LIB_SRCS})
target_include_directories(${MODULE_NAME} PUBLIC ${LIB_PUBLIC_INC})

target_compile_definitions(${MODULE_NAME}
    PUBLIC
        LVGL_PC_SIMULATOR
        LVGL_SIM_ENTRY_FUNC=${CONFIG_SIM_ENTRY_FUNC}
)

########################################
# SDL2: required by LVGL built-in SDL driver (LV_USE_SDL=1)
########################################
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
target_include_directories(${MODULE_NAME} PUBLIC ${SDL2_INCLUDE_DIRS})
target_link_libraries(${MODULE_NAME} PUBLIC ${SDL2_LIBRARIES})
message(STATUS "LVGL Simulator: Linking SDL2 (${SDL2_LIBRARIES})")

########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)

endif()
