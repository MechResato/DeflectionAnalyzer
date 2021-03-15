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
#include <DAVE.h>
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
		&TFT_display_menu1,
		&TFT_display_menu_setup,
		&TFT_display_menu_linset
};

void (*TFT_touch_cur_Menu__fptr_arr[TFT_MENU_SIZE])(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y) = {
		&TFT_touch_menu0,
		&TFT_touch_menu1,
		&TFT_touch_menu_setup,
		&TFT_touch_menu_linset
};

void (*TFT_display_static_cur_Menu__fptr_arr[TFT_MENU_SIZE])(void) = {
		&TFT_display_static_menu0,
		&TFT_display_static_menu1,
		&TFT_display_static_menu_setup,
		&TFT_display_static_menu_linset
};

#define M_0_UPPERBOND 66 // deepest coordinate (greatest number) of the header
menu menu_0 = {
		.index = 0,
		.headerText = "Monitoring",
		.upperBond = M_0_UPPERBOND,
		.headerLayout = {M_0_UPPERBOND, 320, 50, 360}, //[Y1,X1,Y2,X2]
		.bannerColor = MAIN_BANNERCOLOR,
		.dividerColor = MAIN_DIVIDERCOLOR,
		.headerColor = MAIN_TEXTCOLOR,
};

#define M_1_UPPERBOND 66  // deepest coordinate (greatest number) of the header
menu menu_1 = {
		.index = 1,
		.headerText = "Dashboard",
		.upperBond = M_1_UPPERBOND,
		.headerLayout = {M_1_UPPERBOND, 280, 50, 320},//[Y1,X1,Y2,X2]
		.bannerColor = MAIN_BANNERCOLOR,
		.dividerColor = MAIN_DIVIDERCOLOR,
		.headerColor = MAIN_TEXTCOLOR,
};

#define M_SETUP_UPPERBOND 66 // deepest coordinate (greatest number) of the header
menu menu_setup = {
		.index = 2,
		.headerText = "Setup",
		.upperBond = M_SETUP_UPPERBOND,
		.headerLayout = {M_SETUP_UPPERBOND, 240, 50, 280},//[Y1,X1,Y2,X2]
		.bannerColor = MAIN_BANNERCOLOR,
		.dividerColor = MAIN_DIVIDERCOLOR,
		.headerColor = MAIN_TEXTCOLOR,
};

#define M_LINSET_UPPERBOND 30 // deepest coordinate (greatest number) of the header
menu menu_linset = {
		.index = 3,
		.headerText = "",
		.upperBond = M_LINSET_UPPERBOND,
		.headerLayout = {0, EVE_HSIZE-65, 40, EVE_HSIZE-50},//[Y1,X1,Y2,X2]
		.bannerColor = MAIN_BANNERCOLOR,
		.dividerColor = MAIN_DIVIDERCOLOR,
		.headerColor = MAIN_TEXTCOLOR,
};


menu* Menu_Objects[TFT_MENU_SIZE] = {&menu_0, &menu_1, &menu_setup, &menu_linset};










/////////// Menu space and distance conventions
#define M_UPPER_PAD	14	// Common padding from upper border (offset in pixels from the upper header or display edge)
#define M_ROW_DIST 40	// Common distance between rows of content
#define M_COL_1 25		// Start of first Column (is also the padding/indent from the left edge)
#define M_COL_2 140		// Suggestion of an absolute second column coordinate to be used when displaying stuff (no need to use this, but easier to structure)
#define M_COL_3 200		// Suggestion ...
#define M_COL_4 400
#define FONT_COMP 3		// In order to get text of different fonts to the same level an adjustment is need
#ifndef TEXTBOX_PAD_V		// This should be defined in tft.c
#define TEXTBOX_PAD_V 8		// offset of the text from vertical border in pixel
#endif

