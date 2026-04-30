# ─────────────────────────────────────────────────────────────────────────────
# LVGL PC Simulator — App 配置文件
# 修改此文件切换不同 App 的 UI，其他文件无需改动。
# ─────────────────────────────────────────────────────────────────────────────

# App 名称（仅用于窗口标题）
set(SIM_APP_NAME  "tuya_t5_pocket_ai")

# 屏幕分辨率（与目标硬件一致）
set(SIM_SCREEN_W  384)
set(SIM_SCREEN_H  168)

# UI 入口函数（在 main.c 中被调用）
set(SIM_ENTRY_FUNC  "screens_init")

# UI 源码目录（递归搜索 .c 文件）
set(SIM_UI_SRC_DIRS
    "${CMAKE_SOURCE_DIR}/../../../apps/tuya_t5_pocket/tuya_t5_pocket_ai/src/display"
)

# App 专属头文件目录（不含 SDK 路径，stub 已覆盖）
set(SIM_UI_INC_DIRS
    "${CMAKE_SOURCE_DIR}/../../../apps/tuya_t5_pocket/tuya_t5_pocket_ai/include"
    "${CMAKE_SOURCE_DIR}/../../../apps/tuya_t5_pocket/tuya_t5_pocket_ai/src/expand/inc"
)

# ─────────────────────────────────────────────────────────────────────────────
# 切换到其他 App 示例（取消注释并注释上方配置即可）：
#
# your_chat_bot:
#   set(SIM_APP_NAME   "your_chat_bot")
#   set(SIM_SCREEN_W   480)
#   set(SIM_SCREEN_H   320)
#   set(SIM_ENTRY_FUNC "ui_init")
#   set(SIM_UI_SRC_DIRS "${CMAKE_SOURCE_DIR}/../../../apps/tuya.ai/your_chat_bot/src/display2")
#   set(SIM_UI_INC_DIRS "")
#
# your_robot_dog:
#   set(SIM_APP_NAME   "your_robot_dog")
#   set(SIM_SCREEN_W   480)
#   set(SIM_SCREEN_H   320)
#   set(SIM_ENTRY_FUNC "tuya_robot_init")
#   set(SIM_UI_SRC_DIRS "${CMAKE_SOURCE_DIR}/../../../apps/tuya.ai/your_robot_dog/src/display")
#   set(SIM_UI_INC_DIRS "")
# ─────────────────────────────────────────────────────────────────────────────
