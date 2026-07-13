# ─────────────────────────────────────────────────────────────────────────────
# LVGL PC Simulator — App config file
# Modify this file to switch between different apps, other files do not need to be modified.
# After modification, you need to re-run cmake -B .build to take effect (incremental compilation is not enough).
# ─────────────────────────────────────────────────────────────────────────────

# ── current active config: tuya_t5_pocket_ai ────────────────────────────────────
# tuya_t5_pocket_ai (384×168):
set(SIM_APP_NAME   "tuya_t5_pocket_ai")
set(SIM_SCREEN_W   384)
set(SIM_SCREEN_H   168)
set(SIM_ENTRY_FUNC "ui_init")
set(SIM_UI_SRC_DIRS
    "${CMAKE_SOURCE_DIR}/../../../apps/tuya_t5_pocket/tuya_t5_pocket_ai/src/display"
)
set(SIM_UI_INC_DIRS
    "${CMAKE_SOURCE_DIR}/../../../apps/tuya_t5_pocket/tuya_t5_pocket_ai/include"
    "${CMAKE_SOURCE_DIR}/../../../apps/tuya_t5_pocket/tuya_t5_pocket_ai/src/expand/inc"
)

# ─────────────────────────────────────────────────────────────────────────────
#
# your_chat_bot / ui_wechat (480×320):
#   set(SIM_APP_NAME   "your_chat_bot_wechat")
#   set(SIM_SCREEN_W   320)
#   set(SIM_SCREEN_H   480)
#   set(SIM_ENTRY_FUNC "ui_init")
#   set(SIM_UI_SRC_DIRS
#       "${CMAKE_SOURCE_DIR}/../../../apps/tuya.ai/your_chat_bot/src/display2/ui_wechat"
#       "${CMAKE_SOURCE_DIR}/../../../apps/tuya.ai/your_chat_bot/src/display2/app_ui_helper.c"
#   )
#   set(SIM_UI_INC_DIRS
#       "${CMAKE_SOURCE_DIR}/../../../apps/tuya.ai/your_chat_bot/src/display2"
#       "${CMAKE_SOURCE_DIR}/../../../apps/tuya.ai/your_chat_bot/src/display2/ui_wechat"
#       "${CMAKE_SOURCE_DIR}/../../../apps/tuya.ai/ai_components/assets/include"
#   )
#
# ─────────────────────────────────────────────────────────────────────────────
# your_chat_bot / ui_chatbot (480×320):
# set(SIM_APP_NAME  "your_chat_bot_chatbot")
# set(SIM_SCREEN_W  480)
# set(SIM_SCREEN_H  320)
# set(SIM_ENTRY_FUNC  "ui_init")
# set(SIM_UI_SRC_DIRS
#     "${CMAKE_SOURCE_DIR}/../../../apps/tuya.ai/your_chat_bot/src/display2/ui_chatbot"
#     "${CMAKE_SOURCE_DIR}/../../../apps/tuya.ai/your_chat_bot/src/display2/app_ui_helper.c"
# )
# set(SIM_UI_INC_DIRS
#     "${CMAKE_SOURCE_DIR}/../../../apps/tuya.ai/your_chat_bot/src/display2"
#     "${CMAKE_SOURCE_DIR}/../../../apps/tuya.ai/your_chat_bot/src/display2/ui_chatbot"
#     "${CMAKE_SOURCE_DIR}/../../../apps/tuya.ai/ai_components/assets/include"
# )

# ─────────────────────────────────────────────────────────────────────────────
