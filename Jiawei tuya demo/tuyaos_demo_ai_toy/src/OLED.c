#include "OLED.h"
#include <stdint.h>
#include <string.h>  // 用于 memset/memcpy
#include "tkl_i2c.h"
#include "tal_log.h"
#include "tuya_cloud_types.h"
#include "tkl_pinmux.h"
#include "tal_system.h"
#include "tal_thread.h"

#define OLED_I2C_ADDR 0x3C          // 7 位地址
#define OLED_I2C_PORT TUYA_I2C_NUM_0

#define OLED_UNFILLED 0
#define OLED_FILLED   1

#ifndef EXAMPLE_I2C_SCL_PIN
#define EXAMPLE_I2C_SCL_PIN TUYA_GPIO_NUM_13
#endif

#ifndef EXAMPLE_I2C_SDA_PIN
#define EXAMPLE_I2C_SDA_PIN TUYA_GPIO_NUM_15
#endif

//OLED_PAGE 8
//OLED_W 128
uint8_t oled_buf[OLED_PAGE][OLED_W] = {0};

STATIC THREAD_HANDLE emoji_thread = NULL;   // 表情线程句柄
STATIC BOOL_T        emoji_enable = TRUE; 

typedef enum {
    OLED_SAD = 0,
    OLED_LEFT,
    OLED_RIGHT,
    OLED_BLINK,
    OLED_MAX_NUM

} OLED_RANDOM;

STATIC CONST CHAR_T *oled_name[OLED_MAX_NUM] = {
    "eye_sad", "eye_left", "eye_right", "eye_blink"
};

/* 写命令 */
static void oled_wr_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};   // 0x00 表示后面是命令
    tkl_i2c_master_send(OLED_I2C_PORT, OLED_I2C_ADDR, buf, 2, 100);
}
/*static void oled_wr_data(uint8_t *data, uint16_t len)
{
    uint8_t hdr = 0x40;             // 0x40 表示后面是数据
    tkl_i2c_master_send(OLED_I2C_PORT, OLED_I2C_ADDR, &hdr, 1, 100);
    tkl_i2c_master_send(OLED_I2C_PORT, OLED_I2C_ADDR, data, len, 100);
}*/

/* 写数据 */
static void oled_wr_data(uint8_t *data, uint16_t len)
{
    uint8_t buf[129];
    buf[0] = 0x40;          // 控制字节
    memcpy(&buf[1], data, len);
    tkl_i2c_master_send(OLED_I2C_PORT, OLED_I2C_ADDR, buf, len+1, 100);
}

/* 设置页地址 + 列地址低/高 */
static void oled_set_pos(uint8_t page, uint8_t x)
{
    x += 2;                         // SSH1106 偏移 2
    oled_wr_cmd(0xB0 | page);       // 页地址
    oled_wr_cmd(0x00 | (x & 0x0F)); // 列低 4 位
    oled_wr_cmd(0x10 | ((x >> 4) & 0x0F)); // 列高 4 位
}

