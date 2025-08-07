##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
file(GLOB_RECURSE tdl_transport_srcs "${MODULE_PATH}/tdl_transport/src/*.c")
file(GLOB_RECURSE tdd_transport_srcs "${MODULE_PATH}/tdd_transport/src/*.c")

set(LIB_SRCS ${tdl_transport_srcs} ${tdd_transport_srcs})

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC 
    ${MODULE_PATH}/tdl_transport/include
    ${MODULE_PATH}/tdd_transport/include
    )

########################################
# Target Configure
########################################
add_library(${MODULE_NAME})

target_sources(${MODULE_NAME}
    PRIVATE
        ${LIB_SRCS}
    )

target_include_directories(${MODULE_NAME}
    PUBLIC
        ${LIB_PUBLIC_INC}
    )


########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