/////////// Button states
uint16_t toggle_state_graphmode = 0;
uint16_t toggle_state_dimmer = 0;
//TODO: these are implemented in the TFT_control struct but not yet switchid at the touch functions!

/////////// Debug
uint16_t display_list_size = 0; // Current size of the display-list from register. Used by the TFT_display() menu specific functions
uint32_t tracker = 0; // Value of tracker register (1.byte=tag, 2.byte=value). Used by the TFT_display() menu specific functions










/////////// MENU 0 MONITORING

#define BTN_INPUT_TAG 13
control btn_input = {
	.x = 180,		.y = 15,
	.w0 = 70,		.h0 = 30,
	.mytag = BTN_INPUT_TAG,	.font = 27, .options = 0, .state = 0,
	.text = "Sensor",
	.controlType = Button,
	.ignoreScroll = 1
};

#define BTN_GRAPHMODE_TAG 12
control tgl_graphMode = {
	.x = 270,		.y = 24,
	.w0 = 60,		.h0 = 27,
	.mytag = BTN_GRAPHMODE_TAG,	.font = 27, .options = 0, .state = 0,
	.text = "Frame",
	.controlType = Toggle,
	.ignoreScroll = 1
};

label lbl_DLsize = {
		.x = 360,		.y = 10,
		.font = 26,		.options = 0,	.text = "DL-size:",
		.ignoreScroll = 1,
};

label lbl_sensor = {
		.x = 360,		.y = 25,
		.font = 26,		.options = 0,	.text = "Sensor:",
		.ignoreScroll = 1
};


// Graph Definitions
// Graph position and size. Here -> quick an dirty estimation where x, y, width and height must be to fill the whole main area

//uint16_t G_x        = 10;													 // 10 px from left to leave some room
//uint16_t G_y      	= (M_0_UPPERBOND + 15);										 // end of banner plus 10 to leave some room  (for Y1=66: 66+15=81)
//uint16_t G_width 	= (0 + EVE_HSIZE - 10 - (2*G_PADDING) - 10);			   // actual width of the data area, therefore x and the paddings left and right must me accommodated to "fill" the whole main area. Additional 10 px from right to leave some room (for 480x272: 480-10-20-10=440)
//uint16_t G_height	= (0 + EVE_VSIZE - (M_0_UPPERBOND + 15) - (2*G_PADDING) - 10); // actual height of the data area, therefore y and the paddings top and bottom must me accommodated to "fill" the whole main area. Additional 10 px from bottom to leave some room (for 480x272: 272-66+15-20-10=161)
//// axes
//const char unit_Sensor[] = " V"; // unit string used at print of current sensor value
//double G_amp_max = 10.0; // volts - used at print of vertical grid value labels
//double G_t_max = 2.2;    // seconds - used at print of horizontal grid value labels
//// data properties
//double G_y_max = 4095.0; // maximum allowed amplitude y (here for 12bit sensor value)
//// grid
//double G_h_grid_lines = 4.0; // number of grey horizontal grid lines
//double G_v_grid_lines = 2.2; // number of grey vertical grid lines


#define G_PADDING 10 //
graph gph_monitor = {
	.x = 10,																 // 10 px from left to leave some room
	.y = (M_0_UPPERBOND + 15),												// end of banner plus 10 to leave some room  (for Y1=66: 66+15=81)
	.width = (0 + EVE_HSIZE - 10 - (2*G_PADDING) - 10),			   			// actual width of the data area, therefore x and the paddings left and right must me accommodated to "fill" the whole main area. Additional 10 px from right to leave some room (for 480x272: 480-10-20-10=440)
	.height = (0 + EVE_VSIZE - (M_0_UPPERBOND + 15) - (2*G_PADDING) - 10), 	// actual height of the data area, therefore y and the paddings top and bottom must me accommodated to "fill" the whole main area. Additional 10 px from bottom to leave some room (for 480x272: 272-66+15-20-10=161)
	.padding = G_PADDING,
	.x_label = "V",
	.y_label = "t",
	.y_max = 4095.0, 		// maximum allowed amplitude y (here for 12bit sensor value);
	.amp_max = 10.0, 		// volts - used at print of vertical grid value labels
	.t_max = 2.2,    		// seconds - used at print of horizontal grid value labels
	.h_grid_lines = 4.0, 	// number of grey horizontal grid lines
	.v_grid_lines = 2.2, 	// number of grey vertical grid lines
	.graphmode = 0
};
// Graph Definitions END