/* 初始化 */
void oled_init(void)
{

    TUYA_IIC_BASE_CFG_T cfg = {
        .role      = TUYA_IIC_MODE_MASTER,
        .speed     = TUYA_IIC_BUS_SPEED_400K,
        .addr_width= TUYA_IIC_ADDRESS_7BIT
    };

    tkl_io_pinmux_config(EXAMPLE_I2C_SCL_PIN, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(EXAMPLE_I2C_SDA_PIN, TUYA_IIC0_SDA);

    tkl_i2c_init(OLED_I2C_PORT, &cfg);

    static const uint8_t init_seq[] = {
    0xAE,       // Display OFF
    0xD5, 0x80, // Clock divide
    0xA8, 0x3F, // Multiplex ratio (1/64)
    0xD3, 0x00, // Display offset
    0x40,       // Display start line
    0xAD, 0x8B, // DC-DC control mode（SH1106 专用）
    0xA1,       // Segment remap
    0xC8,       // COM scan direction
    0xDA, 0x12, // COM pins
    0x81, 0xFF, // Contrast
    0xD9, 0x1F, // Pre-charge period
    0xDB, 0x40, // VCOMH deselect level
    0xA4,       // Display all on resume
    0xA6,       // Normal display
    0xAF        // Display ON
};
    for (uint8_t i = 0; i < sizeof(init_seq); i++)
        oled_wr_cmd(init_seq[i]);

    memset(oled_buf, 0, sizeof(oled_buf));
    oled_update();   // 清屏
}

void oled_update(void)
{
    for (uint8_t page = 0; page < OLED_PAGE; page++) {
        oled_set_pos(page, 0);
        oled_wr_data(oled_buf[page], OLED_W);
    }
}


/**
  * 函    数：将OLED显存数组全部清零
  * 参    数：无
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void OLED_Clear(void)
{
	uint8_t i, j;
	for (j = 0; j < 8; j ++)				//遍历8页
	{
		for (i = 0; i < 128; i ++)			//遍历128列
		{
			oled_buf[j][i] = 0x00;	//将显存数组数据全部清零
		}
	}
}

/**
  * 函    数：判断指定点是否在指定角度内部
  * 参    数：X Y 指定点的坐标
  * 参    数：StartAngle EndAngle 起始角度和终止角度，范围：-180~180
  *           水平向右为0度，水平向左为180度或-180度，下方为正数，上方为负数，顺时针旋转
  * 返 回 值：指定点是否在指定角度内部，1：在内部，0：不在内部
  */
uint8_t OLED_IsInAngle(int16_t X, int16_t Y, int16_t StartAngle, int16_t EndAngle)
{
	int16_t PointAngle;
	PointAngle = atan2(Y, X) / 3.14 * 180;	//计算指定点的弧度，并转换为角度表示
	if (StartAngle < EndAngle)	//起始角度小于终止角度的情况
	{
		/*如果指定角度在起始终止角度之间，则判定指定点在指定角度*/
		if (PointAngle >= StartAngle && PointAngle <= EndAngle)
		{
			return 1;
		}
	}
	else			//起始角度大于于终止角度的情况
	{
		/*如果指定角度大于起始角度或者小于终止角度，则判定指定点在指定角度*/
		if (PointAngle >= StartAngle || PointAngle <= EndAngle)
		{
			return 1;
		}
	}
	return 0;		//不满足以上条件，则判断判定指定点不在指定角度
}


/**
  * 函    数：OLED画线
  * 参    数：X0 指定一个端点的横坐标，范围：-32768~32767，屏幕区域：0~127
  * 参    数：Y0 指定一个端点的纵坐标，范围：-32768~32767，屏幕区域：0~63
  * 参    数：X1 指定另一个端点的横坐标，范围：-32768~32767，屏幕区域：0~127
  * 参    数：Y1 指定另一个端点的纵坐标，范围：-32768~32767，屏幕区域：0~63
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void OLED_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1)
{
	int16_t x, y, dx, dy, d, incrE, incrNE, temp;
	int16_t x0 = X0, y0 = Y0, x1 = X1, y1 = Y1;
	uint8_t yflag = 0, xyflag = 0;
	
	if (y0 == y1)		//横线单独处理
	{
		/*0号点X坐标大于1号点X坐标，则交换两点X坐标*/
		if (x0 > x1) {temp = x0; x0 = x1; x1 = temp;}
		
		/*遍历X坐标*/
		for (x = x0; x <= x1; x ++)
		{
			OLED_DrawPoint(x, y0);	//依次画点
		}
	}
	else if (x0 == x1)	//竖线单独处理
	{
		/*0号点Y坐标大于1号点Y坐标，则交换两点Y坐标*/
		if (y0 > y1) {temp = y0; y0 = y1; y1 = temp;}
		
		/*遍历Y坐标*/
		for (y = y0; y <= y1; y ++)
		{
			OLED_DrawPoint(x0, y);	//依次画点
		}
	}
	else				//斜线
	{
		/*使用Bresenham算法画直线，可以避免耗时的浮点运算，效率更高*/
		/*参考文档：https://www.cs.montana.edu/courses/spring2009/425/dslectures/Bresenham.pdf*/
		/*参考教程：https://www.bilibili.com/video/BV1364y1d7Lo*/
		
		if (x0 > x1)	//0号点X坐标大于1号点X坐标
		{
			/*交换两点坐标*/
			/*交换后不影响画线，但是画线方向由第一、二、三、四象限变为第一、四象限*/
			temp = x0; x0 = x1; x1 = temp;
			temp = y0; y0 = y1; y1 = temp;
		}
		
		if (y0 > y1)	//0号点Y坐标大于1号点Y坐标
		{
			/*将Y坐标取负*/
			/*取负后影响画线，但是画线方向由第一、四象限变为第一象限*/
			y0 = -y0;
			y1 = -y1;
			
			/*置标志位yflag，记住当前变换，在后续实际画线时，再将坐标换回来*/
			yflag = 1;
		}
		
		if (y1 - y0 > x1 - x0)	//画线斜率大于1
		{
			/*将X坐标与Y坐标互换*/
			/*互换后影响画线，但是画线方向由第一象限0~90度范围变为第一象限0~45度范围*/
			temp = x0; x0 = y0; y0 = temp;
			temp = x1; x1 = y1; y1 = temp;
			
			/*置标志位xyflag，记住当前变换，在后续实际画线时，再将坐标换回来*/
			xyflag = 1;
		}
		
		/*以下为Bresenham算法画直线*/
		/*算法要求，画线方向必须为第一象限0~45度范围*/
		dx = x1 - x0;
		dy = y1 - y0;
		incrE = 2 * dy;
		incrNE = 2 * (dy - dx);
		d = 2 * dy - dx;
		x = x0;
		y = y0;
		
		/*画起始点，同时判断标志位，将坐标换回来*/
		if (yflag && xyflag){OLED_DrawPoint(y, -x);}
		else if (yflag)		{OLED_DrawPoint(x, -y);}
		else if (xyflag)	{OLED_DrawPoint(y, x);}
		else				{OLED_DrawPoint(x, y);}
		
		while (x < x1)		//遍历X轴的每个点
		{
			x ++;
			if (d < 0)		//下一个点在当前点东方
			{
				d += incrE;
			}
			else			//下一个点在当前点东北方
			{
				y ++;
				d += incrNE;
			}
			
			/*画每一个点，同时判断标志位，将坐标换回来*/
			if (yflag && xyflag){OLED_DrawPoint(y, -x);}
			else if (yflag)		{OLED_DrawPoint(x, -y);}
			else if (xyflag)	{OLED_DrawPoint(y, x);}
			else				{OLED_DrawPoint(x, y);}
		}	
	}
}

