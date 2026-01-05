/*****************************************************************************
* | File      	:	Debug.h
* | Author      :   Waveshare team
* | Function    :	debug with printf
* | Info        :
*   Image scanning
*      Please use progressive scanning to generate images or fonts
*----------------
* |	This version:   V2.0
* | Date        :   2018-10-30
* | Info        :   
*   1.USE_DEBUG -> DEBUG, If you need to see the debug information, 
*    clear the execution: make DEBUG=-DDEBUG
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
#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdio.h>


#include "tal_log.h"

// Doodle Log Macro definition
#ifndef PR_DEBUG
#define PR_DEBUG(fmt, ...) TAL_PR_DEBUG("EPD", fmt, ##__VA_ARGS__)
#endif

#ifndef PR_INFO
#define PR_INFO(fmt, ...) TAL_PR_INFO("EPD", fmt, ##__VA_ARGS__)
#endif

#ifndef PR_WARN
#define PR_WARN(fmt, ...) TAL_PR_WARN("EPD", fmt, ##__VA_ARGS__)
#endif

#ifndef PR_ERR
#define PR_ERR(fmt, ...) TAL_PR_ERR("EPD", fmt, ##__VA_ARGS__)
#endif

#if DEBUG
	#define Debug(__info,...) printf("Debug: " __info,##__VA_ARGS__)
#else
	#define Debug       PR_DEBUG
#endif

#endif

