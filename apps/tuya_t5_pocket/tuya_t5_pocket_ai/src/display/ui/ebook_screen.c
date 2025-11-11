/**
 * @file ebook_screen.c
 * @brief Implementation of the e-book reader screen with book shelf functionality
 *
 * This file contains the implementation of an enhanced e-book reader screen which provides
 * a book shelf interface for browsing and selecting from multiple books, along with
 * individual reading functionality with position memory for each book.
 *
 * The e-book screen includes:
 * - Book shelf interface with file scanning
 * - Multi-book selection and management
 * - Individual reading position memory per book
 * - Enhanced reading interface with page counter
 * - Up/Down key navigation for both shelf and reading
 * - White background with black text for reading
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "ebook_screen.h"
#include "toast_screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ENABLE_LVGL_HARDWARE
#include "tkl_fs.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tal_kv.h"
#include "tal_system.h"
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

/***********************************************************
************************macro define************************
***********************************************************/

#define EBOOK_MAX_CONTENT_SIZE  (512 * 1024)  /**< Maximum content size (512KB) */
#define EBOOK_LINES_PER_SCREEN  12           /**< Number of visible lines per screen for better readability */
#define EBOOK_CHARS_PER_LINE    80           /**< Maximum characters per line for better screen utilization */
#define BOOK_SCAN_INTERVAL      3000        /**< Book scanning interval in milliseconds (3 seconds) */

/* UI Layout Configuration */
#define SHELF_CONTAINER_PAD     5            /**< Shelf container padding */
#define SHELF_TITLE_Y_OFFSET    5            /**< Title label Y offset from top */
#define SHELF_LIST_MARGIN       10           /**< Shelf list horizontal margin */
#define SHELF_LIST_HEIGHT       60           /**< Height reserved for title and instructions */
#define SHELF_LIST_Y_OFFSET     30           /**< Shelf list Y offset from top */
#define SHELF_INSTR_Y_OFFSET    (-5)         /**< Instructions label Y offset from bottom */

#define READING_CONTAINER_PAD   3            /**< Reading container padding */
#define READING_TITLE_HEIGHT    18           /**< Book title label height */
#define READING_TITLE_Y_OFFSET  2            /**< Book title Y offset from top */
/* #define READING_BATTERY_WIDTH   55 */           /**< Battery indicator width */
#define READING_BATTERY_HEIGHT  18           /**< Battery indicator height */
#define READING_BATTERY_MARGIN  3            /**< Battery indicator right margin */ */
#define READING_TITLE_MARGIN    70           /**< Title label margin (for battery space) */

#define SCROLL_AREA_MARGIN      6            /**< Scroll area margin from container */
#define SCROLL_AREA_TOP_OFFSET  22           /**< Scroll area top offset (title height + spacing) */
#define SCROLL_AREA_PAD         4            /**< Scroll area internal padding */
#define SCROLL_TEXT_LINE_SPACE  2            /**< Text line spacing in pixels */

#define PAGE_INFO_HEIGHT        14           /**< Page info label height */
#define PAGE_INFO_BOTTOM_OFFSET 15           /**< Page info Y offset from bottom */
#define PAGE_INFO_MARGIN        16           /**< Page info horizontal margin */
#define PAGE_INFO_X_OFFSET      8            /**< Page info X offset from left */

/* Color definitions */
#define COLOR_GRAY_100          lv_color_make(100, 100, 100)  /**< Gray color for text */
#define COLOR_GRAY_80           lv_color_make(80, 80, 80)     /**< Darker gray for page info */
#define COLOR_GRAY_150          lv_color_make(150, 150, 150)  /**< Light gray for disabled text */
#define COLOR_GRAY_240          lv_color_make(240, 240, 240)  /**< Very light gray for background */
#define COLOR_BLUE_SELECT       lv_color_make(0, 100, 200)    /**< Blue color for selection */

/* Battery simulation */
/* #define BATTERY_INIT_LEVEL      85  */         /**< Initial battery level */
#define BATTERY_UPDATE_COUNTER  50           /**< Update counter threshold */
#define BATTERY_MIN_LEVEL       10           /**< Minimum battery level */
#define BATTERY_MAX_LEVEL       100          /**< Maximum battery level */ */

/* Timing configuration */
#define PAGE_INFO_UPDATE_DELAY  100          /**< Delay before updating page info (ms) */

/* Font configuration for ebook UI */
#define EBOOK_UI_FONT           &lv_font_montserrat_14  /**< Font used for all ebook UI elements */
#define EBOOK_PAGE_INFO_FONT    &lv_font_montserrat_12  /**< Font used for page info */
#define EBOOK_TITLE_FONT        &lv_font_montserrat_16  /**< Font used for book title */

/* Page navigation configuration - calculated at runtime */
#define CALCULATE_PAGE_SCROLL_LINES()  ((AI_PET_SCREEN_HEIGHT - SCROLL_AREA_TOP_OFFSET - PAGE_INFO_BOTTOM_OFFSET - READING_CONTAINER_PAD) / (lv_font_get_line_height(EBOOK_UI_FONT) + SCROLL_TEXT_LINE_SPACE))  /**< Lines to scroll per page */

#ifdef ENABLE_LVGL_HARDWARE
/* SD card configuration for hardware platform */
#define SDCARD_MOUNT_PATH      "/sdcard"     /**< SD card mount path */
#define SDCARD_MOUNT_RETRY     3             /**< Number of mount retry attempts */
#define SDCARD_MOUNT_DELAY     100          /**< Delay between mount attempts (ms) */

#define EBOOK_TXT_DIR          SDCARD_MOUNT_PATH //"/sdcard/txt"  /**< Books directory on SD card for hardware platform */
#define EBOOK_POSITIONS_FILE   "/sdcard/ebook_positions.txt"  /**< All positions save file on SD card */
#else
#define EBOOK_TXT_DIR          "/home/share/samba/lv_port_pc_vscode/txt"  /**< Books directory for software platform */
#define EBOOK_POSITIONS_FILE   "ebook_positions.txt"  /**< All positions save file for software platform */
#endif
/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_ebook_screen;
static lv_obj_t *shelf_container;
static lv_obj_t *reading_container;
static lv_obj_t *shelf_list;
static lv_obj_t *reading_text;
static lv_obj_t *page_info_label;
static lv_obj_t *book_title_label;
/* static lv_obj_t *battery_label; */
static lv_timer_t *book_scan_timer;  // Timer for periodic book scanning
static ebook_state_t ebook_state = {0};

