# @file CMakeLists.txt
# @brief 示例库的构建配置

# APP_PATH - 当前CMakeLists.txt所在目录
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME - 从当前目录名获取项目名称
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRC - 收集src目录下的所有源文件
aux_source_directory(${APP_PATH}/src APP_SRC)

# 收集子目录下的源文件
aux_source_directory(${APP_PATH}/src/huarongdao LV_GAMES_HRD_SRC)
list(APPEND APP_SRC ${LV_GAMES_HRD_SRC})
aux_source_directory(${APP_PATH}/src/pvz LV_GAMES_PVZ_SRC)
list(APPEND APP_SRC ${LV_GAMES_PVZ_SRC})
aux_source_directory(${APP_PATH}/src/xiaoxiaole LV_GAMES_XXL_SRC)
list(APPEND APP_SRC ${LV_GAMES_XXL_SRC})
aux_source_directory(${APP_PATH}/src/yang LV_GAMES_YANG_SRC)
list(APPEND APP_SRC ${LV_GAMES_YANG_SRC})
aux_source_directory(${APP_PATH}/src/game2048 LV_GAMES_2048_SRC)
list(APPEND APP_SRC ${LV_GAMES_2048_SRC})

# APP_INC - 包含目录
set(APP_INC
    ${APP_PATH}/include
    ${APP_PATH}/src
    ${APP_PATH}/src/huarongdao
    ${APP_PATH}/src/pvz
    ${APP_PATH}/src/xiaoxiaole
    ${APP_PATH}/src/yang
    ${APP_PATH}/src/game2048
)

# APP_OPTIONS - 编译选项
set(APP_OPTIONS "-w")
list(APPEND APP_OPTIONS "-Wall")

########################################
# Target Configure - 配置库目标
########################################
add_library(${EXAMPLE_LIB})
message(STATUS "EXAMPLE_LIB:${APP_PATH}")

# 设置源文件
target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRC}
    )

# 设置包含目录
target_include_directories(${EXAMPLE_LIB}
    PUBLIC
        ${APP_INC}
        ${APP_PATH}/src
        ${APP_PATH}/src/huarongdao
        ${APP_PATH}/src/pvz
        ${APP_PATH}/src/xiaoxiaole
        ${APP_PATH}/src/yang
        ${APP_PATH}/src/game2048
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        ${APP_OPTIONS}
    )
