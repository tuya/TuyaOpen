/**
 * @file lv_games.h
 * @souce https://gitee.com/mgod_wu/AiPi-Eyes-Rx
 * @souce https://gitee.com/mgod_wu/AiPi-Eyes-Rx
 */

#ifndef LV_GAMES_H
#define LV_GAMES_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"


/*********************
 *      DEFINES
 *********************/
#define LV_USE_GAME_PVZ         1   // 植物大战僵尸
#define LV_USE_GAME_2048        1   // 2048
#define LV_USE_GAME_YANG        1   // 羊了个羊
#define LV_USE_GAME_XIAOXIAOLE  1   // 消消乐
#define LV_USE_GAME_HUARONGDAO  1   // 华容道


/* 2048 game */
#if LV_USE_GAME_2048
/* Matrix size Do not modify */
#define LV_100ASK_2048_MATRIX_SIZE 4
#endif

// LVGL v8 to v9 compatibility macros
#if LVGL_VERSION_MAJOR == 9
#if LV_COLOR_DEPTH == 32 && LV_COLOR_16_SWAP == 0

#ifndef LV_IMG_PX_SIZE_ALPHA_BYTE
#define LV_IMG_PX_SIZE_ALPHA_BYTE 4
#endif

#ifndef LV_COLOR_SIZE
#define LV_COLOR_SIZE 32
#endif

#ifndef LV_IMG_CF_TRUE_COLOR
#define LV_IMG_CF_TRUE_COLOR LV_COLOR_FORMAT_XRGB8888
#endif

#ifndef LV_IMG_CF_TRUE_COLOR_ALPHA
#define LV_IMG_CF_TRUE_COLOR_ALPHA LV_COLOR_FORMAT_ARGB8888
#endif

#define lv_obj_del_delayed lv_obj_delete_delayed

#else

#ifndef LV_IMG_PX_SIZE_ALPHA_BYTE
#define LV_IMG_PX_SIZE_ALPHA_BYTE 3
#endif

#ifndef LV_COLOR_SIZE
#define LV_COLOR_SIZE LV_COLOR_DEPTH
#endif

#ifndef LV_IMG_CF_TRUE_COLOR
#define LV_IMG_CF_TRUE_COLOR LV_COLOR_FORMAT_NATIVE
#endif

#ifndef LV_IMG_CF_TRUE_COLOR_ALPHA
#define LV_IMG_CF_TRUE_COLOR_ALPHA LV_COLOR_FORMAT_ARGB8888
#endif

#define lv_obj_del_delayed lv_obj_delete_delayed

#endif /* LV_COLOR_DEPTH == 32 && LV_COLOR_16_SWAP == 0 */
#endif /* LVGL_VERSION_MAJOR == 9 */


/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

#if LV_USE_GAME_PVZ
#include "pvz/pvz.h"
#endif

#if LV_USE_GAME_YANG
#include "yang/yang.h"
#endif

#if LV_USE_GAME_XIAOXIAOLE
#include "xiaoxiaole/xiaoxiaole.h"
#endif

#if LV_USE_GAME_HUARONGDAO
#include "huarongdao/huaorngdao.h"
#endif

#if LV_USE_GAME_2048
#include "game2048/lv_2048.h"
#endif


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_GAMES_H */