/////////// MENU 0 MONITORING END ---


/////////// MENU 1 DASHBOARD
#define BTN_STARTREC_TAG 10
control btn_startRec = {
	.x = M_COL_3,	.y = 15,
	.w0 = 55,		.h0 = 31,
	.mytag = BTN_STARTREC_TAG,	.font = 27,	.options = 0, .state = 0,
	.text = "Start",
	.controlType = Button,
	.ignoreScroll = 1
};
/////////// MENU 1 DASHBOARD END ---


/////////// MENU 3 SETUP
label lbl_recording = {
		.x = M_COL_1,		.y = M_UPPER_PAD + M_SETUP_UPPERBOND,
		.font = 27,		.options = 0,		.text = "Recording",
		.ignoreScroll = 0
};

#define STR_FILENAME_MAXLEN 16
char str_filename[STR_FILENAME_MAXLEN] = "test.csv";
int8_t str_filename_curLength = 8;
#define TBX_FILENAME_TAG 20
textbox tbx_filename = {
	.x = M_COL_2,
	.y = M_UPPER_PAD + M_SETUP_UPPERBOND - TEXTBOX_PAD_V + FONT_COMP*1,
	.width = 190,
	.labelOffsetX = 60,
	.labelText = "Filename",
	.mytag = TBX_FILENAME_TAG,
	.text = str_filename,
	.text_maxlen = STR_FILENAME_MAXLEN,
	.text_curlen = &str_filename_curLength,
	.keypadType = Filename,
	.active = 0
};

label lbl_linearisation = {
		.x = M_COL_1,		.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*1),
		.font = 27,		.options = 0,		.text = "Linearisation",
		.ignoreScroll = 0
};

#define STR_S1_LINSPEC_MAXLEN 10
char str_s1_linspec[STR_S1_LINSPEC_MAXLEN] = "s1.lin";
int8_t str_s1_linspec_curLength = 6;
#define TBX_SENSOR1_TAG 21
textbox tbx_sensor1 = {
	.x = M_COL_2,
	.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*1) - TEXTBOX_PAD_V + FONT_COMP*1,
	.width = 120,
	.labelOffsetX = 130,
	.labelText = "Sensor1:   Spec File",
	.mytag = TBX_SENSOR1_TAG,
	.text = str_s1_linspec,
	.text_maxlen = STR_S1_LINSPEC_MAXLEN,
	.text_curlen = &str_s1_linspec_curLength,
	.keypadType = Standard,
	.active = 0
};
#define BTN_LINSENSOR1_TAG 22
control btn_linSensor1 = {
	.x = M_COL_4,	.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*1) - TEXTBOX_PAD_V + FONT_COMP*1,
	.w0 = 55,		.h0 = 31,
	.mytag = BTN_LINSENSOR1_TAG,	.font = 27,	.options = 0, .state = 0,
	.text = "Set",
	.controlType = Button,
	.ignoreScroll = 0
};

