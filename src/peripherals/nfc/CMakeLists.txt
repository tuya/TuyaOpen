##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
if (CONFIG_ENABLE_NFC STREQUAL "y")

set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# LIB_SRCS
set(LIB_SRCS "")
set(LIB_PUBLIC_INC "")

# PN532 NFC module
if (CONFIG_ENABLE_NFC_PN532 STREQUAL "y")
    set(PN532_PATH ${MODULE_PATH}/pn532)
    
    # Source files
    aux_source_directory(${PN532_PATH}/src/core PN532_CORE_SRCS)
    aux_source_directory(${PN532_PATH}/src/chips PN532_CHIPS_SRCS)
    aux_source_directory(${PN532_PATH}/src/drivers PN532_DRIVERS_SRCS)
    aux_source_directory(${PN532_PATH}/src/buses PN532_BUSES_SRCS)
    aux_source_directory(${PN532_PATH}/src/utils PN532_UTILS_SRCS)
    aux_source_directory(${PN532_PATH}/src/ndef PN532_NDEF_SRCS)
    
    list(APPEND LIB_SRCS 
        ${PN532_CORE_SRCS}
        ${PN532_CHIPS_SRCS}
        ${PN532_DRIVERS_SRCS}
        ${PN532_BUSES_SRCS}
        ${PN532_UTILS_SRCS}
        ${PN532_NDEF_SRCS}
    )
    
    # Include directories
    list(APPEND LIB_PUBLIC_INC 
        ${PN532_PATH}/inc
        ${PN532_PATH}/inc/nfc
        ${PN532_PATH}/inc/core
        ${PN532_PATH}/inc/chips
        ${PN532_PATH}/inc/drivers
        ${PN532_PATH}/inc/buses
        ${PN532_PATH}/inc/utils
        ${PN532_PATH}/inc/ndef
    )
endif()

########################################
# Target Configure
########################################
add_library(${MODULE_NAME})

# Add compile definitions for libnfc
if (CONFIG_ENABLE_NFC_PN532 STREQUAL "y")
    target_compile_definitions(${MODULE_NAME}
        PRIVATE
            HAVE_CONFIG_H=1                 # Use config.h header file
    )
endif()

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