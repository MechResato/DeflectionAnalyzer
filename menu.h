/*
 * menu.h
 *
 *  Created on: 25 Feb 2021
 *      Author: Admin
 */

#ifndef MENU_H_
#define MENU_H_


/////////// COLOR SCHEME
#define MAIN_BGCOLOR       0x00F5F1EE // background
#define MAIN_TEXTCOLOR     0x00F3CD29 // main text
#define MAIN_BANNERCOLOR   0x0064584B // banner
#define MAIN_DIVIDERCOLOR  0x00F3CD29 // divider
#define MAIN_BTNCOLOR  	   0x00EAA92B // button
#define MAIN_BTNCTSCOLOR   0x00AD9666 // button contrast
#define MAIN_BTNTXTCOLOR   0x00FFF8EE // button text
#define MAIN_BTNGRDCOLOR   0x008C4B12 // 3D gradient color
#define GRAPH_AXISCOLOR    0x003B1E0B // graph axis
#define GRAPH_GRIDCOLOR    0x00C5DAE2 // graph grid
#define GRAPH_DATA1COLOR   0x00003F9E // graph data 1
#define GRAPH_DATA2COLOR   0x0064584B // graph data 2 (alternative/additional color)
#define GRAPH_POSMARKCOLOR 0x00ff0000 // graph position marker

/* simple pre-defined colors */
#define RED		0xff0000UL
#define ORANGE	0xffa500UL
#define GREEN	0x00ff00UL
#define BLUE	0x0000ffUL
#define BLUE_1	0x5dade2L
#define YELLOW	0xffff00UL
#define PINK	0xff00ffUL
#define PURPLE	0x800080UL
#define WHITE	0xffffffUL
#define BLACK	0x000000UL

void TFT_display_menu0(void);
void TFT_display_menu1(void);

void TFT_touch_menu0(uint8_t, uint8_t, uint8_t*, int32_t*, int32_t*);
void TFT_touch_menu1(uint8_t, uint8_t, uint8_t*, int32_t*, int32_t*);

void TFT_display_static_menu0(void);
void TFT_display_static_menu1(void);


#endif /* MENU_H_ */
