#include "tal_api.h"
#include "tkl_output.h"
#include "tal_cli.h"
#include "board_com_api.h"
#include "board_pixel_api.h"

/***********************************************************
***********************variable define**********************
***********************************************************/
static PIXEL_FRAME_HANDLE_T g_frame = NULL;
static uint32_t g_demo_state = 0;

/***********************************************************
********************function declaration********************
***********************************************************/
static void demo_draw_shapes(void);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Draw different shapes in sequence
 */
static void demo_draw_shapes(void)
{
    if (g_frame == NULL) {
        return;
    }

    // Clear frame
    board_pixel_frame_clear(g_frame);

    switch (g_demo_state % 12) {
    case 0: {
        // Draw filled boxes in different colors
        PR_NOTICE("Demo: Filled boxes");
        board_pixel_draw_box(g_frame, 2, 2, 8, 8, PIXEL_COLOR_RED);
        board_pixel_draw_box(g_frame, 12, 2, 8, 8, PIXEL_COLOR_GREEN);
        board_pixel_draw_box(g_frame, 22, 2, 8, 8, PIXEL_COLOR_BLUE);
        board_pixel_draw_box(g_frame, 2, 12, 8, 8, PIXEL_COLOR_YELLOW);
        board_pixel_draw_box(g_frame, 12, 12, 8, 8, PIXEL_COLOR_CYAN);
        board_pixel_draw_box(g_frame, 22, 12, 8, 8, PIXEL_COLOR_MAGENTA);
        board_pixel_draw_box(g_frame, 2, 22, 8, 8, PIXEL_COLOR_ORANGE);
        board_pixel_draw_box(g_frame, 12, 22, 8, 8, PIXEL_COLOR_PURPLE);
        board_pixel_draw_box(g_frame, 22, 22, 8, 8, PIXEL_COLOR_PINK);
        break;
    }
    case 1: {
        // Draw circles
        PR_NOTICE("Demo: Circles");
        board_pixel_draw_circle(g_frame, 8, 8, 6, PIXEL_COLOR_RED);
        board_pixel_draw_circle(g_frame, 24, 8, 6, PIXEL_COLOR_GREEN);
        board_pixel_draw_circle(g_frame, 8, 24, 6, PIXEL_COLOR_BLUE);
        board_pixel_draw_circle(g_frame, 24, 24, 6, PIXEL_COLOR_YELLOW);
        board_pixel_draw_circle(g_frame, 16, 16, 8, PIXEL_COLOR_WHITE);
        break;
    }
    case 2: {
        // Draw filled circles
        PR_NOTICE("Demo: Filled circles");
        board_pixel_draw_circle_filled(g_frame, 8, 8, 6, PIXEL_COLOR_RED);
        board_pixel_draw_circle_filled(g_frame, 24, 8, 6, PIXEL_COLOR_GREEN);
        board_pixel_draw_circle_filled(g_frame, 8, 24, 6, PIXEL_COLOR_BLUE);
        board_pixel_draw_circle_filled(g_frame, 24, 24, 6, PIXEL_COLOR_YELLOW);
        board_pixel_draw_circle_filled(g_frame, 16, 16, 8, PIXEL_COLOR_CYAN);
        break;
    }
    case 3: {
        // Draw lines forming a pattern
        PR_NOTICE("Demo: Lines pattern");
        board_pixel_draw_line(g_frame, 0, 0, 31, 31, PIXEL_COLOR_RED);
        board_pixel_draw_line(g_frame, 31, 0, 0, 31, PIXEL_COLOR_GREEN);
        board_pixel_draw_line(g_frame, 0, 16, 31, 16, PIXEL_COLOR_BLUE);
        board_pixel_draw_line(g_frame, 16, 0, 16, 31, PIXEL_COLOR_YELLOW);
        board_pixel_draw_line(g_frame, 0, 8, 31, 8, PIXEL_COLOR_CYAN);
        board_pixel_draw_line(g_frame, 0, 24, 31, 24, PIXEL_COLOR_MAGENTA);
        break;
    }
    case 4: {
        // Draw concentric circles
        PR_NOTICE("Demo: Concentric circles");
        board_pixel_draw_circle(g_frame, 16, 16, 15, PIXEL_COLOR_RED);
        board_pixel_draw_circle(g_frame, 16, 16, 12, PIXEL_COLOR_ORANGE);
        board_pixel_draw_circle(g_frame, 16, 16, 9, PIXEL_COLOR_YELLOW);
        board_pixel_draw_circle(g_frame, 16, 16, 6, PIXEL_COLOR_GREEN);
        board_pixel_draw_circle_filled(g_frame, 16, 16, 3, PIXEL_COLOR_BLUE);
        break;
    }
    case 5: {
        // Draw a grid pattern
        PR_NOTICE("Demo: Grid pattern");
        for (uint32_t i = 0; i < 32; i += 4) {
            board_pixel_draw_line(g_frame, i, 0, i, 31, PIXEL_COLOR_CYAN);
            board_pixel_draw_line(g_frame, 0, i, 31, i, PIXEL_COLOR_MAGENTA);
        }
        break;
    }
    case 6: {
        // Draw boxes with borders (using lines)
        PR_NOTICE("Demo: Box borders");
        // Top-left box
        board_pixel_draw_line(g_frame, 2, 2, 13, 2, PIXEL_COLOR_RED);
        board_pixel_draw_line(g_frame, 2, 2, 2, 13, PIXEL_COLOR_RED);
        board_pixel_draw_line(g_frame, 13, 2, 13, 13, PIXEL_COLOR_RED);
        board_pixel_draw_line(g_frame, 2, 13, 13, 13, PIXEL_COLOR_RED);

        // Top-right box
        board_pixel_draw_line(g_frame, 18, 2, 29, 2, PIXEL_COLOR_GREEN);
        board_pixel_draw_line(g_frame, 18, 2, 18, 13, PIXEL_COLOR_GREEN);
        board_pixel_draw_line(g_frame, 29, 2, 29, 13, PIXEL_COLOR_GREEN);
        board_pixel_draw_line(g_frame, 18, 13, 29, 13, PIXEL_COLOR_GREEN);

        // Bottom-left box
        board_pixel_draw_line(g_frame, 2, 18, 13, 18, PIXEL_COLOR_BLUE);
        board_pixel_draw_line(g_frame, 2, 18, 2, 29, PIXEL_COLOR_BLUE);
        board_pixel_draw_line(g_frame, 13, 18, 13, 29, PIXEL_COLOR_BLUE);
        board_pixel_draw_line(g_frame, 2, 29, 13, 29, PIXEL_COLOR_BLUE);

        // Bottom-right box
        board_pixel_draw_line(g_frame, 18, 18, 29, 18, PIXEL_COLOR_YELLOW);
        board_pixel_draw_line(g_frame, 18, 18, 18, 29, PIXEL_COLOR_YELLOW);
        board_pixel_draw_line(g_frame, 29, 18, 29, 29, PIXEL_COLOR_YELLOW);
        board_pixel_draw_line(g_frame, 18, 29, 29, 29, PIXEL_COLOR_YELLOW);
        break;
    }
    case 7: {
        // Draw a colorful spiral pattern
        PR_NOTICE("Demo: Spiral pattern");
        PIXEL_COLOR_ENUM_E colors[] = {PIXEL_COLOR_RED,  PIXEL_COLOR_ORANGE, PIXEL_COLOR_YELLOW, PIXEL_COLOR_GREEN,
                                       PIXEL_COLOR_CYAN, PIXEL_COLOR_BLUE,   PIXEL_COLOR_PURPLE, PIXEL_COLOR_MAGENTA};
        uint32_t num_colors = sizeof(colors) / sizeof(colors[0]);

        for (uint32_t r = 1; r < 16; r++) {
            board_pixel_draw_circle(g_frame, 16, 16, r, colors[r % num_colors]);
        }
        board_pixel_draw_circle_filled(g_frame, 16, 16, 2, PIXEL_COLOR_WHITE);
        break;
    }
    case 8: {
        // Text with Picopixel font - tiny font
        PR_NOTICE("Demo: Text - Picopixel");
        board_pixel_draw_text(g_frame, 2, 5, "HELLO", PIXEL_COLOR_RED, PIXEL_FONT_PICOPIXEL);
        board_pixel_draw_text(g_frame, 2, 13, "WORLD", PIXEL_COLOR_GREEN, PIXEL_FONT_PICOPIXEL);
        break;
    }
    case 9: {
        // Text with FreeMono9pt font
        PR_NOTICE("Demo: Text - FreeMono 9pt");
        board_pixel_draw_text(g_frame, 2, 5, "HELLO", PIXEL_COLOR_CYAN, PIXEL_FONT_FREEMONO_9PT);
        board_pixel_draw_text(g_frame, 2, 15, "WORLD", PIXEL_COLOR_YELLOW, PIXEL_FONT_FREEMONO_9PT);
        break;
    }
    case 10: {
        // Text with FreeMono Bold 9pt font
        PR_NOTICE("Demo: Text - FreeMono Bold 9pt");
        board_pixel_draw_text(g_frame, 2, 5, "HELLO", PIXEL_COLOR_RED, PIXEL_FONT_FREEMONO_BOLD_9PT);
        board_pixel_draw_text(g_frame, 2, 15, "WORLD", PIXEL_COLOR_GREEN, PIXEL_FONT_FREEMONO_BOLD_9PT);
        break;
    }
    case 11: {
        // Mixed: Shapes with text labels
        PR_NOTICE("Demo: Mixed shapes and text");
        board_pixel_draw_circle_filled(g_frame, 8, 8, 5, PIXEL_COLOR_RED);
        board_pixel_draw_text(g_frame, 2, 16, "CIRCLE", PIXEL_COLOR_RED, PIXEL_FONT_PICOPIXEL);
        board_pixel_draw_box(g_frame, 18, 2, 10, 10, PIXEL_COLOR_BLUE);
        board_pixel_draw_text(g_frame, 18, 14, "BOX", PIXEL_COLOR_BLUE, PIXEL_FONT_PICOPIXEL);
        board_pixel_draw_line(g_frame, 2, 24, 15, 31, PIXEL_COLOR_GREEN);
        board_pixel_draw_text(g_frame, 2, 28, "LINE", PIXEL_COLOR_GREEN, PIXEL_FONT_PICOPIXEL);
        break;
    }
    default:
        break;
    }

    // Render frame to display
    board_pixel_frame_render(g_frame);
}

