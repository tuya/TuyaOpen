/**
 * @file lv_100ask_2048.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_2048.h"

#if LV_USE_GAME_2048 != 0
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS    &lv_100ask_2048_class

#define LV_100ASK_2048_BG_COLOR             lv_color_hex(0xb3a397)
#define LV_100ASK_2048_TEXT_BLACK_COLOR     lv_color_hex(0x6c635b)
#define LV_100ASK_2048_TEXT_WHITE_COLOR     lv_color_hex(0xf8f5f0)

#define LV_100ASK_2048_NUMBER_EMPTY_COLOR   lv_color_hex(0xc7b9ac)
#define LV_100ASK_2048_NUMBER_2_COLOR 		lv_color_hex(0xeee4da)
#define LV_100ASK_2048_NUMBER_4_COLOR 		lv_color_hex(0xede0c8)
#define LV_100ASK_2048_NUMBER_8_COLOR 		lv_color_hex(0xf2b179)
#define LV_100ASK_2048_NUMBER_16_COLOR 		lv_color_hex(0xf59563)
#define LV_100ASK_2048_NUMBER_32_COLOR 		lv_color_hex(0xf67c5f)
#define LV_100ASK_2048_NUMBER_64_COLOR 		lv_color_hex(0xf75f3b)
#define LV_100ASK_2048_NUMBER_128_COLOR 	lv_color_hex(0xedcf72)
#define LV_100ASK_2048_NUMBER_256_COLOR 	lv_color_hex(0xedcc61)
#define LV_100ASK_2048_NUMBER_512_COLOR 	lv_color_hex(0xedc850)
#define LV_100ASK_2048_NUMBER_1024_COLOR 	lv_color_hex(0xedc53f)
#define LV_100ASK_2048_NUMBER_2048_COLOR 	lv_color_hex(0xedc22e)

/**********************
 *      TYPEDEFS
 **********************/


/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_100ask_2048_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_100ask_2048_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_100ask_2048_event(const lv_obj_class_t * class_p, lv_event_t * e);
static void btnm_event_cb(lv_event_t * e);

static void init_matrix_num(uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE]);
static void addRandom(uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE]);
static void update_btnm_map(char * lv_100ask_2048_btnm_map[], uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE]);
static lv_color_t get_num_color(uint16_t num);
static char * int_to_str(char * str, uint16_t num);

static uint8_t count_empty(uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE]);
static uint8_t find_target(uint16_t array[MATRIX_SIZE], uint8_t x, uint8_t stop);
static void rotate_matrix(uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE]);
static bool find_pair_down(uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE]);
static bool slide_array(uint16_t * score, uint16_t array[MATRIX_SIZE]);
static bool move_up(uint16_t * score, uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE]);
static bool move_down(uint16_t * score, uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE]);
static bool move_left(uint16_t * score, uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE]);
static bool move_right(uint16_t * score, uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE]);
static bool game_over(uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE]);

static lv_obj_t *screen_2048;

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t lv_100ask_2048_class = {
    .constructor_cb = lv_100ask_2048_constructor,
    .destructor_cb  = lv_100ask_2048_destructor,
    //.event_cb       = lv_100ask_2048_event,
    .width_def      = LV_DPI_DEF * 2,
    .height_def     = LV_DPI_DEF * 2,
    .group_def = LV_OBJ_CLASS_GROUP_DEF_TRUE,
    .instance_size  = sizeof(lv_100ask_2048_t),
    .base_class     = &lv_obj_class,
    .name           = "game 2048"
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_100ask_2048_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}


/*=====================
 * Setter functions
 *====================*/
void lv_100ask_2048_set_new_game(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_100ask_2048_t * game_2048 = (lv_100ask_2048_t *)obj;

    game_2048->score = 0;
    game_2048->game_over = false;
    game_2048->map_count = MATRIX_SIZE * MATRIX_SIZE + MATRIX_SIZE;

    init_matrix_num(game_2048->matrix);
    update_btnm_map(game_2048->btnm_map, game_2048->matrix);
    lv_btnmatrix_set_map(game_2048->btnm, game_2048->btnm_map);

    lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
}


