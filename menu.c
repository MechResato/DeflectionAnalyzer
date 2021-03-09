/*
 * menu.c
 *
 *  Created on: 25 Feb 2021
 *      Author: Admin
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <globals.h>
#include "FT800-FT813-5.x/EVE.h"
#include "FT800-FT813-5.x/tft.h"
#include "record.h"
#include "menu.h"

/////////// Menu function pointers - At the end of the menu control functions (TFT_display_static, TFT_display and TFT_touch inside tft.h) the function referenced to this pointer is executed
// Every new menu needs to be defined in menu.h, declared at the end of this file and registered here in this function pointer array!
// TFT_MENU_SIZE 	   is declared in menu.h and must be changed if menus are added or removed
// TFT_MAIN_MENU_SIZE  is declared in menu.h. It states to where the main menus (accessible via swipe an background) are listed. All higher menus are considered submenus (control on how to get there is on menu.c)
int8_t TFT_cur_Menu = 0; // Index of currently used menu (TFT_display,TFT_touch). Externally declared in menu.c because this is the index used for above function pointers and submenus can to be used by menu.c too.
void (*TFT_display_cur_Menu__fptr_arr[TFT_MENU_SIZE])(void) = {
		&TFT_display_menu0,
		&TFT_display_menu1
};

void (*TFT_touch_cur_Menu__fptr_arr[TFT_MENU_SIZE])(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y) = {
		&TFT_touch_menu0,
		&TFT_touch_menu1
};

void (*TFT_display_static_cur_Menu__fptr_arr[TFT_MENU_SIZE])(void) = {
		&TFT_display_static_menu0,
		&TFT_display_static_menu1
};




/////////// Banner line strip edge positions (from left to right: Y1 is held horizontal till X1, increasing till X2/Y2 and finally held horizontal at Y2 till EVE_HSIZE)
#define LAYOUT_Y1 66
#define LAYOUT_Y2 50
#define LAYOUT_X1 280
#define LAYOUT_X2 320





/////////// Graph Definitions
// Graph position and size. Here -> quick an dirty estimation where x, y, width and height must be to fill the whole main area
#define G_PADDING 10 									// Only needed because we want to calc how much width and height can be used to "fill" the whole main area. The actual passing is set inside TFT_GraphStatic() hard to 10.
uint16_t G_x        = 10;													 // 10 px from left to leave some room
uint16_t G_y      	= (LAYOUT_Y1 + 15);										 // end of banner plus 10 to leave some room  (for Y1=66: 66+15=81)
uint16_t G_width 	= (0 + EVE_HSIZE - 10 - (2*G_PADDING) - 10);			   // actual width of the data area, therefore x and the paddings left and right must me accommodated to "fill" the whole main area. Additional 10 px from right to leave some room (for 480x272: 480-10-20-10=440)
uint16_t G_height	= (0 + EVE_VSIZE - (LAYOUT_Y1 + 15) - (2*G_PADDING) - 10); // actual height of the data area, therefore y and the paddings top and bottom must me accommodated to "fill" the whole main area. Additional 10 px from bottom to leave some room (for 480x272: 272-66+15-20-10=161)
// axes
const char unit_Sensor[] = " V"; // unit string used at print of current sensor value
double G_amp_max = 10.0; // volts - used at print of vertical grid value labels
double G_t_max = 2.2;    // seconds - used at print of horizontal grid value labels
// data properties
double G_y_max = 4095.0; // maximum allowed amplitude y (here for 12bit sensor value)
// grid
double G_h_grid_lines = 4.0; // number of grey horizontal grid lines
double G_v_grid_lines = 2.2; // number of grey vertical grid lines
/////////// Graph Definitions END


/////////// Button states
uint16_t toggle_state_graphmode = 0;
uint16_t toggle_state_dimmer = 0;


/////////// Debug
uint16_t display_list_size = 0; // Current size of the display-list from register. Used by the TFT_display() menu specific functions
uint32_t tracker = 0; // Value of tracker register (1.byte=tag, 2.byte=value). Used by the TFT_display() menu specific functions


/////////// Strings
#define STR_FILENAME_MAXLEN 20
char str_filename[STR_FILENAME_MAXLEN] = "test.csv";
int8_t str_filename_curLength = 8;




void TFT_display_get_values(void){
	// Get size of last display list to be printed on screen (section "Debug Values")
	display_list_size = EVE_memRead16(REG_CMD_DL);
	tracker = EVE_memRead32(REG_TRACKER);
}

void TFT_display_static_menu0(void){
	/// Draw Banner and divider line on top
	// Banner
	EVE_cmd_dl(TAG(1)); /* give everything considered background area tag 1 -> used for wipe feature*/
	EVE_cmd_dl(LINE_WIDTH(1*16)); /* size is in 1/16 pixel */
	EVE_cmd_dl(DL_COLOR_RGB | MAIN_BANNERCOLOR);
	EVE_cmd_dl(DL_BEGIN | EVE_EDGE_STRIP_A);
	EVE_cmd_dl(VERTEX2F(0, LAYOUT_Y1));
	EVE_cmd_dl(VERTEX2F(LAYOUT_X1, LAYOUT_Y1));
	EVE_cmd_dl(VERTEX2F(LAYOUT_X2, LAYOUT_Y2));
	EVE_cmd_dl(VERTEX2F(EVE_HSIZE, LAYOUT_Y2));
	EVE_cmd_dl(DL_END);
	// Divider
	EVE_cmd_dl(DL_COLOR_RGB | MAIN_DIVIDERCOLOR);
	EVE_cmd_dl(DL_BEGIN | EVE_LINE_STRIP);
	EVE_cmd_dl(VERTEX2F(0, LAYOUT_Y1));
	EVE_cmd_dl(VERTEX2F(LAYOUT_X1, LAYOUT_Y1));
	EVE_cmd_dl(VERTEX2F(LAYOUT_X2, LAYOUT_Y2));
	EVE_cmd_dl(VERTEX2F(EVE_HSIZE, LAYOUT_Y2));
	EVE_cmd_dl(DL_END);

	// Add the static text
	EVE_cmd_dl(TAG(0)); /* do not use the following objects for touch-detection */
	EVE_cmd_dl(DL_COLOR_RGB | MAIN_TEXTCOLOR);
	#if defined (EVE_DMA)
		EVE_cmd_text(10, EVE_VSIZE - 65, 26, 0, "Bytes: ");
	#endif
	EVE_cmd_text(360, 10, 26, 0, "DL-size:");
	EVE_cmd_text(360, 25, 26, 0, "Sensor:");

	/// Write the static part of the Graph to the display list
	TFT_GraphStatic(0, G_x, G_y, G_width, G_height, G_PADDING, G_amp_max, G_t_max, G_h_grid_lines, G_v_grid_lines);


}
void TFT_display_static_menu1(void){
	/// Draw Banner and divider line on top
	// Banner
	EVE_cmd_dl(TAG(1)); /* give everything considered background area tag 1 -> used for wipe feature*/
	EVE_cmd_dl(LINE_WIDTH(1*16)); /* size is in 1/16 pixel */
	EVE_cmd_dl(DL_COLOR_RGB | MAIN_BANNERCOLOR);
	EVE_cmd_dl(DL_BEGIN | EVE_EDGE_STRIP_A);
	EVE_cmd_dl(VERTEX2F(0, LAYOUT_Y1));
	EVE_cmd_dl(VERTEX2F(LAYOUT_X1, LAYOUT_Y1));
	EVE_cmd_dl(VERTEX2F(LAYOUT_X2, LAYOUT_Y2));
	EVE_cmd_dl(VERTEX2F(EVE_HSIZE, LAYOUT_Y2));
	EVE_cmd_dl(DL_END);
	// Divider
	EVE_cmd_dl(DL_COLOR_RGB | MAIN_DIVIDERCOLOR);
	EVE_cmd_dl(DL_BEGIN | EVE_LINE_STRIP);
	EVE_cmd_dl(VERTEX2F(0, LAYOUT_Y1));
	EVE_cmd_dl(VERTEX2F(LAYOUT_X1, LAYOUT_Y1));
	EVE_cmd_dl(VERTEX2F(LAYOUT_X2, LAYOUT_Y2));
	EVE_cmd_dl(VERTEX2F(EVE_HSIZE, LAYOUT_Y2));
	EVE_cmd_dl(DL_END);

	// Add the static text
	EVE_cmd_dl(TAG(0)); /* do not use the following objects for touch-detection */
	EVE_cmd_dl(DL_COLOR_RGB | MAIN_TEXTCOLOR);
	EVE_cmd_text(360, 10, 26, 0, "X:");
	EVE_cmd_text(360, 25, 26, 0, "Y:");

	// Textbox Filename
	TFT_textbox_static(0, 20, 70, 190, 20);
}

