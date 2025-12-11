##
# @file CMakeLists.txt
# @brief 
# @author Tuya
# @version 1.0.0
# @date 2023-03-26
#/


cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

# Enable compile commands export for clangd and other tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

MESSAGE(STATUS "#################### TuyaOpen CMakeLists.txt Start ####################")

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

IF (WIN32)
	MESSAGE(STATUS "Now is windows")
ELSEIF (APPLE)
	MESSAGE(STATUS "Now is Apple systens.")
ELSEIF (UNIX)
	MESSAGE(STATUS "Now is UNIX-like OS's.")
ENDIF ()

execute_process(
    COMMAND python ${TOP_SOURCE_DIR}/tools/cmake/get_system_processor.py
    OUTPUT_VARIABLE OPEN_SYSTEM_PROCESSOR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT OPEN_SYSTEM_PROCESSOR)
    set(OPEN_SYSTEM_PROCESSOR "Unknown")
endif()

message(STATUS "[TOP] System processor: ${OPEN_SYSTEM_PROCESSOR}")

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


########################################
# use Kconfig
########################################
get_latest_git_tag("${TOP_SOURCE_DIR}" OPEN_VERSION)
get_latest_git_commit("${TOP_SOURCE_DIR}" OPEN_COMMIT)
get_latest_git_commit("${PLATFORM_PATH}" PLATFORM_COMMIT)
set(HEADER_PARAM_LIST "
OPEN_VERSION:::\"${OPEN_VERSION}\"***
OPEN_COMMIT:::\"${OPEN_COMMIT}\"***
PLATFORM_NAME:::\"${PLATFORM_NAME}\"***
PLATFORM_COMMIT:::\"${PLATFORM_COMMIT}\"***
PLATFORM_CHIP:::\"${PLATFORM_CHIP}\"***
PLATFORM_BOARD:::\"${PLATFORM_BOARD}\"***
")
set(KCONFIG_TOOLS ${TOP_SOURCE_DIR}/tools/kconfiglib)
set(DOT_CONFIG_DIR "${TOP_BINARY_DIR}/cache")
set(DOT_CONFIG "using.config")
set(CMAKE_CONFIG "using.cmake")
set(HEADER_DIR "${TOP_BINARY_DIR}/include")
set(HEADER_FILE "tuya_kconfig.h")
set(HEADER_IN_PATH "${TOP_SOURCE_DIR}/tools/kconfiglib/config.h.in")
set(KCONFIG_CATALOG "${DOT_CONFIG_DIR}/CatalogKconfig")
set(APP_DEFAULT_CONFIG "${TOS_PROJECT_ROOT}/app_default.config")
add_subdirectory("${TOP_SOURCE_DIR}/tools/kconfiglib")
include(${DOT_CONFIG_DIR}/${CMAKE_CONFIG})


########################################
# toolchain cmake
########################################
set(PLATFORM_NEED_HDIR "${TOP_BINARY_DIR}/include")
include("${PLATFORM_PATH}/toolchain_file.cmake")
message(STATUS "[TOP] Using toolchain file [${CMAKE_TOOLCHAIN_FILE}].")


project(opensdk)

# add platform
include("${PLATFORM_PATH}/platform_config.cmake")


########################################
# add src
########################################
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


########################################
# add example
########################################
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

########################################
# Generate build parameters file
########################################
# KEY:::"VALUE"***
set(BUILD_PARAM_LIST "
OPEN_ROOT:::\"${TOP_SOURCE_DIR}\"***
OPEN_HEADER_DIR:::\"${PLATFORM_NEED_HDIR}\"***
OPEN_LIBS_DIR:::\"${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}\"***
PLATFORM_NEED_LIBS:::\"${PLATFORM_NEED_LIBS}\"***
BIN_OUTPUT_DIR:::\"${EXECUTABLE_OUTPUT_PATH}\"***
PLATFORM_CHIP:::\"${PLATFORM_CHIP}\"***
PLATFORM_BOARD:::\"${PLATFORM_BOARD}\"***
")
set(BUILD_PARAM_DIR "${TOP_BINARY_DIR}/build")
set(BUILD_PARAM_FILE_NAME "build_param")
include("${TOP_SOURCE_DIR}/tools/kconfiglib/gen_build_param.cmake")

########################################
# build commands
########################################
set(BUILD_COMMAND "./build_example.sh")
if(EXISTS "${TOP_SOURCE_DIR}/platform/${TOS_PROJECT_PLATFORM}/build_example.py")
    set(BUILD_COMMAND python -u ./build_example.py)
endif()

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
    ${BUILD_COMMAND} "${BUILD_PARAM_DIR}" "build"

    COMMENT
    "[EXAMPLE] Generate execute file [${EXAMPLE_NAME}]:"

    USES_TERMINAL
    )

# clean
add_custom_target(platform_clean
    WORKING_DIRECTORY
    ${PLATFORM_PATH}

    COMMAND
    ${BUILD_COMMAND} "${BUILD_PARAM_DIR}" "clean"

    COMMENT
    "[TOP] Platform [${PLATFORM_NAME}] clean:"

    USES_TERMINAL
    )

add_custom_target(clean_all
    DEPENDS
    platform_clean

    COMMAND ${CMAKE_BUILD_TOOL} clean
)

########################################
# prompt
########################################
MESSAGE(STATUS "${Cyan}#################### TuyaOpen CMakeLists.txt End ####################${ColourReset}")
