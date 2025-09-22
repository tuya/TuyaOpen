##
# @file ut/CMakeLists.txt
# @brief 
# @author Tuya
# @version 1.0.0
# @date 2023-04-02
#/

# Only support Linux
if(NOT ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "Linux")
    message(STATUS "[UT] Disable UT because [CMAKE_SYSTEM_PROCESSOR != Linux]")
    return()
endif()

option(UT_ENABLE "Enable UT" OFF)

if(NOT UT_ENABLE)
    message(STATUS "[UT] Disable UT.")
    return()
endif()

# if(NOT UT_ENABLE)
#     message(STATUS "[UT] Disable UT.
#     If you want to use UT,
#     ${Cyan}[ccmake ..]${ColourReset} and turn on ${Cyan}[UT_ENABLE]${ColourReset},
#     ${Cyan}[cmake ..]${ColourReset} again.
#     ")
#     return()
# endif()

message(STATUS "[UT] Enable UT.
        If you want to use UT,
        ${Cyan}[make pre_test]${ColourReset}.
")


message(STATUS "[UT] CMAKE_SYSTEM_PROCESSOR is [${CMAKE_SYSTEM_PROCESSOR}].")
add_custom_command(
    OUTPUT
    show_ut_prompt

    COMMAND
    ${CMAKE_COMMAND} -E echo "        ${Cyan}[make pre_test]${ColourReset} - Clone [googletest and mockcpp] from github."

    COMMAND
    ${CMAKE_COMMAND} -E echo "        ${Cyan}[make build_test]${ColourReset} - Generate all [test case]"

    COMMAND
    ${CMAKE_COMMAND} -E echo "        ${Cyan}[make run_test]${ColourReset} - Run all [test case] with detail mode."

    COMMAND
    ${CMAKE_COMMAND} -E echo "        ${Cyan}[make cover_test]${ColourReset} - Generate [coverage reports]."

    COMMENT
    "[UT] You can use other commands about [test]:"
    )


########################################
# Git Clone Googletest and Mockcpp
########################################
set(UT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(GTEST_DIR "${UT_ROOT}/googletest")
set(GTEST_REPO "https://github.com/google/googletest")
# set(GTEST_COMMIT "057b4e904fd754135dc19ff557c14036fd316425")
set(MOCKCPP_DIR "${UT_ROOT}/mockcpp")
set(MOCKCPP_REPO "https://github.com/sinojelly/mockcpp")
# set(MOCKCPP_COMMIT "7b7eb8b8a7837813d91ee1ae9354f352605fc270")

add_custom_target(pre_test
    COMMAND
    ${CMAKE_COMMAND} -E echo "[UT] Please run the command ${Cyan}[cmake ..]${ColourReset} once."

    DEPENDS
    mockcpp_repo
    gtest_repo

    COMMENT
    "[UT] If you want to build test case. ${Cyan}[make build_test]${ColourReset}"
    )

add_custom_command(
    OUTPUT
    gtest_repo

    COMMAND
    bash ${UT_ROOT}/git_clone.sh ${GTEST_REPO} ${GTEST_DIR} ${GTEST_COMMIT}

    COMMENT
    "[UT] Git clone [${GTEST_REPO}]"

    VERBATIM
    )

add_custom_command(
    OUTPUT
    mockcpp_repo

    COMMAND
    bash ${UT_ROOT}/git_clone.sh ${MOCKCPP_REPO} ${MOCKCPP_DIR} ${MOCKCPP_COMMIT}

    COMMAND
    bash ${UT_ROOT}/modify_mockcpp.sh ${MOCKCPP_DIR}/src/CMakeLists.txt

    COMMENT
    "[UT] Git clone [${MOCKCPP_REPO}]"

    VERBATIM
    )


if((NOT EXISTS ${GTEST_DIR}) OR (NOT EXISTS ${MOCKCPP_DIR}))
    message("
    Haven't clone the repo for [googletest] yet,
    Can execute command:
    ${Cyan}[make pre_test]${ColourReset}
    ")
    return()
endif()


########################################
# Build Googletest and Mockcpp
########################################
set(CMAKE_C_FLAGS "-g -Werror-implicit-function-declaration ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "-g --std=c++11 ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(${GTEST_DIR})
add_subdirectory(${MOCKCPP_DIR})
set(GTEST_LIB gtest gtest_main mockcpp)  # Replace 'gmock' with 'mockcpp'
target_include_directories(mockcpp
    PUBLIC
        ${MOCKCPP_DIR}/include
        ${MOCKCPP_DIR}/3rdparty
    )
target_compile_options(gtest
    INTERFACE
        --coverage
    )
target_link_options(gtest
    PUBLIC
        --coverage
    )


########################################
# Rebuild Components
########################################
foreach(C ${COMPONENT_LIBS})
    target_compile_options(${C}
    PRIVATE
        --coverage
    )
endforeach(C)


########################################
# Build UT Case
########################################
set(UT_EXES "")
list_uts(UT_LIST "${TOP_SOURCE_DIR}/src")
foreach(comp ${UT_LIST})
    # message(STATUS "comp: ${comp}")
    add_subdirectory("${TOP_SOURCE_DIR}/src/${comp}/ut" "bin/${comp}")
endforeach(comp)
add_custom_target(build_test
    DEPENDS
    build_test_case

    COMMENT
    "[UT] If you want to run test case. ${Cyan}[make run_test]${ColourReset}"
    )
add_custom_command(
    OUTPUT
    build_test_case

    DEPENDS
    ${UT_EXES}

    COMMENT
    "[UT] Build UT Targets. [${UT_EXES}]"
    )


########################################
# Build UT Case
########################################
add_custom_target(run_test
    DEPENDS
    run_test_case

    COMMENT
    "[UT] If you want to generate coverage reports. ${Cyan}[make cover_test]${ColourReset}"
    )
add_custom_command(
    OUTPUT
    run_test_case

    WORKING_DIRECTORY
    ${TOP_BINARY_DIR}

    COMMAND
    ctest -V

    COMMENT
    "[UT] Runing test case."
    )


########################################
# Coverage UT
########################################
set(LCOV_FILE "lcov.info")
set(LCOV_DIR "lcov")
add_custom_target(cover_test
    DEPENDS
    cover_test_case

    COMMENT
    "[UT] If you want to show coverage reports. ${Cyan}[make show_cover]${ColourReset}"
    )
add_custom_command(
    OUTPUT
    cover_test_case

    WORKING_DIRECTORY
    ${TOP_BINARY_DIR}

    COMMAND
    lcov -c -o ${LCOV_FILE} -d ./

    COMMAND
    genhtml ${LCOV_FILE} -o ./${LCOV_DIR}

    COMMAND
    mv ${LCOV_FILE} ${LCOV_DIR}

    COMMENT
    "[UT] Build Coverage for UT."
    )


########################################
# Show Coverage Report
########################################
add_custom_target(show_cover
    DEPENDS
    show_ut_prompt
    show_cover_case
    )
add_custom_command(
    OUTPUT
    show_cover_case

    WORKING_DIRECTORY
    ${TOP_BINARY_DIR}

    COMMAND
    bash ${UT_ROOT}/show_cover.sh "${LCOV_DIR}/index.html"

    COMMENT
    "[UT] Show Coverage for UT."
    )


