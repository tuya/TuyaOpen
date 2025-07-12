##
# @file toolchain_file.cmake
#/

####################################################
# Modify the content of this file
# according to the actual situation
# and configure the actual path of the compilation tool
####################################################

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

# -fsanitize=address -fno-omit-frame-pointer
set(CMAKE_C_FLAGS " -g")
