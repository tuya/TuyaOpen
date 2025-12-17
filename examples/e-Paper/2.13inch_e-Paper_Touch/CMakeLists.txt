##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP SRC - Set the source files by directory (use file(GLOB) to expand wildcards)
file(GLOB APP_SRC_EXAMPLES    "${APP_PATH}/examples/*.c")
file(GLOB APP_SRC_CONFIG      "${APP_PATH}/lib/Config/*.c")
file(GLOB APP_SRC_Driver        "${APP_PATH}/lib/Driver/*.c")
file(GLOB APP_SRC_EPAPER      "${APP_PATH}/lib/e-Paper/*.c")
file(GLOB APP_SRC_FONTS       "${APP_PATH}/lib/Fonts/*.c")
file(GLOB APP_SRC_GUI         "${APP_PATH}/lib/GUI/*.c")


set(APP_SRC
    ${APP_SRC_EXAMPLES}
    ${APP_SRC_CONFIG}
    ${APP_SRC_Driver}
    ${APP_SRC_EPAPER}
    ${APP_SRC_FONTS}
    ${APP_SRC_GUI}
)

# Exclude common build/cache directories from being accidentally globbed
list(FILTER APP_SRC EXCLUDE REGEX "/CMakeFiles/")
list(FILTER APP_SRC EXCLUDE REGEX "\\.build/")
list(FILTER APP_SRC EXCLUDE REGEX "\\.git/")

# Prepare absolute paths for target_sources
set(APP_SRC_ABS)
foreach(_f IN LISTS APP_SRC)
    # If _f is already absolute, keep it; otherwise prefix with APP_PATH
    if(IS_ABSOLUTE "${_f}")
        list(APPEND APP_SRC_ABS "${_f}")
    else()
        list(APPEND APP_SRC_ABS "${_f}")
    endif()
endforeach()

# APP INC - Set the header file directory
set(APP_INC 
    ${APP_PATH}/examples
    ${APP_PATH}/lib/Config
    ${APP_PATH}/lib/Driver
    ${APP_PATH}/lib/e-Paper
    ${APP_PATH}/lib/Fonts
    ${APP_PATH}/lib/GUI
)

# APP_OPTIONS
# set(APP_OPTIONS "-DDEBUG")  # Enable Debug output
set(APP_OPTIONS "")
# Disable treating warnings as errors
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    list(APPEND APP_OPTIONS "-Wno-error")
elseif(MSVC)
    list(APPEND APP_OPTIONS "/WX-")
endif()  
########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})
message(STATUS "EXAMPLE_LIB:${APP_PATH}")

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRC_ABS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        ${APP_OPTIONS}
    )