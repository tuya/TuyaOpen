/*****************************************************************************
* | File      	:   EPD_4in26.h
* | Author      :   Waveshare team
* | Function    :   4.26inch e-paper test demo
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2023-12-19
* | Info        :
* -----------------------------------------------------------------------------
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#ifndef __EPD_4in26_H_
#define __EPD_4in26_H_

#include "DEV_Config.h"

// PR_DEBUG 已在 DEV_Config.h 中定义，这里不需要重复定义

// Display resolution
#define EPD_4in26_WIDTH       800
#define EPD_4in26_HEIGHT      480

void EPD_4in26_Init(void);
void EPD_4in26_Init_Fast(void);
void EPD_4in26_Init_4GRAY(void);
void EPD_4in26_Clear(void);
void EPD_4in26_Display(UBYTE *Image);
void EPD_4in26_Display_Base(UBYTE *Image);
void EPD_4in26_Display_Fast(UBYTE *Image);
void EPD_4in26_Display_Part(UBYTE *Image, UWORD x, UWORD y, UWORD w, UWORD l);
void EPD_4in26_4GrayDisplay(UBYTE *Image);
void EPD_4in26_Sleep(void);


#endif
