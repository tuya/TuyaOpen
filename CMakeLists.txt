##
# @file CMakeLists.txt
# @brief 
# @author Tuya
# @version 1.0.0
# @date 2023-03-26
#/


cmake_minimum_required(VERSION 3.16 FATAL_ERROR)


# include cmake
set(TOP_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(TOP_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${TOP_BINARY_DIR}/lib)
SET(EXECUTABLE_OUTPUT_PATH ${TOP_BINARY_DIR}/bin)

if(${TOP_SOURCE_DIR} STREQUAL ${TOP_BINARY_DIR})
    message(FATAL_ERROR "
    [In-source builds] are not allowed.
    You should create separate directory for build files.
    Remove \"CMakeCache.txt\" and \"CMakeFiles/\".
    Use commands like:
    [mkdir build; cd build; cmake ..]
    or
    [cmake -S . -B build]
    ")
endif()

MESSAGE(STATUS "###################################")
IF (WIN32)
	MESSAGE(STATUS "Now is windows")
ELSEIF (APPLE)
	MESSAGE(STATUS "Now is Apple systens.")
ELSEIF (UNIX)
	MESSAGE(STATUS "Now is UNIX-like OS's.")
ENDIF ()

include(${TOP_SOURCE_DIR}/tools/cmake/colors.cmake)
include(${TOP_SOURCE_DIR}/tools/cmake/util.cmake)
if(NOT DEFINED TOS_FRAMEWORK)
    set(TOS_FRAMEWORK "base")
endif()
message(STATUS "[TOP] Using framework [${TOS_FRAMEWORK}].")
if(DEFINED TOS_PROJECT_PLATFORM)
    set(PLATFORM_NAME ${TOS_PROJECT_PLATFORM})
    set(PLATFORM_CHIP ${TOS_PROJECT_CHIP})
    set(PLATFORM_BOARD ${TOS_PROJECT_BOARD})
    set(PLATFORM_PATH "${TOP_SOURCE_DIR}/platform/${TOS_PROJECT_PLATFORM}")
endif()
if("${PLATFORM_CHIP}" STREQUAL "")
    set(PLATFORM_CHIP "NONE")
endif()
if("${PLATFORM_BOARD}" STREQUAL "")
    set(PLATFORM_BOARD "NONE")
endif()

set(DEFAULT_CONFIG_NAME "default.config")
set(PRE_CONFIG_DIR "${TOS_PROJECT_ROOT}/pre_config")


# use Kconfig
set(DOT_CONFIG_DIR "${TOP_BINARY_DIR}/cache")
set(DOT_CONFIG "using.config")
set(CMAKE_CONFIG "using.cmake")
set(HEADER_DIR "${TOP_BINARY_DIR}/include")
set(PLATFORM_NEED_HDIR "${TOP_BINARY_DIR}/include")
set(HEADER_FILE "tuya_kconfig.h")
set(HEADER_IN_PATH "${TOP_SOURCE_DIR}/tools/kconfiglib/config.h.in")
set(KCONFIG_CATALOG "${DOT_CONFIG_DIR}/CatalogKconfig")
set(APP_DEFAULT_CONFIG "${TOS_PROJECT_ROOT}/app_default.config")
add_subdirectory("${TOP_SOURCE_DIR}/tools/kconfiglib")


# toolchain cmake
include("${PLATFORM_PATH}/toolchain_file.cmake")
message(STATUS "[TOP] Using toolchain file [${CMAKE_TOOLCHAIN_FILE}].")


project(opensdk)

# add platform
include("${PLATFORM_PATH}/platform_config.cmake")


# add src
set(COMPONENT_LIBS "")
set(COMPONENT_PUBINC "")
list_components(COMPONENT_LIST "${TOP_SOURCE_DIR}/src")
foreach(comp ${COMPONENT_LIST})
    # message(STATUS "comp: ${comp}")
    add_subdirectory("${TOP_SOURCE_DIR}/src/${comp}")
endforeach(comp)
# add board
if(EXISTS "${TOP_SOURCE_DIR}/boards/${TOS_PROJECT_PLATFORM}/${TOS_PROJECT_BOARD}/CMakeLists.txt")
    add_subdirectory("${TOP_SOURCE_DIR}/boards/${TOS_PROJECT_PLATFORM}/${TOS_PROJECT_BOARD}")
endif()

# add arduino src
if(TOS_FRAMEWORK STREQUAL "arduino")
    include(${TOP_SOURCE_DIR}/arduino-tuyaopen/CMakeLists.txt)
endif()

# message(STATUS "[TOP] COMPONENT_LIBS: ${COMPONENT_LIBS}")
# copy_public_include("${COMPONENT_PUBINC}" "${HEADER_DIR}")
list(APPEND HEADER_DIR ${PLATFORM_PUBINC})
list(APPEND HEADER_DIR ${COMPONENT_PUBINC})

# message(STATUS "[TOP] HEADER_DIR: ${HEADER_DIR}")
deal_with_components("${COMPONENT_LIBS}" "${HEADER_DIR}")

# build components_all lib
set(COMPONENTS_ALL_LIB "tuyaos")
set(all_need)
foreach(c ${COMPONENT_LIBS})
    list(APPEND all_need $<TARGET_OBJECTS:${c}>)
endforeach(c)
add_library(${COMPONENTS_ALL_LIB} STATIC ${all_need})


# add example
set(EXAMPLE_NAME "${TOS_PROJECT_NAME}")
message(STATUS "[TOP] Using example [${EXAMPLE_NAME}].")
if(NOT DEFINED CONFIG_PROJECT_VERSION)
    set(CONFIG_PROJECT_VERSION "1.0.0")
endif()
set(EXAMPLE_LIB "tuyaapp")
list(APPEND PLATFORM_NEED_HDIR ${COMPONENT_PUBINC})
string(REPLACE ";" " " PLATFORM_NEED_HDIR "${PLATFORM_NEED_HDIR}")
set(PLATFORM_NEED_LIBS "${EXAMPLE_LIB} ${COMPONENTS_ALL_LIB}")
if(TOS_FRAMEWORK STREQUAL "arduino")
    if(PLATFORM_NAME STREQUAL "T2")
        set(PLATFORM_NEED_LIBS "${EXAMPLE_LIB} ${COMPONENTS_ALL_LIB} stdc++")
    endif()
endif()
message(STATUS "[TOP] PLATFORM_NEED_LIBS: ${PLATFORM_NEED_LIBS}")
include(${TOS_PROJECT_ROOT}/CMakeLists.txt)
target_include_directories(${EXAMPLE_LIB}
    PUBLIC
    ${HEADER_DIR}
    )
target_link_libraries(${EXAMPLE_LIB} ${COMPONENTS_ALL_LIB})

add_custom_target(example
    DEPENDS
    build_example_exe
    ${EXAMPLE_LIB}

    COMMENT
    "[TOP] example=[${EXAMPLE_NAME}]"
    )
add_custom_command(
    OUTPUT
    build_example_exe

    WORKING_DIRECTORY
    ${PLATFORM_PATH}

    COMMAND
    ./build_example.sh "${EXAMPLE_NAME}" "${CONFIG_PROJECT_VERSION}" "${PLATFORM_NEED_HDIR}"
    "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}" "${PLATFORM_NEED_LIBS}"
    "${EXECUTABLE_OUTPUT_PATH}" "build" "${PLATFORM_CHIP}" "${PLATFORM_BOARD}" | tee /dev/tty

    COMMENT
    "[EXAMPLE] Generate execute file [${EXAMPLE_NAME}]"
    )

# clean
add_custom_target(platform_clean
    WORKING_DIRECTORY
    ${PLATFORM_PATH}

    COMMAND
    ./build_example.sh "${EXAMPLE_NAME}" "${CONFIG_PROJECT_VERSION}" "${PLATFORM_NEED_HDIR}"
    "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}" "${PLATFORM_NEED_LIBS}"
    "${EXECUTABLE_OUTPUT_PATH}" "clean" "${PLATFORM_CHIP}" "${PLATFORM_BOARD}" | tee /dev/tty

    COMMENT
    "[TOP] Platform clean."
    )

add_custom_target(platform_menuconfig
    WORKING_DIRECTORY
    ${PLATFORM_PATH}

    COMMAND
    ./build_example.sh "${EXAMPLE_NAME}" "${CONFIG_PROJECT_VERSION}" "${PLATFORM_NEED_HDIR}"
    "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}" "${PLATFORM_NEED_LIBS}"
    "${EXECUTABLE_OUTPUT_PATH}" "menuconfig" "${PLATFORM_CHIP}"

    COMMENT
    "[TOP] Platform menuconfig."
    )

add_custom_target(clean_all
    DEPENDS
    platform_clean

    COMMAND ${CMAKE_BUILD_TOOL} clean
)

# add UT
enable_testing()
add_subdirectory("${TOP_SOURCE_DIR}/tools/ut")


# prompt
message(STATUS "[TOP] If you want to build example
        ${Cyan}[tos build]${ColourReset}.
")
message(STATUS "[TOP] If you want to clean project
        ${Cyan}[tos clean]${ColourReset}.
")