//画点函数
void OLED_DrawPoint(int16_t X, int16_t Y)
{
    if (X >= 0 && X < 128 && Y >= 0 && Y < 64) // 注意边界条件
    {
        oled_buf[Y / 8][X] |= 1 << (Y % 8);
    }
}


/**
  * 函    数：OLED画圆弧
  * 参    数：X 指定圆弧的圆心横坐标，范围：-32768~32767，屏幕区域：0~127
  * 参    数：Y 指定圆弧的圆心纵坐标，范围：-32768~32767，屏幕区域：0~63
  * 参    数：Radius 指定圆弧的半径，范围：0~255
  * 参    数：StartAngle 指定圆弧的起始角度，范围：-180~180
  *           水平向右为0度，水平向左为180度或-180度，下方为正数，上方为负数，顺时针旋转
  * 参    数：EndAngle 指定圆弧的终止角度，范围：-180~180
  *           水平向右为0度，水平向左为180度或-180度，下方为正数，上方为负数，顺时针旋转
  * 参    数：IsFilled 指定圆弧是否填充，填充后为扇形
  *           范围：OLED_UNFILLED		不填充
  *                 OLED_FILLED			填充
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void OLED_DrawArc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled)
{
	int16_t x, y, d, j;
	
	/*此函数借用Bresenham算法画圆的方法*/
	
	d = 1 - Radius;
	x = 0;
	y = Radius;
	
	/*在画圆的每个点时，判断指定点是否在指定角度内，在，则画点，不在，则不做处理*/
	if (OLED_IsInAngle(x, y, StartAngle, EndAngle))	{OLED_DrawPoint(X + x, Y + y);}
	if (OLED_IsInAngle(-x, -y, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y - y);}
	if (OLED_IsInAngle(y, x, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y + x);}
	if (OLED_IsInAngle(-y, -x, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y - x);}
	
	if (IsFilled)	//指定圆弧填充
	{
		/*遍历起始点Y坐标*/
		for (j = -y; j < y; j ++)
		{
			/*在填充圆的每个点时，判断指定点是否在指定角度内，在，则画点，不在，则不做处理*/
			if (OLED_IsInAngle(0, j, StartAngle, EndAngle)) {OLED_DrawPoint(X, Y + j);}
		}
	}
	
	while (x < y)		//遍历X轴的每个点
	{
		x ++;
		if (d < 0)		//下一个点在当前点东方
		{
			d += 2 * x + 1;
		}
		else			//下一个点在当前点东南方
		{
			y --;
			d += 2 * (x - y) + 1;
		}
		
		/*在画圆的每个点时，判断指定点是否在指定角度内，在，则画点，不在，则不做处理*/
		if (OLED_IsInAngle(x, y, StartAngle, EndAngle)) {OLED_DrawPoint(X + x, Y + y);}
		if (OLED_IsInAngle(y, x, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y + x);}
		if (OLED_IsInAngle(-x, -y, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y - y);}
		if (OLED_IsInAngle(-y, -x, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y - x);}
		if (OLED_IsInAngle(x, -y, StartAngle, EndAngle)) {OLED_DrawPoint(X + x, Y - y);}
		if (OLED_IsInAngle(y, -x, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y - x);}
		if (OLED_IsInAngle(-x, y, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y + y);}
		if (OLED_IsInAngle(-y, x, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y + x);}
		
		if (IsFilled)	//指定圆弧填充
		{
			/*遍历中间部分*/
			for (j = -y; j < y; j ++)
			{
				/*在填充圆的每个点时，判断指定点是否在指定角度内，在，则画点，不在，则不做处理*/
				if (OLED_IsInAngle(x, j, StartAngle, EndAngle)) {OLED_DrawPoint(X + x, Y + j);}
				if (OLED_IsInAngle(-x, j, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y + j);}
			}
			
			/*遍历两侧部分*/
			for (j = -x; j < x; j ++)
			{
				/*在填充圆的每个点时，判断指定点是否在指定角度内，在，则画点，不在，则不做处理*/
				if (OLED_IsInAngle(-y, j, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y + j);}
				if (OLED_IsInAngle(y, j, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y + j);}
			}
		}
	}
}

