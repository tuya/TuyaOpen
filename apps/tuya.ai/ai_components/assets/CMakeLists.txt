##
# @file CMakeLists.txt
# @brief 
#/
set(COMP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

set(COMP_MODULE_SRCS)

file(GLOB_RECURSE COMP_MODULE_SRCS ${COMP_MODULE_PATH}/src/*.c) 

set(COMP_MODULE_INC 
    ${COMP_MODULE_PATH}/include
)

########################################
# Generate language config header
########################################
set(LANG_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/scripts/gen_lang.py")
# Generate into build tree (source tree may be read-only in some environments).
set(LANG_HEADER "${CMAKE_BINARY_DIR}/include/lang_config.h")

# Select language JSON based on Kconfig
if(CONFIG_ENABLE_AI_LANGUAGE_CHINESE STREQUAL "y")
    set(LANG_JSON "${CMAKE_CURRENT_LIST_DIR}/language/zh-CN/language.json")
    message(STATUS "Selected language: Chinese (zh-CN)")
elseif(CONFIG_ENABLE_AI_LANGUAGE_ENGLISH STREQUAL "y")
    set(LANG_JSON "${CMAKE_CURRENT_LIST_DIR}/language/en-US/language.json")
    message(STATUS "Selected language: English (en-US)")
else()
    # Default to Chinese if not specified
    set(LANG_JSON "${CMAKE_CURRENT_LIST_DIR}/language/zh-CN/language.json")
    message(STATUS "Language not specified, defaulting to Chinese (zh-CN)")
endif()

# Prefer python from OPEN_SDK_PYTHON, fallback to system python3.
set(PYTHON_CMD "")
if(DEFINED ENV{OPEN_SDK_PYTHON} AND NOT "$ENV{OPEN_SDK_PYTHON}" STREQUAL "")
    set(PYTHON_CMD $ENV{OPEN_SDK_PYTHON})
else()
    # Cross toolchains may prevent find_program() from seeing host tools.
    if(EXISTS "/usr/bin/python3")
        set(PYTHON_CMD "/usr/bin/python3")
    else()
        find_program(PYTHON_CMD python3)
    endif()
endif()

# Generate header if python is available.
if(PYTHON_CMD)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include")
    # Make sure header exists before compilation starts (configure-time fallback).
    if(NOT EXISTS ${LANG_HEADER})
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E env PYTHONIOENCODING=utf-8 PYTHONUNBUFFERED=1
                    ${PYTHON_CMD} ${LANG_SCRIPT} --input ${LANG_JSON} --output ${LANG_HEADER}
            RESULT_VARIABLE _lang_gen_ret
        )
        if(NOT _lang_gen_ret EQUAL 0)
            message(FATAL_ERROR "Failed to generate ${LANG_HEADER} (ret=${_lang_gen_ret})")
        endif()
    endif()

    add_custom_command(
        OUTPUT ${LANG_HEADER}
        COMMAND ${CMAKE_COMMAND} -E env PYTHONIOENCODING=utf-8 PYTHONUNBUFFERED=1
                ${PYTHON_CMD} ${LANG_SCRIPT} --input ${LANG_JSON} --output ${LANG_HEADER}
        DEPENDS ${LANG_SCRIPT} ${LANG_JSON}
        COMMENT "Generating language config header from ${LANG_JSON}"
        VERBATIM
    )

    add_custom_target(generate_lang_config
        DEPENDS ${LANG_HEADER}
        COMMENT "Language config generation target"
    )
else()
    message(WARNING "python3 not found; lang_config.h will not be generated (build may fail).")
endif()


########################################
# Target Configure
########################################

# Ensure language config header is generated before building (only if python is found)
if(PYTHON_CMD)
    add_dependencies(${EXAMPLE_LIB} generate_lang_config)
endif()

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${COMP_MODULE_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${COMP_MODULE_INC}
    )
