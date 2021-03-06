/*
 * menu.h
 *
 *  Created on: 25 Feb 2021
 *      Author: RS
 */

#include <tft.h>

#ifndef MENU_H_
#define MENU_H_


/////////// COLOR SCHEME
#define MAIN_BGCOLOR       0xfbfafe // background
#define MAIN_TEXTCOLOR     0xF3CD29 // main text
#define MAIN_BANNERCOLOR   0x54483B // banner 0x64584B
#define MAIN_DIVIDERCOLOR  0xF58800 // divider 0xF3CD29
#define MAIN_BTNCOLOR  	   0xF58800 // button 0xEAA92B
#define MAIN_BTNCTSCOLOR   0x9A8656 // button contrast
#define MAIN_BTNTXTCOLOR   0xFFF8F1 // button text 0xFFF8EE
#define MAIN_BTNGRDCOLOR   0xF8BC24 // 3D gradient color 0x8C4B12
#define GRAPH_AXISCOLOR   		0x3B1E0B // graph axis
#define GRAPH_GRIDCOLOR			0xC5DAE2 // graph grid
#define GRAPH_DATA1COLOR   		0x003F9E // graph data 1
#define GRAPH_DATA2COLOR   		0x64584B // graph data 2 (alternative/additional color)
#define GRAPH_DATA2COLORLIGHT   0xe7cbad // graph data 2 light (alternative/additional color)


// TFT_MENU_SIZE 	   Amount of overall menus. Must be changed if menus are added or removed
#define TFT_MENU_SIZE 6
// TFT_MAIN_MENU_SIZE  States to where the main menus (accessible via swipe an background) are listed. All higher menus are considered sub-menus (control on how to get there is on menu.c)
#define TFT_MAIN_MENU_SIZE 4
void (*TFT_display_static_cur_Menu__fptr_arr[TFT_MENU_SIZE])(void);
void (*TFT_display_cur_Menu__fptr_arr[TFT_MENU_SIZE])(void);
void (*TFT_touch_cur_Menu__fptr_arr[TFT_MENU_SIZE])(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y);


void TFT_display_get_values(void);
void TFT_recordScreenshot(void);

void menu_display_static_0monitor(void);
void menu_display_static_1dash(void);
void menu_display_static_2setup1(void);
void menu_display_static_3setup2(void);
void menu_display_static_curveset(void);
void menu_display_static_filterset(void);

//void menuMonitor_setInput_(uint8_t);
void menu_display_0monitor(void);
void menu_display_1dash(void);
void menu_display_2setup1(void);
void menu_display_3setup2(void);
void menu_display_curveset(void);
void menu_display_filterset(void);

void menu_touch_0monitor(uint8_t, uint8_t*, uint8_t, uint8_t*, int32_t*, int32_t*);
void menu_touch_1dash(uint8_t, uint8_t*, uint8_t, uint8_t*, int32_t*, int32_t*);
void menu_touch_2setup1(uint8_t, uint8_t*, uint8_t, uint8_t*, int32_t*, int32_t*);
void menu_touch_3setup2(uint8_t, uint8_t*, uint8_t, uint8_t*, int32_t*, int32_t*);
void menu_touch_curveset(uint8_t, uint8_t*, uint8_t, uint8_t*, int32_t*, int32_t*);
void menu_touch_filterset(uint8_t, uint8_t*, uint8_t, uint8_t*, int32_t*, int32_t*);



extern menu* menu_objects[TFT_MENU_SIZE];


#endif /* MENU_H_ */
