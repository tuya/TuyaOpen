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
#include "main_screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ENABLE_LVGL_HARDWARE
#include "tkl_fs.h"
#include "tal_api.h"
#include "tkl_output.h"
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

/***********************************************************
************************macro define************************
***********************************************************/

#define EBOOK_MAX_CONTENT_SIZE  (128 * 1024)  /**< Maximum content size (128KB) */
#define EBOOK_LINES_PER_SCREEN  12           /**< Number of visible lines per screen for better readability */
#define EBOOK_CHARS_PER_LINE    80           /**< Maximum characters per line for better screen utilization */
#define BOOK_SCAN_INTERVAL      3000        /**< Book scanning interval in milliseconds (10 seconds) */

#ifdef ENABLE_LVGL_HARDWARE
/* SD card configuration for hardware platform */
#define SDCARD_MOUNT_PATH      "/sdcard"     /**< SD card mount path */
#define SDCARD_MOUNT_RETRY     3             /**< Number of mount retry attempts */
#define SDCARD_MOUNT_DELAY     1000          /**< Delay between mount attempts (ms) */

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
static lv_obj_t *battery_label;
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
static void load_book_position(int book_index);
static void save_book_position(int book_index);
static void update_battery_display(void);
static void ebook_update_shelf_selection(void);
static void book_scan_timer_cb(lv_timer_t *timer);
static bool ebook_compare_book_lists(void);
static int ebook_page_up(void);
static int ebook_page_down(void);
static int find_optimal_end_position(void);
static bool has_meaningful_content_at_position(int position, int min_chars);

// New functions for improved text layout
static void ebook_init_page_metrics_internal(page_metrics_t *metrics, const lv_font_t *font, int display_width, int display_height);
static int ebook_calculate_line_layout_internal(page_layout_t *layout, const char *content, size_t content_size, const page_metrics_t *metrics);
static void ebook_free_line_layout_internal(page_layout_t *layout);
static int ebook_get_screen_text_internal(char *buffer, int buffer_size, const page_layout_t *layout, const char *content, const page_metrics_t *metrics);
static void ebook_update_page_numbers(void);

