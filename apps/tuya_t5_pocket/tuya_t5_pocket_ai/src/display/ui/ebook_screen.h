/**
 * @file ebook_screen.h
 * @brief Declaration of the e-book reader screen for the application
 *
 * This file contains the declarations for the e-book reader screen which provides
 * text reading functionality with scrolling support. It can load and display
 * content from local text files and remember the reading position for continuous
 * reading across sessions.
 *
 * The e-book screen includes:
 * - Book shelf with file browser functionality
 * - Text file loading and display with optimized layout
 * - Up/Down key scrolling navigation
 * - Individual reading position memory for each book
 * - White background with black text for better readability
 * - Page-based navigation with page counter
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef EBOOK_SCREEN_H
#define EBOOK_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"
// #include <dirent.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define MAX_BOOK_FILES         20            /**< Maximum number of book files */
#define MAX_FILENAME_LEN       128           /**< Maximum filename length */
#define SHELF_ITEMS_PER_SCREEN 8             /**< Number of book items visible on shelf */
/***********************************************************
***********************variable define**********************
***********************************************************/

/**
 * @brief Book entry structure for shelf
 */
typedef struct {
    char filename[MAX_FILENAME_LEN];     /**< Full filename */
    char display_name[MAX_FILENAME_LEN]; /**< Display name (without .txt) */
    int saved_line;                      /**< Saved reading position */
    int total_lines;                     /**< Total lines in book */
} book_entry_t;

/**
 * @brief Book shelf state structure
 */
typedef struct {
    book_entry_t books[MAX_BOOK_FILES];  /**< Array of available books */
    int book_count;                      /**< Number of books found */
    int selected_book;                   /**< Currently selected book index */
    int shelf_scroll;                    /**< Scroll position on shelf */
} book_shelf_t;

/**
 * @brief Page display metrics structure based on font and screen size
 */
typedef struct {
    const lv_font_t *font;          /**< Current font being used */
    int font_height;                /**< Height of one line in pixels */
    int char_width;                 /**< Average character width in pixels */
    int display_width;              /**< Available text display width */
    int display_height;             /**< Available text display height */
    int chars_per_line;             /**< Maximum characters per line */
    int lines_per_screen;           /**< Maximum lines per screen */
    int chars_per_screen;           /**< Total characters per screen */
} page_metrics_t;

/**
 * @brief Line information structure for text layout
 */
typedef struct {
    int char_start;                 /**< Character position where this line starts */
    int char_count;                 /**< Number of characters in this line */
    bool is_paragraph_end;          /**< True if this line ends a paragraph */
} line_info_t;

/**
 * @brief Page layout information structure
 */
typedef struct {
    line_info_t *lines;             /**< Array of line information */
    int line_count;                 /**< Total number of lines in content */
    int current_line_index;         /**< Current top line being displayed (0-based) */
    int current_page;               /**< Current page number (1-based) */
    int total_pages;                /**< Total number of pages */
    bool layout_valid;              /**< Flag indicating if layout is up to date */
} page_layout_t;

/**
 * @brief Screen display line structure for precise line-by-line scrolling
 */
typedef struct {
    int char_start;                 /**< Character position where this screen line starts */
    int char_count;                 /**< Number of characters in this screen line */
    char line_text[256];            /**< Actual text content for this screen line */
} screen_line_t;

/**
 * @brief Screen display manager structure
 */
typedef struct {
    screen_line_t lines[20];        /**< Array of screen lines (enough for any screen) */
    int visible_lines;              /**< Number of visible lines on screen */
    int top_line_index;             /**< Index of top line in the layout */
    bool screen_valid;              /**< Flag indicating if screen content is valid */
} screen_display_t;

/**
 * @brief E-book reading state structure
 */
