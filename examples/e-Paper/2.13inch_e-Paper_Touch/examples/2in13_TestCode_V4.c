#include "EPD_Test.h"
#include "EPD_2in13_V4.h"
#include "GT1151.h"

// Timeout configuration (seconds)
#define TOUCH_TIMEOUT_S  10
#define TIMER_ID TUYA_TIMER_NUM_3

extern GT1151_Dev Dev_Now, Dev_Old;
// E-ink screen sleep status marker
static volatile bool is_epd_sleeping = false;
// Idle second counting based on timers
static volatile uint32_t idle_seconds = 0;

// 43*122     6*122
const unsigned char *PhotoPath_S_2in13_V4[7] = {gImage_Photo_1_0, gImage_Photo_1_1, gImage_Photo_1_2, gImage_Photo_1_3, gImage_Photo_1_4,	gImage_Photo_1_5, gImage_Photo_1_6};
// 88*246     11*246
const unsigned char *PhotoPath_L_2in13_V4[7] = {gImage_Photo_2_0, gImage_Photo_2_1, gImage_Photo_2_2, gImage_Photo_2_3, gImage_Photo_2_4, gImage_Photo_2_5, gImage_Photo_2_6};
// 122*250    16*250
const unsigned char *PagePath_2in13_V4[4] = {gImage_Menu, gImage_White_board, gImage_Photo_1, gImage_Photo_2};


static void touch_timeout_cb(void *args)
{
    // If it has already gone into hibernation, there is no need to check again
    if(is_epd_sleeping) {
        return;
    }

    // The timer is called once per second: it accumulates the idle seconds count and determines the timeout
    idle_seconds++;
    if (idle_seconds >= TOUCH_TIMEOUT_S) {
        is_epd_sleeping = true;
    }
}

static void start_inactivity_timer(void)
{
     OPERATE_RET rt = OPRT_OK;

    // Create a timer (periodic mode, with the callback function touch_timeout_cb)
    TUYA_TIMER_BASE_CFG_T sg_timer_cfg = {.mode = TUYA_TIMER_MODE_PERIOD, .args = NULL, .cb = touch_timeout_cb};
    rt = tkl_timer_init(TIMER_ID, &sg_timer_cfg);
    if (rt != OPRT_OK) {
        PR_DEBUG("Failed to create touch timeout timer, rt: %d", rt);
        return;
    }

    // Start the timer (with a cycle of 1000ms, that is, triggered once per second)
    rt = tkl_timer_start(TIMER_ID, 1000 * 1000);
    if (rt != OPRT_OK) {
        PR_DEBUG("Failed to start touch timeout timer, rt: %d", rt);
        tkl_timer_deinit(TIMER_ID);  // Creation failed. Delete the timer
        return;
    }
}

void Handler_2in13_V4(int signo)
{
    //System Exit
    printf("\r\nHandler:exit\r\n");
	EPD_2in13_V4_Sleep();
	DEV_Delay_ms(2000);
    DEV_Module_Exit(); 
    exit(0);
}



void Show_Photo_Small_2in13_V4(UBYTE small)
{
	for(UBYTE t=1; t<5; t++) {
		if(small*2+t > 6)
            Paint_DrawBitMap2(PhotoPath_S_2in13_V4[0], (t-1)/2*45+2, (t%2)*124+2, 48, 122);
		else
            Paint_DrawBitMap2(PhotoPath_S_2in13_V4[small*2+t], (t-1)/2*45+2, (t%2)*124+2, 48, 122);
	}
}

void Show_Photo_Large_2in13_V4(UBYTE large)
{
	if(large > 6)
        Paint_DrawBitMap2(PhotoPath_L_2in13_V4[0], 2, 2, 88, 246);
	else
        Paint_DrawBitMap2(PhotoPath_L_2in13_V4[large], 2, 2, 88, 246);
}

void GPIO_irq_2in13_V4(void *arg)
{
    Dev_Now.Touch = 1;
    // Reset the idle count to avoid accidentally triggering sleep
    idle_seconds = 0;
}

