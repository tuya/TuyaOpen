/**
 * @file demo_video_h264_embed.c
 * @brief Embed demo_video.264 for T5 P2P file mode
 * @copyright Copyright (c) Tuya Inc.
 */
#include "tuya_cloud_types.h"

#if (defined(CAMERA_DEMO_P2P_FILE_H264) && (CAMERA_DEMO_P2P_FILE_H264 == 1))

__asm__(
    ".section .rodata\n"
    ".align 4\n"
    ".global demo_video_264_start\n"
    ".global demo_video_264_end\n"
    "demo_video_264_start:\n"
    ".incbin \"../demo_video.264\"\n"
    "demo_video_264_end:\n");

#endif