typedef struct {
    char *content;              /**< Loaded text content */
    size_t content_size;        /**< Size of loaded content */
    int current_line;           /**< Current top line being displayed (deprecated - use page_layout) */
    int total_lines;            /**< Total number of lines in the content (deprecated - use page_layout) */
    int current_page;           /**< Current page number (1-based) (deprecated - use page_layout) */
    int total_pages;            /**< Total number of pages (deprecated - use page_layout) */
    char current_book[MAX_FILENAME_LEN]; /**< Current book filename */
    bool content_loaded;        /**< Flag indicating if content is loaded */
    page_metrics_t metrics;     /**< Page display metrics */
    page_layout_t layout;       /**< Page layout information */
    screen_display_t screen;    /**< Screen display management */
} ebook_reading_state_t;

/**
 * @brief E-book main state structure
 */
typedef struct {
    book_shelf_t shelf;              /**< Book shelf state */
    ebook_reading_state_t reading;   /**< Reading state */
    bool in_reading_mode;            /**< Flag: true=reading, false=shelf */
} ebook_state_t;

/***********************************************************
********************function declaration********************
***********************************************************/

extern Screen_t ebook_screen;

/**
 * @brief Initialize the e-book screen
 *
 * This function creates the e-book screen UI with book shelf and reading area,
 * scans for available books, and sets up keyboard event handling for navigation.
 */
void ebook_screen_init(void);

/**
 * @brief Deinitialize the e-book screen
 *
 * This function cleans up the e-book screen by saving the current reading
 * position, freeing allocated memory, and removing event callbacks.
 */
void ebook_screen_deinit(void);

/**
 * @brief Scan directory for txt files and populate book shelf
 *
 * Scans the specified directory for .txt files and populates
 * the book shelf with available books.
 *
 * @return int Number of books found, -1 on error
 */
int ebook_scan_books(void);

/**
 * @brief Open and read a book from the shelf
 *
 * Loads the specified book into memory and prepares it for reading.
 * Updates the reading state and switches to reading mode.
 *
 * @param book_index Index of the book in the shelf array
 * @return int 0 on success, -1 on error
 */
int ebook_open_book(int book_index);

/**
 * @brief Save reading position for current book
 *
 * Saves the current reading position to a position file,
 * allowing resume from the same location later.
 *
 * @return int 0 on success, -1 on error
 */
int ebook_save_position(void);

/**
 * @brief Load saved reading position for current book
 *
 * Loads the previously saved reading position for the current book.
 *
 * @return int Saved line position, 0 if no position saved
 */
int ebook_load_position(void);

/**
 * @brief Navigate up in current interface
 *
 * Handles up navigation for both shelf browsing and reading modes.
 *
 * @return int 0 on success, -1 on error
 */
int ebook_navigate_up(void);

/**
 * @brief Navigate down in current interface
 *
 * Handles down navigation for both shelf browsing and reading modes.
 *
 * @return int 0 on success, -1 on error
 */
int ebook_navigate_down(void);

/**
 * @brief Handle enter/select action
 *
 * Handles selection for both shelf browsing (open book) and
 * reading modes (page down).
 *
 * @return int 0 on success, -1 on error
 */
int ebook_handle_select(void);

/**
 * @brief Handle back/return action
 *
 * Handles back action - return to shelf from reading mode,
 * or exit to previous screen from shelf mode.
 *
 * @return int 0 on success, -1 on error
 */
int ebook_handle_back(void);

/**
 * @brief Update book shelf display
 *
 * Refreshes the book shelf UI with current book list and selection.
 */
void ebook_update_shelf_display(void);

/**
 * @brief Update reading display
 *
 * Refreshes the reading UI with current content and position.
 */
void ebook_update_reading_display(void);

/**
 * @brief Calculate total pages for current book
 *
 * Calculates and updates the total page count based on content
 * and lines per screen setting.
 */
void ebook_calculate_pages(void);

/**
 * @brief Clean up e-book resources
 *
 * Frees allocated memory and saves current reading position.
 */
void ebook_cleanup(void);

/**
 * @brief Initialize page metrics based on font and screen size
 *
 * Calculates and sets up page display metrics including characters per line,
 * lines per screen, based on the specified font and available screen space.
 *
 * @param metrics Pointer to page_metrics_t structure to initialize
 * @param font Font to use for calculations
 * @param display_width Available display width in pixels
 * @param display_height Available display height in pixels
 */