Screen_t ebook_screen = {
    .init = ebook_screen_init,
    .deinit = ebook_screen_deinit,
    .screen_obj = &ui_ebook_screen,
    .name = "ebook",
    .state_data = &ebook_state,
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void keyboard_event_cb(lv_event_t *e);
static void create_shelf_ui(void);
static void create_reading_ui(void);
static void switch_to_shelf_mode(void);
static void switch_to_reading_mode(void);
static void calculate_lines(void);
static int calculate_total_lines(void);
static void load_book_position(int book_index);
static void save_book_position(int book_index);
/* static void update_battery_display(void); */
static void ebook_update_shelf_selection(void);
static void book_scan_timer_cb(lv_timer_t *timer);
static bool ebook_compare_book_lists(void);
static int ebook_page_up(void);
static int ebook_page_down(void);

#ifdef ENABLE_LVGL_HARDWARE
static int ebook_mount_sdcard(void);
static int ebook_ensure_directories(void);
static void ebook_log_error(const char *func, const char *msg, int error_code);
#endif

/***********************************************************
***********************function define**********************
***********************************************************/

#ifdef ENABLE_LVGL_HARDWARE
/**
 * @brief Log error with detailed information
 */
static void ebook_log_error(const char *func, const char *msg, int error_code)
{
    printf("[EBOOK ERROR] %s: %s (error code: %d)\n", func, msg, error_code);
}

/**
 * @brief Mount SD card with retry mechanism
 */
static int ebook_mount_sdcard(void)
{
    OPERATE_RET rt = OPRT_OK;

    printf("[EBOOK] Attempting to mount SD card at %s\n", SDCARD_MOUNT_PATH);

    // Use the same mount method as example_sd.c
    rt = tkl_fs_mount(SDCARD_MOUNT_PATH, DEV_SDCARD);
    if (rt == OPRT_OK) {
        printf("[EBOOK] SD card mounted successfully\n");
        return 0;
    }

    ebook_log_error("ebook_mount_sdcard", "All mount attempts failed", rt);
    return -1;
}

/**
 * @brief Ensure required directories exist
 */
static int ebook_ensure_directories(void)
{
    // OPERATE_RET rt = OPRT_OK;
    // BOOL_T is_exist = FALSE;

    // Check and create txt directory
    // rt = tkl_fs_is_exist(EBOOK_TXT_DIR, &is_exist);
    // if (rt != OPRT_OK || !is_exist) {
    //     printf("[EBOOK] Creating directory: %s\n", EBOOK_TXT_DIR);
    //     rt = tkl_fs_mkdir(EBOOK_TXT_DIR);
    //     if (rt != OPRT_OK) {
    //         ebook_log_error("ebook_ensure_directories", "Failed to create txt directory", rt);
    //         return -1;
    //     }
    //     printf("[EBOOK] Directory created successfully: %s\n", EBOOK_TXT_DIR);
    // } else {
    //     printf("[EBOOK] Directory already exists: %s\n", EBOOK_TXT_DIR);
    // }

    return 0;
}
#endif

/**
 * @brief Scan directory for txt files and populate book shelf
 */
int ebook_scan_books(void)
{
#ifdef ENABLE_LVGL_HARDWARE
    TUYA_DIR dir;
    TUYA_FILEINFO info;
    const char *name;
    char filepath[512];
    BOOL_T is_exist = FALSE;
    BOOL_T is_regular = FALSE;

    if (tkl_dir_open(EBOOK_TXT_DIR, &dir) != 0) {
        printf("Failed to open directory: %s\n", EBOOK_TXT_DIR);
        return -1;
    }

    ebook_state.shelf.book_count = 0;

    while (tkl_dir_read(dir, &info) == 0 && ebook_state.shelf.book_count < MAX_BOOK_FILES) {
        if (tkl_dir_name(info, &name) != 0) {
            continue;
        }

        // Skip hidden files and directories
        if (name[0] == '.') {
            continue;
        }

        snprintf(filepath, sizeof(filepath), "%s/%s", EBOOK_TXT_DIR, name);

        if (tkl_fs_is_exist(filepath, &is_exist) == 0 && is_exist) {
            if (tkl_dir_is_regular(info, &is_regular) == 0 && is_regular) {
                // Check for .txt extension
                char *ext = strrchr(name, '.');
                if (ext && strcmp(ext, ".txt") == 0) {
                    // Check file size by opening it
                    TUYA_FILE test_file = tkl_fopen(filepath, "r");
                    if (test_file) {
                        tkl_fseek(test_file, 0, SEEK_END);
                        long file_size = tkl_ftell(test_file);
                        tkl_fclose(test_file);

                        // Skip empty files
                        if (file_size <= 0) {
                            printf("Skipping empty file: %s\n", name);
                            continue;
                        }
                    } else {
                        // Can't open file, skip it
                        continue;
                    }

                    book_entry_t *book = &ebook_state.shelf.books[ebook_state.shelf.book_count];

                    // Store filename
                    strncpy(book->filename, name, MAX_FILENAME_LEN - 1);
                    book->filename[MAX_FILENAME_LEN - 1] = '\0';

                    // Create display name (remove .txt extension)
                    strncpy(book->display_name, name, MAX_FILENAME_LEN - 1);
                    book->display_name[MAX_FILENAME_LEN - 1] = '\0';
                    char *dot = strrchr(book->display_name, '.');
                    if (dot) {
                        *dot = '\0';
                    }

                    // Initialize position tracking
                    book->saved_line = 0;
                    book->total_lines = 0;

                    ebook_state.shelf.book_count++;
                    printf("Found book: %s\n", book->display_name);
                }
            }
        }
    }

    tkl_dir_close(dir);
#else
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char filepath[512];

    dir = opendir(EBOOK_TXT_DIR);
    if (!dir) {
        printf("Failed to open directory: %s\n", EBOOK_TXT_DIR);
        return -1;
    }

    ebook_state.shelf.book_count = 0;

    while ((entry = readdir(dir)) != NULL && ebook_state.shelf.book_count < MAX_BOOK_FILES) {
        // Skip hidden files and directories
        if (entry->d_name[0] == '.') {
            continue;
        }

        snprintf(filepath, sizeof(filepath), "%s/%s", EBOOK_TXT_DIR, entry->d_name);

        if (stat(filepath, &statbuf) == 0 && S_ISREG(statbuf.st_mode)) {
            // Check for .txt extension and non-empty file
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".txt") == 0 && statbuf.st_size > 0) {
                book_entry_t *book = &ebook_state.shelf.books[ebook_state.shelf.book_count];

                // Store filename
                strncpy(book->filename, entry->d_name, MAX_FILENAME_LEN - 1);
                book->filename[MAX_FILENAME_LEN - 1] = '\0';

                // Create display name (remove .txt extension)
                strncpy(book->display_name, entry->d_name, MAX_FILENAME_LEN - 1);
                book->display_name[MAX_FILENAME_LEN - 1] = '\0';
                char *dot = strrchr(book->display_name, '.');
                if (dot) {
                    *dot = '\0';
                }

                // Initialize position tracking
                book->saved_line = 0;
                book->total_lines = 0;

                ebook_state.shelf.book_count++;
                printf("Found book: %s\n", book->display_name);
            }
        }
    }

    closedir(dir);
#endif

    printf("Found %d books in %s\n", ebook_state.shelf.book_count, EBOOK_TXT_DIR);
    return ebook_state.shelf.book_count;
}

