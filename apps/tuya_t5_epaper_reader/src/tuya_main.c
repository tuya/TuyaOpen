/**
 * @file tuya_main.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_fs.h"
#include "utf8_to_gbk.h"
#include "EPD_4in26.h"
#include "GUI_Paint.h"
#include "hzk24.h"
#include "DEV_Config.h"
#include "tdl_button_manage.h"
#include "tdd_button_gpio.h"
#include "tkl_gpio.h"
#include "sd_image_view.h"
#include "tal_time_service.h"
#include "net_time_sync.h"
#include "baidu_netdisk.h"
#include "qrcodegen.h"

#include <stdio.h>
#include <string.h>

#if defined(EBABLE_SD_PINMUX) && (EBABLE_SD_PINMUX == 1)
#include "tkl_pinmux.h"
#endif

#include "board_com_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TASK_SD_PRIORITY THREAD_PRIO_2
#define TASK_SD_SIZE     (1024 * 16)

#define SDCARD_MOUNT_PATH "/sdcard"

// GPIO pin definitions for 7-key
#define GPIO_PIN_UP    TUYA_GPIO_NUM_27 // P27
#define GPIO_PIN_DOWN  TUYA_GPIO_NUM_31 // P31
#define GPIO_PIN_LEFT  TUYA_GPIO_NUM_36 // P36
#define GPIO_PIN_RIGHT TUYA_GPIO_NUM_30 // P30
#define GPIO_PIN_MID   TUYA_GPIO_NUM_37 // P37
#define GPIO_PIN_SET   TUYA_GPIO_NUM_32 // P32
#define GPIO_PIN_RST   TUYA_GPIO_NUM_39 // P39

#define BUTTON_ACTIVE_LEVEL TUYA_GPIO_LEVEL_LOW

// Display Layout
#define MAX_ITEMS_PER_PAGE 24
#define LIST_LINE_HEIGHT   30

#define TEXT_MARGIN_X      10
#define TEXT_MARGIN_TOP    80
#define TEXT_MARGIN_BOTTOM 10
#define TEXT_LINE_HEIGHT   24

#define PAGE_HISTORY_DEPTH 32
#define LINE_HISTORY_DEPTH 64

#define FILE_READ_WINDOW (96 * 1024)

#define PROGRESS_DIR  SDCARD_MOUNT_PATH "/.sd_reader"
#define PROGRESS_FILE PROGRESS_DIR "/progress.bin"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    STATE_FILE_LIST,
    STATE_SHOW_FILE,
    STATE_BD_AUTH,
    STATE_BD_LIST,
    STATE_BD_DETAIL,
    STATE_BD_MSG,
    STATE_ERROR
} APP_STATE_E;

typedef enum { VIEW_TEXT, VIEW_IMAGE } VIEW_KIND_E;

typedef enum { VIEW_ENC_GBK = 0, VIEW_ENC_UTF8, VIEW_ENC_UTF16LE, VIEW_ENC_UTF16BE } VIEW_ENC_E;

typedef struct {
    char    name[128]; // File name (UTF-8)
    BOOL_T  is_dir;
    INT64_T size;
} FILE_ITEM_T;

typedef struct {
    APP_STATE_E state;
    int         current_page;
    int         selected_index;
    int         total_files;
    int         total_pages;
    int         items_per_page;
    FILE_ITEM_T files[MAX_ITEMS_PER_PAGE];
    int         item_count_in_page;
    char        current_path[256];
    char        viewing_file[256]; // Full path of file being viewed
    VIEW_KIND_E view_kind;
    UWORD       rotate;
    VIEW_ENC_E  viewing_enc;
    INT64_T     viewing_offset;
    INT64_T     viewing_size;
    INT64_T     page_history[PAGE_HISTORY_DEPTH];
    int         page_hist_len;
    INT64_T     line_history[LINE_HISTORY_DEPTH];
    int         line_hist_len;
    BOOL_T      need_refresh;
} APP_CONTEXT_T;

typedef struct __attribute__((packed)) {
    uint16_t path_len;
    uint8_t  view_kind;
    uint8_t  rotate;
    INT64_T  offset;
} PROGRESS_REC_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE sg_sd_thrd_hdl;
static APP_CONTEXT_T sg_app_ctx;

// Button configuration
static TDL_BUTTON_HANDLE                 hdl_up, hdl_down, hdl_left, hdl_right, hdl_mid, hdl_set, hdl_rst;
static volatile BOOL_T                   sg_btn_pending          = FALSE;
static char                              sg_btn_pending_name[8]  = {0};
static volatile TDL_BUTTON_TOUCH_EVENT_E sg_btn_pending_event    = TDL_BUTTON_PRESS_NONE;
static int                               sg_bd_page              = 0;
static int                               sg_bd_selected          = 0;
static int                               sg_bd_detail_index      = -1;
static int                               sg_bd_msg_url_offset    = 0;
static int                               sg_bd_detail_url_offset = 0;
static volatile BOOL_T                   sg_sd_mounted           = FALSE;

/***********************************************************
***********************function define**********************
***********************************************************/

// Helper to draw Chinese string with HZK24
static void Paint_DrawString_CN_HZK24(UWORD Xstart, UWORD Ystart, const char *pString, UWORD Color_Foreground,
                                      UWORD Color_Background)
{
    // Extern declaration if header include fails
    extern int hzk24_get_font_data(uint8_t gb_high, uint8_t gb_low, uint8_t *buffer);

    const char *p_text = pString;
    int         x      = Xstart;
    int         y      = Ystart;

    while (*p_text != 0) {
        // Handle control characters
        if ((uint8_t)*p_text < 0x20) {
            if (*p_text == '\n') {
                x = Xstart;
                y += 24;
                p_text++;
                continue;
            }
            if (*p_text == '\r') {
                // Ignore CR if followed by LF, otherwise treat as newline
                if (*(p_text + 1) == '\n') {
                    p_text++;
                    continue;
                } else {
                    x = Xstart;
                    y += 24;
                    p_text++;
                    continue;
                }
            }
            if (*p_text == '\t') {
                // Tab = 4 spaces
                for (int i = 0; i < 4; i++) {
                    if (x + Font24.Width > Paint.WidthMemory) {
                        x = Xstart;
                        y += 24;
                        if (y + 24 > Paint.HeightMemory)
                            break;
                    }
                    Paint_DrawChar(x, y, ' ', &Font24, Color_Foreground, Color_Background);
                    x += Font24.Width;
                }
                p_text++;
                continue;
            }
            // Skip other control characters
            p_text++;
            continue;
        }

        // Stop if out of vertical bounds
        if (y + 24 > Paint.HeightMemory)
            break;

        if ((uint8_t)*p_text < 0x80) {
            // ASCII
            // Check horizontal bounds
            if (x + Font24.Width > Paint.WidthMemory) {
                x = Xstart;
                y += 24;
                if (y + 24 > Paint.HeightMemory)
                    break;
            }

            Paint_DrawChar(x, y, *p_text, &Font24, Color_Foreground, Color_Background);
            x += Font24.Width;
            p_text++;
        } else {
            // GBK - 2 bytes
            // Check horizontal bounds
            if (x + 24 > Paint.WidthMemory) {
                x = Xstart;
                y += 24;
                if (y + 24 > Paint.HeightMemory)
                    break;
            }

            uint8_t gb_high = (uint8_t)*p_text;
            uint8_t gb_low  = (uint8_t)*(p_text + 1);

            // Check bounds and validity
            if (gb_low == 0)
                break;

            uint8_t buffer[72]; // 24*24/8 = 72 bytes
            if (hzk24_get_font_data(gb_high, gb_low, buffer) == 0) {
                // Found font - Draw 24x24 bitmap
                for (int row = 0; row < 24; row++) {
                    for (int col_byte = 0; col_byte < 3; col_byte++) {
                        uint8_t data = buffer[row * 3 + col_byte];
                        for (int bit = 0; bit < 8; bit++) {
                            if (data & (0x80 >> bit)) {
                                Paint_SetPixel(x + col_byte * 8 + bit, y + row, Color_Foreground);
                            } else {
                                Paint_SetPixel(x + col_byte * 8 + bit, y + row, Color_Background);
                            }
                        }
                    }
                }
            } else {
                // Not found, draw '?'
                Paint_DrawChar(x, y, '?', &Font24, Color_Foreground, Color_Background);
            }

            x += 24;
            p_text += 2;
        }
    }
}

