##
# @file CMakeLists.txt
# @brief 
#/

# TAL_CELLULAR_BASE_CFG_T reaches for TUYA_CELLULAR_IF_E / TUYA_CELLULAR_PROTOCOL_E,
# which only the platforms carrying a modem declare, so the module cannot be
# parsed on a platform without one. Gate it the way netconn_cellular.c is gated
# in src/tuya_cloud_service/CMakeLists.txt - its only consumer.
if (CONFIG_ENABLE_CELLULAR STREQUAL "y")

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
aux_source_directory(${MODULE_PATH}/src LIB_SRCS)

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC ${MODULE_PATH}/include)

########################################
# Target Configure
########################################
add_library(${MODULE_NAME})

target_sources(${MODULE_NAME}
    PRIVATE
        ${LIB_SRCS}
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

endif()