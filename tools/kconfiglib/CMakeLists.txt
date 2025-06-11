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
    Please use command: [tos config_choice or tos menuconfig].
    ")
endif()

# use dot-config to generate cmake-config
if(NOT EXISTS "${DOT_CONFIG_DIR}/${CMAKE_CONFIG}")
    message(STATUS "[KCONFIG] Use dot-config to generate cmake-config.")
    execute_process(
        COMMAND
        python3 ${KCONFIG_TOOLS}/conf2cmake.py -c "${DOT_CONFIG_DIR}/${DOT_CONFIG}" -o "${DOT_CONFIG_DIR}/${CMAKE_CONFIG}"
        )
endif()

# use dot-config to generate header-file
if(NOT EXISTS "${HEADER_DIR}/${HEADER_FILE}")
    message(STATUS "[KCONFIG] Use dot-config to generate header-file.")
    execute_process(
        COMMAND
        ${CMAKE_COMMAND} -E make_directory ${HEADER_DIR}

        COMMAND
        python3 ${KCONFIG_TOOLS}/conf2h.py -c "${DOT_CONFIG_DIR}/${DOT_CONFIG}" -i "${HEADER_IN_PATH}" -p "${HEADER_PARAM_LIST}" -o "${HEADER_DIR}/${HEADER_FILE}"
        )
endif()
