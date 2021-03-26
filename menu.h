/*
 * menu.h
 *
 *  Created on: 25 Feb 2021
 *      Author: Admin
 */

#include <tft.h>

#ifndef MENU_H_
#define MENU_H_


/////////// COLOR SCHEME
#define MAIN_BGCOLOR       0xfdf9f6 // background
#define MAIN_TEXTCOLOR     0xF3CD29 // main text
#define MAIN_BANNERCOLOR   0x64584B // banner
#define MAIN_DIVIDERCOLOR  0xF3CD29 // divider
#define MAIN_BTNCOLOR  	   0xEAA92B // button
#define MAIN_BTNCTSCOLOR   0xAD9666 // button contrast
#define MAIN_BTNTXTCOLOR   0xFFF8EE // button text
#define MAIN_BTNGRDCOLOR   0x8C4B12 // 3D gradient color
#define GRAPH_AXISCOLOR   		0x3B1E0B // graph axis
#define GRAPH_GRIDCOLOR			0xC5DAE2 // graph grid
#define GRAPH_DATA1COLOR   		0x003F9E // graph data 1
#define GRAPH_DATA2COLOR   		0x64584B // graph data 2 (alternative/additional color)
#define GRAPH_DATA2COLORLIGHT   0xe7cbad // graph data 2 light (alternative/additional color)

void TFT_display_get_values(void);

void TFT_display_static_menu0(void);
void TFT_display_static_menu1(void);
void TFT_display_static_menu_setup(void);
void TFT_display_static_menu_curveset(void);

void TFT_display_menu0(void);
void TFT_display_menu1(void);
void TFT_display_menu_setup(void);
void TFT_display_menu_curveset(void);

void TFT_touch_menu0(uint8_t, uint8_t*, uint8_t, uint8_t*, int32_t*, int32_t*);
void TFT_touch_menu1(uint8_t, uint8_t*, uint8_t, uint8_t*, int32_t*, int32_t*);
void TFT_touch_menu_setup(uint8_t, uint8_t*, uint8_t, uint8_t*, int32_t*, int32_t*);
void TFT_touch_menu_curveset(uint8_t, uint8_t*, uint8_t, uint8_t*, int32_t*, int32_t*);


// TFT_MENU_SIZE 	   Amount of overall menus. Must be changed if menus are added or removed
#define TFT_MENU_SIZE 4
// TFT_MAIN_MENU_SIZE  States to where the main menus (accessible via swipe an background) are listed. All higher menus are considered sub-menus (control on how to get there is on menu.c)
#define TFT_MAIN_MENU_SIZE 3
void (*TFT_display_static_cur_Menu__fptr_arr[TFT_MENU_SIZE])(void);
void (*TFT_display_cur_Menu__fptr_arr[TFT_MENU_SIZE])(void);
void (*TFT_touch_cur_Menu__fptr_arr[TFT_MENU_SIZE])(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y);

extern menu* Menu_Objects[TFT_MENU_SIZE];


#endif /* MENU_H_ */