/**
 * @brief Keyboard event callback
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", ebook_screen.name, key);

    switch (key) {
        case KEY_UP:
            if (ebook_state.in_reading_mode) {
                // Scroll up in reading mode
                ebook_navigate_up();
            } else {
                // Navigate shelf up
                ebook_navigate_up();
            }
            break;

        case KEY_DOWN:
            if (ebook_state.in_reading_mode) {
                // Scroll down in reading mode
                ebook_navigate_down();
            } else {
                // Navigate shelf down
                ebook_navigate_down();
            }
            break;

        case KEY_LEFT:
            if (ebook_state.in_reading_mode) {
                // Page up in reading mode
                ebook_page_up();
            } else {
                // Navigate shelf up (same as KEY_UP)
                ebook_navigate_up();
            }
            break;

        case KEY_RIGHT:
            if (ebook_state.in_reading_mode) {
                // Page down in reading mode
                ebook_page_down();
            } else {
                // Navigate shelf down (same as KEY_DOWN)
                ebook_navigate_down();
            }
            break;

        case KEY_ENTER:
            ebook_handle_select();
            break;

        case KEY_ESC:
            ebook_handle_back();
            break;

        default:
            printf("Unknown key: %d\n", key);
            break;
    }
}

/**
 * @brief Compare current book list with a new scan to detect changes
 */
static bool ebook_compare_book_lists(void)
{
    // Store current book list
    static book_entry_t previous_books[MAX_BOOK_FILES];
    static int previous_book_count = -1;

    // First run - initialize previous list
    if (previous_book_count == -1) {
        memcpy(previous_books, ebook_state.shelf.books, sizeof(ebook_state.shelf.books));
        previous_book_count = ebook_state.shelf.book_count;
        return false; // No change on first run
    }

    // Check if count changed
    if (previous_book_count != ebook_state.shelf.book_count) {
        printf("Book count changed: %d -> %d\n", previous_book_count, ebook_state.shelf.book_count);
        memcpy(previous_books, ebook_state.shelf.books, sizeof(ebook_state.shelf.books));
        previous_book_count = ebook_state.shelf.book_count;
        return true;
    }

    // Check if any book names changed
    for (int i = 0; i < ebook_state.shelf.book_count; i++) {
        if (strcmp(previous_books[i].filename, ebook_state.shelf.books[i].filename) != 0 ||
            strcmp(previous_books[i].display_name, ebook_state.shelf.books[i].display_name) != 0) {
            printf("Book list content changed at index %d\n", i);
            memcpy(previous_books, ebook_state.shelf.books, sizeof(ebook_state.shelf.books));
            return true;
        }
    }

    return false; // No changes detected
}

/**
 * @brief Timer callback for periodic book scanning
 */
static void book_scan_timer_cb(lv_timer_t *timer)
{
    // Only scan when in shelf mode to avoid interfering with reading
    if (ebook_state.in_reading_mode) {
        printf("Skipping book scan - currently in reading mode\n");
        return;
    }

    printf("Performing periodic book scan...\n");

    // Store current selection to restore after scan
    int current_selection = ebook_state.shelf.selected_book;
    char current_book_name[MAX_FILENAME_LEN] = {0};

    // Remember currently selected book name
    if (current_selection >= 0 && current_selection < ebook_state.shelf.book_count) {
        strncpy(current_book_name, ebook_state.shelf.books[current_selection].filename,
                MAX_FILENAME_LEN - 1);
    }

    // Perform new scan
    int new_book_count = ebook_scan_books();

    // Check if list changed
    if (ebook_compare_book_lists() || new_book_count != ebook_state.shelf.book_count) {
        printf("Book list changed, refreshing display\n");

        // Try to restore selection to same book if it still exists
        ebook_state.shelf.selected_book = 0; // Default to first book

        if (strlen(current_book_name) > 0) {
            for (int i = 0; i < ebook_state.shelf.book_count; i++) {
                if (strcmp(ebook_state.shelf.books[i].filename, current_book_name) == 0) {
                    ebook_state.shelf.selected_book = i;
                    printf("Restored selection to book: %s\n", current_book_name);
                    break;
                }
            }
        }

        // Ensure selection is within bounds
        if (ebook_state.shelf.selected_book >= ebook_state.shelf.book_count) {
            ebook_state.shelf.selected_book = ebook_state.shelf.book_count - 1;
        }
        if (ebook_state.shelf.selected_book < 0) {
            ebook_state.shelf.selected_book = 0;
        }

        // Refresh the shelf display
        ebook_update_shelf_display();

        printf("Book scan completed: %d books found, selection: %d\n",
               ebook_state.shelf.book_count, ebook_state.shelf.selected_book);
    } else {
        printf("No changes detected in book list\n");
    }
}

/**
 * @brief Load saved reading position for a specific book
 */
static void load_book_position(int book_index)
{
    if (book_index < 0 || book_index >= ebook_state.shelf.book_count) {
        return;
    }

    book_entry_t *book = &ebook_state.shelf.books[book_index];
    char pos_filename[512];
#ifdef ENABLE_LVGL_HARDWARE
    snprintf(pos_filename, sizeof(pos_filename), "/sdcard/%s.pos", book->filename);
#else
    snprintf(pos_filename, sizeof(pos_filename), "%s.pos", book->filename);
#endif

#ifdef ENABLE_LVGL_HARDWARE
    {
        /* Use tal_kv to get saved position for this book. Key format: "ebook_pos:<filename>" */
        char kv_key[64];
        snprintf(kv_key, sizeof(kv_key), "ebook_pos:%s", book->filename);
        uint8_t *kv_val = NULL;
        size_t kv_len = 0;
        if (tal_kv_get(kv_key, &kv_val, &kv_len) == 0 && kv_val && kv_len > 0) {
            char buffer[32];
            size_t copy_len = kv_len < sizeof(buffer) - 1 ? kv_len : sizeof(buffer) - 1;
            memcpy(buffer, kv_val, copy_len);
            buffer[copy_len] = '\0';
            book->saved_line = atoi(buffer);
            tal_kv_free(kv_val);
            printf("Loaded KV position for %s: line %d\n", book->display_name, book->saved_line);
        } else {
            /* Key not present or read failed - default to 0 */
            book->saved_line = 0;
        }
    }
#else
    FILE *file = fopen(pos_filename, "r");
    if (file) {
        char buffer[32];
        if (fgets(buffer, sizeof(buffer), file)) {
            sscanf(buffer, "%d", &book->saved_line);
        }
        fclose(file);
        printf("Loaded position for %s: line %d\n", book->display_name, book->saved_line);
    }
#endif
}

/**
 * @brief Save reading position for a specific book
 */
static void save_book_position(int book_index)
{
    if (book_index < 0 || book_index >= ebook_state.shelf.book_count) {
        return;
    }

    book_entry_t *book = &ebook_state.shelf.books[book_index];
    char pos_filename[512];
#ifdef ENABLE_LVGL_HARDWARE
    snprintf(pos_filename, sizeof(pos_filename), "/sdcard/%s.pos", book->filename);
#else
    snprintf(pos_filename, sizeof(pos_filename), "%s.pos", book->filename);
#endif

#ifdef ENABLE_LVGL_HARDWARE
    {
        /* Use tal_kv to set saved position for this book. Key format: "ebook_pos:<filename>" */
        char kv_key[64];
        snprintf(kv_key, sizeof(kv_key), "ebook_pos:%s", book->filename);
        char buffer[32];
        int len = snprintf(buffer, sizeof(buffer), "%d", book->saved_line);
        if (tal_kv_set(kv_key, (const uint8_t *)buffer, (size_t)len) == 0) {
            printf("Saved KV position for %s: line %d\n", book->display_name, book->saved_line);
        } else {
            printf("Failed to save KV position for %s\n", book->display_name);
        }
    }
#else
    FILE *file = fopen(pos_filename, "w");
    if (file) {
        fprintf(file, "%d", book->saved_line);
        fclose(file);
        printf("Saved position for %s: line %d\n", book->display_name, book->saved_line);
    }
#endif
}