/*=====================
 * Getter functions
 *====================*/
uint16_t lv_100ask_2048_get_score(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_100ask_2048_t * game_2048 = (lv_100ask_2048_t *)obj;

    return game_2048->score;
}

bool lv_100ask_2048_get_status(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_100ask_2048_t * game_2048 = (lv_100ask_2048_t *)obj;

    return game_2048->game_over;
}


uint16_t lv_100ask_2048_get_best_tile(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_100ask_2048_t * game_2048 = (lv_100ask_2048_t *)obj;

    uint8_t x, y;
    uint16_t best_tile = 0;

	for (x = 0; x < MATRIX_SIZE; x++) {
		for (y = 0; y < MATRIX_SIZE; y++) {
			if (best_tile < game_2048->matrix[x][y])
                best_tile = game_2048->matrix[x][y];
		}
	}

    return (uint16_t)(1 << best_tile);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/


static void game_play_event(lv_event_t * e)
{
    lv_res_t res;

    bool success = false;
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btnm = lv_event_get_target(e);
    lv_obj_t * obj = lv_event_get_user_data(e);

    lv_100ask_2048_t * game_2048 = (lv_100ask_2048_t *)obj;

    if (code == LV_EVENT_GESTURE)
    {
        game_2048->game_over = game_over(game_2048->matrix);
        if (!game_2048->game_over)
        {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            switch(dir)
            {
                case LV_DIR_TOP:
                    success = move_left(&(game_2048->score), game_2048->matrix);
                    break;
                case LV_DIR_BOTTOM:
                    success = move_right(&(game_2048->score), game_2048->matrix);
                    break;
                case LV_DIR_LEFT:
                    success = move_up(&(game_2048->score), game_2048->matrix);
                    break;
                case LV_DIR_RIGHT:
                    success = move_down(&(game_2048->score), game_2048->matrix);
                    break;
                default: break;
            }
        }
        else
        {
            LV_LOG_USER("100ASK 2048 GAME OVER!");
            res = lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
            if(res != LV_RES_OK) return;
        }
    }
    else if(code == LV_EVENT_KEY)
    {
        game_2048->game_over = game_over(game_2048->matrix);
        if (!game_2048->game_over)
        {
            switch(*((uint8_t *)lv_event_get_param(e)))
            {
                case LV_KEY_UP:
                    success = move_left(&(game_2048->score), game_2048->matrix);
                    break;
                case LV_KEY_DOWN:
                    success = move_right(&(game_2048->score), game_2048->matrix);
                    break;
                case LV_KEY_LEFT:
                    success = move_up(&(game_2048->score), game_2048->matrix);
                    break;
                case LV_KEY_RIGHT:
                    success = move_down(&(game_2048->score), game_2048->matrix);
                    break;
                default: break;
            }
        }
        else
        {
            LV_LOG_USER("100ASK 2048 GAME OVER!");
            res = lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
            if(res != LV_RES_OK) return;
        }
    }

    if (success)
    {
        addRandom(game_2048->matrix);
        update_btnm_map(game_2048->btnm_map, game_2048->matrix);
        lv_btnmatrix_set_map(game_2048->btnm, game_2048->btnm_map);

        res = lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
        if(res != LV_RES_OK) return;
    }
}

static void lv_100ask_2048_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    lv_100ask_2048_t * game_2048 = (lv_100ask_2048_t *)obj;

    game_2048->score = 0;
    game_2048->game_over = false;
    game_2048->map_count = MATRIX_SIZE * MATRIX_SIZE + MATRIX_SIZE;

    uint16_t index;
    for (index = 0; index < game_2048->map_count; index++) {

        if (((index + 1) % 5) == 0)
        {
            game_2048->btnm_map[index] = lv_malloc(2);
            if ((index+1) == game_2048->map_count)
                strcpy(game_2048->btnm_map[index], "");
            else
                strcpy(game_2048->btnm_map[index], "\n");
        }
        else
        {
            game_2048->btnm_map[index] = lv_malloc(5);
            strcpy(game_2048->btnm_map[index], " ");
        }
    }

    init_matrix_num(game_2048->matrix);
    update_btnm_map(game_2048->btnm_map, game_2048->matrix);

    /*obj style init*/
    lv_theme_t * theme = lv_theme_get_from_obj(obj);
    lv_obj_set_style_outline_color(obj, theme->color_primary, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(obj, lv_display_dpx(theme->disp, 2), LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(obj, lv_display_dpx(theme->disp, 2), LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_opa(obj, LV_OPA_50, LV_STATE_FOCUS_KEY);

    /*game_2048->btnm init*/
    game_2048->btnm = lv_btnmatrix_create(obj);
    lv_obj_set_size(game_2048->btnm, LV_PCT(100), LV_PCT(100));
    lv_obj_center(game_2048->btnm);
    lv_obj_set_style_pad_all(game_2048->btnm, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(game_2048->btnm, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_group_remove_obj(game_2048->btnm);
    //lv_obj_add_flag(game_2048->btnm, LV_OBJ_FLAG_EVENT_BUBBLE);
    //lv_obj_add_flag(game_2048->btnm, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(game_2048->btnm, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_remove_flag(game_2048->btnm, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_btnmatrix_set_map(game_2048->btnm, game_2048->btnm_map);
    lv_btnmatrix_set_btn_ctrl_all(game_2048->btnm, LV_BTNMATRIX_CTRL_DISABLED);

    lv_obj_add_event_cb(game_2048->btnm, btnm_event_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);
    lv_obj_add_event_cb(game_2048->btnm, game_play_event, LV_EVENT_ALL, obj);

    LV_TRACE_OBJ_CREATE("finished");
}

static void lv_100ask_2048_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);

    lv_100ask_2048_t * game_2048 = (lv_100ask_2048_t *)obj;

    uint16_t index, count;
    for (index = 0; index < game_2048->map_count; index++)
    {
        lv_free(game_2048->btnm_map[index]);
    }
}


static void lv_100ask_2048_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);

    lv_res_t res;

    /*Call the ancestor's event handler*/
    res = lv_obj_event_base(MY_CLASS, e);
    if(res != LV_RES_OK) return;

    lv_event_code_t code = lv_event_get_code(e);
}


static void btnm_event_cb(lv_event_t * e)
{
    lv_obj_t * btnm = lv_event_get_target(e);
    lv_obj_t * parent = lv_obj_get_parent(btnm);

    lv_100ask_2048_t * game_2048 = (lv_100ask_2048_t *)parent;

    lv_draw_task_t * draw_task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t * base_dsc = draw_task->draw_dsc;

    if(base_dsc->part == LV_PART_ITEMS) {
        //lv_obj_draw_part_dsc_t * dsc = lv_event_get_param(e);

        lv_draw_label_dsc_t * label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
        lv_draw_fill_dsc_t * fill_draw_dsc   = lv_draw_task_get_fill_dsc(draw_task);
        lv_draw_rect_dsc_t * rect_draw_dsc   = lv_draw_task_get_mask_rect_dsc(draw_task);

        /*Change the draw descriptor the button*/
        if(base_dsc->id1 >= 0)
        {
            uint16_t x, y, num;

            x = (uint16_t)((base_dsc->id1) / 4);
            y = (base_dsc->id1) % 4;
            num = (uint16_t)(1 << (game_2048->matrix[x][y]));

            if(fill_draw_dsc)
            {
                fill_draw_dsc->radius= 3;
                fill_draw_dsc->color = get_num_color(num);
            }

            if(label_draw_dsc)
            {
                if (num < 8)
                    label_draw_dsc->color = LV_100ASK_2048_TEXT_BLACK_COLOR;
                else
                    label_draw_dsc->color = LV_100ASK_2048_TEXT_WHITE_COLOR;
            }
        }
        /*Change the draw descriptor the btnm main*/
        else if(base_dsc->id1 == 0)
        {
            if(fill_draw_dsc)
                fill_draw_dsc->radius = 5;
            if(rect_draw_dsc)
                rect_draw_dsc->border_width = 0;
        }

    }
}


static void init_matrix_num(uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE])
{
	uint8_t x, y;

	for (x = 0; x < MATRIX_SIZE; x++) {
		for (y = 0; y < MATRIX_SIZE; y++) {
			matrix[x][y] = 0;
		}
	}

	/* 初始化两个随机位置的随机数 */
	addRandom(matrix);
	addRandom(matrix);
}


