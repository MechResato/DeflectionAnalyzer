/*
 * menu.c
 *
 *  Created on: 25 Feb 2021
 *      Author: Rene Santeler
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <DAVE.h>
#include "globals.h"
#include "tft.h"
#include "FT800-FT813-5.x/EVE.h"
#include "polyfit/polyfit.h"
#include "record.h"
#include "menu.h"



/////////// Menu function pointers - At the end of the menu control functions (TFT_display_static, TFT_display and TFT_touch inside tft.h) the function referenced to this pointer is executed
// Every new menu needs to be defined in menu.h, declared at the end of this file and registered here in this function pointer array!
// TFT_MENU_SIZE 	   is declared in menu.h and must be changed if menus are added or removed
// TFT_MAIN_MENU_SIZE  is declared in menu.h. It states to where the main menus (accessible via swipe an background) are listed. All higher menus are considered submenus (control on how to get there is on menu.c)
void (*TFT_display_cur_Menu__fptr_arr[TFT_MENU_SIZE])(void) = {
		&menu_display_0monitor,
		&menu_display_1dash,
		&menu_display_2setup1,
		&menu_display_3setup2,
		&menu_display_curveset,
		&menu_display_filterset
};

void (*TFT_touch_cur_Menu__fptr_arr[TFT_MENU_SIZE])(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y) = {
		&menu_touch_0monitor,
		&menu_touch_1dash,
		&menu_touch_2setup1,
		&menu_touch_3setup2,
		&menu_touch_curveset,
		&menu_touch_filterset
};

void (*TFT_display_static_cur_Menu__fptr_arr[TFT_MENU_SIZE])(void) = {
		&menu_display_static_0monitor,
		&menu_display_static_1dash,
		&menu_display_static_2setup1,
		&menu_display_static_3setup2,
		&menu_display_static_curveset,
		&menu_display_static_filterset
};



#define M_0_UPPERBOND 66 // deepest coordinate (greatest number) of the header
menu menu_0monitor = {
		.index = 0,
		.headerText = "Monitoring",
		.upperBond = M_0_UPPERBOND,
		.headerLayout = {M_0_UPPERBOND, 320, 50, 360}, //[Y1,X1,Y2,X2]
		.bannerColor = MAIN_BANNERCOLOR,
		.dividerColor = MAIN_DIVIDERCOLOR,
		.headerColor = MAIN_TEXTCOLOR,
};

#define M_1_UPPERBOND 66  // deepest coordinate (greatest number) of the header
menu menu_1dashboard = {
		.index = 1,
		.headerText = "Dashboard",
		.upperBond = M_1_UPPERBOND,
		.headerLayout = {M_1_UPPERBOND, 280, 50, 320}, //[Y1,X1,Y2,X2]
		.bannerColor = MAIN_BANNERCOLOR,
		.dividerColor = MAIN_DIVIDERCOLOR,
		.headerColor = MAIN_TEXTCOLOR,
};

#define M_SETUP_UPPERBOND 65 // deepest coordinate (greatest number) of the header
menu menu_2setup1 = {
		.index = 2,
		.headerText = "Setup1",
		.upperBond = M_SETUP_UPPERBOND,
		.headerLayout = {M_SETUP_UPPERBOND, 240, 50, 280}, //[Y1,X1,Y2,X2]
		.bannerColor = MAIN_BANNERCOLOR,
		.dividerColor = MAIN_DIVIDERCOLOR,
		.headerColor = MAIN_TEXTCOLOR,
};

#define M_SETUP_UPPERBOND 65 // deepest coordinate (greatest number) of the header
menu menu_3setup2 = {
		.index = 3,
		.headerText = "Setup2",
		.upperBond = M_SETUP_UPPERBOND,
		.headerLayout = {M_SETUP_UPPERBOND, 240, 50, 280}, //[Y1,X1,Y2,X2]
		.bannerColor = MAIN_BANNERCOLOR,
		.dividerColor = MAIN_DIVIDERCOLOR,
		.headerColor = MAIN_TEXTCOLOR,
};

#define M_LINSET_UPPERBOND 40 // deepest coordinate (greatest number) of the header
menu menu_curveset = {
		.index = 4,
		.headerText = "",
		.upperBond = 0, // removed upper bond because header is written every TFT_display() in this submenu (on top -> no overlay possible)
		.headerLayout = {0, EVE_HSIZE-65, M_LINSET_UPPERBOND, EVE_HSIZE-50}, //[Y1,X1,Y2,X2]
		.bannerColor = MAIN_BANNERCOLOR,
		.dividerColor = MAIN_DIVIDERCOLOR,
		.headerColor = MAIN_TEXTCOLOR,
};

#define M_LINSET_UPPERBOND 40 // deepest coordinate (greatest number) of the header
menu menu_filterset = {
		.index = 5,
		.headerText = "",
		.upperBond = 0, // removed upper bond because header is written every TFT_display() in this submenu (on top -> no overlay possible)
		.headerLayout = {0, EVE_HSIZE-65, M_LINSET_UPPERBOND, EVE_HSIZE-50}, //[Y1,X1,Y2,X2]
		.bannerColor = MAIN_BANNERCOLOR,
		.dividerColor = MAIN_DIVIDERCOLOR,
		.headerColor = MAIN_TEXTCOLOR,
};


menu* menu_objects[TFT_MENU_SIZE] = {&menu_0monitor, &menu_1dashboard, &menu_2setup1, &menu_3setup2, &menu_curveset, &menu_filterset};










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

/////////// Debug
uint16_t display_list_size = 0; // Current size of the display-list from register. Used by the TFT_display() menu specific functions
uint32_t tracker = 0; // Value of tracker register (1.byte=tag, 2.byte=value). Used by the TFT_display() menu specific functions











// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//		Monitoring Elements         -----------------------------------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#define BTN_INPUT_TAG 13
control btn_input = {
	.x = 180,		.y = 15,
	.w0 = 70,		.h0 = 30,
	.mytag = BTN_INPUT_TAG,	.font = 27, .options = 0, .state = 0,
	.text = "Raw1",
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
		.numSrc.srcType = srcTypeNone
};
label lbl_DLsize_val = {
		.x = 470,		.y = 10,
		.font = 26,		.options = EVE_OPT_RIGHTX,	.text = "%d",
		.ignoreScroll = 1,
		.numSrc.srcType = srcTypeInt, .numSrc.intSrc = &display_list_size, .numSrc.srcOffset = NULL
};

label lbl_sensor = {
		.x = 360,		.y = 25,
		.font = 26,		.options = 0,	.text = "Sensor:",
		.ignoreScroll = 1,
		.numSrc.srcType = srcTypeNone
};
label lbl_sensor_val = {
		.x = 470,		.y = 25,
		.font = 26,		.options = EVE_OPT_RIGHTX,	.text = "%d",//.text = "%d.%.2d V",
		.ignoreScroll = 1,
		.numSrc.srcType = srcTypeInt, //srcTypeFloat,
		.numSrc.intSrc = (int_buffer_t*)&s1_buf_0raw, //(float_buffer_t*)&InputBuffer1_conv,
		.numSrc.srcOffset = (uint16_t*)&sensor1.bufIdx, // (ignore volatile here)
		.fracExp = 2
};


// Graph position and size. Here -> quick an dirty estimation where x, y, width and height must be to fill the whole main area
#define G_PADDING 10 //
graph gph_monitor = {
	.x = 10,																 // 10 px from left to leave some room
	.y = (M_0_UPPERBOND + 15),												// end of banner plus 10 to leave some room  (for Y1=66: 66+15=81)
	.width = (0 + EVE_HSIZE - 10 - (2*G_PADDING) - 10),			   			// actual width of the data area, therefore x and the paddings left and right must me accommodated to "fill" the whole main area. Additional 10 px from right to leave some room (for 480x272: 480-10-20-10=440)
	.height = (0 + EVE_VSIZE - (M_0_UPPERBOND + 15) - (2*G_PADDING) - 10), 	// actual height of the data area, therefore y and the paddings top and bottom must me accommodated to "fill" the whole main area. Additional 10 px from bottom to leave some room (for 480x272: 272-66+15-20-10=161)
	.padding = G_PADDING,
	.x_label = "t  ",
	.y_label = "V  ",
	.y_max = 4095.0, 		// maximum allowed amplitude y (here for 12bit sensor value);
	.amp_max = 5.2, 		// volts - used at print of vertical grid value labels
	.cx_max = 2.2,    		// seconds - used at print of horizontal grid value labels
	.h_grid_lines = 4.0, 	// number of grey horizontal grid lines
	.v_grid_lines = 2.2, 	// number of grey vertical grid lines
	.graphmode = 0
};





// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//		Dashboard Elements         -----------------------------------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#define BTN_STARTREC_TAG 10
control btn_startRec = {
	.x = M_COL_3,	.y = M_UPPER_PAD + M_1_UPPERBOND,
	.w0 = 55,		.h0 = 31,
	.mytag = BTN_STARTREC_TAG,	.font = 27,	.options = 0, .state = 0,
	.text = "Start",
	.controlType = Button,
	.ignoreScroll = 1
};

label lbl_record = {
	.x = M_COL_1,		.y = M_UPPER_PAD + M_1_UPPERBOND,
	.font = 27,		.options = 0,		.text = "Record",
	.ignoreScroll = 0
};






// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//		Setup1 Elements         -----------------------------------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
label lbl_recording = {
		.x = M_COL_1,		.y = M_UPPER_PAD + M_SETUP_UPPERBOND,
		.font = 27,		.options = 0,		.text = "Recording",
		.ignoreScroll = 0
};

#define STR_FILENAME_MAXLEN 16
char str_filename[STR_FILENAME_MAXLEN] = "test.csv";
uint8_t str_filename_curLength = 8;
#define TBX_FILENAME_TAG 20
textbox tbx_filename = {
	.x = M_COL_2,
	.y = M_UPPER_PAD + M_SETUP_UPPERBOND - TEXTBOX_PAD_V + FONT_COMP*1,
	.width = EVE_HSIZE - M_COL_2 - 65 - 25,
	.labelOffsetX = 65,
	.labelText = "Filename:",
	.mytag = TBX_FILENAME_TAG,
	.text = str_filename,
	.text_maxlen = STR_FILENAME_MAXLEN,
	.text_curlen = &str_filename_curLength,
	.keypadType = Filename,
	.active = 0,
	.numSrc.srcType = srcTypeNone
};

label lbl_calibrate = {
		.x = M_COL_1,		.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*1),
		.font = 27,		.options = 0,		.text = "Calibrate",
		.ignoreScroll = 0
};

//char* const chptr = (char* const)&sensor1.filename_spec;
#define TBX_SENSOR1_TAG 21
textbox tbx_sensor1 = {
	.x = M_COL_2,
	.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*1) - TEXTBOX_PAD_V + FONT_COMP*1,
	.width = EVE_HSIZE - (M_COL_2) - 130 - 25,
	.labelOffsetX = 130,
	.labelText = "Sensor1:   Spec File",
	.mytag = TBX_SENSOR1_TAG,
	.text = s1_filename_spec,
	.text_maxlen = STR_SPEC_MAXLEN,
	.text_curlen = (uint8_t*)&sensor1.fitFilename_curLen,  //(ignore volatile here)
	.keypadType = Standard,
	.active = 0,
	.numSrc.srcType = srcTypeNone
};

label lbl_curveset_S1 = {
		.x = 205,	.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*2) + FONT_COMP*1,
		.font = 26,		.options = 0,		.text = "Curve fit:",
		.ignoreScroll = 0
};
#define BTN_CURVESET_S1_TAG 22
control btn_curveset_S1 = {
	.x = 270,	.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*2) - TEXTBOX_PAD_V + FONT_COMP*1,
	.w0 = 55,		.h0 = 31,
	.mytag = BTN_CURVESET_S1_TAG,	.font = 27,	.options = 0, .state = 0,
	.text = "Set",
	.controlType = Button,
	.ignoreScroll = 0
};

label lbl_filterset_S1 = {
		.x = 355,	.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*2) + FONT_COMP*1,
		.font = 26,		.options = 0,		.text = "Filter:",
		.ignoreScroll = 0
};
#define BTN_FILTERSET_S1_TAG 23
control btn_filterset_S1 = {
	.x = EVE_HSIZE - 25 - 55,	.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*2) - TEXTBOX_PAD_V + FONT_COMP*1,
	.w0 = 55,		.h0 = 31,
	.mytag = BTN_FILTERSET_S1_TAG,	.font = 27,	.options = 0, .state = 0,
	.text = "Set",
	.controlType = Button,
	.ignoreScroll = 0
};

#define STR_S2_LINSPEC_MAXLEN 20
char str_s2_linspec[STR_S2_LINSPEC_MAXLEN] = "s2.lin";
uint8_t str_s2_linspec_curLength = 6;
#define TBX_SENSOR2_TAG 24
textbox tbx_sensor2 = {
	.x = M_COL_2,
	.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*3) - TEXTBOX_PAD_V + FONT_COMP*1,
	.width = 120,
	.labelOffsetX = 130,
	.labelText = "Sensor2:   Spec File",
	.mytag = TBX_SENSOR2_TAG,
	.text = str_s2_linspec,
	.text_maxlen = STR_S2_LINSPEC_MAXLEN,
	.text_curlen = &str_s2_linspec_curLength,
	.keypadType = Standard,
	.active = 0,
	.numSrc.srcType = srcTypeNone
};
#define BTN_LINSENSOR2_TAG 25
control btn_linSensor2 = {
	.x = M_COL_4,	.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*3) - TEXTBOX_PAD_V + FONT_COMP*1,
	.w0 = 55,		.h0 = 31,
	.mytag = BTN_LINSENSOR2_TAG,	.font = 27,	.options = 0, .state = 0,
	.text = "Set",
	.controlType = Button,
	.ignoreScroll = 0
};



// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//		Setup2 Elements         -----------------------------------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------

label lbl_backlight = {
		.x = M_COL_1,		.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*0),
		.font = 27,		.options = 0,		.text = "Backlight",
		.ignoreScroll = 0
};

#define BTN_DIMMER_TAG 10
control btn_dimmmer = {
	.x = M_COL_2,	.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*0) - TEXTBOX_PAD_V + FONT_COMP*1,
	.w0 = 80,		.h0 = 30,
	.mytag = BTN_DIMMER_TAG,	.font = 27, .options = 0, .state = 0,
	.text = "Dimmer",
	.controlType = Button,
	.ignoreScroll = 0
};


label lbl_RTC = {
		.x = M_COL_1,		.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*1),
		.font = 27,		.options = 0,		.text = "RTC",
		.ignoreScroll = 0
};

//int_buffer_t hour_val[4] = {1, 22, 333, 444};
//uint8_t hour_val_idx = 0;
#define STR_HOUR_MAXLEN 18
char str_hour[STR_HOUR_MAXLEN] = "12:13:26 24.03.21";
uint8_t str_hour_curLength = 17;
#define TBX_HOUR_TAG 25
textbox tbx_hour = {
	.x = M_COL_2,	.y = M_UPPER_PAD + M_SETUP_UPPERBOND + (M_ROW_DIST*1) - TEXTBOX_PAD_V + FONT_COMP*1,
	.width = 120,
	.labelOffsetX = 130,
	.labelText = "HH:mm:ss dd.MM.yy",
	.mytag = TBX_HOUR_TAG,
	.text = str_hour,
	.text_maxlen = STR_HOUR_MAXLEN,
	.text_curlen = &str_hour_curLength,
	.keypadType = Numeric,
	.active = 0,
	.numSrc.srcType = srcTypeNone,
	//.numSrcFormat = "%d",
	//.numSrc.intSrc = &hour_val[0],
	//.numSrc.srcOffset = (uint16_t*)&hour_val_idx
};



// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//		CurveSet Elements         -----------------------------------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void curveset_prepare(volatile sensor* sens);
void curveset_setEditMode(uint8_t editMode);

// Size and current index of all data point related arrays
uint16_t DP_size = 3;
uint16_t DP_cur = 0;
// Pointer to the currently being recorded sensor - set at prepare and e.g. used when getting the nominal value at display function or storing of the fit values
sensor*  curveset_sens;
// The avgFilterOrder of the sensor is temporarily changed while curveset menu is active. This variable stores the original value it will be reset to when coming back from the submenu.
uint16_t curveset_previousAvgFilterOrder;
// Array to store the coefficients for the fitted polynomial and the state of the fit (0=OK everything else is error)
uint8_t fit_order = 1;
float coefficients[4] = {0,0,0,0}; //{25.0, -0.0235162665374, 0.00001617208884, 0, 0};
uint8_t fit_result = 0;



label lbl_curveset = {
		.x = 20,		.y = 9,
		.font = 27,		.options = 0,		.text = "Curve fit - Sensor 0",
		.ignoreScroll = 0
};

label lbl_fitorder = {
		.x = M_COL_3 + 40,		.y = 9 + FONT_COMP,
		.font = 26,		.options = 0,		.text = "Function",
		.ignoreScroll = 0
};

#define BTN_ORDER_TAG 14
control btn_order = {
	.x = M_COL_3 + 40 + 60 ,	.y = 5,
	.w0 = 60, 	.h0 = 30,
	.mytag = BTN_ORDER_TAG,	.font = 26, .options = 0, .state = 0,
	.text = "Linear",
	.controlType = Button,
	.ignoreScroll = 0
};

#define G_PADDING 10 //
graph gph_curveset = {
	.x = 10,		// 10 px from left to leave some room
	.y = 30 + M_UPPER_PAD,		// end of banner plus 10 to leave some room  (e.g. for Y1=66: 66+15=81)
	.width  = (0 + EVE_HSIZE - 10 - (2*G_PADDING) - 10),		// actual width of the data area, therefore x and the paddings left and right must me accommodated to "fill" the whole main area. Additional 10 px from right to leave some room (for 480x272: 480-10-20-10=440)
	.height = (0 + EVE_VSIZE - 15 - (2*G_PADDING) - 10 - (M_ROW_DIST*2)), 	// actual height of the data area, therefore y and the paddings top and bottom must me accommodated to "fill" the whole main area. Additional 10 px from bottom to leave some room (for 480x272: 272-66+15-20-10=161)
	.padding = G_PADDING,
	.x_label = "nominal",
	.y_label = "actual in mm",
	.y_max = 200.0, 		// maximum allowed amplitude y (here for 12bit sensor value);
	.amp_max = 200.0, 		// in given unit - used at print of vertical grid value labels
	.cx_initial = 0,
	.cx_max = 4096,    		// seconds - used at print of horizontal grid value labels
	.h_grid_lines = 4.0, 	// number of grey horizontal grid lines
	.v_grid_lines = 2.0, 	// number of grey vertical grid lines
	.graphmode = 1,
};

#define BTN_DP_LAST_TAG 11
control btn_db_last = {
	.x = (M_COL_1/2) + 75 + 36 + 1,	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.w0 = 25				  ,	.h0 = 30,
	.mytag = BTN_DP_LAST_TAG,	.font = 27, .options = 0, .state = 0,
	.text = "<",
	.controlType = Button,
	.ignoreScroll = 0
};
#define BTN_DP_NEXT_TAG 12
control btn_db_next = {
	.x = (M_COL_1/2) + 75 + 36 + 1 + 25 + 1,	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.w0 = 25						   , 	.h0 = 30,
	.mytag = BTN_DP_NEXT_TAG,	.font = 27, .options = 0, .state = 0,
	.text = ">",
	.controlType = Button,
	.ignoreScroll = 0
};
#define BTN_DP_SETCHANGE_TAG 13
control btn_setchange = {
	.x = (M_COL_1/2) + 75 + 36 + 1 + 25 + 1 + 30 + 8 + 1 + 50 + 58 + 8 + 50 + 60 + 1,	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.w0 = 50, 	.h0 = 30,
	.mytag = BTN_DP_SETCHANGE_TAG,	.font = 26, .options = 0, .state = 0,
	.text = "Change",
	.controlType = Button,
	.ignoreScroll = 0
};


/// Textboxes

#define STR_DP_MAXLEN 3
char str_dp[STR_DP_MAXLEN] = "0";
uint8_t str_dp_curLength = 1;
#define TBX_DP_TAG 24
textbox tbx_dp = {
	.x = (M_COL_1/2),
	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.width = 36,
	.labelOffsetX = 75,
	.labelText = "Data Point:",
	.mytag = 0, //TBX_DP_TAG, // Todo: Made read-only because there needs to be an border check before this is useful
	.text = str_dp,
	.text_maxlen = STR_DP_MAXLEN,
	.text_curlen = &str_dp_curLength,
	.keypadType = Numeric,
	.active = 0,
	.numSrc.srcType = srcTypeInt,
	.numSrc.intSrc = (int_buffer_t*)&DP_cur,
	.numSrc.srcOffset = NULL,
	.numSrcFormat = "%d"
};

// The pointers the the x and y value arrays to be allocated by malloc and resized by realloc and used by the textboxes and graph are inside tbx_nom and tbx_act.
#define STR_NOM_MAXLEN 10
char str_nom[STR_NOM_MAXLEN] = "4095";
uint8_t str_nom_curLength = 4;
//#define TBX_NOM_TAG 0
textbox tbx_nom = {
	.x = (M_COL_1/2) + 75 + 36 + 1 + 25 + 1 + 30 + 8,
	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.width = 50,
	.labelOffsetX = 58,
	.labelText = "Nominal:",
	.mytag = 0,
	.text = str_nom,
	.text_maxlen = STR_NOM_MAXLEN,
	.text_curlen = &str_nom_curLength,
	.keypadType = Numeric,
	.active = 0,
	.numSrc.srcType = srcTypeFloat,
	.numSrc.floatSrc = NULL,
	.numSrc.srcOffset = NULL,
	.numSrcFormat = "%d.%.0d",
	.fracExp = 1
};
#define STR_ACT_MAXLEN 10
char str_act[STR_ACT_MAXLEN] = "";
uint8_t str_act_curLength = 0;
#define TBX_ACT_TAG 26
textbox tbx_act = {
	.x = (M_COL_1/2) + 75 + 36 + 1 + 25 + 1 + 30 + 8 + 1 + 50 + 58 + 8,
	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.width = 60,
	.labelOffsetX = 50,
	.labelText = "Actual:",
	.mytag = 0,
	.text = str_act,
	.text_maxlen = STR_ACT_MAXLEN,
	.text_curlen = &str_act_curLength,
	.keypadType = Numeric,
	.active = 0,
	.numSrc.srcType = srcTypeFloat,
	.numSrc.intSrc = NULL,
	.numSrc.srcOffset = NULL,
	.numSrcFormat = "%d.%.2d",
	.fracExp = 2
};


// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//		FilterSet Elements         -----------------------------------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void filterset_prepare(volatile sensor* sens);
void filterset_setEditMode(uint8_t editMode);

// Pointer to the currently being recorded sensor - set at prepare and e.g. used when getting the nominal value at display function or storing of the fit values
sensor*  filterset_sens = NULL;
uint16_t filter_errorThreshold = 4095;

label lbl_filterset = {
		.x = 20,		.y = 9,
		.font = 27,		.options = 0,		.text = "Filter set - Sensor 0",
		.ignoreScroll = 0
};

#define G_PADDING 10 //
graph gph_filterset = {
	.x = 10,		// 10 px from left to leave some room
	.y = 30 + M_UPPER_PAD,		// end of banner plus 10 to leave some room  (e.g. for Y1=66: 66+15=81)
	.width  = (0 + EVE_HSIZE - 10 - (2*G_PADDING) - 10),		// actual width of the data area, therefore x and the paddings left and right must me accommodated to "fill" the whole main area. Additional 10 px from right to leave some room (for 480x272: 480-10-20-10=440)
	.height = (0 + EVE_VSIZE - 15 - (2*G_PADDING) - 10 - (M_ROW_DIST*2)), 	// actual height of the data area, therefore y and the paddings top and bottom must me accommodated to "fill" the whole main area. Additional 10 px from bottom to leave some room (for 480x272: 272-66+15-20-10=161)
	.padding = G_PADDING,
	.x_label = "time",
	.y_label = "raw",
	.y_max = 4096.0, 		// maximum allowed amplitude y (here for 12bit sensor value);
	.amp_max = 4096.0, 		// in given unit - used at print of vertical grid value labels
	.cx_initial = 0,
	.cx_max = 2.2,    		// seconds - used at print of horizontal grid value labels
	.h_grid_lines = 4.0, 	// number of grey horizontal grid lines
	.v_grid_lines = 2.0, 	// number of grey vertical grid lines
	.graphmode = 0,
};

#define BTN_FILTER_DOWN_TAG 11
control btn_filter_down = {
	.x = M_COL_1 + 80 + 40 + 1,	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.w0 = 30				  ,	.h0 = 30,
	.mytag = BTN_DP_LAST_TAG,	.font = 27, .options = 0, .state = 0,
	.text = "-",
	.controlType = Button,
	.ignoreScroll = 0
};
#define BTN_FILTER_UP_TAG 12
control btn_filter_up = {
	.x = M_COL_1 + 80 + 40 + 1 + 30 + 1,	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.w0 = 30						   , 	.h0 = 30,
	.mytag = BTN_DP_NEXT_TAG,	.font = 27, .options = 0, .state = 0,
	.text = "+",
	.controlType = Button,
	.ignoreScroll = 0
};
#define BTN_FILTER_SETCHANGE_TAG 13
control btn_filter_setchange = {
	.x = EVE_HSIZE - 55 - 25,	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.w0 = 55, 	.h0 = 30,
	.mytag = BTN_FILTER_SETCHANGE_TAG,	.font = 26, .options = 0, .state = 0,
	.text = "Change",
	.controlType = Button,
	.ignoreScroll = 0
};


/// Textboxes
#define STR_FILTER_ORDER_MAXLEN 3
char str_filter_order[STR_FILTER_ORDER_MAXLEN] = "0";
uint8_t str_filter_order_curLength = 1;
#define TBX_FILTER_ORDER_TAG 24
textbox tbx_filter_order = {
	.x = M_COL_1,
	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.width = 40,
	.labelOffsetX = 80,
	.labelText = "Filter Order:",
	.mytag = 0,
	.text = str_filter_order,
	.text_maxlen = STR_FILTER_ORDER_MAXLEN,
	.text_curlen = &str_filter_order_curLength,
	.keypadType = Numeric,
	.active = 0,
	.numSrc.srcType = srcTypeInt,
	.numSrc.intSrc = NULL,
	.numSrc.srcOffset = NULL,
	.numSrcFormat = "%d"
};
#define STR_ERROR_THRESHOLD_MAXLEN 5
char str_error_threshold[STR_ERROR_THRESHOLD_MAXLEN] = "4095";
uint8_t str_error_threshold_curLength = 4;
#define TBX_FILTER_ERROR_THRESHOLD_TAG 25
textbox tbx_error_threshold_order = {
	.x = EVE_HSIZE - 55 - 25 - 55 - 105 - 5,
	.y = EVE_VSIZE - M_UPPER_PAD - M_ROW_DIST,
	.width = 55,
	.labelOffsetX = 105,
	.labelText = "Error Threshold:",
	.mytag = 0,
	.text = str_error_threshold,
	.text_maxlen = STR_ERROR_THRESHOLD_MAXLEN,
	.text_curlen = &str_error_threshold_curLength,
	.keypadType = Numeric,
	.active = 0,
	.numSrc.srcType = srcTypeInt,
	.numSrc.intSrc = &filter_errorThreshold,
	.numSrc.srcOffset = NULL,
	.numSrcFormat = "%d"
};




// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//		Common elements of set submenus         -----------------------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#define BTN_BACK_TAG 10
control btn_back = {
	.x = EVE_HSIZE-45,	.y = 5,
	.w0 = 40,			.h0 = 30,
	.mytag = BTN_BACK_TAG,	.font = 27, .options = 0, .state = 0,
	.text = "Back",
	.controlType = Button,
	.ignoreScroll = 1
};

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//		End of Element definition         ----------------------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------



















void TFT_display_get_values(void){
	// Get size of last display list to be printed on screen (section "Debug Values")
	display_list_size = EVE_memRead16(REG_CMD_DL);
	tracker = EVE_memRead32(REG_TRACKER);
}




// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//		Monitoring          --------------------------------------------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void menu_display_static_0monitor(void){
	// Set configuration for current menu
	TFT_setMenu(menu_0monitor.index);

	/// Draw Banner and divider line on top
	TFT_header(1, &menu_0monitor);

	// Set Color
	TFT_setColor(1, MAIN_TEXTCOLOR, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);

	// Add the static text
	TFT_label(1, &lbl_DLsize);
	TFT_label(1, &lbl_sensor);

	/// Write the static part of the Graph to the display list
	TFT_graph_static(1, &gph_monitor);

}
void menu_display_0monitor(void){
	/// Menu specific display code. This will run if the corresponding menu is active and the main tft_display() is called.
	/// This menu ...

	/////////////// Display BUTTONS and Toggles
	TFT_setColor(1, MAIN_BTNTXTCOLOR, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);

	// Buttons
	TFT_control(&btn_input);
	TFT_control(&tgl_graphMode);

	/////////////// Debug Values
	//EVE_cmd_number_burst(470, 10, 26, EVE_OPT_RIGHTX, display_list_size); /* number of bytes written to the display-list by the command co-pro */
	TFT_label(1, &lbl_DLsize_val);

	// Write current sensor value with unit
	TFT_label(1, &lbl_sensor_val);

	/////////////// GRAPH
	///// Print dynamic part of the Graph (data & marker)
	if(inputType == 4)
		TFT_graph_pixeldata_f(&gph_monitor, &s1_buf_2conv[0], S1_BUF_SIZE, (uint16_t*)&sensor1.bufIdx, GRAPH_DATA1COLOR); // ignore volatile sensor
	else
		TFT_graph_pixeldata_i(&gph_monitor, &s1_buf_0raw[0], S1_BUF_SIZE, (uint16_t*)&sensor1.bufIdx, GRAPH_DATA1COLOR); // ignore volatile sensor

}
void menu_touch_0monitor(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y){
	/// Menu specific touch code. This will run if the corresponding menu is active and the main tft_touch() registers an unknown tag value
	/// Do not use predefined TAG values! See tft.c "TAG ASSIGNMENT"!


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
				//InputType++;
				if(inputType == 0){ inputType = 4; }
				else{ inputType = 0; }

				// Switch label of button to new input type
				if(inputType == 0){
					btn_input.text = "Raw1";
					lbl_sensor_val.text = "%d";
					lbl_sensor_val.numSrc.srcType = srcTypeInt;
					lbl_sensor_val.numSrc.intSrc = (int_buffer_t*)&s1_buf_0raw;
					lbl_sensor_val.fracExp = 1;

					gph_monitor.amp_max = 5.2;
					gph_monitor.y_max = 4095.0;
					gph_monitor.y_label = "V";
				}
				else if(inputType == 1){	btn_input.text = "Imp";	}
				else if(inputType == 2){	btn_input.text = "Saw";	}
				else if(inputType == 3){	btn_input.text = "Sine"; }
				else{
					btn_input.text = "Sensor1";
					lbl_sensor_val.text = "%d.%.2d mm";
					lbl_sensor_val.numSrc.srcType = srcTypeFloat;
					lbl_sensor_val.numSrc.floatSrc = (float_buffer_t*)&s1_buf_2conv;
					lbl_sensor_val.fracExp = 2;

					gph_monitor.amp_max = 70;
					gph_monitor.y_max = 70;
					gph_monitor.y_label = "mm";
				}

				// Refresh menu (needed to reprint static part of graph)
				TFT_setMenu(-1);
			}
			break;
	}
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//		Dashboard          --------------------------------------------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void menu_display_static_1dash(void){
	// Set configuration for current menu
	TFT_setMenu(menu_1dashboard.index);

	/// Draw Banner and divider line on top
	TFT_header(1, &menu_1dashboard);

	// Add the static text
	EVE_cmd_dl_burst(TAG(0)); /* do not use the following objects for touch-detection */
	EVE_cmd_dl_burst(DL_COLOR_RGB | MAIN_TEXTCOLOR);
	EVE_cmd_text_burst(360, 10, 26, 0, "X:");
	EVE_cmd_text_burst(360, 25, 26, 0, "Y:");

	// Text
	TFT_setColor(1, BLACK, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);
	TFT_label(1, &lbl_record);
}
void menu_display_1dash(void){
	/// Menu specific display code. This will run if the corresponding menu is active and the main tft_display() is called.
	/// This menu ...


	/////////////// Display BUTTONS and Toggles
	TFT_setColor(1, MAIN_BTNTXTCOLOR, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);

	// Button start recording
	TFT_control(&btn_startRec);

	// Debug
	TFT_setColor(1, MAIN_TEXTCOLOR, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);
	EVE_cmd_number_burst(470, 10, 26, EVE_OPT_RIGHTX | EVE_OPT_SIGNED, swipeDistance_X);
	EVE_cmd_number_burst(470, 25, 26, EVE_OPT_RIGHTX | EVE_OPT_SIGNED, swipeDistance_Y);
}
void menu_touch_1dash(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y){
	/// Menu specific touch code. This will run if the corresponding menu is active and the main tft_touch() registers an unknown tag value
	/// Do not use predefined TAG values! See tft.c "TAG ASSIGNMENT"!


	// Determine which tag was touched
	switch(tag)
	{
		// textbox
		case BTN_STARTREC_TAG:
			if(*toggle_lock == 0) {
				printf("Button StartRec\n");
				*toggle_lock = 42;

				//record_buffer();

				// Load Values from SD-Card if possible
				record_readSpecFile(&sensor1, NULL, NULL, NULL);
			}
			break;
		default:
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

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//		Setup1              --------------------------------------------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void menu_display_static_2setup1(void){
	// Set configuration for current menu
	TFT_setMenu(menu_2setup1.index);

	/// Draw Banner and divider line on top
	TFT_header(1, &menu_2setup1);

	// Set Color
	TFT_setColor(1, BLACK, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);

	/// Recording section
	TFT_label(1, &lbl_recording);
	// Filename
	TFT_textbox_static(1, &tbx_filename);

	/// Sensor curve fit section
	TFT_label(1, &lbl_calibrate);
	TFT_textbox_static(1, &tbx_sensor1);
	TFT_label(1, &lbl_curveset);
	TFT_label(1, &lbl_curveset_S1);
	TFT_label(1, &lbl_filterset_S1);
	TFT_textbox_static(1, &tbx_sensor2);
}
void menu_display_2setup1(void){
	/// Menu specific display code. This will run if the corresponding menu is active and the main tft_display() is called.
	/// This menu ...


	// Set button color for header
	TFT_setColor(1, MAIN_BTNTXTCOLOR, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);
	TFT_control(&btn_curveset_S1);
	TFT_control(&btn_filterset_S1);
	TFT_control(&btn_linSensor2);

	// Set Color
	TFT_setColor(1, BLACK, -1, -1, -1);

	// Filename textbox
	//TFT_textbox_display(20, 70, 20, str_filename);
	TFT_textbox_display(&tbx_filename);
	TFT_textbox_display(&tbx_sensor1);
	TFT_textbox_display(&tbx_sensor2);
}
void menu_touch_2setup1(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y){
	/// Menu specific touch code. This will run if the corresponding menu is active and the main tft_touch() registers an unknown tag value
	/// Do not use predefined TAG values! See tft.c "TAG ASSIGNMENT"!


	// Determine which tag was touched
	switch(tag)
	{
		// textbox
		case TBX_FILENAME_TAG:
			if(*toggle_lock == 0) {
				printf("Textbox filename\n");
				*toggle_lock = 42;

				// Activate Keypad and set cursor to end
				TFT_textbox_setStatus(&tbx_filename, 1, *(tbx_filename.text_curlen)-4);
			}
			break;
		case TBX_SENSOR1_TAG:
			if(*toggle_lock == 0) {
				printf("Textbox S1\n");
				*toggle_lock = 42;

				// Activate textbox/keypad and set cursor to end
				TFT_textbox_setStatus(&tbx_sensor1, 1, *(tbx_sensor1.text_curlen));
			}
			break;
		case BTN_CURVESET_S1_TAG:
			if(*toggle_lock == 0) {
				printf("Button CurveFit S1\n");
				*toggle_lock = 42;

				// Prepare linSet menu for current sensor
				curveset_prepare(&sensor1);

				// Change menu
				TFT_setMenu(menu_curveset.index);
			}
			break;
		case BTN_FILTERSET_S1_TAG:
			if(*toggle_lock == 0) {
				printf("Button FilterSet S1\n");
				*toggle_lock = 42;

				// Prepare linSet menu for current sensor
				filterset_prepare(&sensor1);

				// Change menu
				TFT_setMenu(menu_filterset.index);
			}
			break;
		case BTN_LINSENSOR2_TAG:
			if(*toggle_lock == 0) {
				printf("Button LinS1\n");
				*toggle_lock = 42;

				// Prepare linSet menu for current sensor
				//curveset_prepare(&InputBuffer1[0], &sensor1.bufferIdx);

				// Change menu
				//TFT_setMenu(menu_curveset.index);
			}
			break;
		case TBX_SENSOR2_TAG:
			if(*toggle_lock == 0) {
				printf("Textbox S2\n");
				*toggle_lock = 42;

				// Activate Keypad and set cursor to end
				TFT_textbox_setStatus(&tbx_sensor2, 1, -1);
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

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//		Setup2              --------------------------------------------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void menu_display_static_3setup2(void){
	// Set configuration for current menu
	TFT_setMenu(menu_3setup2.index);

	/// Draw Banner and divider line on top
	TFT_header(1, &menu_3setup2);

	// Set Color
	TFT_setColor(1, BLACK, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);

	// RTC
	TFT_label(1, &lbl_RTC);
	TFT_textbox_static(1, &tbx_hour);

	/// Backlight
	TFT_label(1, &lbl_backlight);
}
void menu_display_3setup2(void){
	/// Menu specific display code. This will run if the corresponding menu is active and the main tft_display() is called.
	/// This menu ...


	// Set button color for header
	TFT_setColor(1, MAIN_BTNTXTCOLOR, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);
	TFT_control(&btn_dimmmer);

	// Set Color
	TFT_setColor(1, BLACK, -1, -1, -1);

	// Filename textbox
	//TFT_textbox_display(20, 70, 20, str_filename);
	TFT_textbox_display(&tbx_hour);
}
void menu_touch_3setup2(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y){
	/// Menu specific touch code. This will run if the corresponding menu is active and the main tft_touch() registers an unknown tag value
	/// Do not use predefined TAG values! See tft.c "TAG ASSIGNMENT"!


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
		case TBX_HOUR_TAG:
			if(*toggle_lock == 0) {
				printf("Textbox Hour\n");
				*toggle_lock = 42;

				// Activate Keypad and set cursor to end
				TFT_textbox_setStatus(&tbx_hour, 1, -1);
			}
		default:
			TFT_textbox_touch(&tbx_hour);
			break;
	}
}



// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//		CurveSet             --------------------------------------------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void curveset_prepare(volatile sensor* sens){
	/// Prepares the linearisation settings menu to use the current sensor and reads current configuration if available


	// Store pointer to referenced Sensor buffer (ignore volatile here)
	curveset_sens = (sensor*)sens;
	// Store current value of average filter order
	curveset_previousAvgFilterOrder = sens->avgFilterOrder;

	// Set avg filter order higher. This way its easier to get precise measurements. The user should be able to keep the distance for minimum 1 second, therefore filter over 1 second
	uint16_t newFilOrder = (uint16_t)ceil(1000.0 / MEASUREMENT_INTERVAL);
	if(newFilOrder < sens->bufMaxIdx)
		curveset_sens->avgFilterOrder = newFilOrder;
	else
		curveset_sens->avgFilterOrder = sens->bufMaxIdx-1;
	printf("Using Avg filter order %d during curveset!\n", sens->avgFilterOrder);
	// Do a clean filter value calculation to sync it to the new filter order
	measure_movAvgFilter_clean(curveset_sens);

	// Check if spec file exists and load current settings if possible
	// Load Values from SD-Card if possible, or use standard values
	DP_size = 0;
	record_readSpecFile(&sensor1, &tbx_nom.numSrc.floatSrc, &tbx_act.numSrc.floatSrc, &DP_size);
	// If there are still no data points - use default ones
	if(DP_size == 0){
		// Allocate and set the y-value array
		DP_size = 3;
		tbx_act.numSrc.floatSrc = (float*)malloc((DP_size+1)*sizeof(float));
		tbx_act.numSrc.floatSrc[0] = 0.0;
		tbx_act.numSrc.floatSrc[1] = 100.0;
		tbx_act.numSrc.floatSrc[2] = 200.0;

		// Allocate and set the x-value array
		tbx_nom.numSrc.floatSrc = (float*)malloc((DP_size+1)*sizeof(float));
		tbx_nom.numSrc.floatSrc[0] = 0.0;
		tbx_nom.numSrc.floatSrc[1] = 2048.0;
		tbx_nom.numSrc.floatSrc[2] = 4096.0;

	}
	else{
		// Set Function button text accordingly
		switch (sens->fitOrder) {
			case 1:
				btn_order.text = "Linear";
				break;
			case 2:
				btn_order.text = "Square";
				break;
			case 3:
				btn_order.text = "Cubic";
				break;
		}
	}

	// Check for allocation errors
	if(tbx_act.numSrc.floatSrc == NULL || tbx_nom.numSrc.floatSrc == NULL)
		printf("Memory malloc failed!\n");

	// Link actual value array to corresponding textbox
	//tbx_act.numSrc.floatSrc = &tbx_act.numSrc.floatSrc[0];
	tbx_act.numSrc.srcOffset = &DP_cur;

	// Link actual value array to corresponding textbox
	//tbx_nom.numSrc.floatSrc = &tbx_nom.numSrc.floatSrc[0];
	tbx_nom.numSrc.srcOffset = &DP_cur;

	// Determine fitted polynomial for the first time
	fit_result = polyfit(tbx_nom.numSrc.floatSrc, tbx_act.numSrc.floatSrc, DP_size, fit_order, coefficients);
}
void curveset_setEditMode(uint8_t editMode){
	/// Changes the GUI to data point editing mode and back (disable/enable of textboxes and buttons)


	// Set to edit mode - activate textbox for actual value and deactivate data point selection
	if(editMode == 1){
		// Change button appearance and activate textbox
		btn_setchange.state = EVE_OPT_FLAT;
		btn_setchange.text = "Save";
		tbx_act.mytag = TBX_ACT_TAG;

		// Deactivate data point selector buttons and textbox (read-only)
		btn_db_last.mytag = 0;
		btn_db_next.mytag = 0;
	}
	// Set to view mode - deactivate textbox for actual value and activate data point selection again
	else{
		// Change button appearance and deactivate textbox
		btn_setchange.state = 0;
		btn_setchange.text = "Change";
		tbx_act.mytag = 0;

		// Activate data point selector buttons and textbox (read/write)
		btn_db_last.mytag = BTN_DP_LAST_TAG;
		btn_db_next.mytag = BTN_DP_NEXT_TAG;

		/// Set graph boundaries
		// Get biggest y-value
		float cur_y_max = 0;
		for(uint8_t i = 0; i < DP_size; i++)
			if(tbx_act.numSrc.floatSrc[i] > cur_y_max)
				cur_y_max = tbx_act.numSrc.floatSrc[i];

		// Set biggest y-value as graph
		gph_curveset.y_max = gph_curveset.amp_max = cur_y_max;

		// Refresh static part of display (e.g. to show new textbox background when one changes from read-only to read-write)
		//TFT_setMenu(-1);
	}

}
uint16_t menu_shelf_datapoint(float* x_buf, float* y_buf, uint8_t buf_size, uint8_t buf_cur_idx){
	/// Takes two arrays of corresponding x/y-values and moves the current data point so that its x coordinate is between a smaller and a bigger one.
	/// If the current data point is the only unsorted one, this will result in a sorted list (make sure this is the case)!
	/// First the new target position is determined by iterating through and checking the value of the last and next point. Then the points between the current and target position are shifted forth/back before the current point is copied to its target.
	///
	///	Input:	x_buf, y_buf	Pointers to the float buffers of x and y (must be equal in size and corresponding)
	///			buf_size		Number of elements in the buffers
	///			buf_cur_idx		Index of the current point, which will be moved to its rightful place
	///
	/// Output:	target_pos		The new position of the current data point. Returns buf_cur_idx if the point is already at the right position.


	// Set return value to current position
	uint8_t target_pos = buf_cur_idx;

	// Get target position of to be shelved point in x_buf
	for(uint8_t i = 0; i < buf_size; i++){
		// Loop through till the to be shelved point is bigger than last but smaller than next (ignore the last value if i == 0 because it does not exist)
		if((i == 0 || x_buf[buf_cur_idx] > x_buf[i-1]) && x_buf[buf_cur_idx] < x_buf[i]){
			// Found target location -> save position
			target_pos = i;

			// Stop loop
			break;
		}
		// Special check for the last value in array
		else if(i == buf_size-1 && x_buf[buf_cur_idx] > x_buf[i]){
			// Found target location -> save position
			target_pos = buf_size-1;

			// Stop loop
			break;
		}
	}

	// Only move if the to be shelved point isn't already at the right spot
	if(buf_cur_idx != target_pos){
		// Save values of to be shelved point
		float shelf_x = x_buf[buf_cur_idx];
		float shelf_y = y_buf[buf_cur_idx];

		/// Move all elements between target position and current position to be shelved point forward or backward
		// If target position is after current position - move all elements between towards the beginning
		if(buf_cur_idx < target_pos){
			for(uint8_t i = buf_cur_idx; i < target_pos; i++){
				x_buf[i] = x_buf[i+1];
				y_buf[i] = y_buf[i+1];
			}
		}
		// If target position is before current position - move all elements between towards end
		else{ //else if(buf_cur_idx > target_pos){
			for(uint8_t i = buf_cur_idx; i > target_pos; i--){
				x_buf[i] = x_buf[i-1];
				y_buf[i] = y_buf[i-1];
			}
		}

		// Copy the to be shelved point to target position
		x_buf[target_pos] = shelf_x;
		y_buf[target_pos] = shelf_y;
	}

	// Return target position (aka new index of shelf element)
	return target_pos;
}

void menu_display_static_curveset(void){
	// Set configuration for current menu
	TFT_setMenu(menu_curveset.index);

	// Set Color
	TFT_setColor(1, BLACK, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);

	/// Write the static part of the Graph to the display list
	TFT_graph_static(1, &gph_curveset);

	TFT_textbox_static(1, &tbx_dp);
	TFT_textbox_static(1, &tbx_nom);
	TFT_textbox_static(1, &tbx_act);
}
void menu_display_curveset(void){
	/// Menu specific display code. This will run if the corresponding menu is active and the main tft_display() is called.
	/// This menu ...


	// If current data point is in edit mode
	if(tbx_act.mytag != 0){
		// Save current nominal value
		tbx_nom.numSrc.floatSrc[*tbx_nom.numSrc.srcOffset] = (float)curveset_sens->bufFilter[curveset_sens->bufIdx]; //(float)curveset_sens->bufRaw[curveset_sens->bufIdx];//

		// Sort tbx_act.numSrc.floatSrc & tbx_nom.numSrc.floatSrc based on nomx and change current datapoint if necessary
		DP_cur = menu_shelf_datapoint(tbx_nom.numSrc.floatSrc, tbx_act.numSrc.floatSrc, DP_size, DP_cur);

		// Determine fitted polynomial
		fit_result = polyfit(tbx_nom.numSrc.floatSrc, tbx_act.numSrc.floatSrc, DP_size, fit_order, coefficients);
	}

	// Change "right" button to "new" if on the edge of points
	if(DP_cur >= DP_size-1)
		btn_db_next.text = "+";
	else
		btn_db_next.text = ">";


	/////////////// GRAPH
	///// Print dynamic part of the Graph (data & marker)
	// Current data points and trace
	TFT_graph_XYdata(&gph_curveset, tbx_act.numSrc.floatSrc, tbx_nom.numSrc.floatSrc, DP_size, (int16_t*)&DP_cur, graphLine, GRAPH_DATA2COLORLIGHT);
	TFT_graph_XYdata(&gph_curveset, tbx_act.numSrc.floatSrc, tbx_nom.numSrc.floatSrc, DP_size, (int16_t*)&DP_cur, graphPoint, GRAPH_DATA2COLOR);
	// Fitted curve
	TFT_graph_function(&gph_curveset, &coefficients[0], fit_order, 2, graphLine, GRAPH_DATA1COLOR);

	/// Draw Banner and divider line on top
	TFT_header(1, &menu_curveset);

	// Set button color for header
	TFT_setColor(1, MAIN_BTNTXTCOLOR, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);
	// Buttons
	TFT_control(&btn_back); //	 - return from submenu
	TFT_control(&btn_order); //	 - change curve fit function
	TFT_control(&btn_setchange);
	TFT_control(&btn_db_last);
	TFT_control(&btn_db_next);

	// Data point controls
	TFT_textbox_display(&tbx_dp);
	TFT_textbox_display(&tbx_nom);
	TFT_textbox_display(&tbx_act);

	// Header labels
	TFT_label(1, &lbl_curveset);
	TFT_label(1, &lbl_fitorder);

}
void menu_touch_curveset(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y){
	/// Menu specific touch code. This will run if the corresponding menu is active and the main tft_touch() registers an unknown tag value
	/// Do not use predefined TAG values! See tft.c "TAG ASSIGNMENT"!


	// Determine which tag was touched
	switch(tag)
	{
		// BUTTON BACK
		case BTN_BACK_TAG:
			if(*toggle_lock == 0) {
				printf("Button Back\n");
				*toggle_lock = 42;

				// Reset filter order
				curveset_sens->avgFilterOrder = curveset_previousAvgFilterOrder;
				printf("Reseting avg filter order back to %d!\n", curveset_sens->avgFilterOrder);
				// Do a clean filter value calculation to sync it to the new filter order
				measure_movAvgFilter_clean(curveset_sens);

				// Store current polynomial fit to be used
				for (uint8_t i = 0; i < 4; i++) {
					curveset_sens->fitCoefficients[i] = coefficients[i];
					printf("c%i = %.10f\n",i, coefficients[i]);
				}
				curveset_sens->fitOrder = fit_order;

				// Write Spec file
				record_writeSpecFile(curveset_sens, tbx_act.numSrc.floatSrc, tbx_nom.numSrc.floatSrc, DP_size);

				// Free allocated memory
				free(tbx_act.numSrc.floatSrc);
				free(tbx_nom.numSrc.floatSrc);

				// Change menu
				TFT_setMenu(menu_2setup1.index);
			}
			break;
		case BTN_ORDER_TAG:
			if(*toggle_lock == 0) {
				printf("Button order\n");
				*toggle_lock = 42;

				// Change polynomial order
				fit_order++;
				if (fit_order > 3)
					fit_order = 1;

				// Change button name accordingly
				switch (fit_order) {
					case 1:
						btn_order.text = "Linear";
						break;
					case 2:
						btn_order.text = "Square";
						break;
					case 3:
						btn_order.text = "Cubic";
						break;
					default:
						break;
				}

				// TODO: Warn user if there are to less points for selected function
				// ...

				// Determine fitted polynomial after order change
				fit_result = polyfit(tbx_nom.numSrc.floatSrc, tbx_act.numSrc.floatSrc, DP_size, fit_order, coefficients);
			}
			break;
		case TBX_DP_TAG:
			if(*toggle_lock == 0) {
				printf("Textbox actual\n");
				*toggle_lock = 42;

				// Activate Keypad and set cursor to end
				TFT_textbox_setStatus(&tbx_dp, 1, -1);
			}
			break;
		case TBX_ACT_TAG:
			if(*toggle_lock == 0) {
				printf("Textbox actual\n");
				*toggle_lock = 42;

				// Activate Keypad and set cursor to end
				TFT_textbox_setStatus(&tbx_act, 1,-1);
			}
			break;
		case BTN_DP_LAST_TAG:
			if(*toggle_lock == 0) {
				printf("Button last\n");
				*toggle_lock = 42;

				// If the data point isn't at the limits - change current index
				if(DP_cur > 0){
					// Decrease currently selected data point index
					DP_cur--;
				}
			}
			break;
		case BTN_DP_NEXT_TAG:
			if(*toggle_lock == 0) {
				printf("Button last\n");
				*toggle_lock = 42;

				// if the data point isn't at the limits - change current index, set text of DP-textbox and set text of Actual-textbox
				if(DP_cur < 254){
					// Decrease currently selected data point index
					DP_cur++;

					// Add a new data-point if the border of the array is reached
					if(DP_cur > (DP_size-1)){
						// Increase size of array
						DP_size++;
						tbx_act.numSrc.floatSrc = (float*)realloc(tbx_act.numSrc.floatSrc, DP_size*sizeof(float));
						tbx_nom.numSrc.floatSrc = (float*)realloc(tbx_nom.numSrc.floatSrc, DP_size*sizeof(float));

						// Check for allocation errors
						if(tbx_act.numSrc.floatSrc == NULL || tbx_nom.numSrc.floatSrc == NULL)
							printf("Memory realloc failed!\n");

						/// Set initial value's
						// Set initial x value to current sensor value
						tbx_nom.numSrc.floatSrc[DP_cur] = (float)curveset_sens->bufFilter[curveset_sens->bufIdx];//tbx_act.numSrc.floatSrc[DP_cur+1];
						// If an OK fit is available set initial y-value to the one corresponding to the curretn sensor value
						if(fit_result == 0)
							tbx_act.numSrc.floatSrc[DP_cur] = poly_calc(tbx_nom.numSrc.floatSrc[DP_cur], &coefficients[0], fit_order);
						// If no OK fit is available but the new value is after an other one, use the previous y value as initial value
						else if(DP_cur >= 1)
							tbx_act.numSrc.floatSrc[DP_cur] = tbx_act.numSrc.floatSrc[DP_cur-1];
						// If there is no previous y value use first one
						else
							tbx_act.numSrc.floatSrc[DP_cur] = tbx_act.numSrc.floatSrc[0];

						// Set data point to edit mode
						curveset_setEditMode(1);

						// Refresh static part of display (e.g. to show new textboxbackground when one changes from read-only to read-write)
						TFT_setMenu(-1);
					}
				}
			}
			break;
		case BTN_DP_SETCHANGE_TAG:
			if(*toggle_lock == 0) {
				printf("Button set/change\n");
				*toggle_lock = 42;

				// Change from view to edit mode of current data point
				if(btn_setchange.state == EVE_OPT_FLAT)
					curveset_setEditMode(0);
				else
					curveset_setEditMode(1);

				// Refresh static part of display (e.g. to show new textboxbackground when one changes from read-only to read-write)
				TFT_setMenu(-1);
			}
			break;
		default:
			TFT_textbox_touch(&tbx_act);
			//TFT_textbox_touch(&tbx_dp); // TODO: Commented out because there needs to be an border check before this is useful. Meanwhile it's read only this way
			break;
	}
}


// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//		FilterSet             --------------------------------------------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void filterset_prepare(volatile sensor* sens){
	/// Prepares the linearisation settings menu to use the current sensor and reads current configuration if available


	// Store pointer to referenced Sensor buffer (ignore volatile here)
	filterset_sens = (sensor*)sens;

	// Do a clean filter value calculation to sync it
	//measure_movAvgFilter_clean(filterset_sens);

	// Check if spec file exists and load current settings if possible
	// Load Values from SD-Card if possible, or use standard values
	DP_size = 0;
	record_readSpecFile(&sensor1, &tbx_nom.numSrc.floatSrc, &tbx_act.numSrc.floatSrc, &DP_size);
	// If there are still no data points - use default ones
	if(DP_size == 0){
		// Allocate and set the y-value array
		DP_size = 3;
		tbx_act.numSrc.floatSrc = (float*)malloc(DP_size*sizeof(float));
		tbx_act.numSrc.floatSrc[0] = 0.0;
		tbx_act.numSrc.floatSrc[1] = 100.0;
		tbx_act.numSrc.floatSrc[2] = 200.0;

		// Allocate and set the x-value array
		tbx_nom.numSrc.floatSrc = (float*)malloc(DP_size*sizeof(float));
		tbx_nom.numSrc.floatSrc[0] = 0.0;
		tbx_nom.numSrc.floatSrc[1] = 2048.0;
		tbx_nom.numSrc.floatSrc[2] = 4096.0;

	}
	else{

	}

	// Check for allocation errors
	if(tbx_act.numSrc.floatSrc == NULL || tbx_nom.numSrc.floatSrc == NULL)
		printf("Memory malloc failed!\n");

	// Link filter order textbox source to current sensor filter order
	tbx_filter_order.numSrc.intSrc = &filterset_sens->avgFilterOrder;

	// Link actual value array to corresponding textbox
	//tbx_act.numSrc.floatSrc = &tbx_act.numSrc.floatSrc[0];
	//tbx_act.numSrc.srcOffset = &DP_cur;

	// Link actual value array to corresponding textbox
	//tbx_nom.numSrc.floatSrc = &tbx_nom.numSrc.floatSrc[0];
	//tbx_nom.numSrc.srcOffset = &DP_cur;

	// Determine fitted polynomial for the first time
	//fit_result = polyfit(tbx_nom.numSrc.floatSrc, tbx_act.numSrc.floatSrc, DP_size, fit_order, coefficients);
}
void filterset_setEditMode(uint8_t editMode){
	/// Changes the GUI to error threshold editing mode and back (disable/enable of textboxes and buttons)


	// Set to edit mode - activate textbox for error threshold
	if(editMode == 1){
		// Change button appearance and activate textbox
		btn_filter_setchange.state = EVE_OPT_FLAT;
		btn_filter_setchange.text = "Save";
		tbx_error_threshold_order.mytag = TBX_FILTER_ERROR_THRESHOLD_TAG;

	}
	// Set to view mode - deactivate textbox for error threshold
	else{
		// Change button appearance and deactivate textbox
		btn_filter_setchange.state = 0;
		btn_filter_setchange.text = "Change";
		tbx_error_threshold_order.mytag = 0;

	}

}