/**
 * @brief Main user function
 */
static void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("==========================================");
    PR_NOTICE("Tuya T5AI Pixel Simple Shapes Demo");
    PR_NOTICE("==========================================");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("==========================================");

    // Initialize hardware
    rt = board_register_hardware();
    if (OPRT_OK != rt) {
        PR_ERR("board_register_hardware failed: %d", rt);
        return;
    }
    PR_NOTICE("Hardware initialized");

    // Wait a bit for hardware registration to complete
    tal_system_sleep(200);

    // Create frame
    g_frame = board_pixel_frame_create();
    if (g_frame == NULL) {
        PR_ERR("Failed to create pixel frame");
        return;
    }
    PR_NOTICE("Pixel frame created");

    // Clear and render initial frame
    board_pixel_frame_clear(g_frame);
    board_pixel_frame_render(g_frame);
    PR_NOTICE("Pixel display initialized");

    PR_NOTICE("==========================================");
    PR_NOTICE("Demo Ready!");
    PR_NOTICE("==========================================");
    PR_NOTICE("Simple shapes demo will cycle through");
    PR_NOTICE("different drawing patterns every 3 seconds");
    PR_NOTICE("==========================================");

    // Main loop - cycle through different shapes
    uint32_t cnt = 0;
    while (1) {
        // Change demo state every 3 seconds (30 * 100ms)
        if (cnt % 30 == 0) {
            g_demo_state++;
            demo_draw_shapes();
        }

        tal_system_sleep(100);
        cnt++;
    }
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif