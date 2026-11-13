##
# @file CMakeLists.txt
# @brief Generic camera capture and lifecycle self-test example.
#/

set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})
aux_source_directory(${APP_PATH}/src APP_SRCS)

add_library(${EXAMPLE_LIB})
target_sources(${EXAMPLE_LIB} PRIVATE ${APP_SRCS})
