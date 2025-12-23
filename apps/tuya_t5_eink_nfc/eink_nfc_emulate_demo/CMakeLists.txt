##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRCS
aux_source_directory(${APP_PATH}/src APP_SRCS)

########################################
# libnfc library sources
########################################
set(LIBNFC_PATH ${APP_PATH}/libnfc)

# libnfc core sources
aux_source_directory(${LIBNFC_PATH}/src/core LIBNFC_CORE_SRCS)

# libnfc chips sources
aux_source_directory(${LIBNFC_PATH}/src/chips LIBNFC_CHIPS_SRCS)

# libnfc drivers sources
aux_source_directory(${LIBNFC_PATH}/src/drivers LIBNFC_DRIVERS_SRCS)

# libnfc buses sources
aux_source_directory(${LIBNFC_PATH}/src/buses LIBNFC_BUSES_SRCS)

# libnfc utils sources
aux_source_directory(${LIBNFC_PATH}/src/utils LIBNFC_UTILS_SRCS)

# libnfc examples sources
aux_source_directory(${LIBNFC_PATH}/examples LIBNFC_EXAMPLES_SRCS)

# Combine all libnfc sources
set(LIBNFC_SRCS
    ${LIBNFC_CORE_SRCS}
    ${LIBNFC_CHIPS_SRCS}
    ${LIBNFC_DRIVERS_SRCS}
    ${LIBNFC_BUSES_SRCS}
    ${LIBNFC_UTILS_SRCS}
    ${LIBNFC_EXAMPLES_SRCS}
)

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
        ${LIBNFC_SRCS}
    )

# Add compile definitions for libnfc
target_compile_definitions(${EXAMPLE_LIB}
    PRIVATE
        HAVE_CONFIG_H=1                 # Use config.h header file
)

# libnfc include directories
target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${LIBNFC_PATH}/include
        ${LIBNFC_PATH}/include/nfc
        ${LIBNFC_PATH}/include/core
        ${LIBNFC_PATH}/include/chips
        ${LIBNFC_PATH}/include/drivers
        ${LIBNFC_PATH}/include/buses
        ${LIBNFC_PATH}/include/utils
    )