/**
 * @brief Open and read a book from the shelf
 */
int ebook_open_book(int book_index)
{
    if (book_index < 0 || book_index >= ebook_state.shelf.book_count) {
        return -1;
    }

    book_entry_t *book = &ebook_state.shelf.books[book_index];
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", EBOOK_TXT_DIR, book->filename);

    if (!ebook_load_file(filepath)) {
        return -1;
    }

    // Load saved position for this book
    load_book_position(book_index);

    // Use saved line position
    ebook_state.reading.current_line = book->saved_line;

    // Update book info
    book->total_lines = ebook_state.reading.total_lines;

    // Store current book info
    strncpy(ebook_state.reading.current_book, book->filename, MAX_FILENAME_LEN - 1);
    ebook_state.reading.current_book[MAX_FILENAME_LEN - 1] = '\0';

    // Calculate pages (legacy support)
    ebook_calculate_pages();

    // Switch to reading mode
    ebook_state.in_reading_mode = true;
    switch_to_reading_mode();
    lv_obj_clear_flag(page_info_label, LV_OBJ_FLAG_HIDDEN);
    ebook_update_reading_display();

    printf("Opened book: %s at line %d\n", book->display_name, ebook_state.reading.current_line);
    return 0;
}

/**
 * @brief Calculate total pages for current book
 */
void ebook_calculate_pages(void)
{
    if (ebook_state.reading.total_lines <= 0) {
        ebook_state.reading.total_pages = 0;
        ebook_state.reading.current_page = 0;
        return;
    }

    ebook_state.reading.total_pages = (ebook_state.reading.total_lines + EBOOK_LINES_PER_SCREEN - 1) / EBOOK_LINES_PER_SCREEN;
    ebook_state.reading.current_page = (ebook_state.reading.current_line / EBOOK_LINES_PER_SCREEN) + 1;

    // Ensure current page is valid
    if (ebook_state.reading.current_page < 1) {
        ebook_state.reading.current_page = 1;
    }
    if (ebook_state.reading.current_page > ebook_state.reading.total_pages) {
        ebook_state.reading.current_page = ebook_state.reading.total_pages;
    }
}

/**
 * @brief Load text content from a file (internal function)
 */
bool ebook_load_file(const char *filename)
{
    if (!filename) {
        return false;
    }

#ifdef ENABLE_LVGL_HARDWARE
    TUYA_FILE file = tkl_fopen(filename, "r");
    if (!file) {
        printf("Failed to open file: %s\n", filename);
        return false;
    }

    // Get file size
    tkl_fseek(file, 0, SEEK_END);
    long file_size = tkl_ftell(file);
    tkl_fseek(file, 0, SEEK_SET);

    // Check for empty file
    if (file_size <= 0) {
        printf("File is empty: %s\n", filename);
        tkl_fclose(file);
        return false;
    }

    if (file_size > EBOOK_MAX_CONTENT_SIZE) {
        printf("File too large: %ld bytes (max: %d)\n", file_size, EBOOK_MAX_CONTENT_SIZE);
        toast_screen_show("File too large to open!", 1500);
        tkl_fclose(file);
        return false;
    }

    // Allocate memory for content
    if (ebook_state.reading.content) {
        tal_psram_free(ebook_state.reading.content);
    }

    ebook_state.reading.content = tal_psram_malloc(file_size + 1);
    if (!ebook_state.reading.content) {
        printf("Failed to allocate memory for content\n");
        tkl_fclose(file);
        return false;
    }

    // Read file content
    size_t bytes_read = tkl_fread(ebook_state.reading.content, file_size, file);
    ebook_state.reading.content[bytes_read] = '\0';
    ebook_state.reading.content_size = bytes_read;

    tkl_fclose(file);
#else
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Failed to open file: %s\n", filename);
        return false;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Check for empty file
    if (file_size <= 0) {
        printf("File is empty: %s\n", filename);
        fclose(file);
        return false;
    }

    if (file_size > EBOOK_MAX_CONTENT_SIZE) {
        printf("File too large: %ld bytes (max: %d)\n", file_size, EBOOK_MAX_CONTENT_SIZE);
        toast_screen_show("File too large to open!", 1500);
        fclose(file);
        return false;
    }

    // Allocate memory for content
    if (ebook_state.reading.content) {
        free(ebook_state.reading.content);
    }

    ebook_state.reading.content = malloc(file_size + 1);
    if (!ebook_state.reading.content) {
        printf("Failed to allocate memory for content\n");
        fclose(file);
        return false;
    }

    // Read file content
    size_t bytes_read = fread(ebook_state.reading.content, 1, file_size, file);
    ebook_state.reading.content[bytes_read] = '\0';
    ebook_state.reading.content_size = bytes_read;

    fclose(file);
#endif

    // Update state
    ebook_state.reading.content_loaded = true;
    ebook_state.reading.current_line = 0;

    // Calculate total lines (legacy support - will be updated accurately after text is set)
    calculate_lines();

    printf("Loaded file: %s (%zu bytes, %d lines)\n",
           filename, ebook_state.reading.content_size, ebook_state.reading.total_lines);

    return true;
}

/**
 * @brief Calculate total number of lines in the content
 */
static void calculate_lines(void)
{
    if (!ebook_state.reading.content || ebook_state.reading.content_size == 0) {
        ebook_state.reading.total_lines = 0;
        return;
    }

    int lines = 1; // At least one line if there's content
    char *ptr = ebook_state.reading.content;

    while (*ptr) {
        if (*ptr == '\n') {
            lines++;
        }
        ptr++;
    }

    ebook_state.reading.total_lines = lines;
}

/**
 * @brief Calculate accurate total lines using LVGL text rendering
 */
static int calculate_total_lines(void)
{
    if (!reading_text || !ebook_state.reading.content_loaded || !ebook_state.reading.content) {
        return 0;
    }

    /* 1. Get current label style, width, and text */
    const lv_font_t * font = lv_obj_get_style_text_font(reading_text, LV_PART_MAIN);
    int32_t letter_space = lv_obj_get_style_text_letter_space(reading_text, LV_PART_MAIN);
    int32_t line_space   = lv_obj_get_style_text_line_space(reading_text, LV_PART_MAIN);
    lv_coord_t max_w     = lv_obj_get_content_width(reading_text);   /* Drawable width excluding padding */
    const char * txt       = lv_label_get_text(reading_text);

    /* 2. Use lv_txt_get_size to calculate how many pixels it would occupy with WRAP layout */
    lv_point_t size;
    lv_txt_get_size(&size, txt, font, letter_space, line_space, max_w, LV_TEXT_FLAG_NONE);

    /* 3. Calculate the number of lines */
    lv_coord_t line_height = lv_font_get_line_height(font) + line_space;
    int lines = (size.y + line_space) / line_height;   /* +line_space to round up the last line */

    return lines;
}

