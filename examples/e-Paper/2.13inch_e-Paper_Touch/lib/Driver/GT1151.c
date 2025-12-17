#include "GT1151.h"

GT1151_Dev Dev_Now, Dev_Old;
UBYTE GT_Gesture_Mode = 0;

void GT_Reset(void)
{
	DEV_Digital_Write(EPD_TRST_PIN, 1);
	DEV_Delay_ms(100);
	DEV_Digital_Write(EPD_TRST_PIN, 0);
	DEV_Delay_ms(100);
	DEV_Digital_Write(EPD_TRST_PIN, 1);
	DEV_Delay_ms(100);
}

void GT_Write(UWORD Reg, char *Data, UBYTE len)
{
	I2C_Write_Byte(Reg, Data, len);
}

void GT_Read(UWORD Reg, char *Data, UBYTE len)
{
	I2C_Read_Byte(Reg, Data, len);
}

void GT_ReadVersion(void)
{
	char buf[4];
	GT_Read(0x8140, (char *)buf, 4);
	Debug("Product ID is %02x %02x %02x %02x \r\n", buf[0], buf[1], buf[2] ,buf[3]);
}

void GT_Init(void)
{
	GT_Reset();
	GT_ReadVersion();
}

void GT_Gesture(void)
{
	char buf[3] ={0x08, 0x00, 0xf8};
	GT_Write(0x8040, &buf[0], 1);
	GT_Write(0x8041, &buf[1], 1);
	GT_Write(0x8042, &buf[2], 1);
	GT_Gesture_Mode = 1;
	Debug("into gesture mode \r\n");
	DEV_Delay_ms(1);
}

void GT_Gesture_Scan(void)
{
	uint8_t buf;
	GT_Read(0x814c, (char *)&buf, 1);
	if(buf == 0xcc)
	{
		Debug("gesture mode exiting \r\n");
		GT_Gesture_Mode = 0;
		GT_Reset();
		Dev_Old.X[0] = Dev_Now.X[0];
		Dev_Old.Y[0] = Dev_Now.Y[0];
		Dev_Old.S[0] = Dev_Now.S[0];
	}
	else
	{
		buf = 0x00;
		GT_Write(0x814c, (char *)&buf, 1);
	}
		
}

UBYTE GT_Scan(void)
{
	uint8_t buf[100];
	uint8_t mask[1] = {0x00};
	
	if (Dev_Now.Touch == 1) {
		Dev_Now.Touch = 0;
        // PR_DEBUG("Dev_Now.Touch");
		if(GT_Gesture_Mode == 1)
		{
			GT_Gesture_Scan();
			return 1;
		}
		else 
		{
			GT_Read(0x814E, (char *)buf, 1);
			if ((buf[0]&0x80) == 0x00) {		//No new touch
				GT_Write(0x814E, (char *)mask, 1);
				DEV_Delay_ms(1);
				// Debug("buffers status is 0 \r\n");
				return 1;
			}
			else {
				Dev_Now.TouchpointFlag = buf[0]&0x80;
				Dev_Now.TouchCount = buf[0]&0x0f;
				if (Dev_Now.TouchCount > 5 || Dev_Now.TouchCount < 1) {
					GT_Write(0x814E, (char *)mask, 1);
					// Debug("TouchCount number is wrong \r\n");
					return 1;
				}
				GT_Read(0x814F, (char *)&buf[1], Dev_Now.TouchCount*8);
				GT_Write(0x814E, (char *)mask, 1);
				
				Dev_Old.X[0] = Dev_Now.X[0];
				Dev_Old.Y[0] = Dev_Now.Y[0];
				Dev_Old.S[0] = Dev_Now.S[0];
				
				for(UBYTE i=0; i<Dev_Now.TouchCount; i++) {
					Dev_Now.Touchkeytrackid[i] = buf[1 + 8*i];
					Dev_Now.X[i] = (UWORD)(((uint16_t)buf[3+8*i] << 8) | (uint16_t)buf[2+8*i]);
                    Dev_Now.Y[i] = (UWORD)(((uint16_t)buf[5+8*i] << 8) | (uint16_t)buf[4+8*i]);
                    Dev_Now.S[i] = (UWORD)(((uint16_t)buf[7+8*i] << 8) | (uint16_t)buf[6+8*i]);
				}
				
				for(UBYTE i=0; i<Dev_Now.TouchCount; i++)
					Debug("Point %d: X is %d, Y is %d, Size is %d \r\n", i+1, (unsigned)Dev_Now.X[i], (unsigned)Dev_Now.Y[i], (unsigned)Dev_Now.S[i]);
				return 0;
			}
		}
	}
	return 1;
}