/*
 *  以左上角 (x,y) 为起点，画出带圆角的矩形
 *  Radius : 圆角半径
 *  IsFilled: 0-只画轮廓   1-填充
 */
void OLED_DrawRoundRect(int16_t x, int16_t y, uint16_t Width, uint16_t Height, uint8_t Radius, uint8_t IsFilled)
{
    /* ---- 基础保护 ---- */
    if (Radius > Height / 2) Radius = Height / 2;
    if (Radius > Width  / 2) Radius = Width  / 2;

    int16_t r  = Radius;
    int16_t x0 = x;
    int16_t y0 = y;
    int16_t x1 = x + Width  - 1;
    int16_t y1 = y + Height - 1;

    if (!IsFilled) {
        /* 1. 四条圆弧（四分圆） */
        /* 左上 270°-360° */
        OLED_DrawArc(x0 + r, y0 + r, r, 270, 360, 0);
        /* 右上 0°-90° */
        OLED_DrawArc(x1 - r, y0 + r, r, 0, 90, 0);
        /* 右下 90°-180° */
        OLED_DrawArc(x1 - r, y1 - r, r, 90, 180, 0);
        /* 左下 180°-270° */
        OLED_DrawArc(x0 + r, y1 - r, r, 180, 270, 0);

        /* 2. 四条直线（补缺口） */
        OLED_DrawLine(x0 + r, y0, x1 - r, y0);   // 上
        OLED_DrawLine(x0 + r, y1, x1 - r, y1);   // 下
        OLED_DrawLine(x0, y0 + r, x0, y1 - r);   // 左
        OLED_DrawLine(x1, y0 + r, x1, y1 - r);   // 右
    } else {
        /* ===== 填充版：扫描线 ===== */
        for (int16_t py = y0; py <= y1; py++) {
            int16_t lx = x0, rx = x1;

            /* 上圆角区 */
            if (py - y0 < r) {
                int16_t dy = r - (py - y0);
                lx = x0 + r - (int16_t)sqrt(r*r - dy*dy);
                rx = x1 - r + (int16_t)sqrt(r*r - dy*dy);
            }
            /* 下圆角区 */
            else if (y1 - py < r) {
                int16_t dy = r - (y1 - py);
                lx = x0 + r - (int16_t)sqrt(r*r - dy*dy);
                rx = x1 - r + (int16_t)sqrt(r*r - dy*dy);
            }

            for (int16_t px = lx; px <= rx; px++)
                OLED_DrawPoint(px, py);
        }
    }
}

