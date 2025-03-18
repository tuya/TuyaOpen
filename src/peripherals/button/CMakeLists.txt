##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
# aux_source_directory(${MODULE_PATH}/src LIB_SRCS)

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC 
        ${MODULE_PATH}/tdd_button_driver/include
        ${MODULE_PATH}/tdl_button_manage/include
    )


########################################
# Target Configure
########################################
add_library(${MODULE_NAME})

target_sources(${MODULE_NAME}
    PRIVATE
        ${MODULE_PATH}/tdd_button_driver/src/tdd_button_gpio.c
        ${MODULE_PATH}/tdl_button_manage/src/tdl_button_manage.c
    )

target_include_directories(${MODULE_NAME}
    PRIVATE
        ${LIB_PRIVATE_INC}

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