// Heuristic to detect UTF-8
static BOOL_T is_utf8(const uint8_t *data, int len)
{
    int score = 0;
    int i     = 0;
    while (i < len) {
        if (data[i] < 0x80) {
            i++;
            continue;
        }
        if ((data[i] & 0xE0) == 0xC0) { // 2 bytes
            if (i + 1 >= len)
                return score > 0; // Truncated at end: assume valid if we saw good chars
            if ((data[i + 1] & 0xC0) != 0x80)
                return FALSE;
            score++;
            i += 2;
        } else if ((data[i] & 0xF0) == 0xE0) { // 3 bytes
            if (i + 2 >= len)
                return score > 0; // Truncated at end
            if ((data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80)
                return FALSE;
            score++;
            i += 3;
        } else if ((data[i] & 0xF8) == 0xF0) { // 4 bytes
            if (i + 3 >= len)
                return score > 0; // Truncated at end
            if ((data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 || (data[i + 3] & 0xC0) != 0x80)
                return FALSE;
            score++;
            i += 4;
        } else {
            return FALSE;
        }
    }
    return score > 0;
}

static void Paint_DrawText_CN_HZK24_Adaptive(UWORD Xstart, UWORD Ystart, UWORD Width, UWORD Height, const char *pString,
                                             UWORD Color_Foreground, UWORD Color_Background)
{
    extern int hzk24_get_font_data(uint8_t gb_high, uint8_t gb_low, uint8_t *buffer);

    const uint8_t *p_text      = (const uint8_t *)pString;
    int            x           = Xstart;
    int            y           = Ystart;
    int            line_height = 24;

    while (*p_text != 0) {
        // 1. Handle Control Characters
        if (*p_text == '\n') {
            x = Xstart;
            y += line_height;
            p_text++;
            continue;
        }
        if (*p_text == '\r') {
            if (*(p_text + 1) == '\n') {
                p_text++; // Skip CR, let \n handle the newline
            } else {
                x = Xstart;
                p_text++;
            }
            continue;
        }
        if (*p_text == '\t') {
            x += Font24.Width * 4;
            if (x > Xstart + Width) {
                x = Xstart;
                y += line_height;
            }
            p_text++;
            continue;
        }

        // 2. Check Vertical Bounds
        if (y + line_height > Ystart + Height)
            break;
        if (y + line_height > Paint.Height)
            break;

        // 3. Handle Visible Characters
        if (*p_text < 0x80) {
            // ASCII
            if (*p_text < 0x20) {
                p_text++; // Skip other control chars
                continue;
            }

            if (x + Font24.Width > Xstart + Width || x + Font24.Width > Paint.Width) {
                x = Xstart;
                y += line_height;
                if (y + line_height > Ystart + Height || y + line_height > Paint.Height)
                    break;
            }

            Paint_DrawChar(x, y, *p_text, &Font24, Color_Foreground, Color_Background);
            x += Font24.Width;
            p_text++;
        } else {
            // GBK (2 bytes)
            if (*(p_text + 1) == 0)
                break; // Incomplete

            if (x + 24 > Xstart + Width || x + 24 > Paint.Width) {
                x = Xstart;
                y += line_height;
                if (y + line_height > Ystart + Height || y + line_height > Paint.Height)
                    break;
            }

            uint8_t gb_high = *p_text;
            uint8_t gb_low  = *(p_text + 1);

            uint8_t buffer[72];
            if (hzk24_get_font_data(gb_high, gb_low, buffer) == 0) {
                for (int row = 0; row < 24; row++) {
                    for (int col_byte = 0; col_byte < 3; col_byte++) {
                        uint8_t data = buffer[row * 3 + col_byte];
                        for (int bit = 0; bit < 8; bit++) {
                            if (data & (0x80 >> bit)) {
                                Paint_SetPixel(x + col_byte * 8 + bit, y + row, Color_Foreground);
                            } else {
                                Paint_SetPixel(x + col_byte * 8 + bit, y + row, Color_Background);
                            }
                        }
                    }
                }
            } else {
                // Not found, draw space instead of '?' to avoid garbled look for unsupported chars (like fullwidth
                // space)
                Paint_DrawChar(x, y, ' ', &Font24, Color_Foreground, Color_Background);
            }
            x += 24;
            p_text += 2;
        }
    }
}

// Convert UTF-8 to GBK for display if needed (simple heuristic)
// Note: In this system, filenames read from FATFS might already be GBK if not LFN-enabled or configured differently.
// But based on previous logs, we see some need conversion and some don't.
// We will assume filenames are UTF-8 in the struct, and we convert them for display if they are not ASCII.
// Wait, the previous logs showed: "Found file (GBK->UTF8): ... [Raw: ...]"
// This implies the raw name from `tkl_dir_read` was GBK.
// So `file_info` contains GBK. We should store GBK in our struct to easily open files,
// but for display, we might want to check encoding.
// Actually, `Paint_DrawString_CN_HZK24` expects GBK.
// So if the filesystem returns GBK, we can pass it directly to `Paint_DrawString_CN_HZK24`.
// If the filesystem returns UTF-8, we need to convert to GBK.
// Based on "Found file (GBK->UTF8)", the RAW was GBK. So we store Raw (GBK).

static void   path_join(char *out, size_t out_len, const char *base, const char *name);
static size_t gbk_prefix_fit_px(const char *s, int max_px, int *out_px);

static void format_size_human(char *out, size_t out_len, INT64_T size)
{
    if (!out || out_len == 0)
        return;
    if (size < 0) {
        snprintf(out, out_len, "--");
        return;
    }
    double      v    = (double)size;
    const char *unit = "B";
    if (v >= 1024.0) {
        v /= 1024.0;
        unit = "K";
    }
    if (v >= 1024.0) {
        v /= 1024.0;
        unit = "M";
    }
    if (v >= 1024.0) {
        v /= 1024.0;
        unit = "G";
    }
    if (strcmp(unit, "B") == 0) {
        snprintf(out, out_len, "%lldB", (long long)size);
    } else if (v < 10.0) {
        snprintf(out, out_len, "%.1f%s", v, unit);
    } else {
        snprintf(out, out_len, "%.0f%s", v, unit);
    }
}

static void clip_name_to_px(char *out, size_t out_len, const char *name, int max_px)
{
    if (!out || out_len == 0)
        return;
    out[0] = 0;
    if (!name || max_px <= 0)
        return;
    int    used_px = 0;
    size_t fit     = gbk_prefix_fit_px(name, max_px, &used_px);
    if (fit >= out_len)
        fit = out_len - 1;
    memcpy(out, name, fit);
    out[fit] = 0;
}

static int draw_wrapped_text_en_offset(int x, int y, int max_w, int max_lines, const char *s, int start_off,
                                       int *out_used)
{
    if (!s || max_lines <= 0 || max_w <= 0)
        return y;
    size_t len = strlen(s);
    if (start_off < 0)
        start_off = 0;
    if ((size_t)start_off > len)
        start_off = (int)len;
    const char *p    = s + start_off;
    size_t      used = 0;
    for (int i = 0; i < max_lines; i++) {
        while (*p == ' ') {
            p++;
            used++;
        }
        if (*p == 0)
            break;
        int    used_px = 0;
        size_t fit     = gbk_prefix_fit_px(p, max_w, &used_px);
        if (fit == 0)
            break;
        char line[200];
        if (fit >= sizeof(line))
            fit = sizeof(line) - 1;
        memcpy(line, p, fit);
        line[fit] = 0;
        Paint_DrawString_EN((UWORD)x, (UWORD)y, line, &Font24, BLACK, WHITE);
        p += fit;
        used += fit;
        y += 28;
    }
    if (out_used)
        *out_used = (int)used;
    return y;
}

static void scan_files(void)
{
    sg_app_ctx.item_count_in_page = 0;

    TUYA_DIR dir_hdl = NULL;
    if (tkl_dir_open(sg_app_ctx.current_path, &dir_hdl) != OPRT_OK) {
        PR_ERR("Failed to open dir: %s", sg_app_ctx.current_path);
        return;
    }

    TUYA_FILEINFO file_info   = {0};
    int           total_files = 0;
    int           skip_files  = sg_app_ctx.current_page * sg_app_ctx.items_per_page;
    int           files_added = 0;

    // First pass: count total files (optional, but good for pagination)
    // For simplicity, we just scan linearly and pick the slice we want.
    // Optimization: We could cache total count.

    while (tkl_dir_read(dir_hdl, &file_info) == OPRT_OK) {
        char *name = NULL;
        if (tkl_dir_name(file_info, (const char **)&name) == OPRT_OK) {
            if (name[0] == '.')
                continue; // Skip hidden files

            if (total_files >= skip_files && files_added < sg_app_ctx.items_per_page) {
                strncpy(sg_app_ctx.files[files_added].name, name, 127);
                sg_app_ctx.files[files_added].name[127] = 0;
                BOOL_T is_dir                           = FALSE;
                if (tkl_dir_is_directory(file_info, &is_dir) != OPRT_OK) {
                    is_dir = FALSE;
                }
                sg_app_ctx.files[files_added].is_dir = is_dir;
                if (is_dir) {
                    sg_app_ctx.files[files_added].size = -1;
                } else {
                    char full_path[256];
                    path_join(full_path, sizeof(full_path), sg_app_ctx.current_path,
                              sg_app_ctx.files[files_added].name);
                    sg_app_ctx.files[files_added].size = tkl_fgetsize(full_path);
                }
                files_added++;
            }
            total_files++;
        }
    }
    tkl_dir_close(dir_hdl);

    sg_app_ctx.total_files        = total_files;
    sg_app_ctx.item_count_in_page = files_added;
    sg_app_ctx.total_pages        = (total_files + sg_app_ctx.items_per_page - 1) / sg_app_ctx.items_per_page;

    if (sg_app_ctx.total_pages == 0)
        sg_app_ctx.total_pages = 1;

    // Adjust selected index if out of bounds
    if (sg_app_ctx.selected_index >= sg_app_ctx.item_count_in_page) {
        sg_app_ctx.selected_index = sg_app_ctx.item_count_in_page - 1;
    }
    if (sg_app_ctx.selected_index < 0)
        sg_app_ctx.selected_index = 0;

    PR_NOTICE("Scanned page %d: %d files. Total: %d", sg_app_ctx.current_page, sg_app_ctx.item_count_in_page,
              total_files);
}

static void path_join(char *out, size_t out_len, const char *base, const char *name)
{
    if (!base || !name) {
        if (out_len)
            out[0] = 0;
        return;
    }
    if (strcmp(base, "/") == 0) {
        snprintf(out, out_len, "/%s", name);
        return;
    }
    if (base[strlen(base) - 1] == '/') {
        snprintf(out, out_len, "%s%s", base, name);
    } else {
        snprintf(out, out_len, "%s/%s", base, name);
    }
}

static BOOL_T path_is_root(const char *path)
{
    return (path && strcmp(path, SDCARD_MOUNT_PATH) == 0) ? TRUE : FALSE;
}

static void path_to_parent(char *path, size_t path_len)
{
    if (!path || path_len == 0)
        return;
    if (path_is_root(path))
        return;
    size_t n = strlen(path);
    while (n > 0 && path[n - 1] == '/') {
        path[n - 1] = 0;
        n--;
    }
    char *slash = strrchr(path, '/');
    if (!slash) {
        strncpy(path, SDCARD_MOUNT_PATH, path_len - 1);
        path[path_len - 1] = 0;
        return;
    }
    if (slash == path) {
        strncpy(path, SDCARD_MOUNT_PATH, path_len - 1);
        path[path_len - 1] = 0;
        return;
    }
    *slash = 0;
    if (strlen(path) == 0) {
        strncpy(path, SDCARD_MOUNT_PATH, path_len - 1);
        path[path_len - 1] = 0;
    }
}

static int mkdir_p(const char *path)
{
    if (!path || !path[0])
        return -1;
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t n = strlen(tmp);
    if (n == 0)
        return -1;
    if (tmp[n - 1] == '/')
        tmp[n - 1] = 0;

    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = 0;
        if (tmp[0]) {
            if (tkl_fs_mkdir(tmp) != 0) {
                TUYA_DIR d = NULL;
                if (tkl_dir_open(tmp, &d) != OPRT_OK) {
                    *p = '/';
                    return -1;
                }
                tkl_dir_close(d);
            }
        }
        *p = '/';
    }

    if (tkl_fs_mkdir(tmp) != 0) {
        TUYA_DIR d = NULL;
        if (tkl_dir_open(tmp, &d) != OPRT_OK)
            return -1;
        tkl_dir_close(d);
    }
    return 0;
}

static void progress_init(void)
{
    mkdir_p(PROGRESS_DIR);
}

static int progress_load(const char *path, VIEW_KIND_E kind, UWORD *out_rotate, INT64_T *out_offset)
{
    if (!path || !out_rotate || !out_offset)
        return -1;
    TUYA_FILE f = tkl_fopen(PROGRESS_FILE, "r");
    if (!f)
        return -1;
    uint8_t magic[4];
    if (tkl_fread(magic, 4, f) != 4 || memcmp(magic, "PRG1", 4) != 0) {
        tkl_fclose(f);
        return -1;
    }
    for (;;) {
        PROGRESS_REC_T rec;
        int            rd = tkl_fread(&rec, (int)sizeof(rec), f);
        if (rd != (int)sizeof(rec))
            break;
        if (rec.path_len == 0 || rec.path_len > 512)
            break;
        char *p = (char *)tal_malloc(rec.path_len);
        if (!p)
            break;
        if (tkl_fread(p, rec.path_len, f) != (int)rec.path_len) {
            tal_free(p);
            break;
        }
        int match =
            (rec.view_kind == (uint8_t)kind) && (rec.path_len == strlen(path)) && (memcmp(p, path, rec.path_len) == 0);
        tal_free(p);
        if (match) {
            *out_rotate = (UWORD)rec.rotate;
            *out_offset = rec.offset;
            tkl_fclose(f);
            return 0;
        }
    }
    tkl_fclose(f);
    return -1;
}

static int progress_save(const char *path, VIEW_KIND_E kind, UWORD rotate, INT64_T offset)
{
    if (!path)
        return -1;
    size_t path_len = strlen(path);
    if (path_len == 0 || path_len > 512)
        return -1;

    uint8_t  *old_buf = NULL;
    size_t    old_len = 0;
    TUYA_FILE f       = tkl_fopen(PROGRESS_FILE, "r");
    if (f) {
        int sz = tkl_fgetsize(PROGRESS_FILE);
        if (sz > 0 && sz < (1024 * 128)) {
            old_buf = (uint8_t *)tal_malloc((size_t)sz);
            if (old_buf) {
                int rd = tkl_fread(old_buf, sz, f);
                if (rd == sz)
                    old_len = (size_t)sz;
                else {
                    tal_free(old_buf);
                    old_buf = NULL;
                    old_len = 0;
                }
            }
        }
        tkl_fclose(f);
    }

    size_t   new_cap = old_len + sizeof(PROGRESS_REC_T) + path_len + 16;
    uint8_t *new_buf = (uint8_t *)tal_malloc(new_cap);
    if (!new_buf) {
        if (old_buf)
            tal_free(old_buf);
        return -1;
    }
    size_t w = 0;
    memcpy(new_buf + w, "PRG1", 4);
    w += 4;

    int updated = 0;
    if (old_buf && old_len >= 4 && memcmp(old_buf, "PRG1", 4) == 0) {
        size_t pos = 4;
        while (pos + sizeof(PROGRESS_REC_T) <= old_len) {
            PROGRESS_REC_T rec;
            memcpy(&rec, old_buf + pos, sizeof(rec));
            pos += sizeof(rec);
            if (rec.path_len == 0 || rec.path_len > 512)
                break;
            if (pos + rec.path_len > old_len)
                break;
            const uint8_t *p = old_buf + pos;
            int            match =
                (rec.view_kind == (uint8_t)kind) && (rec.path_len == path_len) && (memcmp(p, path, path_len) == 0);
            pos += rec.path_len;

            if (match) {
                rec.rotate = (uint8_t)rotate;
                rec.offset = offset;
                updated    = 1;
            }

            memcpy(new_buf + w, &rec, sizeof(rec));
            w += sizeof(rec);
            memcpy(new_buf + w, p, rec.path_len);
            w += rec.path_len;
        }
    }

    if (!updated) {
        PROGRESS_REC_T rec;
        rec.path_len  = (uint16_t)path_len;
        rec.view_kind = (uint8_t)kind;
        rec.rotate    = (uint8_t)rotate;
        rec.offset    = offset;
        memcpy(new_buf + w, &rec, sizeof(rec));
        w += sizeof(rec);
        memcpy(new_buf + w, path, path_len);
        w += path_len;
    }

    if (old_buf)
        tal_free(old_buf);

    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s.tmp", PROGRESS_FILE);
    TUYA_FILE wf = tkl_fopen(tmp, "w");
    if (!wf) {
        tal_free(new_buf);
        return -1;
    }
    int wrote = tkl_fwrite(new_buf, (int)w, wf);
    tkl_fclose(wf);
    tal_free(new_buf);
    if (wrote != (int)w) {
        tkl_fs_remove(tmp);
        return -1;
    }
    tkl_fs_remove(PROGRESS_FILE);
    if (tkl_fs_rename(tmp, PROGRESS_FILE) != 0) {
        tkl_fs_remove(tmp);
        return -1;
    }
    return 0;
}

static const char *file_ext(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name)
        return "";
    return dot + 1;
}