#define STR_S2_LINSPEC_MAXLEN 10
char str_s2_linspec[STR_S2_LINSPEC_MAXLEN] = "s2.lin";
int8_t str_s2_linspec_curLength = 6;
#define TBX_SENSOR2_TAG 23
textbox tbx_sensor2 = {
	.x = M_COL_2,
	.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*2) - TEXTBOX_PAD_V + FONT_COMP*1,
	.width = 120,
	.labelOffsetX = 130,
	.labelText = "Sensor2:   Spec File",
	.mytag = TBX_SENSOR2_TAG,
	.text = str_s2_linspec,
	.text_maxlen = STR_S2_LINSPEC_MAXLEN,
	.text_curlen = &str_s2_linspec_curLength,
	.keypadType = Standard,
	.active = 0
};
#define BTN_LINSENSOR2_TAG 24
control btn_linSensor2 = {
	.x = M_COL_4,	.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*2) - TEXTBOX_PAD_V + FONT_COMP*1,
	.w0 = 55,		.h0 = 31,
	.mytag = BTN_LINSENSOR2_TAG,	.font = 27,	.options = 0, .state = 0,
	.text = "Set",
	.controlType = Button,
	.ignoreScroll = 0
};

label lbl_backlight = {
		.x = M_COL_1,		.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*3),
		.font = 27,		.options = 0,		.text = "Backlight",
		.ignoreScroll = 0
};

#define BTN_DIMMER_TAG 10
control btn_dimmmer = {
	.x = M_COL_2,	.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*3) - TEXTBOX_PAD_V + FONT_COMP*1,
	.w0 = 80,		.h0 = 30,
	.mytag = BTN_DIMMER_TAG,	.font = 27, .options = 0, .state = 0,
	.text = "Dimmer",
	.controlType = Button,
	.ignoreScroll = 0
};

/////////// MENU 3 SETUP END ---

/////////// MENU LINSET
#define BTN_BACK_TAG 10
control btn_back = {
	.x = EVE_HSIZE-45,	.y = 5,
	.w0 = 40,			.h0 = 30,
	.mytag = BTN_BACK_TAG,	.font = 27, .options = 0, .state = 0,
	.text = "Back",
	.controlType = Button,
	.ignoreScroll = 1
};

uint16_t buf_linset_idx = 0; // Current index in Buffer
uint16_t buf_linset_size = 3; // TODO - see TODO below
INPUT_BUFFER_SIZE_t buf_linset[3] = { 0, 100, 200}; // all elements 0. TODO - change this to a pointer and let array be created dynamically on heap
#define G_PADDING 10 //
graph gph_linset = {
	.x = 10,		// 10 px from left to leave some room
	.y = M_LINSET_UPPERBOND + M_UPPER_PAD,		// end of banner plus 10 to leave some room  (e.g. for Y1=66: 66+15=81)
	.width  = (0 + EVE_HSIZE - 10 - (2*G_PADDING) - 10),		// actual width of the data area, therefore x and the paddings left and right must me accommodated to "fill" the whole main area. Additional 10 px from right to leave some room (for 480x272: 480-10-20-10=440)
	.height = (0 + EVE_VSIZE - 15 - (2*G_PADDING) - 10 - (M_ROW_DIST*2)), 	// actual height of the data area, therefore y and the paddings top and bottom must me accommodated to "fill" the whole main area. Additional 10 px from bottom to leave some room (for 480x272: 272-66+15-20-10=161)
	.padding = G_PADDING,
	.x_label = "nominal",
	.y_label = "actual in mm",
	.y_max = 200.0, 		// maximum allowed amplitude y (here for 12bit sensor value);
	.amp_max = 200.0, 		// in given unit - used at print of vertical grid value labels
	.t_max = 4096,    		// seconds - used at print of horizontal grid value labels
	.h_grid_lines = 4.0, 	// number of grey horizontal grid lines
	.v_grid_lines = 2.0, 	// number of grey vertical grid lines
	.graphmode = 1
};

#define STR_DP_MAXLEN 3
char str_dp[STR_S2_LINSPEC_MAXLEN] = "0";
int8_t str_dp_curLength = 1;
#define TBX_DP_TAG 24
textbox tbx_dp = {
	.x = M_COL_1,
	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.width = 36,
	.labelOffsetX = 75,
	.labelText = "Data Point:",
	.mytag = TBX_DP_TAG,
	.text = str_dp,
	.text_maxlen = STR_DP_MAXLEN,
	.text_curlen = &str_dp_curLength,
	.keypadType = Numeric,
	.active = 0
};