static void addRandom(uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE])
{
	static bool initialized = false;
	uint16_t x, y;
	uint16_t r, len = 0;
	uint16_t n, list[MATRIX_SIZE * MATRIX_SIZE][2];

	if (!initialized) {
		srand(time(NULL));
		initialized = true;
	}

	for (x = 0; x < MATRIX_SIZE; x++) {
		for (y = 0; y < MATRIX_SIZE; y++) {
			if (matrix[x][y] == 0) {
				list[len][0] = x;
				list[len][1] = y;
				len++;
			}
		}
	}

	if (len > 0) {
		r = rand() % len;
		x = list[r][0];
		y = list[r][1];
		n = ((rand() % 10) / 9) + 1;
		matrix[x][y] = n;
	}
 }


static void update_btnm_map(char * btnm_map[], uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE])
{
    uint8_t x, y, index;

    index = 0;
    for (x = 0; x < MATRIX_SIZE; x++) {
		for (y = 0; y < MATRIX_SIZE; y++) {

            if (((index + 1) % 5) == 0)
                index++;

            if (matrix[x][y] != 0)
                int_to_str(btnm_map[index], (uint16_t)(1 << matrix[x][y]));
            else
            	strcpy(btnm_map[index], " ");

            index++;

        }
    }
}


//10进制
static char* int_to_str(char * str, uint16_t num)
{
	uint8_t i = 0;//指示填充str
	if(num < 0)//如果num为负数，将num变正
	{
		num = -num;
		str[i++] = '-';
	}
	//转换
	do
	{
		str[i++] = num%10+48;//取num最低位 字符0~9的ASCII码是48~57；简单来说数字0+48=48，ASCII码对应字符'0'
		num /= 10;//去掉最低位
	}while(num);//num不为0继续循环

	str[i] = '\0';

	//确定开始调整的位置
	uint8_t j = 0;
	if(str[0] == '-')//如果有负号，负号不用调整
	{
		j = 1;//从第二位开始调整
		++i;//由于有负号，所以交换的对称轴也要后移1位
	}
	//对称交换
	for(; j < (i / 2); j++)
	{
		//对称交换两端的值 其实就是省下中间变量交换a+b的值：a=a+b;b=a-b;a=a-b;
		str[j] = str[j] + str[i-1-j];
		str[i-1-j] = str[j] - str[i-1-j];
		str[j] = str[j] - str[i-1-j];
	}

	return str;//返回转换后的值
}



