#include <stdint.h>

#ifndef __OLED_H__
#define __OLED_H__

#define OLED_W 128
#define OLED_H 64
#define OLED_PAGE 8        // 8 页，每页 8 行
extern uint8_t oled_buf[OLED_PAGE][OLED_W];   // 显存
void oled_update(void);
void oled_init(void);
void OLED_DrawPoint(int16_t X, int16_t Y);
void OLED_DrawRectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled);
void OLED_DrawRoundRect(int16_t x, int16_t y, uint16_t Width, uint16_t Height, uint8_t Radius, uint8_t IsFilled);
void OLED_DrawArc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled);
void OLED_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1);
void OLED_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled);
void OLED_Draw_qua(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, int16_t X3, int16_t Y3, uint8_t IsFilled);
void eye_blink(int speed);
void OLED_Clear(void);
void oled_draweys(void);
void move_eyes(int direction);
void eye_left(void);
void eye_right(void);
void w_eye_move(void);
void eye_center(void);
void oled_update(void);
void w_eye_blink(void);
void eye_sad(void);
#endif