/**
  * 函    数：OLED矩形
  * 参    数：X 指定矩形左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
  * 参    数：Y 指定矩形左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
  * 参    数：Width 指定矩形的宽度，范围：0~128
  * 参    数：Height 指定矩形的高度，范围：0~64
  * 参    数：IsFilled 指定矩形是否填充
  *           范围：OLED_UNFILLED		不填充
  *                 OLED_FILLED			填充
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void OLED_DrawRectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled)
{
	int16_t i, j;
	if (!IsFilled)		//指定矩形不填充
	{
		/*遍历上下X坐标，画矩形上下两条线*/
		for (i = X; i < X + Width; i ++)
		{
			OLED_DrawPoint(i, Y);
			OLED_DrawPoint(i, Y + Height - 1);
		}
		/*遍历左右Y坐标，画矩形左右两条线*/
		for (i = Y; i < Y + Height; i ++)
		{
			OLED_DrawPoint(X, i);
			OLED_DrawPoint(X + Width - 1, i);
		}
	}
	else				//指定矩形填充
	{
		/*遍历X坐标*/
		for (i = X; i < X + Width; i ++)
		{
			/*遍历Y坐标*/
			for (j = Y; j < Y + Height; j ++)
			{
				/*在指定区域画点，填充满矩形*/
				OLED_DrawPoint(i, j);
			}
		}
	}
}

/**
  * 函    数：判断指定点是否在指定多边形内部
  * 参    数：nvert 多边形的顶点数
  * 参    数：vertx verty 包含多边形顶点的x和y坐标的数组
  * 参    数：testx testy 测试点的X和y坐标
  * 返 回 值：指定点是否在指定多边形内部，1：在内部，0：不在内部
  */
uint8_t OLED_pnpoly(uint8_t nvert, int16_t *vertx, int16_t *verty, int16_t testx, int16_t testy)
{
	int16_t i, j, c = 0;
	
	/*此算法由W. Randolph Franklin提出*/
	/*参考链接：https://wrfranklin.org/Research/Short_Notes/pnpoly.html*/
	for (i = 0, j = nvert - 1; i < nvert; j = i++)
	{
		if (((verty[i] > testy) != (verty[j] > testy)) &&
			(testx < (vertx[j] - vertx[i]) * (testy - verty[i]) / (verty[j] - verty[i]) + vertx[i]))
		{
			c = !c;
		}
	}
	return c;
}