static uint8_t find_target(uint16_t array[MATRIX_SIZE], uint8_t x, uint8_t stop) {
	uint8_t t;
	// if the position is already on the first, don't evaluate
	if (x == 0) {
		return x;
	}
	for(t = x-1; ; t--) {
		if (array[t] != 0) {
			if (array[t] != array[x]) {
				// merge is not possible, take next position
				return t + 1;
			}
			return t;
		} else {
			// we should not slide further, return this one
			if (t == stop) {
				return t;
			}
		}
	}
	// we did not find a
	return x;
}

static bool slide_array(uint16_t * score, uint16_t array[MATRIX_SIZE]) {
	bool success = false;
	uint8_t x, t, stop = 0;

	for (x = 0; x < MATRIX_SIZE; x++) {
		if (array[x] != 0) {
			t = find_target(array,x,stop);
			// if target is not original position, then move or merge
			if (t != x) {
				// if target is zero, this is a move
				if (array[t] == 0) {
					array[t] = array[x];
				} else if (array[t] == array[x]) {
					// merge (increase power of two)
					array[t]++;
					// increase score
					*score += (uint32_t)1<<array[t];
					// set stop to avoid double merge
					stop = t + 1;
				}
				array[x] = 0;
				success = true;
			}
		}
	}
	return success;
}