void menu_display_static_filterset(void){
	// Set configuration for current menu
	TFT_setMenu(menu_filterset.index);

	// Set Color
	TFT_setColor(1, BLACK, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);

	/// Write the static part of the Graph to the display list
	TFT_graph_static(1, &gph_filterset);

	TFT_textbox_static(1, &tbx_filter_order);
	TFT_textbox_static(1, &tbx_error_threshold_order);
}
void menu_display_filterset(void){
	/// Menu specific display code. This will run if the corresponding menu is active and the main tft_display() is called.
	/// This menu ...


	// If error threshold is in edit mode show current sensor value
	if(tbx_error_threshold_order.mytag != 0 && tbx_error_threshold_order.active == 0 ){
		// Save current senosr value
		*tbx_error_threshold_order.numSrc.intSrc = (int_buffer_t)filterset_sens->bufFilter[filterset_sens->bufIdx];
	}

	//// Get highest y value and set graph axis boundaries
	/// Get biggest y-value
	float cur_y_max = 0;
	// On every leap-over of the buffer reevaluate all values (sets the cur_y_max bigger or lower!)
	if(filterset_sens->bufIdx == 0){
		for(int16_t i = filterset_sens->bufMaxIdx; i >= 0; i--)
			if(filterset_sens->bufRaw[i] > cur_y_max)
				cur_y_max = (float)filterset_sens->bufRaw[i];


		// Set axis bounds
		gph_filterset.y_max = gph_filterset.amp_max = cur_y_max;
		// Refresh static part of display (graph static part changed)
		TFT_setMenu(-1);

	}
	// In every other case only check if the current value is higher than last highest (sets cur_y_max only higher and only when needed)
	else{
		cur_y_max = filterset_sens->bufRaw[filterset_sens->bufIdx];

		/// Change graph if necessary
		if(cur_y_max >= gph_filterset.y_max){
			// Set axis bounds
			gph_filterset.y_max = gph_filterset.amp_max = cur_y_max;
			// Refresh static part of display (graph static part changed)
			TFT_setMenu(-1);
		}
	}





	/////////////// GRAPH
	///// Print dynamic part of the Graph (data & marker)
	// Current data points and trace
	TFT_graph_pixeldata_i(&gph_filterset, filterset_sens->bufRaw, filterset_sens->bufMaxIdx, &filterset_sens->bufIdx, GRAPH_DATA2COLORLIGHT);
	TFT_graph_pixeldata_f(&gph_filterset, filterset_sens->bufFilter, filterset_sens->bufMaxIdx, &filterset_sens->bufIdx, GRAPH_DATA2COLOR);

	/// Draw Banner and divider line on top
	TFT_header(1, &menu_filterset);

	// Set button color for header
	TFT_setColor(1, MAIN_BTNTXTCOLOR, MAIN_BTNCOLOR, MAIN_BTNCTSCOLOR, MAIN_BTNGRDCOLOR);
	// Buttons
	TFT_control(&btn_back); //	 - return from submenu
	TFT_control(&btn_filter_down); //	 - change curve fit function
	TFT_control(&btn_filter_up);
	TFT_control(&btn_filter_setchange);

	// Data point controls
	TFT_textbox_display(&tbx_error_threshold_order);
	TFT_textbox_display(&tbx_filter_order);

	// Header labels
	TFT_label(1, &lbl_filterset);

}
void menu_touch_filterset(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y){
	/// Menu specific touch code. This will run if the corresponding menu is active and the main tft_touch() registers an unknown tag value
	/// Do not use predefined TAG values! See tft.c "TAG ASSIGNMENT"!


	// Determine which tag was touched
	switch(tag)
	{
		// BUTTON BACK
		case BTN_BACK_TAG:
			if(*toggle_lock == 0) {
				printf("Button Back\n");
				*toggle_lock = 42;

				// Reset filter order
				//filterset_sens->avgFilterOrder = filterset_previousAvgFilterOrder;
				printf("Reseting avg filter order back to %d!\n", filterset_sens->avgFilterOrder);
				// Do a clean filter value calculation to sync it to the new filter order
				measure_movAvgFilter_clean(filterset_sens);

				// Store current error threshold to be used (filter order is changed direct)
				filterset_sens->errorThreshold = *tbx_error_threshold_order.numSrc.intSrc;

				// Write Spec file
				record_writeSpecFile(filterset_sens, tbx_act.numSrc.floatSrc, tbx_nom.numSrc.floatSrc, DP_size);

				// Free allocated memory
				free(tbx_act.numSrc.floatSrc);
				free(tbx_nom.numSrc.floatSrc);

				// Change menu
				TFT_setMenu(menu_2setup1.index);
			}
			break;
		case BTN_FILTER_DOWN_TAG:
			if(*toggle_lock == 0) {
				printf("Button down\n");
				*toggle_lock = 42;

				// If the filter isn't at the limits...
				if(filterset_sens->avgFilterOrder > 0){
					// Decrease current order
					filterset_sens->avgFilterOrder--;

					// Do a clean filter value calculation to sync it
					measure_movAvgFilter_clean(filterset_sens);
				}
			}
			break;
		case BTN_FILTER_UP_TAG:
			if(*toggle_lock == 0) {
				printf("Button up\n");
				*toggle_lock = 42;

				// If the filter isn't at the limits...
				if(filterset_sens->avgFilterOrder < 254){
					// Increase current order
					filterset_sens->avgFilterOrder++;

					// Do a clean filter value calculation to sync it
					measure_movAvgFilter_clean(filterset_sens);
				}
			}
			break;
		case BTN_FILTER_SETCHANGE_TAG:
			if(*toggle_lock == 0) {
				printf("Button set/change\n");
				*toggle_lock = 42;

				// Change from view to edit mode of current data point
				if(btn_filter_setchange.state == EVE_OPT_FLAT)
					filterset_setEditMode(0);
				else
					filterset_setEditMode(1);

				// Refresh static part of display (e.g. to show new textbox background when one changes from read-only to read-write)
				TFT_setMenu(-1);
			}
			break;
		case TBX_FILTER_ERROR_THRESHOLD_TAG:
				if(*toggle_lock == 0) {
					printf("Textbox error_threshold\n");
					*toggle_lock = 42;

					// Activate Keypad and set cursor to end
					TFT_textbox_setStatus(&tbx_error_threshold_order, 1, -1);
				}
				break;
		default:
			// Handle textbox and if an OK was pressed an the active keypad stop cell from being updated
			if(TFT_textbox_touch(&tbx_error_threshold_order))
				filterset_setEditMode(0);

			break;
	}
}
