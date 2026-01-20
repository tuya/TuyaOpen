#ifndef CLOCK_UI_H_
#define CLOCK_UI_H_

#include "clock_types.h"

// ---------------------------------------------------------------------------
// Layout (override in examples/tuya_config.h or via compiler -D)
// ---------------------------------------------------------------------------
#ifndef EPD_CLOCK_DATE_X
#define EPD_CLOCK_DATE_X 0
#endif
#ifndef EPD_CLOCK_DATE_Y
#define EPD_CLOCK_DATE_Y 16
#endif
#ifndef EPD_CLOCK_DATE_W
#define EPD_CLOCK_DATE_W 800
#endif
#ifndef EPD_CLOCK_DATE_H
#define EPD_CLOCK_DATE_H 48
#endif

#ifndef EPD_CLOCK_TIME_X
#define EPD_CLOCK_TIME_X 0
#endif
#ifndef EPD_CLOCK_TIME_Y
#define EPD_CLOCK_TIME_Y 80
#endif
#ifndef EPD_CLOCK_TIME_W
#define EPD_CLOCK_TIME_W 560
#endif
#ifndef EPD_CLOCK_TIME_H
#define EPD_CLOCK_TIME_H 320
#endif

#ifndef EPD_CLOCK_STAT_X
#define EPD_CLOCK_STAT_X 560
#endif
#ifndef EPD_CLOCK_STAT_Y
#define EPD_CLOCK_STAT_Y EPD_CLOCK_TIME_Y
#endif
#ifndef EPD_CLOCK_STAT_W
#define EPD_CLOCK_STAT_W 240
#endif
#ifndef EPD_CLOCK_STAT_H
#define EPD_CLOCK_STAT_H EPD_CLOCK_TIME_H
#endif

#ifndef EPD_CLOCK_TIME_INNER_TOP
#define EPD_CLOCK_TIME_INNER_TOP 0
#endif

// Big digits (7-seg)
#ifndef EPD_CLOCK_DIGIT_W
#define EPD_CLOCK_DIGIT_W 120
#endif
#ifndef EPD_CLOCK_DIGIT_H
#define EPD_CLOCK_DIGIT_H 220
#endif
#ifndef EPD_CLOCK_DIGIT_THICK
#define EPD_CLOCK_DIGIT_THICK 16
#endif
#ifndef EPD_CLOCK_DIGIT_GAP
#define EPD_CLOCK_DIGIT_GAP 14
#endif
#ifndef EPD_CLOCK_COLON_W
#define EPD_CLOCK_COLON_W 24
#endif
#ifndef EPD_CLOCK_COLON_DOT
#define EPD_CLOCK_COLON_DOT 16
#endif

BOOL_T clock_ui_validate_layout(void);
void clock_ui_render(uint8_t *framebuffer, const clock_ui_state_t *state);

#endif /* CLOCK_UI_H_ */

