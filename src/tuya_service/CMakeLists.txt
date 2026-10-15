##
# @file CMakeLists.txt
# @brief 交付仓保留的服务组件（core 未覆盖的部分），由平台构建统一编入
#/

# tal_kv：core 的 tal_kv 分发器（iot-core/tal_system/tal_kv.c）所需的
# ops 卡实现（LittleFS 介质 + AES-128-CBC 加密，见 tal_kv/src/tal_kv.c）
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tal_kv)

# 子目录把各自的库/include 追加到本作用域，这里再上抛给顶层
# （与 src/ai_components/CMakeLists.txt 的传播方式一致）
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