static void rotate_matrix(uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE]) {
	uint8_t i,j,n = MATRIX_SIZE;
	uint16_t tmp;

	for (i = 0; i < (n/2); i++) {
		for (j = i; j < (n-i-1); j++) {
			tmp = matrix[i][j];
			matrix[i][j] = matrix[j][n-i-1];
			matrix[j][n-i-1] = matrix[n-i-1][n-j-1];
			matrix[n-i-1][n-j-1] = matrix[n-j-1][i];
			matrix[n-j-1][i] = tmp;
		}
	}
}

static bool move_up(uint16_t * score, uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE])
{
	bool success = false;
	uint8_t x;

	for (x = 0; x < MATRIX_SIZE; x++) {
		success |= slide_array(score, matrix[x]);
	}
	return success;
}

static bool move_left(uint16_t * score, uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE])
{
	bool success;

	rotate_matrix(matrix);

    success = move_up(score, matrix);
	rotate_matrix(matrix);
	rotate_matrix(matrix);
	rotate_matrix(matrix);

    return success;
}

static bool move_down(uint16_t * score, uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE])
{
	bool success;

	rotate_matrix(matrix);
	rotate_matrix(matrix);

    success = move_up(score, matrix);
	rotate_matrix(matrix);
	rotate_matrix(matrix);

    return success;
}

static bool move_right(uint16_t * score, uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE])
{
	bool success;

	rotate_matrix(matrix);
	rotate_matrix(matrix);
	rotate_matrix(matrix);

    success = move_up(score, matrix);
	rotate_matrix(matrix);

    return success;
}

static bool find_pair_down(uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE]) {
	bool success = false;
	uint8_t x, y;

	for (x = 0; x < MATRIX_SIZE; x++) {
		for (y = 0; y < (MATRIX_SIZE-1); y++) {
			if (matrix[x][y] == matrix[x][y+1])
                return true;
		}
	}
	return success;
}

static uint8_t count_empty(uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE]) {
	uint8_t x, y;
	uint8_t count = 0;

	for (x = 0; x < MATRIX_SIZE; x++) {
		for (y = 0; y < MATRIX_SIZE; y++) {
			if (matrix[x][y] == 0) {
				count++;
			}
		}
	}
	return count;
}

static lv_color_t get_num_color(uint16_t num)
{
    lv_color_t color;

    switch (num)
    {
        case 0:		color = LV_100ASK_2048_NUMBER_EMPTY_COLOR;  break;
        case 1:		color = LV_100ASK_2048_NUMBER_EMPTY_COLOR;  break;
        case 2:		color = LV_100ASK_2048_NUMBER_2_COLOR;	    break;
        case 4:		color = LV_100ASK_2048_NUMBER_4_COLOR;	    break;
        case 8:		color = LV_100ASK_2048_NUMBER_8_COLOR;	    break;
        case 16:	color = LV_100ASK_2048_NUMBER_16_COLOR;	    break;
        case 32:	color = LV_100ASK_2048_NUMBER_32_COLOR;	    break;
        case 64:	color = LV_100ASK_2048_NUMBER_64_COLOR;	    break;
        case 128:	color = LV_100ASK_2048_NUMBER_128_COLOR;	break;
        case 256:	color = LV_100ASK_2048_NUMBER_256_COLOR;	break;
        case 512:	color = LV_100ASK_2048_NUMBER_512_COLOR;	break;
        case 1024:	color = LV_100ASK_2048_NUMBER_1024_COLOR;   break;
        case 2048:	color = LV_100ASK_2048_NUMBER_2048_COLOR;   break;
        default:	color =  LV_100ASK_2048_NUMBER_2048_COLOR;break;
    }
    return color;
}