/**
 * @brief Update battery display
 */
/*
static void update_battery_display(void)
{
    if (!battery_label) {
        return;
    }

    // Use system battery status - this will get the real battery level from main screen
    static uint8_t battery_level = BATTERY_INIT_LEVEL;
    static bool charging = false;

    // In real implementation, this would get actual battery status
    // For now, we simulate it
    static int update_counter = 0;
    update_counter++;
    if (update_counter > BATTERY_UPDATE_COUNTER) {
        update_counter = 0;
        // Simulate battery level changes
        if (!charging && battery_level > BATTERY_MIN_LEVEL) {
            battery_level--;
        } else if (charging && battery_level < BATTERY_MAX_LEVEL) {
            battery_level++;
        }

        // Update the main screen battery status (convert 0-100 to 0-6 level)
        uint8_t battery_state = (battery_level * 6) / BATTERY_MAX_LEVEL;
        main_screen_set_battery_state(battery_state, charging);
    }

    // Choose appropriate battery icon based on level and charging status
    const char* battery_icon;
    if (charging) {
        battery_icon = LV_SYMBOL_CHARGE;
    } else if (battery_level > 75) {
        battery_icon = LV_SYMBOL_BATTERY_FULL;
    } else if (battery_level > 50) {
        battery_icon = LV_SYMBOL_BATTERY_3;
    } else if (battery_level > 25) {
        battery_icon = LV_SYMBOL_BATTERY_2;
    } else {
        battery_icon = LV_SYMBOL_BATTERY_1;
    }

    static char battery_text[32];
    snprintf(battery_text, sizeof(battery_text), "%s%d%%", battery_icon, battery_level);
    lv_label_set_text(battery_label, battery_text);
}
*/

/**
 * @brief Create book shelf UI with list-based scrolling
 */
