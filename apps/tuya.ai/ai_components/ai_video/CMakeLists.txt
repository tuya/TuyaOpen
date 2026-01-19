##
# @file CMakeLists.txt
# @brief 
#/
if (CONFIG_ENABLE_COMP_AI_VIDEO STREQUAL "y")

set(COMP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

set(COMP_MODULE_SRCS)

file(GLOB_RECURSE COMP_MODULE_SRCS ${COMP_MODULE_PATH}/src/*.c) 

set(COMP_MODULE_INC 
    ${COMP_MODULE_PATH}/include
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
    
endif()