void TFT_display_menu0(void)
{
	/// The inputs are used to draw the Graph data. Note that also some predefined graph settings are used direct (#define G_... )

	/////////////// Display BUTTONS and Toggles
	EVE_cmd_gradcolor_burst(MAIN_BTNGRDCOLOR);
	EVE_cmd_dl_burst(DL_COLOR_RGB | MAIN_BTNTXTCOLOR);
	EVE_cmd_fgcolor_burst(MAIN_BTNCOLOR);
	EVE_cmd_bgcolor_burst(MAIN_BTNCTSCOLOR);

	EVE_cmd_dl_burst(TAG(13)); /* assign tag-value '13' to the button that follows */
	if(InputType == 0){ 		EVE_cmd_button_burst(20,15,80,30, 27, 0,"Sensor");	}
	else if(InputType == 1){	EVE_cmd_button_burst(20,15,80,30, 27, 0,"Imp");	}
	else if(InputType == 2){	EVE_cmd_button_burst(20,15,80,30, 27, 0,"Saw");	}
	else{						EVE_cmd_button_burst(20,15,80,30, 27, 0,"Sine");	}

	EVE_cmd_dl_burst(TAG(12)); /* assign tag-value '12' to the toggle that follows */
	if(toggle_state_graphmode){
		EVE_cmd_toggle_burst(120,24,62, 27, 0, 0xFFFF, "Roll");
	}
	else{
		EVE_cmd_toggle_burst(120,24,62, 27, 0, 0x0000, "Frame");
	}

	EVE_cmd_dl_burst(TAG(10)); /* assign tag-value '10' to the button that follows */
	EVE_cmd_button_burst(205,15,80,30, 27, toggle_state_dimmer,"Dimmer");

	EVE_cmd_dl_burst(TAG(0)); /* no touch from here on */



	/////////////// Debug Values
	#if defined (EVE_DMA)
	EVE_cmd_number_burst(120, EVE_VSIZE - 65, 26, EVE_OPT_RIGHTX, cmd_fifo_size); /* number of bytes written to the cmd-fifo */
	#endif
	EVE_cmd_number_burst(470, 10, 26, EVE_OPT_RIGHTX, display_list_size); /* number of bytes written to the display-list by the command co-pro */

	// Write current sensor value with unit
	char buffer[32]; // buffer for double to string conversion
	sprintf(buffer, "%.2lf", (G_amp_max/G_y_max)*InputBuffer1[InputBuffer1_idx]); // double to string conversion
	strcat(buffer, unit_Sensor);
	EVE_cmd_text_burst(470, 25, 26, EVE_OPT_RIGHTX, buffer);



	/////////////// GRAPH
	///// Print dynamic part of the Graph (data & marker)
	TFT_GraphData(G_x, G_y, G_width, G_height, G_PADDING, G_y_max, &InputBuffer1[0], INPUTBUFFER1_SIZE, &InputBuffer1_idx, toggle_state_graphmode, GRAPH_DATA1COLOR, GRAPH_POSMARKCOLOR);

}
void TFT_display_menu1(void){
	/// Test menu

	/////////////// Display BUTTONS and Toggles
	EVE_cmd_gradcolor_burst(MAIN_BTNGRDCOLOR);
	EVE_cmd_dl_burst(DL_COLOR_RGB | MAIN_BTNTXTCOLOR);
	EVE_cmd_fgcolor_burst(MAIN_BTNCOLOR);
	EVE_cmd_bgcolor_burst(MAIN_BTNCTSCOLOR);

	EVE_cmd_dl_burst(TAG(12)); /* assign tag-value '12' to the toggle that follows */
	if(toggle_state_graphmode){
		EVE_cmd_toggle_burst(120,24,62, 27, 0, 0xFFFF, "li");
	}
	else{
		EVE_cmd_toggle_burst(120,24,62, 27, 0, 0x0000, "re");
	}

	EVE_cmd_dl_burst(TAG(0)); /* no touch from here on */

	EVE_cmd_fgcolor_burst(MAIN_TEXTCOLOR);
	//EVE_cmd_int_burst(470, 10, 26, EVE_OPT_RIGHTX, swipeDistance_X);
	//EVE_cmd_int_burst(470, 25, 26, EVE_OPT_RIGHTX, swipeDistance_Y);
	EVE_cmd_number_burst(470, 10, 26, EVE_OPT_RIGHTX | EVE_OPT_SIGNED, swipeDistance_X);
	EVE_cmd_number_burst(470, 25, 26, EVE_OPT_RIGHTX | EVE_OPT_SIGNED, swipeDistance_Y);
	//EVE_cmd_text_var_burst(470, 25, 26, EVE_OPT_RIGHTX, "%d", swipeDistance_Y);

	//str_filename[1] = 'a';
	TFT_textbox_display(20, 70, 20, str_filename);

//	char c [] = "0123456789";
//
//	c[0] = (char)120;
//	c[1] = (char)121;
//	c[2] = (char)122;
//	c[3] = (char)123;
//	c[4] = (char)124;
//	c[5] = (char)125;
//	c[6] = (char)126;
//	c[7] = (char)127;
//	c[8] = (char)128;
//	c[9] = (char)129;
//	EVE_cmd_keys_burst(2, EVE_VSIZE-2-21-(24*6), EVE_HSIZE-4, 21, 17, 0, &c);

}
//void TFT_display_menu_setup(void)
//{
//	///
//
//	/////////////// Display BUTTONS and Toggles
//	EVE_cmd_gradcolor_burst(MAIN_BTNGRDCOLOR);
//	EVE_cmd_dl_burst(DL_COLOR_RGB | MAIN_BTNTXTCOLOR);
//	EVE_cmd_fgcolor_burst(MAIN_BTNCOLOR);
//	EVE_cmd_bgcolor_burst(MAIN_BTNCTSCOLOR);
//
//	EVE_cmd_dl_burst(TAG(10)); /* assign tag-value '10' to the button that follows */
//	EVE_cmd_button_burst(205,15,80,30, 27, toggle_state_dimmer,"Dimmer");
//
//	EVE_cmd_dl_burst(TAG(0)); /* no touch from here on */
//
//}