static void create_shelf_ui(void)
{
    shelf_container = lv_obj_create(ui_ebook_screen);
    lv_obj_set_size(shelf_container, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(shelf_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(shelf_container, 0, 0);
    lv_obj_set_style_pad_all(shelf_container, SHELF_CONTAINER_PAD, 0);

    // Title label with auto-refresh indicator
    lv_obj_t *title_label = lv_label_create(shelf_container);
    lv_label_set_text(title_label, "Book Shelf - Auto-Refresh ON");
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, SHELF_TITLE_Y_OFFSET);

    // Book list using lv_list for automatic scrolling
    shelf_list = lv_list_create(shelf_container);
    lv_obj_set_size(shelf_list, AI_PET_SCREEN_WIDTH - SHELF_LIST_MARGIN,
                    AI_PET_SCREEN_HEIGHT - SHELF_LIST_HEIGHT);
    lv_obj_align(shelf_list, LV_ALIGN_TOP_MID, 0, SHELF_LIST_Y_OFFSET);
    lv_obj_set_style_bg_color(shelf_list, lv_color_white(), 0);
    lv_obj_set_style_border_width(shelf_list, 1, 0);
    lv_obj_set_style_border_color(shelf_list, lv_color_black(), 0);
    lv_obj_set_style_pad_all(shelf_list, SHELF_CONTAINER_PAD, 0);

    // Instructions
    lv_obj_t *instr_label = lv_label_create(shelf_container);
    lv_label_set_text(instr_label, LV_SYMBOL_UP LV_SYMBOL_DOWN " Navigate | " LV_SYMBOL_OK " Select | " LV_SYMBOL_CLOSE " Exit");
    lv_obj_set_style_text_color(instr_label, COLOR_GRAY_100, 0);
    lv_obj_set_style_text_font(instr_label, EBOOK_UI_FONT, 0);
    lv_obj_align(instr_label, LV_ALIGN_BOTTOM_MID, 0, SHELF_INSTR_Y_OFFSET);
}

/**
 * @brief Create reading UI - consistent with original layout
 */
static void create_reading_ui(void)
{
    // Main reading container (not scrollable, just a holder)
    reading_container = lv_obj_create(ui_ebook_screen);
    lv_obj_set_size(reading_container, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(reading_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(reading_container, 0, 0);
    lv_obj_set_style_pad_all(reading_container, READING_CONTAINER_PAD, 0);
    lv_obj_clear_flag(reading_container, LV_OBJ_FLAG_SCROLLABLE);  // Container itself not scrollable

    // Book title at top center (fixed position)
    book_title_label = lv_label_create(reading_container);
    lv_obj_set_size(book_title_label, AI_PET_SCREEN_WIDTH - READING_TITLE_MARGIN, READING_TITLE_HEIGHT);
    lv_obj_set_pos(book_title_label, (AI_PET_SCREEN_WIDTH - (AI_PET_SCREEN_WIDTH - READING_TITLE_MARGIN)) / 2,
                   READING_TITLE_Y_OFFSET);
    lv_obj_set_style_text_color(book_title_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(book_title_label, EBOOK_TITLE_FONT, 0);
    lv_obj_set_style_text_align(book_title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(book_title_label, LV_LABEL_LONG_DOT);

    // Battery indicator at top right (fixed position)
    /*
    battery_label = lv_label_create(reading_container);
    lv_obj_set_size(battery_label, READING_BATTERY_WIDTH, READING_BATTERY_HEIGHT);
    lv_obj_set_pos(battery_label, AI_PET_SCREEN_WIDTH - READING_BATTERY_WIDTH - READING_BATTERY_MARGIN,
                   READING_TITLE_Y_OFFSET);
    lv_obj_set_style_text_color(battery_label, COLOR_GRAY_100, 0);
    lv_obj_set_style_text_font(battery_label, EBOOK_UI_FONT, 0);
    lv_obj_set_style_text_align(battery_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_FULL "85%");
    */

    // Scrollable text area in the middle
    lv_obj_t *scroll_area = lv_obj_create(reading_container);
    int scroll_height = AI_PET_SCREEN_HEIGHT - SCROLL_AREA_TOP_OFFSET - PAGE_INFO_BOTTOM_OFFSET -
                        READING_CONTAINER_PAD;
    lv_obj_set_size(scroll_area, AI_PET_SCREEN_WIDTH - SCROLL_AREA_MARGIN, scroll_height);
    lv_obj_set_pos(scroll_area, READING_CONTAINER_PAD, SCROLL_AREA_TOP_OFFSET);
    lv_obj_set_style_bg_color(scroll_area, lv_color_white(), 0);
    lv_obj_set_style_border_width(scroll_area, 0, 0);
    lv_obj_set_style_pad_all(scroll_area, SCROLL_AREA_PAD, 0);
    lv_obj_set_scrollbar_mode(scroll_area, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(scroll_area, LV_DIR_VER);  // Only vertical scroll

    // Text label inside scroll area - auto height
    reading_text = lv_label_create(scroll_area);
    lv_obj_set_width(reading_text, lv_pct(100));  // Full width of parent
    lv_obj_set_style_text_color(reading_text, lv_color_black(), 0);
    lv_obj_set_style_text_font(reading_text, EBOOK_UI_FONT, 0);
    lv_label_set_long_mode(reading_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(reading_text, SCROLL_TEXT_LINE_SPACE, 0);
    lv_obj_set_style_text_align(reading_text, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(reading_text, "No book loaded");

    // Page info at very bottom of screen (fixed position)
    page_info_label = lv_label_create(reading_container);
    lv_obj_set_size(page_info_label, AI_PET_SCREEN_WIDTH - PAGE_INFO_MARGIN, PAGE_INFO_HEIGHT);
    lv_obj_set_pos(page_info_label, PAGE_INFO_X_OFFSET, AI_PET_SCREEN_HEIGHT - PAGE_INFO_BOTTOM_OFFSET);
    lv_obj_set_style_text_color(page_info_label, COLOR_GRAY_80, 0);
    lv_obj_set_style_text_font(page_info_label, EBOOK_PAGE_INFO_FONT, 0);
    lv_obj_set_style_text_align(page_info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(page_info_label, COLOR_GRAY_240, 0);
    lv_obj_set_style_bg_opa(page_info_label, LV_OPA_80, 0);

    // Store scroll_area reference in reading_container's user_data for later access
    lv_obj_set_user_data(reading_container, scroll_area);

    // Initially hidden
    lv_obj_add_flag(reading_container, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief Switch to reading mode
 */
static void switch_to_reading_mode(void)
{
    lv_obj_add_flag(shelf_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(reading_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(page_info_label, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief Switch to shelf mode
 */
static void switch_to_shelf_mode(void)
{
    lv_obj_clear_flag(shelf_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(reading_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(page_info_label, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief Update book shelf display using list-based scrolling (similar to food menu)
 */
void ebook_update_shelf_display(void)
{
    if (!shelf_list) {
        return;
    }

    // Clear existing items
    lv_obj_clean(shelf_list);

    if (ebook_state.shelf.book_count == 0) {
        lv_obj_t *empty_btn = lv_list_add_btn(shelf_list, LV_SYMBOL_FILE, "No books found in txt directory");
        lv_obj_set_style_text_color(empty_btn, COLOR_GRAY_150, 0);
        return;
    }

    // Create list items for all books
    for (int i = 0; i < ebook_state.shelf.book_count; i++) {
        book_entry_t *book = &ebook_state.shelf.books[i];

        // Create list button with book icon and name
        lv_obj_t *book_btn = lv_list_add_btn(shelf_list, LV_SYMBOL_FILE, book->display_name);

        // Set font and basic styling
        lv_obj_set_style_text_font(book_btn, EBOOK_UI_FONT, 0);

        // Add book info (pages/size) on the right
        if (book->total_lines > 0) {
            lv_obj_t *info_label = lv_label_create(book_btn);
            char info_text[32];
            int pages = (book->total_lines + EBOOK_LINES_PER_SCREEN - 1) / EBOOK_LINES_PER_SCREEN;
            snprintf(info_text, sizeof(info_text), "%d pages", pages);
            lv_label_set_text(info_label, info_text);
            lv_obj_align(info_label, LV_ALIGN_RIGHT_MID, -10, 0);
            lv_obj_set_style_text_color(info_label, COLOR_GRAY_100, 0);
            lv_obj_set_style_text_font(info_label, EBOOK_UI_FONT, 0);
        }
    }

    // Apply selection highlighting
    ebook_update_shelf_selection();

    printf("Shelf display updated: %d books, selected: %d\n",
           ebook_state.shelf.book_count, ebook_state.shelf.selected_book);
}

/**
 * @brief Update shelf selection highlighting (similar to food menu)
 */
static void ebook_update_shelf_selection(void)
{
    uint32_t child_count = lv_obj_get_child_cnt(shelf_list);
    if (child_count == 0) return;

    // Reset all items to default style
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *btn = lv_obj_get_child(shelf_list, i);
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        lv_obj_set_style_text_color(btn, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    }

    // Highlight selected item
    if (ebook_state.shelf.selected_book < (int)child_count) {
        lv_obj_t *selected_btn = lv_obj_get_child(shelf_list, ebook_state.shelf.selected_book);
        lv_obj_set_style_bg_color(selected_btn, COLOR_BLUE_SELECT, 0);
        lv_obj_set_style_text_color(selected_btn, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(selected_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(selected_btn, 4, 0);

        // Scroll to view with animation (key feature from food menu)
        lv_obj_scroll_to_view(selected_btn, LV_ANIM_ON);
    }
}

/**
 * @brief Timer callback to update page info after scroll animation
 */
static void update_page_info_timer_cb(lv_timer_t *timer)
{
    if (!reading_container || !page_info_label) {
        if (timer) {
            lv_timer_del(timer);
        }
        return;
    }

    // Get scroll area from reading_container's user_data
    lv_obj_t *scroll_area = (lv_obj_t *)lv_obj_get_user_data(reading_container);
    if (!scroll_area) {
        if (timer) {
            lv_timer_del(timer);
        }
        return;
    }

    // Get scroll position from scroll_area
    lv_coord_t scroll_y = lv_obj_get_scroll_y(scroll_area);

    // Calculate line height (font height + line spacing)
    const lv_font_t *font = EBOOK_UI_FONT;
    int line_height = lv_font_get_line_height(font) + SCROLL_TEXT_LINE_SPACE;

    // Calculate current line based on scroll position
    int current_line = scroll_y / line_height + 1; // +1 for 1-based indexing
    if (current_line < 1) current_line = 1;

    // Ensure current line doesn't exceed total lines
    if (current_line > ebook_state.reading.total_lines) {
        current_line = ebook_state.reading.total_lines;
    }

    // Update state
    ebook_state.reading.current_line = current_line;

    // Calculate current page
    ebook_calculate_pages();

    // Update page info label
    char page_text[100];
    snprintf(page_text, sizeof(page_text),
            "Line %d/%d | Page %d/%d " LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " Page " LV_SYMBOL_UP LV_SYMBOL_DOWN " Scroll " LV_SYMBOL_CLOSE " Back",
            current_line, ebook_state.reading.total_lines,
            ebook_state.reading.current_page, ebook_state.reading.total_pages);
    lv_label_set_text(page_info_label, page_text);

    // Delete the timer after update (only if timer is not NULL)
    if (timer) {
        lv_timer_del(timer);
    }
}

/**
 * @brief Update page info based on scroll position
 */
static void update_page_info(void)
{
    update_page_info_timer_cb(NULL);
}

/**
 * @brief Update reading display - consistent with original behavior
 */
void ebook_update_reading_display(void)
{
    if (!reading_text || !ebook_state.reading.content_loaded || !ebook_state.reading.content) {
        lv_label_set_text(reading_text, "No content loaded");
        lv_label_set_text(book_title_label, "E-Book Reader");
        if (page_info_label) {
            lv_label_set_text(page_info_label, LV_SYMBOL_CLOSE " Back to shelf");
        }
        return;
    }

    // Find current book to get display name
    char *display_name = "Unknown Book";
    for (int i = 0; i < ebook_state.shelf.book_count; i++) {
        if (strcmp(ebook_state.shelf.books[i].filename, ebook_state.reading.current_book) == 0) {
            display_name = ebook_state.shelf.books[i].display_name;
            break;
        }
    }
    lv_label_set_text(book_title_label, display_name);

    // Display full content in label - let LVGL handle scrolling
    lv_label_set_text(reading_text, ebook_state.reading.content);

    // Force layout update to ensure content height is calculated correctly
    lv_obj_update_layout(reading_text);

    // Calculate accurate total lines using LVGL rendering (now that text is set)
    ebook_state.reading.total_lines = calculate_total_lines();

    // Get scroll area from reading_container's user_data
    lv_obj_t *scroll_area = (lv_obj_t *)lv_obj_get_user_data(reading_container);
    if (scroll_area) {
        // Update scroll area layout to reflect new content
        lv_obj_update_layout(scroll_area);

        // Scroll to saved position
        const lv_font_t *font = EBOOK_UI_FONT;
        int line_height = lv_font_get_line_height(font) + SCROLL_TEXT_LINE_SPACE;
        int scroll_y = (ebook_state.reading.current_line - 1) * line_height;
        lv_obj_scroll_to_y(scroll_area, scroll_y, LV_ANIM_OFF);
    }

    // Update page info
    update_page_info();

    // Update battery display
    /* update_battery_display(); */

    printf("Display updated: %s (%zu bytes, line %d/%d)\n",
           ebook_state.reading.current_book,
           ebook_state.reading.content_size,
           ebook_state.reading.current_line,
           ebook_state.reading.total_lines);
}

/**
 * @brief Navigate up in reading mode - with boundary check
 */
int ebook_navigate_up(void)
{
    if (ebook_state.in_reading_mode) {
        // Get scroll area from reading_container's user_data
        lv_obj_t *scroll_area = (lv_obj_t *)lv_obj_get_user_data(reading_container);
        if (!scroll_area) {
            printf("Error: scroll_area is NULL\n");
            return 0;
        }

        // Get current scroll position
        lv_coord_t scroll_y = lv_obj_get_scroll_y(scroll_area);

        // Calculate line height
        const lv_font_t *font = EBOOK_UI_FONT;
        int line_height = lv_font_get_line_height(font) + SCROLL_TEXT_LINE_SPACE;

        // Check if already at top (scroll_y <= 0 means can't scroll up more)
        if (scroll_y <= 0) {
            printf("Already at top, cannot scroll up\n");
            return 0;
        }

        // Scroll up by one line (positive value scrolls up in LVGL)
        lv_obj_scroll_by(scroll_area, 0, line_height, LV_ANIM_ON);

        // Update page info after scroll animation
        lv_timer_create(update_page_info_timer_cb, PAGE_INFO_UPDATE_DELAY, NULL);

        printf("Scrolled up by %d pixels\n", line_height);
        return 0;
    } else {
        // Navigate shelf up
        if (ebook_state.shelf.selected_book > 0) {
            ebook_state.shelf.selected_book--;
            ebook_update_shelf_selection();
            printf("Selected book: %d\n", ebook_state.shelf.selected_book);
        }
        return 0;
    }
}

/**
 * @brief Navigate down in reading mode - with boundary check
 */
int ebook_navigate_down(void)
{
    if (ebook_state.in_reading_mode) {
        // Get scroll area from reading_container's user_data
        lv_obj_t *scroll_area = (lv_obj_t *)lv_obj_get_user_data(reading_container);
        if (!scroll_area) {
            printf("Error: scroll_area is NULL\n");
            return 0;
        }

        // Get current scroll position
        lv_coord_t scroll_y = lv_obj_get_scroll_y(scroll_area);

        // Get the actual text content height from reading_text object
        lv_coord_t text_height = lv_obj_get_height(reading_text);
        lv_coord_t scroll_area_height = lv_obj_get_height(scroll_area);

        // Get scroll area padding
        lv_coord_t pad_top = lv_obj_get_style_pad_top(scroll_area, LV_PART_MAIN);
        lv_coord_t pad_bottom = lv_obj_get_style_pad_bottom(scroll_area, LV_PART_MAIN);
        lv_coord_t content_height = text_height + pad_top + pad_bottom;

        // Calculate line height
        const lv_font_t *font = EBOOK_UI_FONT;
        int line_height = lv_font_get_line_height(font) + SCROLL_TEXT_LINE_SPACE;

        // Check if already at bottom (scroll_y >= content_height - scroll_area_height means can't scroll down more)
        if (scroll_y >= content_height - scroll_area_height) {
            printf("Already at bottom, cannot scroll down\n");
            return 0;
        }

        // Scroll down by one line (negative value scrolls down in LVGL)
        lv_obj_scroll_by(scroll_area, 0, -line_height, LV_ANIM_ON);

        // Update page info after scroll animation
        lv_timer_create(update_page_info_timer_cb, PAGE_INFO_UPDATE_DELAY, NULL);

        printf("Scrolled down by %d pixels\n", line_height);
        return 0;
    } else {
        // Navigate shelf down
        if (ebook_state.shelf.selected_book < ebook_state.shelf.book_count - 1) {
            ebook_state.shelf.selected_book++;
            ebook_update_shelf_selection();
            printf("Selected book: %d\n", ebook_state.shelf.selected_book);
        }
        return 0;
    }
}

/**
 * @brief Page up in reading mode - scroll by fixed number of lines
 */
int ebook_page_up(void)
{
    if (!ebook_state.in_reading_mode) {
        return 0;
    }

    // Get scroll area from reading_container's user_data
    lv_obj_t *scroll_area = (lv_obj_t *)lv_obj_get_user_data(reading_container);
    if (!scroll_area) {
        printf("Error: scroll_area is NULL\n");
        return 0;
    }

    // Get current scroll position
    lv_coord_t scroll_y = lv_obj_get_scroll_y(scroll_area);

    // Calculate line height and page scroll lines
    const lv_font_t *font = EBOOK_UI_FONT;
    int line_height = lv_font_get_line_height(font) + SCROLL_TEXT_LINE_SPACE;
    int page_lines = CALCULATE_PAGE_SCROLL_LINES();
    int scroll_pixels = page_lines * line_height;

    // Calculate actual scroll amount (don't scroll past top)
    int actual_scroll_pixels = (scroll_y < scroll_pixels) ? scroll_y : scroll_pixels;

    // Check if already at top
    if (actual_scroll_pixels <= 0) {
        printf("Already at top, cannot page up\n");
        return 0;
    }

    // Scroll up by actual_scroll_pixels (positive value scrolls up in LVGL)
    lv_obj_scroll_by(scroll_area, 0, actual_scroll_pixels, LV_ANIM_ON);

    // Update page info after scroll animation
    lv_timer_create(update_page_info_timer_cb, PAGE_INFO_UPDATE_DELAY, NULL);

    printf("Paged up by %d lines (%d pixels)\n", page_lines, actual_scroll_pixels);
    return 0;
}

/**
 * @brief Page down in reading mode - scroll by fixed number of lines
 */
int ebook_page_down(void)
{
    if (!ebook_state.in_reading_mode) {
        return 0;
    }

    // Get scroll area from reading_container's user_data
    lv_obj_t *scroll_area = (lv_obj_t *)lv_obj_get_user_data(reading_container);
    if (!scroll_area) {
        printf("Error: scroll_area is NULL\n");
        return 0;
    }

    // Get current scroll position
    lv_coord_t scroll_y = lv_obj_get_scroll_y(scroll_area);

    // Get the actual text content height from reading_text object
    lv_coord_t text_height = lv_obj_get_height(reading_text);
    lv_coord_t scroll_area_height = lv_obj_get_height(scroll_area);

    // Get scroll area padding
    lv_coord_t pad_top = lv_obj_get_style_pad_top(scroll_area, LV_PART_MAIN);
    lv_coord_t pad_bottom = lv_obj_get_style_pad_bottom(scroll_area, LV_PART_MAIN);
    lv_coord_t content_height = text_height + pad_top + pad_bottom;

    // Calculate line height and page scroll lines
    const lv_font_t *font = EBOOK_UI_FONT;
    int line_height = lv_font_get_line_height(font) + SCROLL_TEXT_LINE_SPACE;
    int page_lines = CALCULATE_PAGE_SCROLL_LINES();
    int scroll_pixels = page_lines * line_height;

    // Calculate maximum scroll possible (don't scroll past bottom)
    lv_coord_t max_scroll_down = content_height - scroll_area_height - scroll_y;
    int actual_scroll_pixels = (scroll_pixels > max_scroll_down) ? max_scroll_down : scroll_pixels;

    // Check if already at bottom
    if (actual_scroll_pixels <= 0) {
        printf("Already at bottom, cannot page down\n");
        return 0;
    }

    // Scroll down by actual_scroll_pixels (negative value scrolls down in LVGL)
    lv_obj_scroll_by(scroll_area, 0, -actual_scroll_pixels, LV_ANIM_ON);

    // Update page info after scroll animation
    lv_timer_create(update_page_info_timer_cb, PAGE_INFO_UPDATE_DELAY, NULL);

    printf("Paged down by %d lines (%d pixels)\n", page_lines, actual_scroll_pixels);
    return 0;
}

/**
 * @brief Handle select/enter action
 */
int ebook_handle_select(void)
{
    if (ebook_state.in_reading_mode) {
        // In reading mode, select doesn't do anything special
        return 0;
    } else {
        // In shelf mode, open selected book
        if (ebook_state.shelf.selected_book >= 0 && ebook_state.shelf.selected_book < ebook_state.shelf.book_count) {
            return ebook_open_book(ebook_state.shelf.selected_book);
        }
    }
    return -1;
}

/**
 * @brief Handle back/return action
 */
int ebook_handle_back(void)
{
    if (ebook_state.in_reading_mode) {
        // Save current position before going back
        ebook_save_position();

        // Switch back to shelf mode
        ebook_state.in_reading_mode = false;
        switch_to_shelf_mode();
        ebook_update_shelf_display();
        return 0;
    } else {
        // Exit from shelf mode
        screen_back();
        return 0;
    }
}

/**
 * @brief Save reading position for current book
 */
int ebook_save_position(void)
{
    if (!ebook_state.in_reading_mode || !ebook_state.reading.content_loaded) {
        return -1;
    }

    // Find the current book in shelf
    for (int i = 0; i < ebook_state.shelf.book_count; i++) {
        if (strcmp(ebook_state.shelf.books[i].filename, ebook_state.reading.current_book) == 0) {
            // Update book's saved position
            ebook_state.shelf.books[i].saved_line = ebook_state.reading.current_line;

            // Save to file
            save_book_position(i);

            printf("Saved reading position: book=%s, line=%d\n",
                   ebook_state.shelf.books[i].filename,
                   ebook_state.reading.current_line);
            return 0;
        }
    }
    return -1;
}

/**
 * @brief Load saved reading position for current book
 */
int ebook_load_position(void)
{
    if (!ebook_state.in_reading_mode || !ebook_state.reading.content_loaded) {
        return -1;
    }

    // Find the current book in shelf
    for (int i = 0; i < ebook_state.shelf.book_count; i++) {
        if (strcmp(ebook_state.shelf.books[i].filename, ebook_state.reading.current_book) == 0) {
            load_book_position(i);
            ebook_state.reading.current_line = ebook_state.shelf.books[i].saved_line;
            return ebook_state.reading.current_line;
        }
    }
    return 0;
}

/**
 * @brief Clean up e-book resources
 */
void ebook_cleanup(void)
{
    // Save current position if in reading mode
    if (ebook_state.in_reading_mode) {
        ebook_save_position();
    }

    // Stop the book scanning timer
    if (book_scan_timer) {
        lv_timer_set_period(book_scan_timer, 0);
        lv_timer_del(book_scan_timer);
        book_scan_timer = NULL;
    }

    // Free allocated content
    if (ebook_state.reading.content) {
#ifdef ENABLE_LVGL_HARDWARE
        tal_psram_free(ebook_state.reading.content);
#else
        free(ebook_state.reading.content);
#endif
        ebook_state.reading.content = NULL;
    }
}

/**
 * @brief Initialize the e-book screen
 */
void ebook_screen_init(void)
{
    printf("[%s] Initializing e-book screen\n", ebook_screen.name);

#ifdef ENABLE_LVGL_HARDWARE
    // Mount SD card and ensure directories exist
    if (ebook_mount_sdcard() != 0) {
        printf("[EBOOK ERROR] Failed to mount SD card\n");
        toast_screen_show("SD Card mount failed", 3000);
        screen_back();
        return;
    }

    if (ebook_ensure_directories() != 0) {
        printf("[EBOOK ERROR] Failed to ensure directories exist\n");
        toast_screen_show("Failed to ensure directories exist", 2000);
        screen_back();
        return;
    }
#endif

    // Initialize state
    memset(&ebook_state, 0, sizeof(ebook_state));

    // Create the main screen
    ui_ebook_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_ebook_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_ebook_screen, lv_color_white(), 0);
    lv_obj_set_style_pad_all(ui_ebook_screen, 0, 0);

    // Create UI components
    create_shelf_ui();
    create_reading_ui();

    // Set up keyboard event handling
    lv_obj_add_event_cb(ui_ebook_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_ebook_screen);
    lv_group_focus_obj(ui_ebook_screen);

    // Scan for books
    ebook_scan_books();

    // Start periodic book scanning timer
    book_scan_timer = lv_timer_create(book_scan_timer_cb, BOOK_SCAN_INTERVAL, NULL);

    // Show shelf initially
    ebook_state.in_reading_mode = false;
    switch_to_shelf_mode();
    ebook_update_shelf_display();

    printf("[%s] E-book screen initialized with %d books\n", ebook_screen.name, ebook_state.shelf.book_count);
}

/**
 * @brief Deinitialize the e-book screen
 */
void ebook_screen_deinit(void)
{
    printf("[%s] Deinitializing e-book screen\n", ebook_screen.name);

    // Clean up resources
    ebook_cleanup();

    // Remove event callback
    if (ui_ebook_screen) {
        lv_obj_remove_event_cb(ui_ebook_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_ebook_screen);
    }

    printf("[%s] E-book screen deinitialized\n", ebook_screen.name);
}