void ebook_init_page_metrics(page_metrics_t *metrics, const lv_font_t *font, int display_width, int display_height);

/**
 * @brief Calculate line layout for text content
 *
 * Analyzes text content and creates line layout information based on
 * current page metrics. This function handles word wrapping and paragraph breaks.
 *
 * @param layout Pointer to page_layout_t structure to populate
 * @param content Text content to analyze
 * @param content_size Size of text content
 * @param metrics Page display metrics
 * @return int 0 on success, -1 on error
 */
int ebook_calculate_line_layout(page_layout_t *layout, const char *content, size_t content_size, const page_metrics_t *metrics);

/**
 * @brief Free line layout memory
 *
 * Frees memory allocated for line layout information.
 *
 * @param layout Pointer to page_layout_t structure to free
 */
void ebook_free_line_layout(page_layout_t *layout);

/**
 * @brief Navigate to specific line index
 *
 * Moves to the specified line index and updates display.
 *
 * @param line_index Target line index (0-based)
 * @return int 0 on success, -1 on error
 */
int ebook_goto_line(int line_index);

/**
 * @brief Navigate to specific page number
 *
 * Moves to the specified page number and updates display.
 *
 * @param page_number Target page number (1-based)
 * @return int 0 on success, -1 on error
 */
int ebook_goto_page(int page_number);

/**
 * @brief Get text content for current screen
 *
 * Extracts and formats text content for the current screen based on
 * line layout information.
 *
 * @param buffer Buffer to store formatted text
 * @param buffer_size Size of buffer
 * @param layout Page layout information
 * @param content Source text content
 * @param metrics Page display metrics
 * @return int Number of characters written, -1 on error
 */
int ebook_get_screen_text(char *buffer, int buffer_size, const page_layout_t *layout, const char *content, const page_metrics_t *metrics);

/**
 * @brief Initialize screen display from layout
 *
 * Fills the screen display structure with content from the layout,
 * starting from the specified top line index.
 *
 * @param screen Pointer to screen display structure
 * @param layout Page layout information
 * @param content Source text content
 * @param metrics Page display metrics
 * @param top_line_index Starting line index for display
 * @return int 0 on success, -1 on error
 */
int ebook_init_screen_display(screen_display_t *screen, const page_layout_t *layout, const char *content, const page_metrics_t *metrics, int top_line_index);

/**
 * @brief Scroll screen display up by one line
 *
 * Moves all screen lines up by one position and loads new content
 * for the top line from the layout.
 *
 * @param screen Pointer to screen display structure
 * @param layout Page layout information
 * @param content Source text content
 * @param metrics Page display metrics
 * @return int 0 on success, -1 on error or no more content
 */
int ebook_scroll_screen_up(screen_display_t *screen, const page_layout_t *layout, const char *content, const page_metrics_t *metrics);

/**
 * @brief Scroll screen display down by one line
 *
 * Moves all screen lines down by one position and loads new content
 * for the bottom line from the layout.
 *
 * @param screen Pointer to screen display structure
 * @param layout Page layout information
 * @param content Source text content
 * @param metrics Page display metrics
 * @return int 0 on success, -1 on error or no more content
 */
int ebook_scroll_screen_down(screen_display_t *screen, const page_layout_t *layout, const char *content, const page_metrics_t *metrics);

/**
 * @brief Generate display text from screen lines
 *
 * Combines all screen lines into a single display text buffer
 * for rendering by LVGL.
 *
 * @param buffer Buffer to store combined text
 * @param buffer_size Size of buffer
 * @param screen Screen display structure
 * @return int Number of characters written, -1 on error
 */
int ebook_generate_screen_text(char *buffer, int buffer_size, const screen_display_t *screen);

/**
 * @brief Load text content from a file (internal function)
 *
 * @param filename Path to the text file to load
 * @return true if file loaded successfully, false otherwise
 *
 * This function loads the content of a text file into memory and
 * calculates the total number of lines for navigation.
 */
bool ebook_load_file(const char *filename);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*EBOOK_SCREEN_H*/