static BOOL_T ext_eq(const char *ext, const char *rhs)
{
    if (!ext || !rhs)
        return FALSE;
    while (*ext && *rhs) {
        char a = *ext++;
        char b = *rhs++;
        if (a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z')
            b = (char)(b - 'A' + 'a');
        if (a != b)
            return FALSE;
    }
    return (*ext == 0 && *rhs == 0);
}

static BOOL_T is_image_file(const char *name)
{
    const char *ext = file_ext(name);
    return ext_eq(ext, "bmp") || ext_eq(ext, "jpg") || ext_eq(ext, "jpeg") || ext_eq(ext, "png");
}

static void update_items_per_page(void)
{
    int screen_h =
        (sg_app_ctx.rotate == ROTATE_0 || sg_app_ctx.rotate == ROTATE_180) ? EPD_4in26_HEIGHT : EPD_4in26_WIDTH;
    int header_h  = 45;
    int available = screen_h - header_h - 10;
    int n         = available / LIST_LINE_HEIGHT;
    if (n < 1)
        n = 1;
    if (n > MAX_ITEMS_PER_PAGE)
        n = MAX_ITEMS_PER_PAGE;
    sg_app_ctx.items_per_page = n;
}

static VIEW_ENC_E detect_file_encoding(const char *path, INT64_T *out_bom_skip)
{
    if (out_bom_skip)
        *out_bom_skip = 0;
    TUYA_FILE f = tkl_fopen(path, "r");
    if (!f)
        return VIEW_ENC_GBK;
    uint8_t *buf = (uint8_t *)tal_malloc(4096);
    if (!buf) {
        tkl_fclose(f);
        return VIEW_ENC_GBK;
    }
    int len = tkl_fread(buf, 4096, f);
    if (len < 0)
        len = 0;
    tkl_fclose(f);

    if (len >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) {
        if (out_bom_skip)
            *out_bom_skip = 3;
        tal_free(buf);
        return VIEW_ENC_UTF8;
    }
    if (len >= 2 && buf[0] == 0xFF && buf[1] == 0xFE) {
        if (out_bom_skip)
            *out_bom_skip = 2;
        tal_free(buf);
        return VIEW_ENC_UTF16LE;
    }
    if (len >= 2 && buf[0] == 0xFE && buf[1] == 0xFF) {
        if (out_bom_skip)
            *out_bom_skip = 2;
        tal_free(buf);
        return VIEW_ENC_UTF16BE;
    }

    if (is_utf8(buf, len)) {
        tal_free(buf);
        return VIEW_ENC_UTF8;
    }

    int zeros_even = 0, zeros_odd = 0, pairs = 0;
    for (int i = 0; i + 1 < len; i += 2) {
        if (buf[i] == 0)
            zeros_even++;
        if (buf[i + 1] == 0)
            zeros_odd++;
        pairs++;
    }
    tal_free(buf);
    if (pairs > 32) {
        if (zeros_even > (pairs / 3) && zeros_odd < (pairs / 10))
            return VIEW_ENC_UTF16BE;
        if (zeros_odd > (pairs / 3) && zeros_even < (pairs / 10))
            return VIEW_ENC_UTF16LE;
    }
    return VIEW_ENC_GBK;
}

static void open_item_for_view(void)
{
    if (sg_app_ctx.item_count_in_page <= 0)
        return;
    FILE_ITEM_T *it = &sg_app_ctx.files[sg_app_ctx.selected_index];
    if (it->is_dir)
        return;
    char full_path[256];
    path_join(full_path, sizeof(full_path), sg_app_ctx.current_path, it->name);

    if (ext_eq(file_ext(it->name), "pdf")) {
        char pages_dir[256];
        snprintf(pages_dir, sizeof(pages_dir), "%s", full_path);
        char *dot = strrchr(pages_dir, '.');
        if (dot)
            *dot = 0;
        strncat(pages_dir, "_pages", sizeof(pages_dir) - strlen(pages_dir) - 1);
        TUYA_DIR dir = NULL;
        if (tkl_dir_open(pages_dir, &dir) == OPRT_OK) {
            tkl_dir_close(dir);
            strncpy(sg_app_ctx.current_path, pages_dir, sizeof(sg_app_ctx.current_path) - 1);
            sg_app_ctx.current_path[sizeof(sg_app_ctx.current_path) - 1] = 0;
            sg_app_ctx.state                                             = STATE_FILE_LIST;
            sg_app_ctx.current_page                                      = 0;
            sg_app_ctx.selected_index                                    = 0;
            scan_files();
            sg_app_ctx.need_refresh = TRUE;
            return;
        }
    }

    strncpy(sg_app_ctx.viewing_file, full_path, sizeof(sg_app_ctx.viewing_file) - 1);
    sg_app_ctx.viewing_file[sizeof(sg_app_ctx.viewing_file) - 1] = 0;
    sg_app_ctx.viewing_size                                      = tkl_fgetsize(sg_app_ctx.viewing_file);
    sg_app_ctx.page_hist_len                                     = 0;
    sg_app_ctx.line_hist_len                                     = 0;
    sg_app_ctx.view_kind                                         = is_image_file(it->name) ? VIEW_IMAGE : VIEW_TEXT;
    INT64_T bom                                                  = 0;
    sg_app_ctx.viewing_enc =
        (sg_app_ctx.view_kind == VIEW_TEXT) ? detect_file_encoding(sg_app_ctx.viewing_file, &bom) : VIEW_ENC_GBK;
    sg_app_ctx.viewing_offset = (sg_app_ctx.view_kind == VIEW_TEXT) ? bom : 0;

    UWORD   saved_rot = 0;
    INT64_T saved_off = 0;
    if (progress_load(sg_app_ctx.viewing_file, sg_app_ctx.view_kind, &saved_rot, &saved_off) == 0) {
        if (saved_rot == ROTATE_0 || saved_rot == ROTATE_90 || saved_rot == ROTATE_180 || saved_rot == ROTATE_270) {
            sg_app_ctx.rotate = saved_rot;
        }
        if (saved_off >= 0 && saved_off < sg_app_ctx.viewing_size) {
            sg_app_ctx.viewing_offset = saved_off;
        }
    }
    if ((sg_app_ctx.viewing_enc == VIEW_ENC_UTF16LE || sg_app_ctx.viewing_enc == VIEW_ENC_UTF16BE) &&
        (sg_app_ctx.viewing_offset & 1)) {
        sg_app_ctx.viewing_offset--;
    }
}

static int display_image_1bit(const char *path, int x, int y, int w, int h)
{
    return sd_draw_image_1bit(path, x, y, w, h);
}

static const char *path_basename(const char *path)
{
    if (!path)
        return "";
    const char *p = strrchr(path, '/');
    return p ? (p + 1) : path;
}

static void format_time_hhmm(char out[6])
{
    POSIX_TM_S tm;
    if (tal_time_get_local_time_custom(0, &tm) == OPRT_OK) {
        snprintf(out, 6, "%02d:%02d", tm.tm_hour, tm.tm_min);
    } else {
        snprintf(out, 6, "--:--");
    }
}

#define BRAND_GBK "\xBC\xD6-AIDevLog"

static void build_time_brand_line(char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return;
    }

    POSIX_TM_S tm;
    int        n = 0;
    if (tal_time_get_local_time_custom(0, &tm) == OPRT_OK) {
        n = snprintf(out, out_len, "%04d-%02d-%02d %02d:%02d ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                     tm.tm_hour, tm.tm_min);
    } else {
        n = snprintf(out, out_len, "0000-00-00 00:00 ");
    }

    if (n < 0) {
        out[0] = 0;
        return;
    }

    size_t used = (size_t)n;
    if (used >= out_len) {
        used = out_len - 1;
    }

    size_t brand_len = strlen(BRAND_GBK);
    size_t remain    = out_len - used;
    if (remain == 0) {
        return;
    }

    if (brand_len + 1 <= remain) {
        memcpy(out + used, BRAND_GBK, brand_len + 1);
    } else if (remain > 1) {
        memcpy(out + used, BRAND_GBK, remain - 1);
        out[out_len - 1] = 0;
    } else {
        out[out_len - 1] = 0;
    }
}

static int gbk_pixel_width(const char *s)
{
    if (!s)
        return 0;
    int            w = 0;
    const uint8_t *p = (const uint8_t *)s;
    while (*p) {
        if (*p < 0x80) {
            if (*p < 0x20) {
                p++;
                continue;
            }
            w += Font24.Width;
            p++;
        } else {
            if (*(p + 1) == 0)
                break;
            w += 24;
            p += 2;
        }
    }
    return w;
}

static void draw_qrcode(int x0, int y0, int max_w, int max_h, const char *text)
{
    if (!text || !text[0])
        return;
    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    uint8_t temp[qrcodegen_BUFFER_LEN_MAX];
    BOOL_T  ok = qrcodegen_encodeText(text, temp, qrcode, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN,
                                      qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);
    if (!ok)
        return;

    int size    = qrcodegen_getSize(qrcode);
    int border  = 2;
    int modules = size + border * 2;
    if (modules <= 0)
        return;
    int sx    = max_w / modules;
    int sy    = max_h / modules;
    int scale = sx < sy ? sx : sy;
    if (scale < 1)
        scale = 1;
    if (scale > 8)
        scale = 8;
    int draw_w = modules * scale;
    int draw_h = modules * scale;
    int x      = x0 + (max_w - draw_w) / 2;
    int y      = y0 + (max_h - draw_h) / 2;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;

    Paint_DrawRectangle((UWORD)x, (UWORD)y, (UWORD)(x + draw_w - 1), (UWORD)(y + draw_h - 1), WHITE, DOT_PIXEL_1X1,
                        DRAW_FILL_FULL);

    for (int iy = -border; iy < size + border; iy++) {
        for (int ix = -border; ix < size + border; ix++) {
            BOOL_T m = FALSE;
            if (ix >= 0 && ix < size && iy >= 0 && iy < size) {
                m = qrcodegen_getModule(qrcode, ix, iy) ? TRUE : FALSE;
            }
            if (!m)
                continue;
            int px = x + (ix + border) * scale;
            int py = y + (iy + border) * scale;
            Paint_DrawRectangle((UWORD)px, (UWORD)py, (UWORD)(px + scale - 1), (UWORD)(py + scale - 1), BLACK,
                                DOT_PIXEL_1X1, DRAW_FILL_FULL);
        }
    }
}

static size_t gbk_prefix_fit_px(const char *s, int max_px, int *out_px)
{
    if (!s || max_px <= 0) {
        if (out_px)
            *out_px = 0;
        return 0;
    }
    size_t i = 0;
    int    w = 0;
    while (s[i]) {
        unsigned char c    = (unsigned char)s[i];
        int           cw   = 0;
        size_t        step = 1;
        if (c < 0x80) {
            if (c < 0x20) {
                i += 1;
                continue;
            }
            cw   = Font24.Width;
            step = 1;
        } else {
            if (s[i + 1] == 0)
                break;
            cw   = 24;
            step = 2;
        }
        if (w + cw > max_px)
            break;
        w += cw;
        i += step;
    }
    if (out_px)
        *out_px = w;
    return i;
}

static void build_preview_header(char *out, size_t out_len, int max_px, const char *file_path, int cur_page,
                                 int total_pages, int percent, const char time_hhmm[6])
{
    if (!out_len)
        return;
    const char *name = path_basename(file_path);
    char        base[96];
    snprintf(base, sizeof(base), "%s", name ? name : "");

    char suffix[96];
    snprintf(suffix, sizeof(suffix), " %d/%d %02d%% %s %s", cur_page, total_pages, percent, time_hhmm, BRAND_GBK);
    if (gbk_pixel_width(suffix) > max_px) {
        snprintf(suffix, sizeof(suffix), " %02d%% %s %s", percent, time_hhmm, BRAND_GBK);
    }
    if (gbk_pixel_width(suffix) > max_px) {
        snprintf(suffix, sizeof(suffix), " %02d%% %s", percent, BRAND_GBK);
    }

    int suffix_px     = gbk_pixel_width(suffix);
    int base_px_allow = max_px - suffix_px;
    if (base_px_allow <= 0) {
        const char *s = suffix;
        if (s[0] == ' ')
            s++;
        snprintf(out, out_len, "%s", s);
        return;
    }

    int    base_px   = 0;
    size_t base_keep = gbk_prefix_fit_px(base, base_px_allow, &base_px);

    char   clipped[100];
    size_t base_len = strlen(base);
    if (base_keep < base_len) {
        int tilde_px = Font24.Width;
        if (base_px + tilde_px <= base_px_allow && base_keep + 1 < sizeof(clipped)) {
            memcpy(clipped, base, base_keep);
            clipped[base_keep]     = '~';
            clipped[base_keep + 1] = 0;
        } else {
            snprintf(clipped, sizeof(clipped), "~");
        }
    } else {
        snprintf(clipped, sizeof(clipped), "%s", base);
    }

    snprintf(out, out_len, "%s%s", clipped, suffix);
}

static size_t utf8_seq_len(uint8_t c)
{
    if (c < 0x80)
        return 1;
    if ((c & 0xE0) == 0xC0)
        return 2;
    if ((c & 0xF0) == 0xE0)
        return 3;
    if ((c & 0xF8) == 0xF0)
        return 4;
    return 1;
}

static uint16_t u16_at(const uint8_t *p, BOOL_T le)
{
    return le ? (uint16_t)p[0] | ((uint16_t)p[1] << 8) : (uint16_t)p[1] | ((uint16_t)p[0] << 8);
}

static int utf16_to_utf8_buf(const uint8_t *in, int in_len, BOOL_T le, uint8_t *out, int out_cap)
{
    int out_len = 0;
    int i       = 0;
    while (i + 1 < in_len) {
        uint16_t u = u16_at(in + i, le);
        i += 2;
        uint32_t cp = u;
        if (u >= 0xD800 && u <= 0xDBFF) {
            if (i + 1 < in_len) {
                uint16_t lo = u16_at(in + i, le);
                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000u + (((uint32_t)(u - 0xD800) << 10) | (uint32_t)(lo - 0xDC00));
                    i += 2;
                }
            }
        }
        if (cp <= 0x7Fu) {
            if (out_len + 1 > out_cap)
                break;
            out[out_len++] = (uint8_t)cp;
        } else if (cp <= 0x7FFu) {
            if (out_len + 2 > out_cap)
                break;
            out[out_len++] = (uint8_t)(0xC0 | ((cp >> 6) & 0x1F));
            out[out_len++] = (uint8_t)(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFFu) {
            if (out_len + 3 > out_cap)
                break;
            out[out_len++] = (uint8_t)(0xE0 | ((cp >> 12) & 0x0F));
            out[out_len++] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
            out[out_len++] = (uint8_t)(0x80 | (cp & 0x3F));
        } else if (cp <= 0x10FFFFu) {
            if (out_len + 4 > out_cap)
                break;
            out[out_len++] = (uint8_t)(0xF0 | ((cp >> 18) & 0x07));
            out[out_len++] = (uint8_t)(0x80 | ((cp >> 12) & 0x3F));
            out[out_len++] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
            out[out_len++] = (uint8_t)(0x80 | (cp & 0x3F));
        } else {
            if (out_len + 1 > out_cap)
                break;
            out[out_len++] = '?';
        }
    }
    return out_len;
}

static size_t advance_one_line_in_buf_utf16(const uint8_t *buf, size_t len, BOOL_T le, int max_width)
{
    int    x = 0;
    size_t i = 0;
    while (i + 1 < len) {
        uint16_t u = u16_at(buf + i, le);
        if (u == 0x000A)
            return i + 2;
        if (u == 0x000D) {
            if (i + 3 < len && u16_at(buf + i + 2, le) == 0x000A)
                return i + 4;
            return i + 2;
        }
        int    glyph_w = 0;
        size_t step    = 2;
        if (u < 0x20) {
            i += 2;
            continue;
        }
        if (u >= 0xD800 && u <= 0xDBFF) {
            if (i + 3 < len) {
                uint16_t lo = u16_at(buf + i + 2, le);
                if (lo >= 0xDC00 && lo <= 0xDFFF)
                    step = 4;
            }
            glyph_w = 24;
        } else if (u < 0x80) {
            glyph_w = Font24.Width;
        } else {
            glyph_w = 24;
        }

        if (x > 0 && x + glyph_w > max_width)
            return i;
        x += glyph_w;
        i += step;
    }
    return i;
}

static size_t advance_one_line_in_buf_8bit(const uint8_t *buf, size_t len, VIEW_ENC_E enc, int max_width)
{
    int    x = 0;
    size_t i = 0;
    while (i < len) {
        uint8_t c = buf[i];
        if (c == '\n') {
            return i + 1;
        }
        if (c == '\r') {
            if (i + 1 < len && buf[i + 1] == '\n')
                return i + 2;
            return i + 1;
        }
        int    glyph_w = 0;
        size_t step    = 1;
        if (c < 0x80) {
            if (c < 0x20) {
                i += 1;
                continue;
            }
            glyph_w = Font24.Width;
            step    = 1;
        } else {
            glyph_w = 24;
            if (enc == VIEW_ENC_UTF8) {
                step = utf8_seq_len(c);
                if (i + step > len)
                    step = len - i;
            } else {
                step = (i + 2 <= len) ? 2 : 1;
            }
        }
        if (x > 0 && x + glyph_w > max_width) {
            return i;
        }
        x += glyph_w;
        i += step;
    }
    return i;
}

static INT64_T advance_lines_in_file(const char *path, INT64_T start_off, int lines, VIEW_ENC_E enc, int max_width)
{
    if (lines <= 0)
        return start_off;
    TUYA_FILE f = tkl_fopen(path, "r");
    if (!f)
        return start_off;
    if ((enc == VIEW_ENC_UTF16LE || enc == VIEW_ENC_UTF16BE) && (start_off & 1))
        start_off--;
    if (tkl_fseek(f, start_off, SEEK_SET) != 0) {
        tkl_fclose(f);
        return start_off;
    }

    uint8_t *win = (uint8_t *)tal_malloc(FILE_READ_WINDOW);
    if (!win) {
        tkl_fclose(f);
        return start_off;
    }
    int rd = tkl_fread(win, FILE_READ_WINDOW, f);
    if (rd < 0)
        rd = 0;
    if ((enc == VIEW_ENC_UTF16LE || enc == VIEW_ENC_UTF16BE) && (rd & 1))
        rd--;
    size_t pos = 0;
    for (int i = 0; i < lines && pos < (size_t)rd; i++) {
        size_t step = 0;
        if (enc == VIEW_ENC_UTF16LE)
            step = advance_one_line_in_buf_utf16(win + pos, (size_t)rd - pos, TRUE, max_width);
        else if (enc == VIEW_ENC_UTF16BE)
            step = advance_one_line_in_buf_utf16(win + pos, (size_t)rd - pos, FALSE, max_width);
        else
            step = advance_one_line_in_buf_8bit(win + pos, (size_t)rd - pos, enc, max_width);
        if (step == 0)
            break;
        pos += step;
    }
    tal_free(win);
    tkl_fclose(f);
    return start_off + (INT64_T)pos;
}

static void refresh_ui(void)
{
    PR_NOTICE("Refreshing UI...");

    if (DEV_Module_Init() != 0) {
        PR_ERR("E-Paper DEV_Module_Init failed");
        return;
    }

    EPD_4in26_Init();
    // EPD_4in26_Clear(); // Avoid full clear every time to speed up? Or maybe needed for ghosting.
    // For now, keep clear.

    // Allocate memory
    UBYTE  *BlackImage;
    UDOUBLE Imagesize =
        ((EPD_4in26_WIDTH % 8 == 0) ? (EPD_4in26_WIDTH / 8) : (EPD_4in26_WIDTH / 8 + 1)) * EPD_4in26_HEIGHT + 256;

    if ((BlackImage = (UBYTE *)tal_malloc(Imagesize)) == NULL) {
        PR_ERR("Failed to allocate memory...");
        return;
    }

    Paint_NewImage(BlackImage, EPD_4in26_WIDTH, EPD_4in26_HEIGHT, sg_app_ctx.rotate, WHITE);
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);

    if (sg_app_ctx.state == STATE_FILE_LIST) {
        // Draw Title
        char title[96];
        snprintf(title, sizeof(title), "%s (%d/%d)", sg_app_ctx.current_path, sg_app_ctx.current_page + 1,
                 sg_app_ctx.total_pages);
        char time_brand[80];
        build_time_brand_line(time_brand, sizeof(time_brand));
        int time_w = gbk_pixel_width(time_brand);
        int time_x = (int)Paint.Width - 10 - time_w;
        if (time_x < 10) {
            time_x = 10;
        }
        Paint_DrawString_CN_HZK24((UWORD)time_x, 10, time_brand, BLACK, WHITE);

        int title_max_px = time_x - 20;
        if (title_max_px < 24) {
            title_max_px = 24;
        }
        char title_clipped[96];
        clip_name_to_px(title_clipped, sizeof(title_clipped), title, title_max_px);
        Paint_DrawString_EN(10, 10, title_clipped, &Font24, BLACK, WHITE);
        Paint_DrawLine(10, 35, Paint.Width - 10, 35, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

        int y_pos = 45;
        for (int i = 0; i < sg_app_ctx.item_count_in_page; i++) {
            UWORD fg = BLACK;
            UWORD bg = WHITE;

            // Highlight selected
            if (i == sg_app_ctx.selected_index) {
                fg = WHITE;
                bg = BLACK;
                Paint_DrawRectangle(5, y_pos - 2, Paint.Width - 5, y_pos + 26, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            }

            char size_str[16];
            if (sg_app_ctx.files[i].is_dir)
                snprintf(size_str, sizeof(size_str), "DIR");
            else
                format_size_human(size_str, sizeof(size_str), sg_app_ctx.files[i].size);
            int size_px = (int)strlen(size_str) * (int)Font24.Width;
            int size_x  = (int)Paint.Width - 10 - size_px;
            if (size_x < 10)
                size_x = 10;

            char        display_name[140];
            const char *name = sg_app_ctx.files[i].name;
            if (sg_app_ctx.files[i].is_dir) {
                snprintf(display_name, sizeof(display_name), "%s/", name);
                name = display_name;
            }
            int name_max_px = size_x - 10 - Font24.Width;
            if (name_max_px < 24)
                name_max_px = 24;
            char clipped[140];
            clip_name_to_px(clipped, sizeof(clipped), name, name_max_px);
            name         = clipped;
            int is_ascii = 1;
            for (int j = 0; name[j]; j++) {
                if ((unsigned char)name[j] >= 0x80) {
                    is_ascii = 0;
                    break;
                }
            }

            if (is_ascii) {
                Paint_DrawString_EN(10, y_pos, name, &Font24, fg, bg);
            } else {
                Paint_DrawString_CN_HZK24(10, y_pos, name, fg, bg);
            }
            Paint_DrawString_EN((UWORD)size_x, y_pos, size_str, &Font24, fg, bg);

            y_pos += LIST_LINE_HEIGHT;
        }

        if (sg_app_ctx.item_count_in_page == 0) {
            Paint_DrawString_EN(10, 50, "No files found", &Font24, BLACK, WHITE);
        }
        Paint_DrawString_EN(10, Paint.Height - 28, "MID:Open  RST:Up  RST(hold):BD", &Font24, BLACK, WHITE);
    } else if (sg_app_ctx.state == STATE_SHOW_FILE) {
        int header_y      = 10;
        int header_line_y = 35;
        int footer_h      = 28;
        int footer_y      = (Paint.Height > footer_h) ? (Paint.Height - footer_h) : 0;
        int content_y     = header_line_y + 6;
        int content_h     = footer_y - content_y - 2;
        if (content_h < TEXT_LINE_HEIGHT)
            content_h = TEXT_LINE_HEIGHT;

        char time_hhmm[6];
        format_time_hhmm(time_hhmm);

        int percent = 0;
        if (sg_app_ctx.view_kind == VIEW_TEXT && sg_app_ctx.viewing_size > 0) {
            percent = (int)((sg_app_ctx.viewing_offset * 100) / sg_app_ctx.viewing_size);
            if (percent < 0)
                percent = 0;
            if (percent > 100)
                percent = 100;
        } else if (sg_app_ctx.view_kind == VIEW_IMAGE) {
            percent = 100;
        }

        int cur_page    = 1;
        int total_pages = 1;
        int max_px      = (int)Paint.Width - 20;

        int max_w          = Paint.Width - 2 * TEXT_MARGIN_X;
        int lines_per_page = content_h / TEXT_LINE_HEIGHT;
        if (lines_per_page < 1)
            lines_per_page = 1;
        if (sg_app_ctx.view_kind == VIEW_TEXT) {
            INT64_T end_off = advance_lines_in_file(sg_app_ctx.viewing_file, sg_app_ctx.viewing_offset, lines_per_page,
                                                    sg_app_ctx.viewing_enc, max_w);
            INT64_T bytes_per_page = end_off - sg_app_ctx.viewing_offset;
            if (bytes_per_page <= 0)
                bytes_per_page = 1;
            if (sg_app_ctx.viewing_size > 0) {
                total_pages = (int)((sg_app_ctx.viewing_size + bytes_per_page - 1) / bytes_per_page);
                if (total_pages < 1)
                    total_pages = 1;
            }
            cur_page = sg_app_ctx.page_hist_len + 1;
            if (cur_page < 1)
                cur_page = 1;
            if (total_pages < cur_page)
                total_pages = cur_page;
        }

        char header[160];
        build_preview_header(header, sizeof(header), max_px, sg_app_ctx.viewing_file, cur_page, total_pages, percent,
                             time_hhmm);
        Paint_DrawString_CN_HZK24(10, header_y, header, BLACK, WHITE);
        Paint_DrawLine(10, header_line_y, Paint.Width - 10, header_line_y, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

        if (sg_app_ctx.view_kind == VIEW_IMAGE) {
            int x = 0;
            int y = content_y;
            int w = Paint.Width;
            int h = content_h;
            if (display_image_1bit(sg_app_ctx.viewing_file, x, y, w, h) != 0) {
                Paint_DrawString_EN(10, 50, "Image decode failed/unsupported", &Font24, BLACK, WHITE);
            }
        } else {
            int avail_h = content_h;

            if (ext_eq(file_ext(sg_app_ctx.viewing_file), "pdf")) {
                char        msg1[96];
                char        msg2[96];
                const char *bn = path_basename(sg_app_ctx.viewing_file);
                snprintf(msg1, sizeof(msg1), "PDF: put pages as images in");
                snprintf(msg2, sizeof(msg2), "%s_pages/", bn ? bn : "file");
                Paint_DrawString_EN(TEXT_MARGIN_X, content_y, msg1, &Font24, BLACK, WHITE);
                Paint_DrawString_EN(TEXT_MARGIN_X, content_y + TEXT_LINE_HEIGHT, msg2, &Font24, BLACK, WHITE);
            } else {
                INT64_T end_off = advance_lines_in_file(sg_app_ctx.viewing_file, sg_app_ctx.viewing_offset,
                                                        lines_per_page, sg_app_ctx.viewing_enc, max_w);
                if (end_off < sg_app_ctx.viewing_offset)
                    end_off = sg_app_ctx.viewing_offset;
                INT64_T need = end_off - sg_app_ctx.viewing_offset;
                if (need < 0)
                    need = 0;
                if (need > (INT64_T)FILE_READ_WINDOW)
                    need = (INT64_T)FILE_READ_WINDOW;

                TUYA_FILE f = tkl_fopen(sg_app_ctx.viewing_file, "r");
                if (f) {
                    if (tkl_fseek(f, sg_app_ctx.viewing_offset, SEEK_SET) == 0) {
                        char *raw = (char *)tal_malloc((size_t)need + 1);
                        if (raw) {
                            int rd = tkl_fread(raw, (int)need, f);
                            if (rd < 0)
                                rd = 0;
                            raw[rd] = 0;
                            if (sg_app_ctx.viewing_enc == VIEW_ENC_UTF8) {
                                int   gbk_len = rd * 2 + 2;
                                char *gbk     = (char *)tal_malloc(gbk_len);
                                if (gbk) {
                                    int out_len = utf8_to_gbk_buf((uint8_t *)raw, rd, (uint8_t *)gbk, gbk_len - 1);
                                    if (out_len < 0)
                                        out_len = 0;
                                    gbk[out_len] = 0;
                                    Paint_DrawText_CN_HZK24_Adaptive(TEXT_MARGIN_X, content_y, max_w, avail_h, gbk,
                                                                     BLACK, WHITE);
                                    tal_free(gbk);
                                } else {
                                    Paint_DrawString_EN(10, 50, "Memory Error", &Font24, BLACK, WHITE);
                                }
                            } else if (sg_app_ctx.viewing_enc == VIEW_ENC_UTF16LE ||
                                       sg_app_ctx.viewing_enc == VIEW_ENC_UTF16BE) {
                                int      utf8_cap = rd * 2 + 4;
                                uint8_t *utf8     = (uint8_t *)tal_malloc(utf8_cap);
                                if (utf8) {
                                    int utf8_len = utf16_to_utf8_buf(
                                        (uint8_t *)raw, rd, (sg_app_ctx.viewing_enc == VIEW_ENC_UTF16LE) ? TRUE : FALSE,
                                        utf8, utf8_cap - 1);
                                    if (utf8_len < 0)
                                        utf8_len = 0;
                                    utf8[utf8_len] = 0;
                                    int   gbk_len  = utf8_len * 2 + 2;
                                    char *gbk      = (char *)tal_malloc(gbk_len);
                                    if (gbk) {
                                        int out_len = utf8_to_gbk_buf(utf8, (size_t)utf8_len, (uint8_t *)gbk,
                                                                      (size_t)gbk_len - 1);
                                        if (out_len < 0)
                                            out_len = 0;
                                        gbk[out_len] = 0;
                                        Paint_DrawText_CN_HZK24_Adaptive(TEXT_MARGIN_X, content_y, max_w, avail_h, gbk,
                                                                         BLACK, WHITE);
                                        tal_free(gbk);
                                    } else {
                                        Paint_DrawString_EN(10, 50, "Memory Error", &Font24, BLACK, WHITE);
                                    }
                                    tal_free(utf8);
                                } else {
                                    Paint_DrawString_EN(10, 50, "Memory Error", &Font24, BLACK, WHITE);
                                }
                            } else {
                                Paint_DrawText_CN_HZK24_Adaptive(TEXT_MARGIN_X, content_y, max_w, avail_h, raw, BLACK,
                                                                 WHITE);
                            }
                            tal_free(raw);
                        } else {
                            Paint_DrawString_EN(10, 50, "Memory Error", &Font24, BLACK, WHITE);
                        }
                    }
                    tkl_fclose(f);
                } else {
                    Paint_DrawString_EN(10, 50, "Error opening file.", &Font24, BLACK, WHITE);
                }
            }

            char status[96];
            if (Paint.Width < 600) {
                snprintf(status, sizeof(status), "UP/DN  LT/RT  SET  RST  RST(hold)BD  %d%%", percent);
            } else {
                snprintf(status, sizeof(status), "UP/DN line  LT/RT page  SET rot  RST back  RST(hold)BD  %d%%",
                         percent);
            }
            Paint_DrawString_EN(10, footer_y, status, &Font24, BLACK, WHITE);
        }
    } else if (sg_app_ctx.state == STATE_BD_AUTH) {
        Paint_DrawString_EN(10, 10, "Baidu NetDisk", &Font24, BLACK, WHITE);
        Paint_DrawLine(10, 35, Paint.Width - 10, 35, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        char qr_url[256], v_url[128], u_code[20];
        memset(qr_url, 0, sizeof(qr_url));
        memset(v_url, 0, sizeof(v_url));
        memset(u_code, 0, sizeof(u_code));
        bdndk_auth_info_get(qr_url, sizeof(qr_url), v_url, sizeof(v_url), u_code, sizeof(u_code));
        char msg[96];
        bdndk_message_get(msg, sizeof(msg));
        if (msg[0]) {
            Paint_DrawString_EN(10, 42, msg, &Font24, BLACK, WHITE);
        }
        if (v_url[0]) {
            Paint_DrawString_EN(10, 70, "URL:", &Font24, BLACK, WHITE);
            Paint_DrawString_EN(10, 98, v_url, &Font24, BLACK, WHITE);
        }
        if (u_code[0]) {
            char code_line[32];
            snprintf(code_line, sizeof(code_line), "CODE: %s", u_code);
            Paint_DrawString_EN(10, 126, code_line, &Font24, BLACK, WHITE);
        }
        int qr_x = 10;
        int qr_y = 160;
        int qr_w = Paint.Width - 20;
        int qr_h = Paint.Height - qr_y - 10;
        if (qr_w > 240)
            qr_w = 240;
        if (qr_h > 240)
            qr_h = 240;
        if (qr_url[0] && qr_w > 60 && qr_h > 60) {
            draw_qrcode(qr_x, qr_y, qr_w, qr_h, qr_url);
        } else {
            Paint_DrawString_EN(10, 160, "QR: waiting...", &Font24, BLACK, WHITE);
        }
        Paint_DrawString_EN(10, Paint.Height - 28, "RST(hold): SD", &Font24, BLACK, WHITE);
    } else if (sg_app_ctx.state == STATE_BD_LIST) {
        int total = bdndk_list_count();
        int items = sg_app_ctx.items_per_page;
        if (items < 1)
            items = 1;
        int total_pages = (total + items - 1) / items;
        if (total_pages < 1)
            total_pages = 1;
        if (sg_bd_page < 0)
            sg_bd_page = 0;
        if (sg_bd_page > total_pages - 1)
            sg_bd_page = total_pages - 1;

        int page_start = sg_bd_page * items;
        int page_count = total - page_start;
        if (page_count < 0)
            page_count = 0;
        if (page_count > items)
            page_count = items;
        if (page_count == 0)
            sg_bd_selected = 0;
        if (sg_bd_selected < 0)
            sg_bd_selected = 0;
        if (sg_bd_selected > page_count - 1)
            sg_bd_selected = page_count - 1;

        char title[64];
        snprintf(title, sizeof(title), "/TuyaT5AI (%d/%d)", sg_bd_page + 1, total_pages);
        Paint_DrawString_EN(10, 10, title, &Font24, BLACK, WHITE);
        Paint_DrawLine(10, 35, Paint.Width - 10, 35, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

        int y_pos = 45;
        for (int i = 0; i < page_count; i++) {
            UWORD fg = BLACK;
            UWORD bg = WHITE;
            if (i == sg_bd_selected) {
                fg = WHITE;
                bg = BLACK;
                Paint_DrawRectangle(5, y_pos - 2, Paint.Width - 5, y_pos + 26, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            }

            BDNDK_FILE_T fi;
            memset(&fi, 0, sizeof(fi));
            if (!bdndk_list_get(page_start + i, &fi)) {
                y_pos += LIST_LINE_HEIGHT;
                continue;
            }

            char size_str[16];
            if (fi.is_dir)
                snprintf(size_str, sizeof(size_str), "DIR");
            else
                format_size_human(size_str, sizeof(size_str), fi.size);
            int size_px = (int)strlen(size_str) * (int)Font24.Width;
            int size_x  = (int)Paint.Width - 10 - size_px;
            if (size_x < 10)
                size_x = 10;

            const char *name = fi.name;
            char        name_utf8[128];
            strncpy(name_utf8, name, sizeof(name_utf8) - 1);
            name_utf8[sizeof(name_utf8) - 1] = 0;
            if (fi.is_dir) {
                size_t ln = strlen(name_utf8);
                if (ln + 2 < sizeof(name_utf8)) {
                    name_utf8[ln]     = '/';
                    name_utf8[ln + 1] = 0;
                }
            }

            int ascii = 1;
            for (int j = 0; name_utf8[j]; j++) {
                if ((uint8_t)name_utf8[j] >= 0x80) {
                    ascii = 0;
                    break;
                }
            }

            if (ascii) {
                char clipped[140];
                int  name_max_px = size_x - 10 - Font24.Width;
                if (name_max_px < 24)
                    name_max_px = 24;
                clip_name_to_px(clipped, sizeof(clipped), name_utf8, name_max_px);
                Paint_DrawString_EN(10, y_pos, clipped, &Font24, fg, bg);
            } else {
                uint8_t gbk[256];
                int     out_len = utf8_to_gbk_buf((uint8_t *)name_utf8, strlen(name_utf8), gbk, sizeof(gbk) - 1);
                if (out_len < 0)
                    out_len = 0;
                gbk[out_len] = 0;
                char clipped[140];
                int  name_max_px = size_x - 10 - Font24.Width;
                if (name_max_px < 24)
                    name_max_px = 24;
                clip_name_to_px(clipped, sizeof(clipped), (char *)gbk, name_max_px);
                Paint_DrawString_CN_HZK24(10, y_pos, clipped, fg, bg);
            }

            Paint_DrawString_EN((UWORD)size_x, y_pos, size_str, &Font24, fg, bg);
            y_pos += LIST_LINE_HEIGHT;
        }

        if (page_count == 0) {
            char msg[96];
            bdndk_message_get(msg, sizeof(msg));
            if (msg[0])
                Paint_DrawString_EN(10, 50, msg, &Font24, BLACK, WHITE);
            else
                Paint_DrawString_EN(10, 50, "No files", &Font24, BLACK, WHITE);
        }

        Paint_DrawString_EN(10, Paint.Height - 28, "MID:Detail RST:Back RST(hold):SD", &Font24, BLACK, WHITE);
    } else if (sg_app_ctx.state == STATE_BD_DETAIL) {
        BDNDK_FILE_T fi;
        memset(&fi, 0, sizeof(fi));
        bdndk_detail_get(&fi);
        BDNDK_DETAIL_META_T meta;
        memset(&meta, 0, sizeof(meta));
        bdndk_detail_meta_get(&meta);

        Paint_DrawString_EN(10, 10, "Baidu File Detail", &Font24, BLACK, WHITE);
        Paint_DrawLine(10, 35, Paint.Width - 10, 35, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        int  y      = 45;
        int  max_px = (int)Paint.Width - 20;
        char clipped[160];

        Paint_DrawString_EN(10, y, "Name:", &Font24, BLACK, WHITE);
        y += 28;
        clip_name_to_px(clipped, sizeof(clipped), fi.name, max_px);
        Paint_DrawString_EN(10, y, clipped, &Font24, BLACK, WHITE);
        y += 28;

        char size_str[16];
        if (fi.is_dir)
            snprintf(size_str, sizeof(size_str), "DIR");
        else
            format_size_human(size_str, sizeof(size_str), fi.size);
        char size_line[64];
        snprintf(size_line, sizeof(size_line), "Size: %s", size_str);
        Paint_DrawString_EN(10, y, size_line, &Font24, BLACK, WHITE);
        y += 28;

        char fsid_line[96];
        snprintf(fsid_line, sizeof(fsid_line), "FSID: %s", fi.fsid[0] ? fi.fsid : "--");
        Paint_DrawString_EN(10, y, fsid_line, &Font24, BLACK, WHITE);
        y += 28;

        char ctime_line[64];
        snprintf(ctime_line, sizeof(ctime_line), "CTime: %lld", (long long)meta.ctime);
        Paint_DrawString_EN(10, y, ctime_line, &Font24, BLACK, WHITE);
        y += 28;

        char mtime_line[64];
        snprintf(mtime_line, sizeof(mtime_line), "MTime: %lld", (long long)meta.mtime);
        Paint_DrawString_EN(10, y, mtime_line, &Font24, BLACK, WHITE);
        y += 28;

        char md5_line[96];
        snprintf(md5_line, sizeof(md5_line), "MD5: %s", meta.md5[0] ? meta.md5 : "--");
        Paint_DrawString_EN(10, y, md5_line, &Font24, BLACK, WHITE);
        y += 28;

        char dtitle[64];
        snprintf(dtitle, sizeof(dtitle), "DLink(%d):", sg_bd_detail_url_offset);
        Paint_DrawString_EN(10, y, dtitle, &Font24, BLACK, WHITE);
        y += 28;
        draw_wrapped_text_en_offset(10, y, (int)Paint.Width - 20, 2, meta.dlink[0] ? meta.dlink : "--",
                                    sg_bd_detail_url_offset, NULL);

        Paint_DrawString_EN(10, Paint.Height - 28, "MID:DL RST:Back RST(hold):SD", &Font24, BLACK, WHITE);
    } else if (sg_app_ctx.state == STATE_BD_MSG) {
        Paint_DrawString_EN(10, 10, "Baidu NetDisk", &Font24, BLACK, WHITE);
        Paint_DrawLine(10, 35, Paint.Width - 10, 35, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        char msg[96];
        bdndk_message_get(msg, sizeof(msg));
        if (!msg[0])
            strncpy(msg, "Working...", sizeof(msg) - 1);
        msg[sizeof(msg) - 1] = 0;
        Paint_DrawString_EN(10, 50, msg, &Font24, BLACK, WHITE);
        int pct = -1;
        bdndk_work_progress_get(&pct);
        if (pct >= 0) {
            char pstr[24];
            snprintf(pstr, sizeof(pstr), "%d%%", pct);
            Paint_DrawString_EN(10, 78, pstr, &Font24, BLACK, WHITE);
        } else if (bdndk_work_get() == BDNDK_WORK_ERR) {
            int  err_no = 0;
            char err_msg[96];
            char req_id[64];
            memset(err_msg, 0, sizeof(err_msg));
            memset(req_id, 0, sizeof(req_id));
            bdndk_last_error_get(&err_no, err_msg, sizeof(err_msg), req_id, sizeof(req_id));
            if (err_no != 0) {
                char line[64];
                snprintf(line, sizeof(line), "Errno: %d", err_no);
                Paint_DrawString_EN(10, 78, line, &Font24, BLACK, WHITE);
            }
            if (req_id[0]) {
                char line[96];
                snprintf(line, sizeof(line), "Req: %s", req_id);
                Paint_DrawString_EN(10, 106, line, &Font24, BLACK, WHITE);
            } else if (err_msg[0]) {
                char line[96];
                snprintf(line, sizeof(line), "Msg: %.64s", err_msg);
                Paint_DrawString_EN(10, 106, line, &Font24, BLACK, WHITE);
            }
            char url[1024];
            bdndk_last_download_url_get(url, sizeof(url));
            if (url[0]) {
                int  url_y     = 134;
                int  max_lines = (Paint.Height - 28 - url_y) / 28;
                char title[64];
                snprintf(title, sizeof(title), "URL(%d):", sg_bd_msg_url_offset);
                Paint_DrawString_EN(10, url_y, title, &Font24, BLACK, WHITE);
                url_y += 28;
                draw_wrapped_text_en_offset(10, url_y, (int)Paint.Width - 20, max_lines - 1, url, sg_bd_msg_url_offset,
                                            NULL);
            }
        }
        Paint_DrawString_EN(10, Paint.Height - 28, "MID:Back RST:Back RST(hold):SD", &Font24, BLACK, WHITE);
    }

    EPD_4in26_Display(BlackImage);
    tal_free(BlackImage);
    EPD_4in26_Sleep();
}

static void handle_button_press(const char *name, TDL_BUTTON_TOUCH_EVENT_E event)
{
    BOOL_T changed              = FALSE;
    BOOL_T save_progress_needed = FALSE;

    if (sg_app_ctx.state == STATE_FILE_LIST) {
        if (strcmp(name, "UP") == 0) {
            if (sg_app_ctx.selected_index > 0) {
                sg_app_ctx.selected_index--;
                changed = TRUE;
            } else if (sg_app_ctx.item_count_in_page > 0) {
                sg_app_ctx.selected_index = sg_app_ctx.item_count_in_page - 1;
                changed                   = TRUE;
            }
        } else if (strcmp(name, "DOWN") == 0) {
            if (sg_app_ctx.selected_index < sg_app_ctx.item_count_in_page - 1) {
                sg_app_ctx.selected_index++;
                changed = TRUE;
            } else {
                sg_app_ctx.selected_index = 0;
                changed                   = TRUE;
            }
        } else if (strcmp(name, "LEFT") == 0) {
            if (sg_app_ctx.current_page > 0) {
                sg_app_ctx.current_page--;
                scan_files();
                changed = TRUE;
            }
        } else if (strcmp(name, "RIGHT") == 0) {
            if (sg_app_ctx.current_page < sg_app_ctx.total_pages - 1) {
                sg_app_ctx.current_page++;
                scan_files();
                changed = TRUE;
            }
        } else if (strcmp(name, "MID") == 0) {
            if (sg_app_ctx.item_count_in_page > 0) {
                FILE_ITEM_T *it = &sg_app_ctx.files[sg_app_ctx.selected_index];
                if (it->is_dir) {
                    char next_path[256];
                    path_join(next_path, sizeof(next_path), sg_app_ctx.current_path, it->name);
                    strncpy(sg_app_ctx.current_path, next_path, sizeof(sg_app_ctx.current_path) - 1);
                    sg_app_ctx.current_path[sizeof(sg_app_ctx.current_path) - 1] = 0;
                    sg_app_ctx.current_page                                      = 0;
                    sg_app_ctx.selected_index                                    = 0;
                    scan_files();
                    changed = TRUE;
                } else {
                    sg_app_ctx.state = STATE_SHOW_FILE;
                    open_item_for_view();
                    changed = TRUE;
                }
            }
        } else if (strcmp(name, "RST") == 0) {
            if (event == TDL_BUTTON_LONG_PRESS_START) {
                sg_app_ctx.state        = STATE_BD_AUTH;
                sg_bd_page              = 0;
                sg_bd_selected          = 0;
                sg_bd_detail_index      = -1;
                sg_bd_msg_url_offset    = 0;
                sg_bd_detail_url_offset = 0;
                bdndk_storage_set(sg_sd_mounted, SDCARD_MOUNT_PATH "/TuyaT5AI");
                bdndk_start();
                changed = TRUE;
            } else {
                if (!path_is_root(sg_app_ctx.current_path)) {
                    path_to_parent(sg_app_ctx.current_path, sizeof(sg_app_ctx.current_path));
                    sg_app_ctx.current_page   = 0;
                    sg_app_ctx.selected_index = 0;
                    scan_files();
                    changed = TRUE;
                }
            }
        } else if (strcmp(name, "SET") == 0) {
            sg_app_ctx.rotate = (sg_app_ctx.rotate == ROTATE_0) ? ROTATE_90 : ROTATE_0;
            update_items_per_page();
            sg_app_ctx.current_page   = 0;
            sg_app_ctx.selected_index = 0;
            scan_files();
            changed = TRUE;
        }
    } else if (sg_app_ctx.state == STATE_SHOW_FILE) {
        if (strcmp(name, "RST") == 0) {
            if (event == TDL_BUTTON_LONG_PRESS_START) {
                save_progress_needed    = TRUE;
                sg_app_ctx.state        = STATE_BD_AUTH;
                sg_bd_page              = 0;
                sg_bd_selected          = 0;
                sg_bd_detail_index      = -1;
                sg_bd_msg_url_offset    = 0;
                sg_bd_detail_url_offset = 0;
                bdndk_storage_set(sg_sd_mounted, SDCARD_MOUNT_PATH "/TuyaT5AI");
                bdndk_start();
                changed = TRUE;
            } else {
                save_progress_needed = TRUE;
                sg_app_ctx.state     = STATE_FILE_LIST;
                changed              = TRUE;
            }
        } else if (strcmp(name, "SET") == 0) {
            sg_app_ctx.rotate = (sg_app_ctx.rotate == ROTATE_0) ? ROTATE_90 : ROTATE_0;
            update_items_per_page();
            save_progress_needed = TRUE;
            changed              = TRUE;
        } else if (sg_app_ctx.view_kind == VIEW_TEXT) {
            int screen_w =
                (sg_app_ctx.rotate == ROTATE_0 || sg_app_ctx.rotate == ROTATE_180) ? EPD_4in26_WIDTH : EPD_4in26_HEIGHT;
            int screen_h =
                (sg_app_ctx.rotate == ROTATE_0 || sg_app_ctx.rotate == ROTATE_180) ? EPD_4in26_HEIGHT : EPD_4in26_WIDTH;
            int header_line_y = 35;
            int footer_h      = 28;
            int content_y     = header_line_y + 6;
            int content_h     = screen_h - content_y - footer_h - 2;
            if (content_h < TEXT_LINE_HEIGHT)
                content_h = TEXT_LINE_HEIGHT;
            int max_w          = screen_w - 2 * TEXT_MARGIN_X;
            int lines_per_page = content_h / TEXT_LINE_HEIGHT;
            if (lines_per_page < 1)
                lines_per_page = 1;

            if (strcmp(name, "DOWN") == 0) {
                if (sg_app_ctx.line_hist_len < LINE_HISTORY_DEPTH) {
                    sg_app_ctx.line_history[sg_app_ctx.line_hist_len++] = sg_app_ctx.viewing_offset;
                }
                INT64_T next = advance_lines_in_file(sg_app_ctx.viewing_file, sg_app_ctx.viewing_offset, 1,
                                                     sg_app_ctx.viewing_enc, max_w);
                if (next > sg_app_ctx.viewing_offset) {
                    sg_app_ctx.viewing_offset = next;
                    save_progress_needed      = TRUE;
                    changed                   = TRUE;
                } else if (sg_app_ctx.line_hist_len > 0) {
                    sg_app_ctx.line_hist_len--;
                }
            } else if (strcmp(name, "UP") == 0) {
                if (sg_app_ctx.line_hist_len > 0) {
                    sg_app_ctx.viewing_offset = sg_app_ctx.line_history[--sg_app_ctx.line_hist_len];
                    save_progress_needed      = TRUE;
                    changed                   = TRUE;
                }
            } else if (strcmp(name, "RIGHT") == 0) {
                if (sg_app_ctx.page_hist_len < PAGE_HISTORY_DEPTH) {
                    sg_app_ctx.page_history[sg_app_ctx.page_hist_len++] = sg_app_ctx.viewing_offset;
                }
                sg_app_ctx.line_hist_len = 0;
                INT64_T next = advance_lines_in_file(sg_app_ctx.viewing_file, sg_app_ctx.viewing_offset, lines_per_page,
                                                     sg_app_ctx.viewing_enc, max_w);
                if (next > sg_app_ctx.viewing_offset) {
                    sg_app_ctx.viewing_offset = next;
                    save_progress_needed      = TRUE;
                    changed                   = TRUE;
                } else if (sg_app_ctx.page_hist_len > 0) {
                    sg_app_ctx.page_hist_len--;
                }
            } else if (strcmp(name, "LEFT") == 0) {
                sg_app_ctx.line_hist_len = 0;
                if (sg_app_ctx.page_hist_len > 0) {
                    sg_app_ctx.viewing_offset = sg_app_ctx.page_history[--sg_app_ctx.page_hist_len];
                    save_progress_needed      = TRUE;
                    changed                   = TRUE;
                }
            }
        }
    } else if (sg_app_ctx.state == STATE_BD_AUTH || sg_app_ctx.state == STATE_BD_LIST ||
               sg_app_ctx.state == STATE_BD_DETAIL || sg_app_ctx.state == STATE_BD_MSG) {
        int items = sg_app_ctx.items_per_page;
        if (items < 1)
            items = 1;
        int total       = bdndk_list_count();
        int total_pages = (total + items - 1) / items;
        if (total_pages < 1)
            total_pages = 1;
        if (sg_bd_page < 0)
            sg_bd_page = 0;
        if (sg_bd_page > total_pages - 1)
            sg_bd_page = total_pages - 1;

        if (strcmp(name, "RST") == 0) {
            if (event == TDL_BUTTON_LONG_PRESS_START) {
                bdndk_stop();
                sg_app_ctx.state = STATE_FILE_LIST;
                scan_files();
                changed = TRUE;
            } else {
                if (sg_app_ctx.state == STATE_BD_DETAIL || sg_app_ctx.state == STATE_BD_MSG) {
                    sg_app_ctx.state        = STATE_BD_LIST;
                    sg_bd_detail_url_offset = 0;
                    changed                 = TRUE;
                } else if (sg_app_ctx.state == STATE_BD_LIST) {
                    sg_app_ctx.state = STATE_BD_AUTH;
                    changed          = TRUE;
                }
            }
        } else if (sg_app_ctx.state == STATE_BD_LIST) {
            int page_start = sg_bd_page * items;
            int page_count = total - page_start;
            if (page_count < 0)
                page_count = 0;
            if (page_count > items)
                page_count = items;
            if (page_count == 0)
                sg_bd_selected = 0;
            if (sg_bd_selected < 0)
                sg_bd_selected = 0;
            if (sg_bd_selected > page_count - 1)
                sg_bd_selected = page_count - 1;

            if (strcmp(name, "UP") == 0) {
                if (page_count > 0) {
                    sg_bd_selected = (sg_bd_selected > 0) ? (sg_bd_selected - 1) : (page_count - 1);
                    changed        = TRUE;
                }
            } else if (strcmp(name, "DOWN") == 0) {
                if (page_count > 0) {
                    sg_bd_selected = (sg_bd_selected < page_count - 1) ? (sg_bd_selected + 1) : 0;
                    changed        = TRUE;
                }
            } else if (strcmp(name, "LEFT") == 0) {
                if (sg_bd_page > 0) {
                    sg_bd_page--;
                    sg_bd_selected = 0;
                    changed        = TRUE;
                }
            } else if (strcmp(name, "RIGHT") == 0) {
                if (sg_bd_page < total_pages - 1) {
                    sg_bd_page++;
                    sg_bd_selected = 0;
                    changed        = TRUE;
                }
            } else if (strcmp(name, "MID") == 0) {
                int idx                 = page_start + sg_bd_selected;
                sg_bd_detail_index      = idx;
                sg_bd_detail_url_offset = 0;
                bdndk_request_detail(idx);
                sg_app_ctx.state = STATE_BD_MSG;
                changed          = TRUE;
            }
        } else if (sg_app_ctx.state == STATE_BD_DETAIL) {
            if (strcmp(name, "MID") == 0 && event == TDL_BUTTON_PRESS_DOWN) {
                if (sg_bd_detail_index >= 0) {
                    bdndk_request_download(sg_bd_detail_index, SDCARD_MOUNT_PATH "/TuyaT5AI");
                    sg_app_ctx.state = STATE_BD_MSG;
                } else {
                    sg_app_ctx.state = STATE_BD_LIST;
                }
                changed = TRUE;
            } else if (strcmp(name, "DOWN") == 0) {
                sg_bd_detail_url_offset += 200;
                changed = TRUE;
            } else if (strcmp(name, "UP") == 0) {
                sg_bd_detail_url_offset -= 200;
                if (sg_bd_detail_url_offset < 0)
                    sg_bd_detail_url_offset = 0;
                changed = TRUE;
            }
        } else if (sg_app_ctx.state == STATE_BD_MSG) {
            if (strcmp(name, "MID") == 0 && event == TDL_BUTTON_PRESS_DOWN) {
                sg_app_ctx.state     = STATE_BD_LIST;
                sg_bd_msg_url_offset = 0;
                changed              = TRUE;
            } else if (strcmp(name, "DOWN") == 0) {
                sg_bd_msg_url_offset += 200;
                changed = TRUE;
            } else if (strcmp(name, "UP") == 0) {
                sg_bd_msg_url_offset -= 200;
                if (sg_bd_msg_url_offset < 0)
                    sg_bd_msg_url_offset = 0;
                changed = TRUE;
            }
        }
    }

    if (save_progress_needed && sg_app_ctx.viewing_file[0]) {
        progress_save(sg_app_ctx.viewing_file, sg_app_ctx.view_kind, sg_app_ctx.rotate, sg_app_ctx.viewing_offset);
    }
    if (changed) {
        sg_app_ctx.need_refresh = TRUE;
    }
}

static void button_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    (void)argc;
    if (event != TDL_BUTTON_PRESS_DOWN && event != TDL_BUTTON_LONG_PRESS_START)
        return;
    if (!name || !name[0])
        return;
    if (sg_btn_pending)
        return;
    strncpy(sg_btn_pending_name, name, sizeof(sg_btn_pending_name) - 1);
    sg_btn_pending_name[sizeof(sg_btn_pending_name) - 1] = 0;
    sg_btn_pending_event                                 = event;
    sg_btn_pending                                       = TRUE;
}

static void init_buttons(void)
{
    TDL_BUTTON_CFG_T config = {.long_start_valid_time     = 2000,
                               .long_keep_timer           = 500,
                               .button_debounce_time      = 50,
                               .button_repeat_valid_count = 2,
                               .button_repeat_valid_time  = 500};

    BUTTON_GPIO_CFG_T gpio_cfg = {
        .level = BUTTON_ACTIVE_LEVEL, .mode = BUTTON_TIMER_SCAN_MODE, .pin_type.gpio_pull = TUYA_GPIO_PULLUP};

    struct {
        char              *name;
        TUYA_GPIO_NUM_E    pin;
        TDL_BUTTON_HANDLE *hdl;
    } btns[] = {{"UP", GPIO_PIN_UP, &hdl_up},       {"DOWN", GPIO_PIN_DOWN, &hdl_down},
                {"LEFT", GPIO_PIN_LEFT, &hdl_left}, {"RIGHT", GPIO_PIN_RIGHT, &hdl_right},
                {"MID", GPIO_PIN_MID, &hdl_mid},    {"SET", GPIO_PIN_SET, &hdl_set},
                {"RST", GPIO_PIN_RST, &hdl_rst}};

    for (int i = 0; i < 7; i++) {
        gpio_cfg.pin = btns[i].pin;
        tdd_gpio_button_register(btns[i].name, &gpio_cfg);
        tdl_button_create(btns[i].name, &config, btns[i].hdl);
        tdl_button_event_register(*btns[i].hdl, TDL_BUTTON_PRESS_DOWN, button_cb);
        if (strcmp(btns[i].name, "MID") == 0 || strcmp(btns[i].name, "RST") == 0) {
            tdl_button_event_register(*btns[i].hdl, TDL_BUTTON_LONG_PRESS_START, button_cb);
        }
    }
}

static void __tuya_main_task(void *param)
{
// Pinmux config
#if defined(EBABLE_SD_PINMUX) && (EBABLE_SD_PINMUX == 1)
    tkl_io_pinmux_config(SD_CLK_PIN, TUYA_SDIO_HOST_CLK);
    tkl_io_pinmux_config(SD_CMD_PIN, TUYA_SDIO_HOST_CMD);
    tkl_io_pinmux_config(SD_D0_PIN, TUYA_SDIO_HOST_D0);
    tkl_io_pinmux_config(SD_D1_PIN, TUYA_SDIO_HOST_D1);
    tkl_io_pinmux_config(SD_D2_PIN, TUYA_SDIO_HOST_D2);
    tkl_io_pinmux_config(SD_D3_PIN, TUYA_SDIO_HOST_D3);
#endif

    // Init Keys
    init_buttons();

    // Mount SD
    int         retry = 0;
    OPERATE_RET mret  = OPRT_COM_ERROR;
    while ((mret = tkl_fs_mount(SDCARD_MOUNT_PATH, DEV_SDCARD)) != OPRT_OK) {
        PR_ERR("Mount SD card failed, retrying...");
        tal_system_sleep(1000);
        retry++;
        if (retry > 10)
            break; // Don't block forever
    }
    sg_sd_mounted = (mret == OPRT_OK) ? TRUE : FALSE;

    progress_init();

    // Init App State
    memset(&sg_app_ctx, 0, sizeof(sg_app_ctx));
    sg_app_ctx.rotate = ROTATE_0;
    update_items_per_page();
    strcpy(sg_app_ctx.current_path, SDCARD_MOUNT_PATH);
    sg_app_ctx.state          = STATE_FILE_LIST;
    sg_app_ctx.current_page   = 0;
    sg_app_ctx.selected_index = 0;
    sg_app_ctx.need_refresh   = TRUE;

    scan_files();

    while (1) {
        if (sg_btn_pending) {
            char btn[8];
            strncpy(btn, sg_btn_pending_name, sizeof(btn) - 1);
            btn[sizeof(btn) - 1]        = 0;
            sg_btn_pending              = FALSE;
            TDL_BUTTON_TOUCH_EVENT_E ev = sg_btn_pending_event;
            PR_NOTICE("Button %s pressed", btn);
            handle_button_press(btn, ev);
        }
        if (sg_app_ctx.state == STATE_BD_AUTH || sg_app_ctx.state == STATE_BD_LIST ||
            sg_app_ctx.state == STATE_BD_DETAIL || sg_app_ctx.state == STATE_BD_MSG) {
            BDNDK_VIEW_E v  = bdndk_view_get();
            APP_STATE_E  ns = sg_app_ctx.state;
            if (v == BDNDK_VIEW_AUTH)
                ns = STATE_BD_AUTH;
            else if (v == BDNDK_VIEW_LIST)
                ns = STATE_BD_LIST;
            else if (v == BDNDK_VIEW_DETAIL)
                ns = STATE_BD_DETAIL;
            else if (v == BDNDK_VIEW_MSG)
                ns = STATE_BD_MSG;
            if (ns != sg_app_ctx.state) {
                sg_app_ctx.state        = ns;
                sg_app_ctx.need_refresh = TRUE;
            }
            if (bdndk_need_refresh_fetch()) {
                sg_app_ctx.need_refresh = TRUE;
            }
        }
        if (sg_app_ctx.need_refresh) {
            sg_app_ctx.need_refresh = FALSE;
            refresh_ui();
        }
        tal_system_sleep(100);
    }
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    board_register_hardware();

    PR_NOTICE("SD Card Reader Demo Started");

    example_time_sync_on_startup(15000);

    static THREAD_CFG_T thrd_param = {.priority = TASK_SD_PRIORITY, .stackDepth = TASK_SD_SIZE, .thrdname = "sd"};
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_sd_thrd_hdl, NULL, NULL, __tuya_main_task, NULL, &thrd_param));
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();
    while (1) {
        tal_system_sleep(500);
    }
}
#else
static THREAD_HANDLE ty_app_thread = NULL;
static void          tuya_app_thread(void *arg)
{
    user_main();
    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}
void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
