##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRC
aux_source_directory(${APP_PATH}/src APP_SRC)

# APP_INC
set(APP_INC ${APP_PATH}/include)

# APP_OPTIONS
set(APP_OPTIONS "-W")
list(APPEND APP_OPTIONS "-Wall")
list(APPEND APP_OPTIONS "-DSTATIC_IN_RELEASE=static")
list(APPEND APP_OPTIONS -DEXAMPLE_VER="${EXAMPLE_VER}")
add_definitions(-DMAJOR_VERSION=4 -DMINOR_VERSION=1 -DMICRO_VERSION=1 -DVERSION=\"4.1.1\")

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
