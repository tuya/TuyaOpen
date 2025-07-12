cmake_minimum_required(VERSION 3.14)

if(NOT DEFINED BUILD_PARAM_PATH)
    message(FATAL_ERROR "BUILD_PARAM_PATH is not defined, using default value")
endif()

if(EXISTS ${BUILD_PARAM_PATH}/build_param.cmake)
    include(${BUILD_PARAM_PATH}/build_param.cmake)
else()
    message(FATAL_ERROR "${BUILD_PARAM_PATH}/build_param.cmake not found")
endif()

set(COMPILE_PREX "")
message(STATUS "Using cross compile prefix: ${COMPILE_PREX}")
set(CMAKE_C_COMPILER ${COMPILE_PREX}gcc)
set(CMAKE_CXX_COMPILER ${COMPILE_PREX}g++)
set(CMAKE_ASM_COMPILER ${COMPILE_PREX}gcc)
set(CMAKE_AR ${COMPILE_PREX}ar)
set(CMAKE_RANLIB ${COMPILE_PREX}ranlib)
set(CMAKE_STRIP ${COMPILE_PREX}strip)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_C_FLAGS " -g")

project(TuyaOpen)


# Set a default build type if none was specified, and print it
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build: Debug, Release, RelWithDebInfo, MinSizeRel" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Set global compiler flags
add_compile_options(-fdiagnostics-color=always)

# Add an option to enable AddressSanitizer (ASan)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

if(ENABLE_ASAN)
    if(CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE MATCHES "Debug")
        message(WARNING "ASan is best used with Debug build type. Current build type: ${CMAKE_BUILD_TYPE}")
    endif()
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address)
endif()

if(CONFIG_ENABLE_WIFI STREQUAL "y")
    set(CONFIG_ENABLE_WIFI ON)
else()
    set(CONFIG_ENABLE_WIFI OFF)
endif()

if(CONFIG_ENABLE_BLUETOOTH STREQUAL "y")
    set(CONFIG_ENABLE_BLUETOOTH ON)
else()
    set(CONFIG_ENABLE_BLUETOOTH OFF)
endif()

if(CONFIG_ENABLE_WIRED STREQUAL "y")
    set(CONFIG_ENABLE_WIRED ON)
else()
    set(CONFIG_ENABLE_WIRED OFF)
endif()

set(TUYA_ADAPTER_PATH "tuyaos/tuyaos_adapter")

file(GLOB_RECURSE UTILITIES_SRC "${TUYA_ADAPTER_PATH}/include/utilities/*.c")
file(GLOB ADAPTER_SRC "${TUYA_ADAPTER_PATH}/src/*.c")
set(INIT_SRC "${TUYA_ADAPTER_PATH}/include/init/src/tkl_init_common.c")

if(CONFIG_ENABLE_WIFI)
    file(GLOB_RECURSE WIFI_SRC "${TUYA_ADAPTER_PATH}/src/tkl_wifi/*.c")
    list(APPEND INIT_SRC "${TUYA_ADAPTER_PATH}/include/init/src/tkl_init_wifi.c")
endif()

if(CONFIG_ENABLE_BLUETOOTH)
    file(GLOB_RECURSE BT_SRC "${TUYA_ADAPTER_PATH}/src/tkl_bt/*.c")
endif()

if(CONFIG_ENABLE_WIRED)
    list(APPEND INIT_SRC "${TUYA_ADAPTER_PATH}/include/init/src/tkl_init_wired.c")
endif()

set(SOURCES
    ${UTILITIES_SRC}
    ${ADAPTER_SRC}
    ${INIT_SRC}
    ${WIFI_SRC}
    ${BT_SRC}
)

file(GLOB_RECURSE ADAPTER_INCLUDE_SUB_DIRS LIST_DIRECTORIES true "${TUYA_ADAPTER_PATH}/include/*")
list(FILTER ADAPTER_INCLUDE_SUB_DIRS INCLUDE REGEX ".*/$")
foreach(dir ${ADAPTER_INCLUDE_SUB_DIRS})
    if(IS_DIRECTORY ${dir})
        include_directories(${dir})
    endif()
endforeach()

include_directories(
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/adc
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/bluetooth
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/flash
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/gpio
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/hci
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/i2c
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/init/include
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/network
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/pinmux
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/pwm
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/rtc
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/security
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/system
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/timer
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/uart
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/utilities/include
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/watchdog
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/wifi
    ${CMAKE_SOURCE_DIR}/${TUYA_ADAPTER_PATH}/include/wired
)

if(OPEN_HEADER_DIR)
    string(REPLACE ";" " " HEADER_DIR_LIST "${OPEN_HEADER_DIR}")
    separate_arguments(HEADER_DIR_LIST)
    foreach(header_dir ${HEADER_DIR_LIST})
        include_directories(${header_dir})
    endforeach()
endif()

add_executable(${CONFIG_PROJECT_NAME} ${SOURCES})

# Set target-specific compile options based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${CONFIG_PROJECT_NAME} PRIVATE -g -O0)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${CONFIG_PROJECT_NAME} PRIVATE -O3 -DNDEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(${CONFIG_PROJECT_NAME} PRIVATE -O2 -g -DNDEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    target_compile_options(${CONFIG_PROJECT_NAME} PRIVATE -Os -DNDEBUG)
endif()

if(ENABLE_ASAN)
    target_compile_options(${CONFIG_PROJECT_NAME} PRIVATE -fsanitize=address)
    target_link_libraries(${CONFIG_PROJECT_NAME} PRIVATE -fsanitize=address)
endif()

target_link_libraries(${CONFIG_PROJECT_NAME}
    -L${OPEN_LIBS_DIR}
    pthread
    m
)

if(NOT DEFINED CONFIG_COMPILE_PREX OR "${CONFIG_COMPILE_PREX}" STREQUAL "")
    target_link_libraries(${CONFIG_PROJECT_NAME}
        systemd
    )
endif()

if(PLATFORM_NEED_LIBS)
    string(REPLACE " " ";" LIBS_LIST "${PLATFORM_NEED_LIBS}")
    target_link_libraries(${CONFIG_PROJECT_NAME}
        ${LIBS_LIST}
    )
endif()
