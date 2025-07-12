##
# @file platform_config.cmake
#/

####################################################
# By configuring the variable PLATFORM_PUBINC,
# the header file in the platform is provided to TuyaOpen for use.
####################################################

list_subdirectories(PLATFORM_PUBINC ${PLATFORM_PATH}/tuyaos/tuyaos_adapter)
