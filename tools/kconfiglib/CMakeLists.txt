##
# @file kconfiglib/CMakeLists.txt
# @brief 
# @author Tuya
# @version 1.0.0
# @date 2023-04-08
#/

########################################
# First Run
########################################
# check dot-config
if(NOT EXISTS "${DOT_CONFIG_DIR}/${DOT_CONFIG}")  # copy pre-config to dot-config
    message(FATAL_ERROR "
    [KCONFIG] Don't found
    [${DOT_CONFIG_DIR}/${DOT_CONFIG}]
    Please use command: [tos.py config choice or tos.py config menu].
    ")
endif()

# use dot-config to generate cmake-config
if(NOT EXISTS "${DOT_CONFIG_DIR}/${CMAKE_CONFIG}")
    message(STATUS "[KCONFIG] Use dot-config to generate cmake-config.")
    execute_process(
        COMMAND
        python ${KCONFIG_TOOLS}/conf2cmake.py -c "${DOT_CONFIG_DIR}/${DOT_CONFIG}" -o "${DOT_CONFIG_DIR}/${CMAKE_CONFIG}"
        )
endif()

# use dot-config to generate header-file
if(NOT EXISTS "${HEADER_DIR}/${HEADER_FILE}")
    message(STATUS "[KCONFIG] Use dot-config to generate header-file.")
    # Create the output directory first: execute_process runs multiple COMMANDs
    # as a concurrent pipeline, so conf2h.py could otherwise start before
    # make_directory finished.
    file(MAKE_DIRECTORY "${HEADER_DIR}")
    execute_process(
        COMMAND
        python ${KCONFIG_TOOLS}/conf2h.py -c "${DOT_CONFIG_DIR}/${DOT_CONFIG}" -i "${HEADER_IN_PATH}" -p "${HEADER_PARAM_LIST}" -o "${HEADER_DIR}/${HEADER_FILE}"
        )
endif()