void GPIO_irq_init(void)
{
    PR_DEBUG("[GPIO_irq_init] Entry");
    PR_DEBUG("[GPIO_irq_init] EPD_TINT_PIN=%d", EPD_TINT_PIN);
    TUYA_GPIO_IRQ_T irq_cfg = {
        .cb = (void (*)(void *))GPIO_irq_2in13_V4,
        .arg = NULL,
        .mode = TUYA_GPIO_IRQ_LOW,
    };

    int ret = tkl_gpio_irq_init(EPD_TINT_PIN, &irq_cfg);
    PR_DEBUG("[GPIO_irq_init] irq_init ret=%d", ret);
    
    ret = tkl_gpio_irq_enable(EPD_TINT_PIN);
}
int EPD_test(void)
{
    UDOUBLE i=0, j=0, k=0;
	UBYTE Page=0, Photo_L=0, Photo_S=0;
	UBYTE ReFlag=0, SelfFlag=0;
	DEV_Module_Init();
    GPIO_irq_init();
    EPD_2in13_V4_Init(EPD_2IN13_V4_FULL);
	
    EPD_2in13_V4_Clear();
	GT_Init();
	DEV_Delay_ms(100);
    //Create a new image cache
    UBYTE *BlackImage;
    UWORD Imagesize = ((EPD_2in13_V4_WIDTH % 8 == 0)? (EPD_2in13_V4_WIDTH / 8 ): (EPD_2in13_V4_WIDTH / 8 + 1)) * EPD_2in13_V4_HEIGHT;
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        return -1;
    }
    printf("Paint_NewImage\r\n");
    Paint_NewImage(BlackImage, EPD_2in13_V4_WIDTH, EPD_2in13_V4_HEIGHT, 0, WHITE);
    Paint_SelectImage(BlackImage);
    Paint_SetMirroring(MIRROR_ORIGIN);
    Paint_Clear(WHITE);
    Paint_DrawBitMap(gImage_Menu);
	EPD_2in13_V4_Display(BlackImage);
	EPD_2in13_V4_Init(EPD_2IN13_V4_PART);
	EPD_2in13_V4_Display_Partial_Wait(BlackImage);

    // Start the offline timer (start timing when the program starts)
    start_inactivity_timer();

	while(1) {
        // If it has gone into sleep mode, wake up the e-ink screen first
        while(is_epd_sleeping){
            Debug("Touch timeout, entering sleep mode...");
            EPD_2in13_V4_Sleep(); 
            while(is_epd_sleeping)
            {
                if(!Dev_Now.Touch){
                    Debug("Wait for Waking up EPD");
                    DEV_Delay_ms(500);
                }else{
                    Debug("Waking up EPD...");
                    EPD_2in13_V4_Init(EPD_2IN13_V4_FULL);
                    DEV_Delay_ms(10);
                    EPD_2in13_V4_Init(EPD_2IN13_V4_PART);  
                    EPD_2in13_V4_Display_Partial(BlackImage);
                    is_epd_sleeping = false;
                }
            }
        }

		// k++;
		if(i > 12 || ReFlag == 1) {
			if(Page == 1 && SelfFlag != 1)
				EPD_2in13_V4_Display_Partial(BlackImage);
			else 
				EPD_2in13_V4_Display_Partial_Wait(BlackImage);
			i = 0;
			k = 0;
			j++;
			ReFlag = 0;
			printf("*** Draw Refresh ***\r\n");
		}else if(k++>30000000 && i>0 && Page == 1) {
			EPD_2in13_V4_Display(BlackImage);
			i = 0;
			k = 0;
			j++;
			printf("*** Overtime Refresh ***\r\n");
		}else if(j > 100 || SelfFlag) {
			SelfFlag = 0;
			j = 0;
			EPD_2in13_V4_Init(EPD_2IN13_V4_FULL);
			EPD_2in13_V4_Display_Base(BlackImage);
			EPD_2in13_V4_Init(EPD_2IN13_V4_PART);
			printf("--- Self Refresh ---\r\n");
		}

		if(GT_Scan()==1 || (Dev_Now.X[0] == Dev_Old.X[0] && Dev_Now.Y[0] == Dev_Old.Y[0])) { // No new touch
			// printf("%d %d \r\n", j, SelfFlag);
			// printf("No new touch \r\n");
			continue;
		}

		if(Dev_Now.TouchpointFlag) {
			i++;
			Dev_Now.TouchpointFlag = 0;

			if(Page == 0  && ReFlag == 0) {	//main menu
				if(Dev_Now.X[0] > 29 && Dev_Now.X[0] < 92 && Dev_Now.Y[0] > 56 && Dev_Now.Y[0] < 95) {
					printf("Photo ...\r\n");
					Page = 2;
                    Paint_DrawBitMap(PagePath_2in13_V4[Page]);
					Show_Photo_Small_2in13_V4(Photo_S);
					ReFlag = 1;
				}
				else if(Dev_Now.X[0] > 29 && Dev_Now.X[0] < 92 && Dev_Now.Y[0] > 153 && Dev_Now.Y[0] < 193) {
					printf("Draw ...\r\n");
					Page = 1;
                    Paint_DrawBitMap(PagePath_2in13_V4[Page]);
					ReFlag = 1;
				}
			}

			if(Page == 1 && ReFlag == 0) {	//white board
				Paint_DrawPoint(Dev_Now.X[0], Dev_Now.Y[0], BLACK, Dev_Now.S[0]/8+1, DOT_STYLE_DFT);
				
				if(Dev_Now.X[0] > 96 && Dev_Now.X[0] < 118 && Dev_Now.Y[0] > 6 && Dev_Now.Y[0] < 30) {
					printf("Home ...\r\n");
					Page = 1;
                    Paint_DrawBitMap(PagePath_2in13_V4[Page]);
					ReFlag = 1;
				}
				else if(Dev_Now.X[0] > 96 && Dev_Now.X[0] < 118 && Dev_Now.Y[0] > 113 && Dev_Now.Y[0] < 136) {
					printf("Clear ...\r\n");
					Page = 0;
                    Paint_DrawBitMap(PagePath_2in13_V4[Page]);
					ReFlag = 1;
				}
				else if(Dev_Now.X[0] > 96 && Dev_Now.X[0] < 118 && Dev_Now.Y[0] > 220 && Dev_Now.Y[0] < 242) {
					printf("Refresh ...\r\n");
					SelfFlag = 1;
					ReFlag = 1;
				}
			}

			if(Page == 2  && ReFlag == 0) { //photo menu
				if(Dev_Now.X[0] > 97 && Dev_Now.X[0] < 119 && Dev_Now.Y[0] > 113 && Dev_Now.Y[0] < 136) {
					printf("Home ...\r\n");
					Page = 0;
                    Paint_DrawBitMap(PagePath_2in13_V4[Page]);
					ReFlag = 1;
				}
				else if(Dev_Now.X[0] > 97 && Dev_Now.X[0] < 119 && Dev_Now.Y[0] > 57 && Dev_Now.Y[0] < 78) {
					printf("Next page ...\r\n");
					Photo_S++;
					if(Photo_S > 2)	// 6 photos is a maximum of three pages
						Photo_S=0;
					ReFlag = 2;
				}
				else if(Dev_Now.X[0] > 97 && Dev_Now.X[0] < 119 && Dev_Now.Y[0] > 169 && Dev_Now.Y[0] < 190) {
					printf("Last page ...\r\n");
					if(Photo_S == 0)
						printf("Top page ...\r\n");
					else {
						Photo_S--;
						ReFlag = 2;
					}
				}
				else if(Dev_Now.X[0] > 97 && Dev_Now.X[0] < 119 && Dev_Now.Y[0] > 220 && Dev_Now.Y[0] < 242) {
					printf("Refresh ...\r\n");
					SelfFlag = 1;
					ReFlag = 1;
				}
				else if(Dev_Now.X[0] > 2 && Dev_Now.X[0] < 90 && Dev_Now.Y[0] > 2 && Dev_Now.Y[0] < 248 && ReFlag == 0) {
					printf("Select photo ...\r\n");
					Page = 3;
                    Paint_DrawBitMap(PagePath_2in13_V4[Page]);	
					Photo_L = Dev_Now.X[0]/46*2 + 2-Dev_Now.Y[0]/124 + Photo_S*2;
					Show_Photo_Large_2in13_V4(Photo_L);
					ReFlag = 1;
				}
				if(ReFlag == 2) { // Refresh small photo
					ReFlag = 1;
                    Paint_DrawBitMap(PagePath_2in13_V4[Page]);
					Show_Photo_Small_2in13_V4(Photo_S);	// show small photo
				}
			}
			
			if(Page == 3  && ReFlag == 0) {	//view the photo
				if(Dev_Now.X[0] > 96 && Dev_Now.X[0] < 117 && Dev_Now.Y[0] > 4 && Dev_Now.Y[0] < 25) {
					printf("Photo menu ...\r\n");
					Page = 2;
                    Paint_DrawBitMap(PagePath_2in13_V4[Page]);
					Show_Photo_Small_2in13_V4(Photo_S);
					ReFlag = 1;
				}
				else if(Dev_Now.X[0] > 96 && Dev_Now.X[0] < 117 && Dev_Now.Y[0] > 57 && Dev_Now.Y[0] < 78) {
					printf("Next photo ...\r\n");
					Photo_L++;
					if(Photo_L > 6)
						Photo_L=1;
					ReFlag = 2;
				}
				else if(Dev_Now.X[0] > 96 && Dev_Now.X[0] < 117 && Dev_Now.Y[0] > 113 && Dev_Now.Y[0] < 136) {
					printf("Home ...\r\n");
					Page = 0;
                    Paint_DrawBitMap(PagePath_2in13_V4[Page]);
					ReFlag = 1;
				}
				else if(Dev_Now.X[0] > 96 && Dev_Now.X[0] < 117 && Dev_Now.Y[0] > 169 && Dev_Now.Y[0] < 190) {
					printf("Last page ...\r\n");
					if(Photo_L == 1)
						printf("Top photo ...\r\n");
					else {
						Photo_L--;
						ReFlag = 2;
					}
				}
				else if(Dev_Now.X[0] > 96 && Dev_Now.X[0] < 117 && Dev_Now.Y[0] > 220 && Dev_Now.Y[0] < 242) {
					printf("Refresh photo ...\r\n");
					SelfFlag = 1;
					ReFlag = 1;
				}
				if(ReFlag == 2) {	// Refresh large photo
					ReFlag = 1;
					Show_Photo_Large_2in13_V4(Photo_L);
				}
			}
		}
	}
    tkl_timer_stop(TIMER_ID);  // 停止定时器
    tkl_timer_deinit(TIMER_ID);  // 删除定时器
    free(BlackImage);
	return 0;
}