/**
  * 函    数：OLED三角形
  * 参    数：X0 指定第一个端点的横坐标，范围：-32768~32767，屏幕区域：0~127
  * 参    数：Y0 指定第一个端点的纵坐标，范围：-32768~32767，屏幕区域：0~63
  * 参    数：X1 指定第二个端点的横坐标，范围：-32768~32767，屏幕区域：0~127
  * 参    数：Y1 指定第二个端点的纵坐标，范围：-32768~32767，屏幕区域：0~63
  * 参    数：X2 指定第三个端点的横坐标，范围：-32768~32767，屏幕区域：0~127
  * 参    数：Y2 指定第三个端点的纵坐标，范围：-32768~32767，屏幕区域：0~63
  * 参    数：IsFilled 指定三角形是否填充
  *           范围：OLED_UNFILLED		不填充
  *                 OLED_FILLED			填充
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void OLED_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled)
{
	int16_t minx = X0, miny = Y0, maxx = X0, maxy = Y0;
	int16_t i, j;
	int16_t vx[] = {X0, X1, X2};
	int16_t vy[] = {Y0, Y1, Y2};
	
	if (!IsFilled)			//指定三角形不填充
	{
		/*调用画线函数，将三个点用直线连接*/
		OLED_DrawLine(X0, Y0, X1, Y1);
		OLED_DrawLine(X0, Y0, X2, Y2);
		OLED_DrawLine(X1, Y1, X2, Y2);
	}
	else					//指定三角形填充
	{
		/*找到三个点最小的X、Y坐标*/
		if (X1 < minx) {minx = X1;}
		if (X2 < minx) {minx = X2;}
		if (Y1 < miny) {miny = Y1;}
		if (Y2 < miny) {miny = Y2;}
		
		/*找到三个点最大的X、Y坐标*/
		if (X1 > maxx) {maxx = X1;}
		if (X2 > maxx) {maxx = X2;}
		if (Y1 > maxy) {maxy = Y1;}
		if (Y2 > maxy) {maxy = Y2;}
		
		/*最小最大坐标之间的矩形为可能需要填充的区域*/
		/*遍历此区域中所有的点*/
		/*遍历X坐标*/		
		for (i = minx; i <= maxx; i ++)
		{
			/*遍历Y坐标*/	
			for (j = miny; j <= maxy; j ++)
			{
				/*调用OLED_pnpoly，判断指定点是否在指定三角形之中*/
				/*如果在，则画点，如果不在，则不做处理*/
				if (OLED_pnpoly(3, vx, vy, i, j)) 
                {OLED_DrawPoint(i, j);}
			}
		}
	}
}
//画伤心眼睛的函数
void OLED_Draw_qua(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint8_t IsFilled)
{
{
    if (!IsFilled) {                 
        OLED_DrawLine(x0, y0, x1, y1);
        OLED_DrawLine(x1, y1, x2, y2);
        OLED_DrawLine(x2, y2, x3, y3);
        OLED_DrawLine(x3, y3, x0, y0);
    } 
    else
    {                         /* 拆成两个三角形 */
        OLED_DrawTriangle(x0, y0, x1, y1, x2, y2, 1);
        OLED_DrawTriangle(x1, y1, x2, y2, x3, y3, 1);
        
    }
}
}

//定义左眼宽度高度
const int ref_left_EYE_W = 40;
const int ref_left_EYE_H = 40;
//定义右眼宽度高度
const int ref_right_EYE_W = 40;
const int ref_right_EYE_H = 40;

int left_EYE_W =  ref_left_EYE_W;   //左单眼宽
int left_EYE_H =  ref_left_EYE_H;   //左单眼高