static bool game_over(uint16_t matrix[MATRIX_SIZE][MATRIX_SIZE])
{
	bool ended = true;

    if (count_empty(matrix) > 0) return false;
	if (find_pair_down(matrix)) return false;

	rotate_matrix(matrix);
	if (find_pair_down(matrix)) ended = false;

    rotate_matrix(matrix);
	rotate_matrix(matrix);
	rotate_matrix(matrix);

    return ended;
}

/*=====================
 * Other functions
 *====================*/

static void game_2048_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj_2048 = lv_event_get_target(e);
    lv_obj_t * label = lv_event_get_user_data(e);

    if(code == LV_EVENT_VALUE_CHANGED) {
        if (lv_100ask_2048_get_best_tile(obj_2048) >= 2048)
            //lv_label_set_text(label, "#00b329 YOU WIN! #");
            lv_label_set_text(label, "YOU WIN!");
        else if(lv_100ask_2048_get_status(obj_2048))
            //lv_label_set_text(label, "#00b329 www.100ask.net: # #ff0000 GAME OVER! #");
            lv_label_set_text(label, "GAME OVER!");
        else
            //lv_label_set_text_fmt(label, "SCORE: #ff00ff %d #", lv_100ask_2048_get_score(obj_2048));
            lv_label_set_text_fmt(label, "SCORE: %d", lv_100ask_2048_get_score(obj_2048));
    }

}

static void new_game_btn_event_handler(lv_event_t * e)
{
    lv_obj_t * obj_2048 = lv_event_get_user_data(e);

    lv_100ask_2048_set_new_game(obj_2048);
}

static void exit_game2048_cb(lv_event_t *e)
{
    lv_obj_del(screen_2048);
}

void lv_2048_start(void)
{
    screen_2048 = lv_tileview_create(lv_scr_act());

    lv_obj_set_scroll_dir(screen_2048, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(screen_2048, LV_SCROLLBAR_MODE_OFF);

    /*Create 2048 game*/
    lv_obj_t * obj_2048 = lv_100ask_2048_create(screen_2048);
#if LV_FONT_MONTSERRAT_40
    lv_obj_set_style_text_font(obj_2048, &lv_font_montserrat_40, 0);
#endif
    lv_obj_set_size(obj_2048, 300, 300);
    lv_obj_center(obj_2048);

    /*Information*/
    lv_obj_t * label = lv_label_create(screen_2048);
    lv_label_set_text_fmt(label, "SCORE: %d", lv_100ask_2048_get_score(obj_2048));
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_add_event_cb(obj_2048, game_2048_event_cb, LV_EVENT_ALL, label);

    /*New Game*/
    lv_obj_t * btn = lv_btn_create(screen_2048);
    lv_obj_align_to(btn, obj_2048, LV_ALIGN_OUT_TOP_RIGHT, -30, -45);
    lv_obj_add_event_cb(btn, new_game_btn_event_handler, LV_EVENT_CLICKED, obj_2048);

    label = lv_label_create(btn);
    lv_label_set_text(label, "New");
    lv_obj_center(label);

    /*Exit Game*/
    lv_obj_t * exit_btn = lv_btn_create(screen_2048);
    lv_obj_align_to(exit_btn, obj_2048, LV_ALIGN_OUT_TOP_LEFT, 10, -45);
    lv_obj_add_event_cb(exit_btn, exit_game2048_cb, LV_EVENT_CLICKED, obj_2048);

    label = lv_label_create(exit_btn);
    lv_label_set_text(label, "Exit");
    lv_obj_center(label);
}


#endif  /*LV_USE_GAME_2048*/