#define BTN_DP_LAST_TAG 11
control btn_db_last = {
	.x = M_COL_1 + 75 + 36 + 1,	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.w0 = 25				  ,	.h0 = 30,
	.mytag = BTN_DP_LAST_TAG,	.font = 27, .options = 0, .state = 0,
	.text = "<",
	.controlType = Button,
	.ignoreScroll = 0
};
#define BTN_DP_NEXT_TAG 12
control btn_db_next = {
	.x = M_COL_1 + 75 + 36 + 1 + 25 + 1,	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.w0 = 25						   , 	.h0 = 30,
	.mytag = BTN_DP_NEXT_TAG,	.font = 27, .options = 0, .state = 0,
	.text = ">",
	.controlType = Button,
	.ignoreScroll = 0
};
#define STR_NOM_MAXLEN 10
char str_nom[STR_NOM_MAXLEN] = "4095";
int8_t str_nom_curLength = 4;
#define TBX_NOM_TAG 25
textbox tbx_nom = {
	.x = M_COL_1 + 75 + 36 + 1 + 25 + 1 + 30 + 10,
	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.width = 50,
	.labelOffsetX = 60,
	.labelText = "Nominal:",
	.mytag = TBX_NOM_TAG,
	.text = str_nom,
	.text_maxlen = STR_NOM_MAXLEN,
	.text_curlen = &str_nom_curLength,
	.keypadType = Numeric,
	.active = 0
};
#define STR_ACT_MAXLEN 10
char str_act[STR_ACT_MAXLEN] = "";
int8_t str_act_curLength = 0;
#define TBX_ACT_TAG 26
textbox tbx_act = {
	.x = M_COL_1 + 75 + 36 + 1 + 25 + 1 + 30 + 10 + 1 + 120,
	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.width = 60,
	.labelOffsetX = 60,
	.labelText = "Actual:",
	.mytag = TBX_ACT_TAG,
	.text = str_act,
	.text_maxlen = STR_ACT_MAXLEN,
	.text_curlen = &str_act_curLength,
	.keypadType = Numeric,
	.active = 0
};


/////////// MENU LINSET END ---

void TFT_display_get_values(void){
	// Get size of last display list to be printed on screen (section "Debug Values")
	display_list_size = EVE_memRead16(REG_CMD_DL);
	tracker = EVE_memRead32(REG_TRACKER);
}

