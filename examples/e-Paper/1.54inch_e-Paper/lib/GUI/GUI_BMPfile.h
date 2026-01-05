/*****************************************************************************
* | File      	:   GUI_BMPfile.h
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*                Used to shield the underlying layers of each master
*                and enhance portability
*----------------
* |	This version:   V2.3
* | Date        :   2020-07-27
* | Info        :   
* -----------------------------------------------------------------------------
* V2.3(2022-07-27):
* 1.Add GUI_ReadBmp_RGB_4Color()
* V2.2(2020-07-08):
* 1.Add GUI_ReadBmp_RGB_7Color()
* V2.1(2019-10-10):
* 1.Add GUI_ReadBmp_4Gray()
* V2.0(2018-11-12):
* 1.Change file name: GUI_BMP.h -> GUI_BMPfile.h
* 2.fix: GUI_ReadBmp()
*   Now Xstart and Xstart can control the position of the picture normally, 
*   and support the display of images of any size. If it is larger than 
*   the actual display range, it will not be displayed.
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
#ifndef __GUI_BMPFILE_H
#define __GUI_BMPFILE_H

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#include "DEV_Config.h"

/*Bitmap file header   14bit*/
typedef struct BMP_FILE_HEADER {
    UWORD bType;        //File identifier
    UDOUBLE bSize;      //The size of the file
    UWORD bReserved1;   //Reserved value, must be set to 0
    UWORD bReserved2;   //Reserved value, must be set to 0
    UDOUBLE bOffset;    //The offset from the beginning of the file header to the beginning of the image data bit
} __attribute__ ((packed)) BMPFILEHEADER;    // 14bit

/*Bitmap information header  40bit*/
typedef struct BMP_INFO {
    UDOUBLE biInfoSize;      //The size of the header
    UDOUBLE biWidth;         //The width of the image
    UDOUBLE biHeight;        //The height of the image
    UWORD biPlanes;          //The number of planes in the image
    UWORD biBitCount;        //The number of bits per pixel
    UDOUBLE biCompression;   //Compression type
    UDOUBLE bimpImageSize;   //The size of the image, in bytes
    UDOUBLE biXPelsPerMeter; //Horizontal resolution
    UDOUBLE biYPelsPerMeter; //Vertical resolution
    UDOUBLE biClrUsed;       //The number of colors used
    UDOUBLE biClrImportant;  //The number of important colors
} __attribute__ ((packed)) BMPINFOHEADER;

/*Color table: palette */
typedef struct RGB_QUAD {
    UBYTE rgbBlue;               //Blue intensity
    UBYTE rgbGreen;              //Green strength
    UBYTE rgbRed;                //Red intensity
    UBYTE rgbReversed;           //Reserved value
} __attribute__ ((packed)) BMPRGBQUAD;
/**************************************** end ***********************************************/

UBYTE GUI_ReadBmp(const char *path, UWORD Xstart, UWORD Ystart);
UBYTE GUI_ReadBmp_4Gray(const char *path, UWORD Xstart, UWORD Ystart);
UBYTE GUI_ReadBmp_16Gray(const char *path, UWORD Xstart, UWORD Ystart);
UBYTE GUI_ReadBmp_RGB_4Color(const char *path, UWORD Xstart, UWORD Ystart);
UBYTE GUI_ReadBmp_RGB_6Color(const char *path, UWORD Xstart, UWORD Ystart);
UBYTE GUI_ReadBmp_RGB_7Color(const char *path, UWORD Xstart, UWORD Ystart);
#endif
