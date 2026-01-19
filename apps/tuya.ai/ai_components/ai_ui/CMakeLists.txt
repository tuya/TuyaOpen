##
# @file CMakeLists.txt
# @brief 
#/
if (CONFIG_ENABLE_COMP_AI_DISPLAY STREQUAL "y")

set(COMP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

set(COMP_MODULE_SRCS)

file(GLOB_RECURSE COMP_MODULE_SRCS ${COMP_MODULE_PATH}/src/*.c) 

aux_source_directory(${COMP_MODULE_PATH}/font FONT_SRCS)
aux_source_directory(${COMP_MODULE_PATH}/font/emoji EMOJI_SRCS)

list(APPEND COMP_MODULE_SRCS
    ${FONT_SRCS}
    ${EMOJI_SRCS}
)

set(COMP_MODULE_INC 
    ${COMP_MODULE_PATH}/include
    ${COMP_MODULE_PATH}/font
)

########################################
# Target Configure
########################################
target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${COMP_MODULE_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${COMP_MODULE_INC}
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        "-DLV_LVGL_H_INCLUDE_SIMPLE"
    )

endif()