void TFT_display_static_menu0(void){
	// Set configuration for current menu
	TFT_setMenu(0);

	/// Draw Banner and divider line on top
	TFT_header_static(1, &menu_0);

	// Set Color
	TFT_setColor(1, MAIN_TEXTCOLOR, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, 0);

	// Add the static text
	//EVE_cmd_dl_burst(TAG(0)); /* do not use the following objects for touch-detection */
	//EVE_cmd_dl_burst(DL_COLOR_RGB | MAIN_TEXTCOLOR);
	//EVE_cmd_text_burst(360, 10, 26, 0, "DL-size:");
	//EVE_cmd_text_burst(360, 25, 26, 0, "Sensor:");
	TFT_label(1, &lbl_DLsize);
	TFT_label(1, &lbl_sensor);
	//TFT_label(1, 360, 10, 26, 0, "DL-size:");
	//TFT_label(1, 360, 25, 26, 0, "Sensor:");

	/// Write the static part of the Graph to the display list
	TFT_GraphStatic(1, &gph_monitor);
	//TFT_GraphStatic(1, G_x, G_y, G_width, G_height, G_PADDING, G_amp_max, G_t_max, G_h_grid_lines, G_v_grid_lines);


}
void TFT_display_static_menu1(void){
	// Set configuration for current menu
	TFT_setMenu(1);

	/// Draw Banner and divider line on top
	TFT_header_static(1, &menu_1);

	// Add the static text
	EVE_cmd_dl_burst(TAG(0)); /* do not use the following objects for touch-detection */
	EVE_cmd_dl_burst(DL_COLOR_RGB | MAIN_TEXTCOLOR);
	EVE_cmd_text_burst(360, 10, 26, 0, "X:");
	EVE_cmd_text_burst(360, 25, 26, 0, "Y:");

	// Textbox Filename
	//TFT_textbox_static(0, 20, 70, 190, 20, "test", 50);
}
void TFT_display_static_menu_setup(void){
	// Set configuration for current menu
	TFT_setMenu(2);

	/// Draw Banner and divider line on top
	TFT_header_static(1, &menu_setup);

	// Set Color
	TFT_setColor(1, BLACK, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, 0);

	/// Recording section
	TFT_label(1, &lbl_recording);
	// Filename
	TFT_textbox_static(1, &tbx_filename);

	/// Linearisation section
	TFT_label(1, &lbl_linearisation);
	TFT_textbox_static(1, &tbx_sensor1);
	TFT_textbox_static(1, &tbx_sensor2);

	/// Backlight
	TFT_label(1, &lbl_backlight);
}
void TFT_display_static_menu_linset(void){
	// Set configuration for current menu
	TFT_setMenu(3);

	// Set Color
	TFT_setColor(1, BLACK, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, 0);

	/// Write the static part of the Graph to the display list
	TFT_GraphStatic(1, &gph_linset);

	TFT_textbox_static(1, &tbx_dp);
	TFT_textbox_static(1, &tbx_nom);
	TFT_textbox_static(1, &tbx_act);
}