int right_EYE_W =  ref_right_EYE_W;   //右单眼宽
int right_EYE_H =  ref_right_EYE_H;   //右单眼高

//定义左眼左上角参考x点
const int ref_left_x = 32;

int LEFT_X = ref_left_x;
int RIGHT_X = ref_left_x + ref_right_EYE_W + 24;

#define left_Y          32
#define right_Y          32
#define RADIUS   4


//画眼睛函数
void oled_draweys(void)
{
    OLED_Clear();                    // 整屏清黑
    int x = (LEFT_X - left_EYE_W / 2);
    int y = (left_Y - left_EYE_H / 2);
    OLED_DrawRoundRect(x, y, left_EYE_W, left_EYE_H, RADIUS, 1);//左眼
    x = (RIGHT_X - right_EYE_W / 2);
    y = (right_Y - right_EYE_H / 2);
    OLED_DrawRoundRect(x, y, right_EYE_W, right_EYE_H, RADIUS, 1);//右眼
    oled_update();
}


//眼睛回到中间的函数，每次运动函数表情执行完了之后，都要回到中间准备下一次的表情
void eye_center(void)
{
	int x = 12;//左眼左上角x坐标
	int y = 12;//左眼左上角x坐标
	OLED_DrawRoundRect(x, y, left_EYE_W, left_EYE_H, RADIUS, 1);//左眼
	x = 76;//右眼左上角x坐标
	y = 12;//右眼左上角y坐标
	OLED_DrawRoundRect(x, y, right_EYE_W, right_EYE_H, RADIUS, 1);//右眼
}

//移动眼睛的函数，可以左移和右移
//方向：direction ==  1  向右
//      direction == -1  向左

void move_eyes(int direction)
{
    const int step      = 2;   // 水平移动步长
    const int blinkStep = 5;   // 眨眼高度变化步长
    const int over      = 1;   // 移动方向上的“变大”增量

    /* -------- 第一段：边眨眼边朝目标方向移动 -------- */
    for (int i = 0; i < 3; ++i) {
        LEFT_X  += step * direction;
        RIGHT_X += step * direction;

        left_EYE_H  -= blinkStep;
        right_EYE_H -= blinkStep;

        if (direction == 1) {          // 向右：右眼稍大
            right_EYE_H += over;
            right_EYE_W += over;
        } else {                       // 向左：左眼稍大
            left_EYE_H  += over;
            left_EYE_W  += over;
        }
        oled_draweys();
        tal_system_sleep(100);
    }

    /* -------- 第二段：边睁眼边继续移动（3 帧） -------- */
    for (int i = 0; i < 3; ++i) {
        LEFT_X  += step * direction;
        RIGHT_X += step * direction;

        left_EYE_H  += blinkStep;
        right_EYE_H += blinkStep;

        if (direction == 1) {
            right_EYE_H += over;
            right_EYE_W += over;
        } else {
            left_EYE_H  += over;
            left_EYE_W  += over;
        }
        oled_draweys();
        tal_system_sleep(100);
    }

    /* -------- 停顿 1 秒，保持“看过去”状态 -------- */
    tal_system_sleep(1000);

    /* -------- 第三段：边眨眼边回位 -------- */
    for (int i = 0; i < 3; ++i) {
        LEFT_X  -= step * direction;
        RIGHT_X -= step * direction;

        left_EYE_H  -= blinkStep;
        right_EYE_H -= blinkStep;

        if (direction == 1) {          // 撤销右眼放大
            right_EYE_H -= over;
            right_EYE_W -= over;
        } else {                       // 撤销左眼放大
            left_EYE_H  -= over;
            left_EYE_W  -= over;
        }
        oled_draweys();
        tal_system_sleep(100);
    }

    /* -------- 第四段：边睁眼边彻底回位（3 帧） -------- */
    for (int i = 0; i < 3; ++i) {
        LEFT_X  -= step * direction;
        RIGHT_X -= step * direction;

        left_EYE_H  += blinkStep;
        right_EYE_H += blinkStep;

        if (direction == 1) {
            right_EYE_H -= over;
            right_EYE_W -= over;
        } else {
            left_EYE_H  -= over;
            left_EYE_W  -= over;
        }
        oled_draweys();
        tal_system_sleep(100);
    }

    eye_center();   
}

