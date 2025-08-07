##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRC
aux_source_directory(${APP_PATH}/src EXAMPLE_SRC)

aux_source_directory(${APP_PATH}/src/ducky DUCKY_SRCS)
aux_source_directory(${APP_PATH}/src/icons ICONS_SRCS)
aux_source_directory(${APP_PATH}/src/logo LOGO_SRCS)
aux_source_directory(${APP_PATH}/src/ui UI_SRCS)


list(APPEND APP_SRC
    ${EXAMPLE_SRC}
    ${DUCKY_SRCS}
    ${ICONS_SRCS}
    ${LOGO_SRCS}
    ${UI_SRCS}
)

# APP_INC
set(APP_INC ${APP_PATH}/src ${APP_PATH}/src/ui)

# APP_OPTIONS
set(APP_OPTIONS "-W")
list(APPEND APP_OPTIONS "-Wall")

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})
message(STATUS "EXAMPLE_LIB:${APP_PATH}")

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRC}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        ${APP_OPTIONS}
    )