void TFT_display_menu0(void){
	/// The inputs are used to draw the Graph data. Note that also some predefined graph settings are used direct (#define G_... )

	/////////////// Display BUTTONS and Toggles
	TFT_setColor(1, MAIN_BTNTXTCOLOR, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);
	//TFT_control(&btn_dimmmer, 1);
	//EVE_cmd_gradcolor_burst(MAIN_BTNGRDCOLOR);
	//EVE_cmd_dl_burst(DL_COLOR_RGB | MAIN_BTNTXTCOLOR);
	//EVE_cmd_fgcolor_burst(MAIN_BTNCOLOR);
	//EVE_cmd_bgcolor_burst(MAIN_BTNCTSCOLOR);

	//EVE_cmd_dl_burst(TAG(13)); /* assign tag-value '13' to the button that follows */
	TFT_control(&btn_input);

	//EVE_cmd_dl_burst(TAG(12)); /* assign tag-value '12' to the toggle that follows */
	TFT_control(&tgl_graphMode);
	//if(toggle_state_graphmode){
	//	EVE_cmd_toggle_burst(120,24,62, 27, 0, 0xFFFF, "Roll");
	//}
	//else{
	//	EVE_cmd_toggle_burst(120,24,62, 27, 0, 0x0000, "Frame");
	//}

	//EVE_cmd_dl_burst(TAG(10)); /* assign tag-value '10' to the button that follows */
	//EVE_cmd_button_burst(205,15,80,30, 27, toggle_state_dimmer,"Dimmer");

	//EVE_cmd_dl_burst(TAG(0)); /* no touch from here on */



	/////////////// Debug Values
	EVE_cmd_number_burst(470, 10, 26, EVE_OPT_RIGHTX, display_list_size); /* number of bytes written to the display-list by the command co-pro */

	// Write current sensor value with unit
	char buffer[32]; // buffe.ouble to string conversion
	sprintf(buffer, "%.2lf", (gph_monitor.amp_max/gph_monitor.y_max)*InputBuffer1[InputBuffer1_idx]); // double to string conversion
	strcat(buffer, " V"); //unit_Sensor
	EVE_cmd_text_burst(470, 25, 26, EVE_OPT_RIGHTX, buffer);



	/////////////// GRAPH
	///// Print dynamic part of the Graph (data & marker)
	TFT_GraphData(&gph_monitor, &InputBuffer1[0], INPUTBUFFER1_SIZE, &InputBuffer1_idx, GRAPH_DATA1COLOR);
	//TFT_GraphData(G_x, G_y, G_width, G_height, G_PADDING, G_y_max, &InputBuffer1[0], INPUTBUFFER1_SIZE, &InputBuffer1_idx, tgl_graphMode.state, GRAPH_DATA1COLOR, GRAPH_POSMARKCOLOR);

}
void TFT_display_menu1(void){
	/// Test menu

	/////////////// Display BUTTONS and Toggles
	TFT_setColor(1, MAIN_BTNTXTCOLOR, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);
	// Button start recording
	TFT_control(&btn_startRec);


	TFT_setColor(1, MAIN_TEXTCOLOR, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);
	EVE_cmd_number_burst(470, 10, 26, EVE_OPT_RIGHTX | EVE_OPT_SIGNED, swipeDistance_X);
	EVE_cmd_number_burst(470, 25, 26, EVE_OPT_RIGHTX | EVE_OPT_SIGNED, swipeDistance_Y);
	//EVE_cmd_text_var_burst(470, 25, 26, EVE_OPT_RIGHTX, "%d", swipeDistance_Y);

	//str_filename[1] = 'a';
	//TFT_textbox_display(20, 70, 20, str_filename);

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
void TFT_display_menu_setup(void){
	///


	// Set button color for header
	TFT_setColor(1, MAIN_BTNTXTCOLOR, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, 0);
	TFT_control(&btn_linSensor1);
	TFT_control(&btn_linSensor2);
	TFT_control(&btn_dimmmer);

	// Set Color
	TFT_setColor(1, BLACK, 1, 1, 1);

	// Filename textbox
	//TFT_textbox_display(20, 70, 20, str_filename);
	TFT_textbox_display(&tbx_filename);
	TFT_textbox_display(&tbx_sensor1);
	TFT_textbox_display(&tbx_sensor2);
}
void TFT_display_menu_linset(void){
	///

	/////////////// GRAPH
	///// Print dynamic part of the Graph (data & marker)
	TFT_GraphData(&gph_linset, &buf_linset[0], buf_linset_size, &buf_linset_idx, GRAPH_DATA1COLOR);

	/// Draw Banner and divider line on top
	TFT_header_static(1, &menu_linset);

	// Set button color for header
	TFT_setColor(1, MAIN_BTNTXTCOLOR, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, 0);
	TFT_control(&btn_back);

	TFT_textbox_display(&tbx_dp);
	TFT_control(&btn_db_last);
	TFT_control(&btn_db_next);
	TFT_textbox_display(&tbx_nom);
	TFT_textbox_display(&tbx_act);

}

void TFT_touch_menu0(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y){
	/// ...
	/// Do not use tags higher than 32 (they will be interpreted as keyboard input) or predefined TAGs -> see tft.c "TAG ASSIGNMENT"!

	// Determine which tag was touched
	switch(tag)
	{
		case 12:
			if(*toggle_lock == 0) {
				printf("Toggle Roll touched\n");
				*toggle_lock = 42;
				if(tgl_graphMode.state == 0){
					tgl_graphMode.state = 0xFFFF;
					tgl_graphMode.text = "Roll";
					gph_monitor.graphmode = 1;
				}
				else {
					tgl_graphMode.state = 0;
					tgl_graphMode.text = "Frame";
					gph_monitor.graphmode = 0;
				}
			}
			break;
		// signal switcher button
		case 13:
			if(*toggle_lock == 0) {
				printf("Switch Signal\n");
				*toggle_lock = 42;

				// Switch signal type
				InputType++;
				if(InputType > 3){ InputType = 0; }

				// Switch label of button to new input type
				if(InputType == 0){ 		btn_input.text = "Sensor";	}
				else if(InputType == 1){	btn_input.text = "Imp";	}
				else if(InputType == 2){	btn_input.text = "Saw";	}
				else{						btn_input.text = "Sine";	}
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
		// textbox
		case BTN_STARTREC_TAG:
			if(*toggle_lock == 0) {
				printf("Button StartRec\n");
				*toggle_lock = 42;

				record_buffer();
			}
			break;
		default:
			//TFT_textbox_touch(20, str_filename, STR_FILENAME_MAXLEN, &str_filename_curLength);
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
void TFT_touch_menu_setup(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y){
	/// ...
	/// Do not use tags higher than 32 (they will be interpreted as keyboard input) or predefined TAGs -> see tft.c "TAG ASSIGNMENT"!

	// Determine which tag was touched
	switch(tag)
	{
		// dimmer button on top as on/off radio-switch
		case BTN_DIMMER_TAG:
			if(*toggle_lock == 0) {
				printf("Button Dimmer touched\n");
				*toggle_lock = 42;
				if(btn_dimmmer.state == 0){
					btn_dimmmer.state = EVE_OPT_FLAT;
					EVE_memWrite8(REG_PWM_DUTY, 0x01);	/* setup backlight, range is from 0 = off to 0x80 = max */
				}
				else {
					btn_dimmmer.state = 0;
					EVE_memWrite8(REG_PWM_DUTY, 0x80);	/* setup backlight, range is from 0 = off to 0x80 = max */
				}
			}
			break;
		// textbox
		case TBX_FILENAME_TAG:
			if(*toggle_lock == 0) {
				printf("Textbox filename\n");
				*toggle_lock = 42;

				// Activate Keypad and set cursor to end
				TFT_textbox_setStatus(&tbx_filename, 1, *(tbx_filename.text_curlen)-4);
				//keypad_open(20, Filename);
				//TFT_textbox_setCursor(str_filename_curLength-4, str_filename_curLength);
			}
			break;
		case TBX_SENSOR1_TAG:
			if(*toggle_lock == 0) {
				printf("Textbox S1\n");
				*toggle_lock = 42;

				// Activate Keypad and set cursor to end
				TFT_textbox_setStatus(&tbx_sensor1, 1, *(tbx_sensor1.text_curlen));
				//keypad_open(20, Filename);
				//TFT_textbox_setCursor(str_filename_curLength-4, str_filename_curLength);
			}
			break;
		case BTN_LINSENSOR1_TAG:
			if(*toggle_lock == 0) {
				printf("Button LinS1\n");
				*toggle_lock = 42;

				// Change menu
				TFT_setMenu(menu_linset.index);
			}
			break;
		case TBX_SENSOR2_TAG:
			if(*toggle_lock == 0) {
				printf("Textbox S2\n");
				*toggle_lock = 42;

				// Activate Keypad and set cursor to end
				TFT_textbox_setStatus(&tbx_sensor2, 1, *(tbx_sensor2.text_curlen));
			}
			break;
		default:
			//TFT_textbox_touch(20, str_filename, STR_FILENAME_MAXLEN, &str_filename_curLength);
			TFT_textbox_touch(&tbx_filename);
			TFT_textbox_touch(&tbx_sensor1);
			TFT_textbox_touch(&tbx_sensor2);
			break;
	}
}
void TFT_touch_menu_linset(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y){
	/// ...
	/// Do not use tags higher than 32 (they will be interpreted as keyboard input) or predefined TAGs -> see tft.c "TAG ASSIGNMENT"!

	// Determine which tag was touched
	switch(tag)
	{
		// BUTTON BACK
		case BTN_BACK_TAG:
			if(*toggle_lock == 0) {
				printf("Button Back\n");
				*toggle_lock = 42;

				// Change menu
				TFT_setMenu(menu_setup.index);
			}
			break;
		default:
			break;
	}
}