void TFT_touch_menu0(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y){
	/// ...
	/// Do not use tags higher than 32 (they will be interpreted as keyboard input) or predefined TAGs -> see tft.c "TAG ASSIGNMENT"!

	// Determine which tag was touched
	switch(tag)
	{
		// dimmer button on top as on/off radio-switch
		case 10:
			if(*toggle_lock == 0) {
				printf("Button Dimmer touched\n");
				*toggle_lock = 42;
				if(toggle_state_dimmer == 0){
					toggle_state_dimmer = EVE_OPT_FLAT;
					EVE_memWrite8(REG_PWM_DUTY, 0x01);	/* setup backlight, range is from 0 = off to 0x80 = max */
				}
				else {
					toggle_state_dimmer = 0;
					EVE_memWrite8(REG_PWM_DUTY, 0x80);	/* setup backlight, range is from 0 = off to 0x80 = max */
				}
			}
			break;
		// roll/frame mode toggle on top
		case 12:
			if(*toggle_lock == 0) {
				printf("Toggle Roll touched\n");
				*toggle_lock = 42;
				if(toggle_state_graphmode == 0)	{
					toggle_state_graphmode = 1;
				}
				else {
					toggle_state_graphmode = 0;
				}
			}
			break;
		// signal switcher button
		case 13:
			if(*toggle_lock == 0) {
				printf("Switch Signal\n");
				*toggle_lock = 42;
				InputType++;
				if(InputType > 3){ InputType = 0; }
			}
			break;
	}
}
void TFT_touch_menu1(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y){
	/// ...
	/// Do not use tags higher than 32 (they will be interpreted as keyboard input) or predefined TAGs -> see tft.c "TAG ASSIGNMENT"!


	// Determine which tag was touched
	switch(tag)
	{
		// li/re mode toggle on top
		case 12:
			if(*toggle_lock == 0) {
				printf("Toggle li/re touched\n");
				*toggle_lock = 42;
				if(toggle_state_graphmode == 0)	{
					toggle_state_graphmode = 1;
				}
				else {
					toggle_state_graphmode = 0;
				}
			}
			break;

		// textbox
		case 20:
			if(*toggle_lock == 0) {
				printf("Textbox touched\n");
				*toggle_lock = 42;

				// Activate Keypad and set cursor to end
				keypad_open(20, Filename);
				TFT_textbox_setCursor(str_filename_curLength-4, str_filename_curLength);
			}
			break;
		default:
			TFT_textbox_touch(20, str_filename, STR_FILENAME_MAXLEN, &str_filename_curLength);
			break;
	}

	//// If the user swiped more on x-axis he probably wants to swipe left/right
	//if(abs(swipeDistance_X) > abs(swipeDistance_Y)){
	//	if(swipeDistance_X > 50)      	// swipe to left
	//		swipeDetect = Left;
	//	else if(swipeDistance_X < -50)	// swipe to right
	//		swipeDetect = Right;
	//	else
	//		swipeDetect = None;
	//}
	//// If the user swiped more on y-axis he probably wants to swipe up/down
	//else{
	//	if(swipeDistance_Y > 50)		// swipe down
	//		swipeDetect = Down;
	//	else if(swipeDistance_Y < -50)	// swipe up
	//		swipeDetect = Up;
	//	else
	//		swipeDetect = None;
	//}

}