void eye_right(void)
{
  move_eyes(1);
}

void eye_left(void)
{
  move_eyes(-1);
}



void w_eye_move(void)
{
	while(1)
	{
	eye_right();
	eye_left();
	}
}

void eye_blink(int speed)
{
    oled_draweys();
    //tal_system_sleep(500);

    // 闭眼
    for (int i = 0; i < 3; i++) {
		left_EYE_H = left_EYE_H - speed;
        right_EYE_H = right_EYE_H - speed;
        oled_draweys();
        tal_system_sleep(100);
    }

    // 睁眼
    for (int i = 0; i < 3; i++) {
		left_EYE_H = left_EYE_H + speed;
        right_EYE_H = right_EYE_H + speed;
        oled_draweys();
        tal_system_sleep(100);
    }

}

void eye_sad(void)
{
    
    OLED_Clear();      
    int x0 = 12;int y0 = 35;
    int x1 = 52;int y1 = 12;
    int x2 = 52;int y2 = 36;
    OLED_DrawTriangle(x0, y0, x1, y1, x2, y2, 1);
    OLED_DrawRoundRect(12, 32, 40, 20, 4, 1);
    x0 = 76; y0 = 12;
    x1 = 116; y1 = 35;
    x2 = 76; y2 = 36;   
    OLED_DrawTriangle(x0, y0, x1, y1, x2, y2, 1);
    OLED_DrawRoundRect(76, 32, 40, 20, 4, 1);
    oled_update();
    tal_system_sleep(100);
}

void w_eye_blink(void)
{
    while (1)
    {
        eye_blink(6);
        tal_system_sleep(300);
    }
    
}
/*typedef enum {
    OLED_SAD = 0,
    OLED_LEFT,
    OLED_RIGHT,
    OLED_BLINK,
    OLED_MAX_NUM

} OLED_RANDOM;

STATIC CONST CHAR_T *oled_name[OLED_MAX_NUM] = {
    "eye_sad", "eye_left", "eye_right", "eye_blink"
};*/

/* 执行一次“随机表情” */
STATIC VOID emoji_random(VOID)
{
    OLED_RANDOM oled = rand() % OLED_MAX_NUM;

    switch (oled) 
    {
    case OLED_SAD:      
    eye_sad();
    break;
    case OLED_LEFT:        
    eye_left(); 
    break;
    case OLED_RIGHT:  
    eye_right(); 
    break;
    case OLED_BLINK:
     eye_blink(8); 
     eye_blink(8);
     eye_blink(8);
     break;
    
    default: break;
    }
}

void emoji_loop(void)
{
    if (!emoji_enable) return;
    emoji_random();   

}

/* 摇头线程入口 */
VOID_T servo_emoji_thread(VOID_T *arg)
{
    while (emoji_enable) {
        emoji_loop();         // 摇一次
        tal_system_sleep(2000);            // 每 2 s 摇一次
    }
    emoji_enable = NULL;                 // 线程即将结束
    tal_thread_delete(NULL);               
}

/* 表情摇头线程 */
VOID_T emoji_start(void)
{
    if (emoji_thread != NULL) return;    // 已经跑起来了

    THREAD_CFG_T cfg = {
        .stackDepth = 2048,
        .priority   = THREAD_PRIO_4,
        .thrdname   = "emoji_task"
    };
    emoji_enable = TRUE;
    tal_thread_create_and_start(&emoji_thread, NULL, NULL, servo_emoji_thread, NULL, &cfg);
}

/* 停止线程 */
void servo_emoji_stop(void)
{
    emoji_enable = FALSE;   // 线程里下一次判断就会自然退出
}