// New functions for precise line-by-line scrolling
static int ebook_init_screen_display_internal(screen_display_t *screen, const page_layout_t *layout, const char *content, const page_metrics_t *metrics, int top_line_index);
static int ebook_scroll_screen_up_internal(screen_display_t *screen, const page_layout_t *layout, const char *content, const page_metrics_t *metrics);
static int ebook_scroll_screen_down_internal(screen_display_t *screen, const page_layout_t *layout, const char *content, const page_metrics_t *metrics);
static int ebook_generate_screen_text_internal(char *buffer, int buffer_size, const screen_display_t *screen);
static void ebook_extract_line_text(char *line_buffer, int buffer_size, const char *content, const line_info_t *line);

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
    int retry_count = 0;

    printf("[EBOOK] Attempting to mount SD card at %s\n", SDCARD_MOUNT_PATH);

    while (retry_count < SDCARD_MOUNT_RETRY) {
        // Use the same mount method as example_sd.c
        rt = tkl_fs_mount(SDCARD_MOUNT_PATH, DEV_SDCARD);
        if (rt == OPRT_OK) {
            printf("[EBOOK] SD card mounted successfully\n");
            return 0;
        }

        retry_count++;
        ebook_log_error("ebook_mount_sdcard", "Mount failed, retrying", rt);

        if (retry_count < SDCARD_MOUNT_RETRY) {
            tal_system_sleep(SDCARD_MOUNT_DELAY);
        }
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

    // // Check and create txt directory
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
            // Check for .txt extension
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".txt") == 0) {
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
    TUYA_FILE file = tkl_fopen(pos_filename, "r");
    if (file) {
        char buffer[32];
        if (tkl_fread(buffer, sizeof(buffer) - 1, file) > 0) {
            buffer[sizeof(buffer) - 1] = '\0';
            sscanf(buffer, "%d", &book->saved_line);
        }
        tkl_fclose(file);
        printf("Loaded position for %s: line %d\n", book->display_name, book->saved_line);
    }
#else
    FILE *file = fopen(pos_filename, "r");
    if (file) {
        fscanf(file, "%d", &book->saved_line);
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
    TUYA_FILE file = tkl_fopen(pos_filename, "w");
    if (file) {
        char buffer[32];
        int len = snprintf(buffer, sizeof(buffer), "%d", book->saved_line);
        tkl_fwrite(buffer, len, file);
        tkl_fclose(file);
        printf("Saved position for %s: line %d\n", book->display_name, book->saved_line);
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

    // Convert old position to new layout system if available
    if (ebook_state.reading.layout.layout_valid) {
        // Try to find a reasonable line index based on saved character position
        int target_line_index = 0;
        int saved_char_pos = book->saved_line; // This was actually character position

        // Find the line that contains or is closest to the saved character position
        for (int i = 0; i < ebook_state.reading.layout.line_count; i++) {
            const line_info_t *line = &ebook_state.reading.layout.lines[i];
            if (line->char_start >= saved_char_pos) {
                target_line_index = (i > 0) ? i - 1 : 0; // Go to previous line or stay at 0
                break;
            }
            target_line_index = i; // Keep updating until we find the right position
        }

        ebook_state.reading.layout.current_line_index = target_line_index;

        // Re-initialize screen display at the restored position
        if (ebook_init_screen_display_internal(&ebook_state.reading.screen,
                                             &ebook_state.reading.layout,
                                             ebook_state.reading.content,
                                             &ebook_state.reading.metrics,
                                             target_line_index) == 0) {
            printf("Screen display re-initialized at restored position line %d\n", target_line_index);
        }

        ebook_update_page_numbers();

        printf("Converted saved position %d to line index %d\n", saved_char_pos, target_line_index);
    } else {
        // Use legacy character-based position
        ebook_state.reading.current_line = book->saved_line;
    }

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
    ebook_update_reading_display();

    printf("Opened book: %s at position %d\n", book->display_name,
           ebook_state.reading.layout.layout_valid ?
           ebook_state.reading.layout.current_line_index : ebook_state.reading.current_line);
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

    if (file_size > EBOOK_MAX_CONTENT_SIZE) {
        printf("File too large: %ld bytes (max: %d)\n", file_size, EBOOK_MAX_CONTENT_SIZE);
        tkl_fclose(file);
        return false;
    }

    // Allocate memory for content
    if (ebook_state.reading.content) {
        free(ebook_state.reading.content);
    }

    ebook_state.reading.content = malloc(file_size + 1);
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

    if (file_size > EBOOK_MAX_CONTENT_SIZE) {
        printf("File too large: %ld bytes (max: %d)\n", file_size, EBOOK_MAX_CONTENT_SIZE);
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

    // Calculate total lines (legacy support)
    calculate_lines();

    // Initialize page metrics for the reading text area
    // Use the actual dimensions we set for the reading text area
    const int text_display_width = AI_PET_SCREEN_WIDTH - 16 - 8; // width - margins - padding
    const int text_display_height = AI_PET_SCREEN_HEIGHT - 22 - 16 - 6 - 8; // height - title - page_info - margin - padding

    // Initialize page metrics with Montserrat 12 font
    ebook_init_page_metrics_internal(&ebook_state.reading.metrics,
                                   &lv_font_montserrat_12,
                                   text_display_width,
                                   text_display_height);

    // Calculate line layout for the loaded content
    if (ebook_calculate_line_layout_internal(&ebook_state.reading.layout,
                                            ebook_state.reading.content,
                                            ebook_state.reading.content_size,
                                            &ebook_state.reading.metrics) == 0) {
        printf("Line layout calculated successfully for loaded file\n");

        // Initialize screen display system
        if (ebook_init_screen_display_internal(&ebook_state.reading.screen,
                                             &ebook_state.reading.layout,
                                             ebook_state.reading.content,
                                             &ebook_state.reading.metrics,
                                             0) == 0) {
            printf("Screen display system initialized successfully\n");
        } else {
            printf("Warning: Failed to initialize screen display system\n");
        }
    } else {
        printf("Warning: Failed to calculate line layout, using fallback method\n");
    }

    printf("Loaded file: %s (%zu bytes, %d lines, %d layout lines)\n",
           filename, ebook_state.reading.content_size, ebook_state.reading.total_lines,
           ebook_state.reading.layout.line_count);

    return true;
}

/**
 * @brief Calculate total number of lines in the content
 */
static void calculate_lines(void)
{
    if (!ebook_state.reading.content) {
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
 * @brief Update battery display
 */
static void update_battery_display(void)
{
    if (!battery_label) {
        return;
    }

    // Use system battery status - this will get the real battery level from main screen
    static uint8_t battery_level = 85;
    static bool charging = false;

    // In real implementation, this would get actual battery status
    // For now, we simulate it
    static int update_counter = 0;
    update_counter++;
    if (update_counter > 50) {
        update_counter = 0;
        // Simulate battery level changes
        if (!charging && battery_level > 10) {
            battery_level--;
        } else if (charging && battery_level < 100) {
            battery_level++;
        }

        // Update the main screen battery status
        simple_demo_set_battery_status(battery_level, charging);
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

/**
 * @brief Create book shelf UI with list-based scrolling
 */
static void create_shelf_ui(void)
{
    shelf_container = lv_obj_create(ui_ebook_screen);
    lv_obj_set_size(shelf_container, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(shelf_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(shelf_container, 0, 0);
    lv_obj_set_style_pad_all(shelf_container, 5, 0);

    // Title label with auto-refresh indicator
    lv_obj_t *title_label = lv_label_create(shelf_container);
    lv_label_set_text(title_label, "Book Shelf - Auto-Refresh ON");
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 5);

    // Book list using lv_list for automatic scrolling
    shelf_list = lv_list_create(shelf_container);
    lv_obj_set_size(shelf_list, AI_PET_SCREEN_WIDTH - 10, AI_PET_SCREEN_HEIGHT - 60);
    lv_obj_align(shelf_list, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_color(shelf_list, lv_color_white(), 0);
    lv_obj_set_style_border_width(shelf_list, 1, 0);
    lv_obj_set_style_border_color(shelf_list, lv_color_black(), 0);
    lv_obj_set_style_pad_all(shelf_list, 5, 0);

    // Instructions
    lv_obj_t *instr_label = lv_label_create(shelf_container);
    lv_label_set_text(instr_label, LV_SYMBOL_UP LV_SYMBOL_DOWN " Navigate | " LV_SYMBOL_OK " Select | " LV_SYMBOL_CLOSE " Exit");
    lv_obj_set_style_text_color(instr_label, lv_color_make(100, 100, 100), 0);
    lv_obj_set_style_text_font(instr_label, &lv_font_montserrat_10, 0);
    lv_obj_align(instr_label, LV_ALIGN_BOTTOM_MID, 0, -5);
}

/**
 * @brief Create reading UI
 */
static void create_reading_ui(void)
{
    reading_container = lv_obj_create(ui_ebook_screen);
    lv_obj_set_size(reading_container, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(reading_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(reading_container, 0, 0);
    lv_obj_set_style_pad_all(reading_container, 3, 0); // Reduced padding for more space

    // Book title at top center
    book_title_label = lv_label_create(reading_container);
    lv_obj_set_size(book_title_label, AI_PET_SCREEN_WIDTH - 70, 18); // Adjusted for 384x168 screen
    lv_obj_align(book_title_label, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_text_color(book_title_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(book_title_label, &lv_font_montserrat_12, 0); // Smaller font for title
    lv_obj_set_style_text_align(book_title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(book_title_label, LV_LABEL_LONG_DOT);

    // Battery indicator at top right
    battery_label = lv_label_create(reading_container);
    lv_obj_set_size(battery_label, 55, 18);
    lv_obj_align(battery_label, LV_ALIGN_TOP_RIGHT, -3, 2);
    lv_obj_set_style_text_color(battery_label, lv_color_make(100, 100, 100), 0);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(battery_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_FULL "85%"); // Default battery level

    // Main text display area - optimized for natural reading with LVGL wrapping (original behavior)
    reading_text = lv_label_create(reading_container);
    // Calculate available space: total height - title area - page info area - padding
    int text_height = AI_PET_SCREEN_HEIGHT - 22 - 16 - 6; // 168 - 22 - 16 - 6 = 124
    lv_obj_set_size(reading_text, AI_PET_SCREEN_WIDTH - 16, text_height); // Comfortable margins
    lv_obj_align(reading_text, LV_ALIGN_TOP_LEFT, 8, 22); // Balanced margins
    lv_obj_set_style_text_color(reading_text, lv_color_black(), 0);
    lv_obj_set_style_text_font(reading_text, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(reading_text, LV_LABEL_LONG_WRAP); // Let LVGL handle wrapping naturally (restored)
    lv_obj_set_style_text_line_space(reading_text, 2, 0); // Natural line spacing for reading
    lv_obj_set_style_pad_all(reading_text, 4, 0); // Comfortable padding (restored)
    lv_obj_set_style_text_align(reading_text, LV_TEXT_ALIGN_LEFT, 0); // Left align text

    // Page info at very bottom of screen
    page_info_label = lv_label_create(reading_container);
    lv_obj_set_size(page_info_label, AI_PET_SCREEN_WIDTH - 16, 14);
    lv_obj_align(page_info_label, LV_ALIGN_BOTTOM_MID, 0, -1); // Very bottom
    lv_obj_set_style_text_color(page_info_label, lv_color_make(80, 80, 80), 0);
    lv_obj_set_style_text_font(page_info_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(page_info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(page_info_label, lv_color_make(240, 240, 240), 0); // Light background
    lv_obj_set_style_bg_opa(page_info_label, LV_OPA_80, 0);

    // Initially hidden
    lv_obj_add_flag(reading_container, LV_OBJ_FLAG_HIDDEN);
}/**
 * @brief Switch to shelf mode
 */
static void switch_to_shelf_mode(void)
{
    lv_obj_clear_flag(shelf_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(reading_container, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief Switch to reading mode
 */
static void switch_to_reading_mode(void)
{
    lv_obj_add_flag(shelf_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(reading_container, LV_OBJ_FLAG_HIDDEN);
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
        lv_obj_set_style_text_color(empty_btn, lv_color_make(150, 150, 150), 0);
        return;
    }

    // Create list items for all books
    for (int i = 0; i < ebook_state.shelf.book_count; i++) {
        book_entry_t *book = &ebook_state.shelf.books[i];

        // Create list button with book icon and name
        lv_obj_t *book_btn = lv_list_add_btn(shelf_list, LV_SYMBOL_FILE, book->display_name);

        // Set font and basic styling
        lv_obj_set_style_text_font(book_btn, &lv_font_montserrat_12, 0);

        // Add book info (pages/size) on the right
        if (book->total_lines > 0) {
            lv_obj_t *info_label = lv_label_create(book_btn);
            char info_text[32];
            int pages = (book->total_lines + EBOOK_LINES_PER_SCREEN - 1) / EBOOK_LINES_PER_SCREEN;
            snprintf(info_text, sizeof(info_text), "%d pages", pages);
            lv_label_set_text(info_label, info_text);
            lv_obj_align(info_label, LV_ALIGN_RIGHT_MID, -10, 0);
            lv_obj_set_style_text_color(info_label, lv_color_make(100, 100, 100), 0);
            lv_obj_set_style_text_font(info_label, &lv_font_montserrat_10, 0);
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
        lv_obj_set_style_bg_color(selected_btn, lv_color_make(0, 100, 200), 0);
        lv_obj_set_style_text_color(selected_btn, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(selected_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(selected_btn, 4, 0);

        // Scroll to view with animation (key feature from food menu)
        lv_obj_scroll_to_view(selected_btn, LV_ANIM_ON);
    }
}

/**
 * @brief Update reading display with improved layout-based positioning
 */
void ebook_update_reading_display(void)
{
    if (!reading_text || !ebook_state.reading.content_loaded) {
        if (reading_text) {
            lv_label_set_text(reading_text, "No content loaded");
        }
        if (page_info_label) {
            lv_label_set_text(page_info_label, LV_SYMBOL_CLOSE " Back to shelf");
        }
        if (book_title_label) {
            lv_label_set_text(book_title_label, "No Book Selected");
        }
        return;
    }

    // Update book title
    if (book_title_label) {
        // Find current book to get display name
        char *display_name = "Unknown Book";
        for (int i = 0; i < ebook_state.shelf.book_count; i++) {
            if (strcmp(ebook_state.shelf.books[i].filename, ebook_state.reading.current_book) == 0) {
                display_name = ebook_state.shelf.books[i].display_name;
                break;
            }
        }
        lv_label_set_text(book_title_label, display_name);
    }

    // Use simplified layout-based display (combining previous correct implementations)
    if (ebook_state.reading.layout.layout_valid) {
        // Get current position as character offset
        int char_offset = 0;
        if (ebook_state.reading.layout.current_line_index < ebook_state.reading.layout.line_count) {
            const line_info_t *current_line = &ebook_state.reading.layout.lines[ebook_state.reading.layout.current_line_index];
            char_offset = current_line->char_start;
        }

        printf("Display update: showing from line %d (char offset %d)\n", ebook_state.reading.layout.current_line_index, char_offset);

        // Use the original character-based display method with layout position tracking
        char *content_start = ebook_state.reading.content;
        char *content_ptr = content_start + char_offset;

        // Build display text for current screen (original method)
        static char display_text[4096];
        int chars_copied = 0;
        const int screen_char_limit = 1800;

        while (*content_ptr && chars_copied < screen_char_limit &&
               chars_copied < sizeof(display_text) - 100 &&
               (content_ptr - content_start) < (int)ebook_state.reading.content_size) {

            // Handle paragraph breaks naturally (original logic)
            if (*content_ptr == '\n') {
                char *next_ptr = content_ptr + 1;
                bool is_paragraph_break = false;

                // Skip carriage returns
                while (*next_ptr == '\r' && (next_ptr - content_start) < (int)ebook_state.reading.content_size) {
                    next_ptr++;
                }

                // Check for paragraph break (double newline)
                if (*next_ptr == '\n') {
                    is_paragraph_break = true;
                }

                if (is_paragraph_break) {
                    // Preserve paragraph break
                    display_text[chars_copied++] = '\n';
                    display_text[chars_copied++] = '\n';
                    content_ptr++;
                    // Skip additional newlines/returns
                    while ((*content_ptr == '\n' || *content_ptr == '\r') &&
                           (content_ptr - content_start) < (int)ebook_state.reading.content_size) {
                        content_ptr++;
                    }
                    continue;
                } else {
                    // Single newline - convert to space for natural flow
                    if (chars_copied > 0 && display_text[chars_copied-1] != ' ' && display_text[chars_copied-1] != '\n') {
                        display_text[chars_copied++] = ' ';
                    }
                    content_ptr++;
                    continue;
                }
            }

            // Skip carriage returns
            if (*content_ptr == '\r') {
                content_ptr++;
                continue;
            }

            // Copy regular characters
            display_text[chars_copied++] = *content_ptr;
            content_ptr++;
        }

        display_text[chars_copied] = '\0';

        // Set text and let LVGL handle natural wrapping (original behavior)
        lv_label_set_text(reading_text, display_text);

        // Update page info with layout information
        ebook_update_page_numbers();
        static char page_text[100];
        snprintf(page_text, sizeof(page_text),
                "Line %d/%d | Page %d/%d " LV_SYMBOL_UP LV_SYMBOL_DOWN " Line " LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " Page " LV_SYMBOL_CLOSE " Back",
                ebook_state.reading.layout.current_line_index + 1,
                ebook_state.reading.layout.line_count,
                ebook_state.reading.layout.current_page, ebook_state.reading.layout.total_pages);
        lv_label_set_text(page_info_label, page_text);
    } else {
        // Fallback to original character-based method
        printf("Warning: Using fallback display method - layout not available\n");

        // Use original character offset method based on current_line (fallback)
        char *content_start = ebook_state.reading.content;
        char *content_ptr = content_start;

        // Skip to current_line position (simplified fallback)
        int lines_skipped = 0;
        while (*content_ptr && lines_skipped < ebook_state.reading.current_line &&
               (content_ptr - content_start) < (int)ebook_state.reading.content_size) {
            if (*content_ptr == '\n') {
                lines_skipped++;
            }
            content_ptr++;
        }

        // Generate display text using original algorithm
        static char display_text[4096];
        int chars_copied = 0;
        const int screen_char_limit = 1800;

        while (*content_ptr && chars_copied < screen_char_limit &&
               chars_copied < sizeof(display_text) - 100 &&
               (content_ptr - content_start) < (int)ebook_state.reading.content_size) {

            if (*content_ptr == '\n') {
                char *next_ptr = content_ptr + 1;
                while (*next_ptr == '\r' && (next_ptr - content_start) < (int)ebook_state.reading.content_size) {
                    next_ptr++;
                }

                if (*next_ptr == '\n') {
                    // Paragraph break
                    display_text[chars_copied++] = '\n';
                    display_text[chars_copied++] = '\n';
                    content_ptr++;
                    while ((*content_ptr == '\n' || *content_ptr == '\r') &&
                           (content_ptr - content_start) < (int)ebook_state.reading.content_size) {
                        content_ptr++;
                    }
                    continue;
                } else {
                    // Single newline to space
                    if (chars_copied > 0 && display_text[chars_copied-1] != ' ' && display_text[chars_copied-1] != '\n') {
                        display_text[chars_copied++] = ' ';
                    }
                    content_ptr++;
                    continue;
                }
            }

            if (*content_ptr == '\r') {
                content_ptr++;
                continue;
            }

            display_text[chars_copied++] = *content_ptr;
            content_ptr++;
        }

        display_text[chars_copied] = '\0';
        lv_label_set_text(reading_text, display_text);

        // Simple page info
        int current_offset = content_ptr - content_start;
        static char page_text[100];
        snprintf(page_text, sizeof(page_text),
                "Position %d/%zu " LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " Page " LV_SYMBOL_CLOSE " Back",
                current_offset, ebook_state.reading.content_size);
        lv_label_set_text(page_info_label, page_text);
    }

    // Update battery display
    update_battery_display();
}

/**
 * @brief Page up function for left key with improved line-based navigation
 */
static int ebook_page_up(void)
{
    if (!ebook_state.in_reading_mode) {
        return -1;
    }

    if (ebook_state.reading.layout.layout_valid) {
        // Use improved layout-based page navigation
        int lines_per_screen = ebook_state.reading.metrics.lines_per_screen;
        int current_line = ebook_state.reading.layout.current_line_index;

        // Calculate new line index - jump by full screen minus one line for continuity
        int new_line_index = current_line - (lines_per_screen - 1);

        // Ensure we don't go below 0
        if (new_line_index < 0) {
            new_line_index = 0;
        }

        // Update layout position to the new page start
        ebook_state.reading.layout.current_line_index = new_line_index;

        // Re-initialize screen display at new position
        if (ebook_state.reading.screen.screen_valid) {
            if (ebook_init_screen_display_internal(&ebook_state.reading.screen,
                                                 &ebook_state.reading.layout,
                                                 ebook_state.reading.content,
                                                 &ebook_state.reading.metrics,
                                                 new_line_index) == 0) {
                printf("Page up: re-initialized screen from line %d (jumped by %d lines)\n",
                       new_line_index, lines_per_screen - 1);
            }
        }

        ebook_update_page_numbers();
        ebook_update_reading_display();
        ebook_save_position();

        printf("Page up: moved from line %d to line %d\n", current_line, new_line_index);
        return 0;
    } else {
        // Fallback to character-based navigation
        const int chars_per_screen = 1600;

        if (ebook_state.reading.current_line >= chars_per_screen) {
            ebook_state.reading.current_line -= chars_per_screen;
        } else {
            ebook_state.reading.current_line = 0;
        }

        ebook_update_reading_display();
        ebook_save_position();
        return 0;
    }
}

/**
 * @brief Page down function for right key with improved line-based navigation
 */
static int ebook_page_down(void)
{
    if (!ebook_state.in_reading_mode) {
        return -1;
    }

    if (ebook_state.reading.layout.layout_valid) {
        // Use improved layout-based page navigation
        int lines_per_screen = ebook_state.reading.metrics.lines_per_screen;
        int current_line = ebook_state.reading.layout.current_line_index;
        int total_lines = ebook_state.reading.layout.line_count;

        // Calculate new line index - jump by full screen minus one line for continuity
        int new_line_index = current_line + lines_per_screen - 1;

        // Calculate the maximum line we can scroll to (leave room for a full screen)
        int max_scroll_line = total_lines - lines_per_screen;
        if (max_scroll_line < 0) max_scroll_line = 0;

        // Ensure we don't go beyond the end
        if (new_line_index > max_scroll_line) {
            new_line_index = max_scroll_line;
        }

        // Only update if we actually moved
        if (new_line_index != current_line) {
            // Update layout position to the new page start
            ebook_state.reading.layout.current_line_index = new_line_index;

            // Re-initialize screen display at new position
            if (ebook_state.reading.screen.screen_valid) {
                if (ebook_init_screen_display_internal(&ebook_state.reading.screen,
                                                     &ebook_state.reading.layout,
                                                     ebook_state.reading.content,
                                                     &ebook_state.reading.metrics,
                                                     new_line_index) == 0) {
                    printf("Page down: re-initialized screen from line %d (jumped by %d lines)\n",
                           new_line_index, lines_per_screen - 1);
                }
            }

            ebook_update_page_numbers();
            ebook_update_reading_display();
            ebook_save_position();

            printf("Page down: moved from line %d to line %d (max: %d)\n", current_line, new_line_index, max_scroll_line);
        } else {
            printf("Page down: already at end, staying at line %d\n", current_line);
        }

        return 0;
    } else {
        // Fallback to character-based navigation with improved end detection
        const int chars_per_screen = 1600;
        int new_position = ebook_state.reading.current_line + chars_per_screen;
        int content_size = (int)ebook_state.reading.content_size;

        // First, try the normal increment
        if (new_position < content_size - 300) {
            // Normal case - we have plenty of content ahead
            ebook_state.reading.current_line = new_position;
        } else {
            // We're approaching the end - use optimal end position
            int optimal_end = find_optimal_end_position();

            // If we're already at or past the optimal end position, don't move
            if (ebook_state.reading.current_line >= optimal_end) {
                // Check if we can show a bit more content by moving slightly forward
                int small_increment = 100;
                int test_position = ebook_state.reading.current_line + small_increment;

                if (test_position < content_size - 50 &&
                    has_meaningful_content_at_position(test_position, 50)) {
                    ebook_state.reading.current_line = test_position;
                } else {
                    // No more meaningful content, stay where we are
                    return 0;
                }
            } else {
                // Move to the optimal end position
                ebook_state.reading.current_line = optimal_end;
            }
        }

        ebook_update_reading_display();
        ebook_save_position();
        return 0;
    }
}

/**
 * @brief Navigate up in current interface with precise line-by-line scrolling
 */
int ebook_navigate_up(void)
{
    if (ebook_state.in_reading_mode) {
        if (ebook_state.reading.layout.layout_valid && ebook_state.reading.screen.screen_valid) {
            // Use new precise line scrolling system
            int result = ebook_scroll_screen_up_internal(&ebook_state.reading.screen,
                                                       &ebook_state.reading.layout,
                                                       ebook_state.reading.content,
                                                       &ebook_state.reading.metrics);
            if (result == 0) {
                // Update layout current line index to match screen
                ebook_state.reading.layout.current_line_index = ebook_state.reading.screen.top_line_index;
                ebook_update_page_numbers();
                ebook_update_reading_display();
                ebook_save_position();
                printf("Precise scroll up: screen top line %d\n", ebook_state.reading.screen.top_line_index);
                return 0;
            } else {
                printf("Cannot scroll up further\n");
                return 0; // Already at top
            }
        } else if (ebook_state.reading.layout.layout_valid) {
            // Fallback to layout-based navigation if screen not initialized
            int current_line = ebook_state.reading.layout.current_line_index;
            if (current_line > 0) {
                ebook_state.reading.layout.current_line_index = current_line - 1;

                // Re-initialize screen display at new position
                ebook_init_screen_display_internal(&ebook_state.reading.screen,
                                                 &ebook_state.reading.layout,
                                                 ebook_state.reading.content,
                                                 &ebook_state.reading.metrics,
                                                 ebook_state.reading.layout.current_line_index);

                ebook_update_page_numbers();
                ebook_update_reading_display();
                ebook_save_position();
                printf("Layout scroll up: moved to line %d\n", ebook_state.reading.layout.current_line_index);
                return 0;
            }
            printf("Already at first line, cannot scroll up\n");
            return 0;
        } else {
            // Fallback to character-based scrolling
            const int chars_per_line = 40;

            if (ebook_state.reading.current_line <= 0) {
                ebook_state.reading.current_line = 0;
                return 0;
            }

            int new_position = ebook_state.reading.current_line - chars_per_line;
            if (new_position < 0) {
                new_position = 0;
            }

            if (new_position != ebook_state.reading.current_line) {
                ebook_state.reading.current_line = new_position;
                ebook_update_reading_display();
                ebook_save_position();
                printf("Fallback scroll up: moved to position %d\n", new_position);
            }

            return 0;
        }
    } else {
        // Shelf mode - move selection up (simplified like food menu)
        if (ebook_state.shelf.selected_book > 0) {
            ebook_state.shelf.selected_book--;
            ebook_update_shelf_selection();
            printf("Shelf nav up: selected book %d\n", ebook_state.shelf.selected_book);
            return 0;
        }
    }
    return -1;
}

/**
 * @brief Navigate down in current interface with precise line-by-line scrolling
 */
int ebook_navigate_down(void)
{
    if (ebook_state.in_reading_mode) {
        if (ebook_state.reading.layout.layout_valid && ebook_state.reading.screen.screen_valid) {
            // Use new precise line scrolling system
            int result = ebook_scroll_screen_down_internal(&ebook_state.reading.screen,
                                                         &ebook_state.reading.layout,
                                                         ebook_state.reading.content,
                                                         &ebook_state.reading.metrics);
            if (result == 0) {
                // Update layout current line index to match screen
                ebook_state.reading.layout.current_line_index = ebook_state.reading.screen.top_line_index;
                ebook_update_page_numbers();
                ebook_update_reading_display();
                ebook_save_position();
                printf("Precise scroll down: screen top line %d\n", ebook_state.reading.screen.top_line_index);
                return 0;
            } else {
                printf("Cannot scroll down further\n");
                return 0; // Already at bottom
            }
        } else if (ebook_state.reading.layout.layout_valid) {
            // Fallback to layout-based navigation if screen not initialized
            int current_line = ebook_state.reading.layout.current_line_index;
            int total_lines = ebook_state.reading.layout.line_count;
            int lines_per_screen = ebook_state.reading.metrics.lines_per_screen;

            int max_scroll_line = total_lines - lines_per_screen;
            if (max_scroll_line < 0) max_scroll_line = 0;

            if (current_line < max_scroll_line) {
                ebook_state.reading.layout.current_line_index = current_line + 1;

                // Re-initialize screen display at new position
                ebook_init_screen_display_internal(&ebook_state.reading.screen,
                                                 &ebook_state.reading.layout,
                                                 ebook_state.reading.content,
                                                 &ebook_state.reading.metrics,
                                                 ebook_state.reading.layout.current_line_index);

                ebook_update_page_numbers();
                ebook_update_reading_display();
                ebook_save_position();
                printf("Layout scroll down: moved to line %d (max: %d)\n", ebook_state.reading.layout.current_line_index, max_scroll_line);
                return 0;
            }
            printf("Already at last possible line (%d), cannot scroll down\n", max_scroll_line);
            return 0;
        } else {
            // Fallback to character-based scrolling
            const int chars_per_line = 40;
            int content_size = (int)ebook_state.reading.content_size;

            int new_position = ebook_state.reading.current_line + chars_per_line;

            if (new_position < content_size - 400) {
                ebook_state.reading.current_line = new_position;
            } else {
                int optimal_end = find_optimal_end_position();

                if (ebook_state.reading.current_line >= optimal_end) {
                    int small_increment = 20;
                    int test_position = ebook_state.reading.current_line + small_increment;

                    if (test_position < content_size - 50 &&
                        has_meaningful_content_at_position(test_position, 20)) {
                        ebook_state.reading.current_line = test_position;
                    } else {
                        printf("No more content to scroll, staying at position %d\n", ebook_state.reading.current_line);
                        return 0;
                    }
                } else {
                    int target = (ebook_state.reading.current_line + chars_per_line > optimal_end) ?
                               optimal_end : ebook_state.reading.current_line + chars_per_line;
                    ebook_state.reading.current_line = target;
                }
            }

            ebook_update_reading_display();
            ebook_save_position();
            printf("Fallback scroll down: moved to position %d\n", ebook_state.reading.current_line);
            return 0;
        }
    } else {
        // Shelf mode - move selection down (simplified like food menu)
        if (ebook_state.shelf.selected_book < ebook_state.shelf.book_count - 1) {
            ebook_state.shelf.selected_book++;
            ebook_update_shelf_selection();
            printf("Shelf nav down: selected book %d\n", ebook_state.shelf.selected_book);
            return 0;
        }
    }
    return -1;
}

/**
 * @brief Handle enter/select action
 */
int ebook_handle_select(void)
{
    if (ebook_state.in_reading_mode) {
        // Reading mode - page down (same as right key)
        return ebook_page_down();
    } else {
        // Shelf mode - open selected book
        if (ebook_state.shelf.book_count > 0 && ebook_state.shelf.selected_book >= 0) {
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
            // Save position based on layout system if available
            if (ebook_state.reading.layout.layout_valid &&
                ebook_state.reading.layout.current_line_index < ebook_state.reading.layout.line_count) {
                // Convert line index to character position for compatibility
                const line_info_t *current_line = &ebook_state.reading.layout.lines[ebook_state.reading.layout.current_line_index];
                ebook_state.shelf.books[i].saved_line = current_line->char_start;
            } else {
                // Use legacy character position
                ebook_state.shelf.books[i].saved_line = ebook_state.reading.current_line;
            }

            save_book_position(i);
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
        return 0;
    }

    // Find the current book in shelf
    for (int i = 0; i < ebook_state.shelf.book_count; i++) {
        if (strcmp(ebook_state.shelf.books[i].filename, ebook_state.reading.current_book) == 0) {
            load_book_position(i);
            return ebook_state.shelf.books[i].saved_line;
        }
    }
    return 0;
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
            printf("UP key pressed - scroll up one line\n");
            ebook_navigate_up();
            break;
        case KEY_DOWN:
            printf("DOWN key pressed - scroll down one line\n");
            ebook_navigate_down();
            break;
        case KEY_LEFT:
            printf("LEFT key pressed - page up\n");
            if (ebook_state.in_reading_mode) {
                ebook_page_up();
            }
            break;
        case KEY_RIGHT:
            printf("RIGHT key pressed - page down\n");
            if (ebook_state.in_reading_mode) {
                ebook_page_down();
            }
            break;
        case KEY_ENTER:
            printf("ENTER key pressed\n");
            ebook_handle_select();
            break;
        case KEY_ESC:
            printf("ESC key pressed\n");
            ebook_handle_back();
            break;
        default:
            printf("Unknown key pressed: %d\n", key);
            break;
    }
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
        lv_timer_del(book_scan_timer);
        book_scan_timer = NULL;
        printf("Book scanning timer stopped\n");
    }

    // Free line layout memory
    ebook_free_line_layout_internal(&ebook_state.reading.layout);

    // Free allocated content
    if (ebook_state.reading.content) {
        free(ebook_state.reading.content);
        ebook_state.reading.content = NULL;
    }

    // Reset state
    memset(&ebook_state, 0, sizeof(ebook_state));
}

/**
 * @brief Initialize the e-book screen
 */
void ebook_screen_init(void)
{
    printf("Initializing enhanced e-book screen with book shelf\n");

#ifdef ENABLE_LVGL_HARDWARE
    // Initialize SD card for hardware platform
    printf("Hardware platform detected, initializing SD card...\n");
    if (ebook_mount_sdcard() != 0) {
        printf("Warning: SD card initialization failed, some features may not work\n");
    }

    // Ensure required directories exist
    if (ebook_ensure_directories() != 0) {
        printf("Warning: Failed to create required directories\n");
    }
#endif

    // Initialize state
    memset(&ebook_state, 0, sizeof(ebook_state));
    ebook_state.in_reading_mode = false;
    ebook_state.shelf.selected_book = 0;
    // No need for shelf_scroll with list-based scrolling

    // Create the main screen
    ui_ebook_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_ebook_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_ebook_screen, lv_color_white(), 0);
    lv_obj_set_style_pad_all(ui_ebook_screen, 0, 0);

    // Create UI containers
    create_shelf_ui();
    create_reading_ui();

    // Scan for books
    int book_count = ebook_scan_books();
    if (book_count > 0) {
        printf("Found %d books, displaying shelf\n", book_count);
        ebook_update_shelf_display();
    } else {
        printf("No books found in directory: %s\n", EBOOK_TXT_DIR);
        ebook_update_shelf_display(); // Will show "No books found" message
    }

    // Set up keyboard event handling
    lv_obj_add_event_cb(ui_ebook_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_ebook_screen);
    lv_group_focus_obj(ui_ebook_screen);

    // Start periodic book scanning timer
    book_scan_timer = lv_timer_create(book_scan_timer_cb, BOOK_SCAN_INTERVAL, NULL);
    if (book_scan_timer) {
        printf("Book scanning timer started (interval: %d ms)\n", BOOK_SCAN_INTERVAL);
    } else {
        printf("Warning: Failed to create book scanning timer\n");
    }

    // Start in shelf mode
    switch_to_shelf_mode();
}

/**
 * @brief Deinitialize the e-book screen
 */
void ebook_screen_deinit(void)
{
    printf("Deinitializing e-book screen\n");

    // Clean up resources (includes stopping timer)
    ebook_cleanup();

    // Remove event callbacks and focus
    if (ui_ebook_screen) {
        lv_obj_remove_event_cb(ui_ebook_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_ebook_screen);
    }
}

/**
 * @brief Check if there's meaningful content at a given position
 */
static bool has_meaningful_content_at_position(int position, int min_chars)
{
    if (!ebook_state.reading.content || position < 0 ||
        position >= (int)ebook_state.reading.content_size) {
        return false;
    }

    char *ptr = ebook_state.reading.content + position;
    int meaningful_chars = 0;
    int chars_checked = 0;
    const int max_check = min_chars * 2; // Check double the minimum required

    while (*ptr && chars_checked < max_check &&
           (ptr - ebook_state.reading.content) < (int)ebook_state.reading.content_size) {

        if (*ptr != ' ' && *ptr != '\n' && *ptr != '\r' && *ptr != '\t') {
            meaningful_chars++;
        }

        chars_checked++;
        ptr++;

        if (meaningful_chars >= min_chars) {
            return true;
        }
    }

    return meaningful_chars >= min_chars;
}

/**
 * @brief Find optimal end position for displaying the last page
 */
static int find_optimal_end_position(void)
{
    if (!ebook_state.reading.content || ebook_state.reading.content_size == 0) {
        return 0;
    }

    const int screen_char_limit = 1600;
    const int min_meaningful_chars = 200; // Minimum characters for meaningful content
    int total_size = (int)ebook_state.reading.content_size;

    // Try positions working backwards from the end
    for (int offset = 100; offset <= screen_char_limit; offset += 100) {
        int test_position = total_size - offset;

        if (test_position < 0) {
            return 0;
        }

        // Check if this position has meaningful content
        if (has_meaningful_content_at_position(test_position, min_meaningful_chars)) {
            // This position has good content, check if it's not too early
            if (test_position <= total_size - 50) { // Not too close to the very end
                return test_position;
            }
        }
    }

    // If no good position found, return a safe default
    int safe_position = total_size - 800;
    return safe_position > 0 ? safe_position : 0;
}

/***********************************************************
****************New Layout Functions Implementation*********
***********************************************************/

/**
 * @brief Initialize page metrics based on font and screen size
 */
static void ebook_init_page_metrics_internal(page_metrics_t *metrics, const lv_font_t *font, int display_width, int display_height)
{
    if (!metrics || !font) {
        return;
    }

    metrics->font = font;
    metrics->display_width = display_width;
    metrics->display_height = display_height;

    // Get font metrics - for Montserrat 12, typical values
    metrics->font_height = lv_font_get_line_height(font);

    // Calculate average character width using a mix of characters for better estimation
    lv_point_t size_m, size_w, size_i;
    lv_txt_get_size(&size_m, "M", font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_txt_get_size(&size_w, "W", font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_txt_get_size(&size_i, "i", font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

    // Use average of wide and narrow characters for better estimation
    metrics->char_width = (size_m.x + size_w.x + size_i.x) / 3;
    // Add a small buffer for character spacing
    metrics->char_width += 1;

    // Calculate layout parameters more conservatively
    const int padding = 8; // Account for both widget padding and text padding
    int usable_width = display_width - (padding * 2);
    int usable_height = display_height - (padding * 2);

    // Calculate characters per line conservatively for position tracking (not display)
    metrics->chars_per_line = (usable_width / metrics->char_width) - 4; // More conservative buffer
    if (metrics->chars_per_line < 20) metrics->chars_per_line = 20; // Minimum readable width

    // Keep it reasonable for position tracking - original behavior
    if (metrics->chars_per_line > 60) {
        metrics->chars_per_line = 60; // Conservative for accurate position tracking
    }

    printf("Width calculation: usable_width=%d, char_width=%d, chars_per_line=%d (for position tracking)\n",
           usable_width, metrics->char_width, metrics->chars_per_line);

    // Calculate lines per screen more conservatively
    metrics->lines_per_screen = usable_height / (metrics->font_height + 2); // Account for line spacing
    if (metrics->lines_per_screen < 3) metrics->lines_per_screen = 3; // Minimum readable height
    if (metrics->lines_per_screen > 12) metrics->lines_per_screen = 12; // Maximum for good readability

    // Calculate total characters per screen
    metrics->chars_per_screen = metrics->chars_per_line * metrics->lines_per_screen;

    printf("Page metrics initialized: display_size=%dx%d, font_height=%d, char_width=%d, chars_per_line=%d, lines_per_screen=%d, total_chars=%d\n",
           display_width, display_height, metrics->font_height, metrics->char_width,
           metrics->chars_per_line, metrics->lines_per_screen, metrics->chars_per_screen);
}

/**
 * @brief Calculate line layout for text content
 */
static int ebook_calculate_line_layout_internal(page_layout_t *layout, const char *content, size_t content_size, const page_metrics_t *metrics)
{
    if (!layout || !content || !metrics || content_size == 0) {
        return -1;
    }

    // Free existing layout
    ebook_free_line_layout_internal(layout);

    // Estimate initial number of lines (overestimate for safety)
    int estimated_lines = (content_size / metrics->chars_per_line) + 100;
    layout->lines = malloc(estimated_lines * sizeof(line_info_t));
    if (!layout->lines) {
        printf("Failed to allocate memory for line layout\n");
        return -1;
    }

    layout->line_count = 0;
    layout->current_line_index = 0;
    layout->current_page = 1;
    layout->layout_valid = false;

    const char *ptr = content;
    const char *content_end = content + content_size;
    int char_position = 0;

    while (ptr < content_end && layout->line_count < estimated_lines - 1) {
        line_info_t *line = &layout->lines[layout->line_count];
        line->char_start = char_position;
        line->char_count = 0;
        line->is_paragraph_end = false;

        const char *line_start = ptr;
        int line_chars = 0;
        bool forced_break = false;

        // Process characters for this line
        while (ptr < content_end && line_chars < metrics->chars_per_line) {
            char c = *ptr;

            if (c == '\n') {
                // Handle newline - always treat as line break for accurate position tracking
                ptr++;
                char_position++;

                // Look ahead for another newline (paragraph break)
                if (ptr < content_end && (*ptr == '\n' || *ptr == '\r')) {
                    line->is_paragraph_end = true;
                    // Skip additional newlines/returns
                    while (ptr < content_end && (*ptr == '\n' || *ptr == '\r')) {
                        ptr++;
                        char_position++;
                    }
                }

                forced_break = true;
                break;
            } else if (c == '\r') {
                // Skip carriage returns
                ptr++;
                char_position++;
                continue;
            } else {
                // Regular character
                line_chars++;
                ptr++;
                char_position++;
            }
        }

        // If we filled the line without a natural break, try to break at word boundary
        if (!forced_break && ptr < content_end && line_chars >= metrics->chars_per_line) {
            // Look back for a space to break on (original conservative approach)
            const char *break_ptr = ptr - 1;
            int back_chars = 0;

            while (break_ptr > line_start && back_chars < 20) { // Look back max 20 chars (restored)
                if (*break_ptr == ' ' || *break_ptr == '\t') {
                    // Found a good break point
                    int chars_to_remove = (ptr - break_ptr - 1);
                    ptr = break_ptr + 1; // Skip the space
                    char_position -= chars_to_remove;
                    line_chars -= chars_to_remove;
                    break;
                }
                break_ptr--;
                back_chars++;
            }
        }

        line->char_count = line_chars;

        // Adjust for trailing spaces (don't count them)
        while (line->char_count > 0) {
            char last_char = content[line->char_start + line->char_count - 1];
            if (last_char == ' ' || last_char == '\t') {
                line->char_count--;
            } else {
                break;
            }
        }

        // Debug output for first few lines
        if (layout->line_count < 5) {
            printf("Line %d: start=%d, count=%d, content=%.20s...\n",
                   layout->line_count, line->char_start, line->char_count,
                   content + line->char_start);
        }

        layout->line_count++;

        // Reallocate if we're running out of space
        if (layout->line_count >= estimated_lines - 10) {
            estimated_lines += 100;
            line_info_t *new_lines = realloc(layout->lines, estimated_lines * sizeof(line_info_t));
            if (!new_lines) {
                printf("Failed to reallocate memory for line layout\n");
                ebook_free_line_layout_internal(layout);
                return -1;
            }
            layout->lines = new_lines;
        }
    }

    // Calculate total pages
    layout->total_pages = (layout->line_count + metrics->lines_per_screen - 1) / metrics->lines_per_screen;
    if (layout->total_pages < 1) layout->total_pages = 1;

    layout->layout_valid = true;

    printf("Line layout calculated: %d lines, %d pages\n", layout->line_count, layout->total_pages);
    return 0;
}

/**
 * @brief Free line layout memory
 */
static void ebook_free_line_layout_internal(page_layout_t *layout)
{
    if (!layout) {
        return;
    }

    if (layout->lines) {
        free(layout->lines);
        layout->lines = NULL;
    }

    layout->line_count = 0;
    layout->current_line_index = 0;
    layout->current_page = 1;
    layout->total_pages = 0;
    layout->layout_valid = false;
}

/**
 * @brief Get text content for current screen
 */
static int ebook_get_screen_text_internal(char *buffer, int buffer_size, const page_layout_t *layout, const char *content, const page_metrics_t *metrics)
{
    if (!buffer || !layout || !content || !metrics || !layout->layout_valid) {
        return -1;
    }

    buffer[0] = '\0';
    int buffer_pos = 0;
    int lines_displayed = 0;
    int start_line = layout->current_line_index;
    int end_line = start_line + metrics->lines_per_screen;

    if (end_line > layout->line_count) {
        end_line = layout->line_count;
    }

    printf("Screen text: showing lines %d to %d (total %d lines)\n", start_line, end_line - 1, layout->line_count);

    for (int i = start_line; i < end_line && lines_displayed < metrics->lines_per_screen && buffer_pos < buffer_size - 10; i++) {
        const line_info_t *line = &layout->lines[i];

        // Add line content with improved space handling
        if (line->char_count > 0 && line->char_start < (int)strlen(content)) {
            int chars_to_copy = line->char_count;
            if (buffer_pos + chars_to_copy + 2 >= buffer_size) {
                chars_to_copy = buffer_size - buffer_pos - 2;
            }

            if (chars_to_copy > 0) {
                // Copy content while handling newlines that were converted to spaces
                const char *src = content + line->char_start;
                for (int j = 0; j < chars_to_copy && buffer_pos < buffer_size - 2; j++) {
                    char c = src[j];
                    if (c == '\n') {
                        // Convert internal newlines to spaces for better line flow
                        buffer[buffer_pos++] = ' ';
                    } else if (c != '\r') {
                        // Skip carriage returns, copy everything else
                        buffer[buffer_pos++] = c;
                    }
                }
            }
        }

        // Add line ending
        if (buffer_pos + 2 < buffer_size) {
            buffer[buffer_pos++] = '\n';

            // Add extra newline for paragraph breaks
            if (line->is_paragraph_end && buffer_pos + 1 < buffer_size) {
                buffer[buffer_pos++] = '\n';
            }
        }

        lines_displayed++;
    }

    buffer[buffer_pos] = '\0';

    printf("Generated screen text: %d characters, %d lines displayed\n", buffer_pos, lines_displayed);
    printf("First 100 chars: %.100s\n", buffer);

    return buffer_pos;
}

/**
 * @brief Update page numbers based on current line position
 */
static void ebook_update_page_numbers(void)
{
    if (!ebook_state.reading.layout.layout_valid || !ebook_state.reading.metrics.lines_per_screen) {
        return;
    }

    page_layout_t *layout = &ebook_state.reading.layout;
    const page_metrics_t *metrics = &ebook_state.reading.metrics;

    // Calculate current page based on line index
    layout->current_page = (layout->current_line_index / metrics->lines_per_screen) + 1;

    // Ensure page numbers are valid
    if (layout->current_page < 1) {
        layout->current_page = 1;
    }
    if (layout->current_page > layout->total_pages) {
        layout->current_page = layout->total_pages;
    }
}

/***********************************************************
*************Public Interface Functions********************
***********************************************************/

/**
 * @brief Initialize page metrics based on font and screen size (public interface)
 */
void ebook_init_page_metrics(page_metrics_t *metrics, const lv_font_t *font, int display_width, int display_height)
{
    ebook_init_page_metrics_internal(metrics, font, display_width, display_height);
}

/**
 * @brief Calculate line layout for text content (public interface)
 */
int ebook_calculate_line_layout(page_layout_t *layout, const char *content, size_t content_size, const page_metrics_t *metrics)
{
    return ebook_calculate_line_layout_internal(layout, content, content_size, metrics);
}

/**
 * @brief Free line layout memory (public interface)
 */
void ebook_free_line_layout(page_layout_t *layout)
{
    ebook_free_line_layout_internal(layout);
}

/**
 * @brief Navigate to specific line index
 */
int ebook_goto_line(int line_index)
{
    if (!ebook_state.in_reading_mode || !ebook_state.reading.layout.layout_valid) {
        return -1;
    }

    page_layout_t *layout = &ebook_state.reading.layout;

    // Validate line index
    if (line_index < 0) {
        line_index = 0;
    }
    if (line_index >= layout->line_count) {
        line_index = layout->line_count - 1;
    }

    layout->current_line_index = line_index;
    ebook_update_page_numbers();
    ebook_update_reading_display();
    ebook_save_position();

    return 0;
}

/**
 * @brief Navigate to specific page number
 */
int ebook_goto_page(int page_number)
{
    if (!ebook_state.in_reading_mode || !ebook_state.reading.layout.layout_valid) {
        return -1;
    }

    const page_metrics_t *metrics = &ebook_state.reading.metrics;
    page_layout_t *layout = &ebook_state.reading.layout;

    // Validate page number
    if (page_number < 1) {
        page_number = 1;
    }
    if (page_number > layout->total_pages) {
        page_number = layout->total_pages;
    }

    // Calculate line index for this page
    int target_line = (page_number - 1) * metrics->lines_per_screen;
    return ebook_goto_line(target_line);
}

/**
 * @brief Get text content for current screen (public interface)
 */
int ebook_get_screen_text(char *buffer, int buffer_size, const page_layout_t *layout, const char *content, const page_metrics_t *metrics)
{
    return ebook_get_screen_text_internal(buffer, buffer_size, layout, content, metrics);
}

/***********************************************************
***********New Precise Line Scrolling Functions***********
***********************************************************/

/**
 * @brief Extract text for a specific line from content with natural formatting
 */
static void ebook_extract_line_text(char *line_buffer, int buffer_size, const char *content, const line_info_t *line)
{
    if (!line_buffer || !content || !line || buffer_size <= 0) {
        if (line_buffer) line_buffer[0] = '\0';
        return;
    }

    int chars_to_copy = line->char_count;
    if (chars_to_copy >= buffer_size) {
        chars_to_copy = buffer_size - 1;
    }

    if (chars_to_copy > 0) {
        const char *src = content + line->char_start;
        int dst_pos = 0;

        // Copy characters while preserving natural text flow
        for (int i = 0; i < chars_to_copy && dst_pos < buffer_size - 1; i++) {
            char c = src[i];

            // Handle paragraph breaks (double newlines)
            if (c == '\n') {
                // Look ahead to see if this is a paragraph break
                bool is_paragraph_break = false;
                if (i + 1 < chars_to_copy) {
                    char next_char = src[i + 1];
                    if (next_char == '\n' || next_char == '\r') {
                        is_paragraph_break = true;
                    }
                }

                if (is_paragraph_break) {
                    // Paragraph break - preserve with double space
                    if (dst_pos > 0 && line_buffer[dst_pos-1] != ' ') {
                        line_buffer[dst_pos++] = '.'; // Add period for natural break
                        if (dst_pos < buffer_size - 1) {
                            line_buffer[dst_pos++] = ' ';
                        }
                    }
                    // Skip the extra newline in source
                    i++;
                } else {
                    // Single newline - convert to space for flow
                    if (dst_pos > 0 && line_buffer[dst_pos-1] != ' ') {
                        line_buffer[dst_pos++] = ' ';
                    }
                }
            } else if (c == '\r') {
                // Skip carriage returns
                continue;
            } else {
                // Regular character
                line_buffer[dst_pos++] = c;
            }
        }
        line_buffer[dst_pos] = '\0';
    } else {
        line_buffer[0] = '\0';
    }
}

/**
 * @brief Initialize screen display from layout
 */
static int ebook_init_screen_display_internal(screen_display_t *screen, const page_layout_t *layout, const char *content, const page_metrics_t *metrics, int top_line_index)
{
    if (!screen || !layout || !content || !metrics || !layout->layout_valid) {
        return -1;
    }

    // Clear screen
    memset(screen, 0, sizeof(screen_display_t));

    screen->visible_lines = metrics->lines_per_screen;
    if (screen->visible_lines > 20) screen->visible_lines = 20; // Safety limit

    screen->top_line_index = top_line_index;
    if (screen->top_line_index < 0) screen->top_line_index = 0;
    if (screen->top_line_index >= layout->line_count) {
        screen->top_line_index = layout->line_count - 1;
        if (screen->top_line_index < 0) screen->top_line_index = 0;
    }

    // Fill screen lines
    for (int i = 0; i < screen->visible_lines; i++) {
        int line_index = screen->top_line_index + i;
        screen_line_t *screen_line = &screen->lines[i];

        if (line_index < layout->line_count) {
            const line_info_t *layout_line = &layout->lines[line_index];
            screen_line->char_start = layout_line->char_start;
            screen_line->char_count = layout_line->char_count;

            // Extract actual text for this line
            ebook_extract_line_text(screen_line->line_text, sizeof(screen_line->line_text), content, layout_line);
        } else {
            // Empty line beyond content
            screen_line->char_start = 0;
            screen_line->char_count = 0;
            screen_line->line_text[0] = '\0';
        }
    }

    screen->screen_valid = true;
    printf("Screen display initialized: top_line=%d, visible_lines=%d\n", screen->top_line_index, screen->visible_lines);
    return 0;
}

/**
 * @brief Scroll screen display up by one line
 */
static int ebook_scroll_screen_up_internal(screen_display_t *screen, const page_layout_t *layout, const char *content, const page_metrics_t *metrics)
{
    if (!screen || !layout || !content || !metrics || !screen->screen_valid || !layout->layout_valid) {
        return -1;
    }

    // Check if we can scroll up
    if (screen->top_line_index <= 0) {
        printf("Cannot scroll up: already at top\n");
        return -1; // Already at top
    }

    // Move all lines down by one position
    for (int i = screen->visible_lines - 1; i > 0; i--) {
        screen->lines[i] = screen->lines[i - 1];
    }

    // Load new content for the top line (line that's now appearing at top)
    screen->top_line_index--;
    int new_line_index = screen->top_line_index;

    screen_line_t *top_line = &screen->lines[0];
    if (new_line_index < layout->line_count && new_line_index >= 0) {
        const line_info_t *layout_line = &layout->lines[new_line_index];
        top_line->char_start = layout_line->char_start;
        top_line->char_count = layout_line->char_count;

        // Extract actual text for this line
        ebook_extract_line_text(top_line->line_text, sizeof(top_line->line_text), content, layout_line);

        printf("Scrolled up: new top line %d, content: %.50s...\n", new_line_index, top_line->line_text);
    } else {
        // Safety fallback
        top_line->char_start = 0;
        top_line->char_count = 0;
        top_line->line_text[0] = '\0';
    }

    return 0;
}

/**
 * @brief Scroll screen display down by one line
 */
static int ebook_scroll_screen_down_internal(screen_display_t *screen, const page_layout_t *layout, const char *content, const page_metrics_t *metrics)
{
    if (!screen || !layout || !content || !metrics || !screen->screen_valid || !layout->layout_valid) {
        return -1;
    }

    // Check if we can scroll down
    int max_top_line = layout->line_count - screen->visible_lines;
    if (max_top_line < 0) max_top_line = 0;

    if (screen->top_line_index >= max_top_line) {
        printf("Cannot scroll down: already at bottom (top_line=%d, max=%d)\n", screen->top_line_index, max_top_line);
        return -1; // Already at bottom
    }

    // Move all lines up by one position
    for (int i = 0; i < screen->visible_lines - 1; i++) {
        screen->lines[i] = screen->lines[i + 1];
    }

    // Load new content for the bottom line (line that's now appearing at bottom)
    screen->top_line_index++;
    int new_line_index = screen->top_line_index + screen->visible_lines - 1;

    screen_line_t *bottom_line = &screen->lines[screen->visible_lines - 1];
    if (new_line_index < layout->line_count && new_line_index >= 0) {
        const line_info_t *layout_line = &layout->lines[new_line_index];
        bottom_line->char_start = layout_line->char_start;
        bottom_line->char_count = layout_line->char_count;

        // Extract actual text for this line
        ebook_extract_line_text(bottom_line->line_text, sizeof(bottom_line->line_text), content, layout_line);

        printf("Scrolled down: new bottom line %d, content: %.50s...\n", new_line_index, bottom_line->line_text);
    } else {
        // Empty line beyond content
        bottom_line->char_start = 0;
        bottom_line->char_count = 0;
        bottom_line->line_text[0] = '\0';
    }

    return 0;
}

/**
 * @brief Generate display text from screen lines with natural flow for full-width display
 */
static int ebook_generate_screen_text_internal(char *buffer, int buffer_size, const screen_display_t *screen)
{
    if (!buffer || !screen || buffer_size <= 0 || !screen->screen_valid) {
        if (buffer) buffer[0] = '\0';
        return -1;
    }

    int buffer_pos = 0;
    buffer[0] = '\0';

    // Generate natural text flow that fills the entire display width
    // Concatenate all visible lines into one continuous text flow
    for (int i = 0; i < screen->visible_lines && buffer_pos < buffer_size - 100; i++) {
        const screen_line_t *line = &screen->lines[i];

        // Add line content with natural flow for full-width display
        if (line->char_count > 0 && line->line_text[0] != '\0') {
            int line_len = strlen(line->line_text);
            if (buffer_pos + line_len + 5 < buffer_size) {
                // Copy the line text directly
                strcpy(buffer + buffer_pos, line->line_text);
                buffer_pos += line_len;

                // Add natural spacing between lines for continuous flow
                if (i < screen->visible_lines - 1) { // Not the last line
                    const screen_line_t *next_line = &screen->lines[i + 1];

                    // Check if current line ends with punctuation or if it's a natural break
                    bool line_ends_sentence = false;
                    if (line_len > 0) {
                        char last_char = line->line_text[line_len - 1];
                        line_ends_sentence = (last_char == '.' || last_char == '!' || last_char == '?' ||
                                            last_char == '"' || last_char == '\'' || last_char == ')');
                    }

                    // Check if next line starts with capital letter (new sentence)
                    bool next_starts_sentence = false;
                    if (next_line->char_count > 0 && next_line->line_text[0] != '\0') {
                        char first_char = next_line->line_text[0];
                        next_starts_sentence = (first_char >= 'A' && first_char <= 'Z') ||
                                             (first_char == '"' || first_char == '\'');
                    }

                    // Add appropriate spacing for natural text flow
                    if (line_ends_sentence && next_starts_sentence) {
                        // Sentence boundary - add double space
                        if (buffer_pos + 2 < buffer_size) {
                            buffer[buffer_pos++] = ' ';
                            buffer[buffer_pos++] = ' ';
                        }
                    } else {
                        // Normal word flow - add single space
                        if (buffer_pos + 1 < buffer_size &&
                            buffer[buffer_pos-1] != ' ') {
                            buffer[buffer_pos++] = ' ';
                        }
                    }
                }
            }
        }
    }

    buffer[buffer_pos] = '\0';
    printf("Generated full-width text flow: %d characters from %d lines\n", buffer_pos, screen->visible_lines);
    return buffer_pos;
}

/***********************************************************
**************Public Interface Functions*******************
***********************************************************/

/**
 * @brief Initialize screen display from layout (public interface)
 */
int ebook_init_screen_display(screen_display_t *screen, const page_layout_t *layout, const char *content, const page_metrics_t *metrics, int top_line_index)
{
    return ebook_init_screen_display_internal(screen, layout, content, metrics, top_line_index);
}

/**
 * @brief Scroll screen display up by one line (public interface)
 */
int ebook_scroll_screen_up(screen_display_t *screen, const page_layout_t *layout, const char *content, const page_metrics_t *metrics)
{
    return ebook_scroll_screen_up_internal(screen, layout, content, metrics);
}

/**
 * @brief Scroll screen display down by one line (public interface)
 */
int ebook_scroll_screen_down(screen_display_t *screen, const page_layout_t *layout, const char *content, const page_metrics_t *metrics)
{
    return ebook_scroll_screen_down_internal(screen, layout, content, metrics);
}

/**
 * @brief Generate display text from screen lines (public interface)
 */
int ebook_generate_screen_text(char *buffer, int buffer_size, const screen_display_t *screen)
{
    return ebook_generate_screen_text_internal(buffer, buffer_size, screen);
}
