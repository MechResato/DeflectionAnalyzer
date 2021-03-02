/*
@file    		tft.c
@brief   		Implementation of display communication using the EVE Library. Meant to display a menu and a dynamic graph (implemented for XMC4700 and DAVE)
@version 		2.0 (base lib version was 1.13)
@date    		2020-09-05
@initialauthor  Rudolph Riedel
@author 		RS (Rene Santeler & Stefan Reinmüller) @ MCI 2020/21 (initially created by Rudolph Riedel, completely reworked by RS )

@section History
2.0 (adapted from Rudolph Riedel base version 1.13 - below changes are from RS 2020/21)
- Added color scheme, adaptable banner, dynamic graph implementation (TFT_GraphStatic & TFT_GraphData), a display init which adds the static part of a graph to static DL (initStaticGraphBackground), a display init to show a bitmap (TFT_display_init_screen), ...
- Adapted TFT_init to only do the most necessary thins for init (no static DL creation! you need to call one afterwards before using TFT_display!)

// See https://brtchip.com/eve-toolchains/ for helpful Tools
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <globals.h>
#include "EVE.h"
#include "tft_data.h"
#include "menu.h"

// Memory-map definitions
#define MEM_LOGO 0x00000000 // start-address of logo, needs about 20228 bytes of memory. Will be written by TFT_display_init_screen and overwritten by initStaticGraphBackground (and everything else...)
#define MEM_DL_STATIC (EVE_RAM_G_SIZE - 4096) // 0xff000 -> Start-address of the static part of the display-list. Defined as the upper 4k of gfx-mem (WARNING: If static part gets bigger than 4k it gets clipped. IF dynamic part gets so big that it touches the last 4k (1MiB-4k) it overwrites the static part)
uint32_t num_dl_static; // amount of bytes in the static part of our display-list


/////////// General Variables
uint8_t tft_active = 0;  // Prevents TFT_display of doing anything if EVE_init isn't successful of TFT_init wasn't called

// Current menu display function pointer - At the end of the TFT_display() the function referenced to this pointer is executed
int8_t TFT_cur_Menu = 0; // Used as index of currently used menu (TFT_display,TFT_touch)
int8_t TFT_last_Menu = -1; // Used as index of last used menu (TFT_display_static). If this differs from TFT_cur_Menu the initial TFT_display_static function of the menu is executed. Also helpful to determine what was the last menu during the TFT_display_static.

void (*TFT_display_cur_Menu__fptr_arr[])(void) = {
		&TFT_display_menu0,
		&TFT_display_menu1
};

void (*TFT_touch_cur_Menu__fptr_arr[])(uint8_t tag, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y) = {
		&TFT_touch_menu0,
		&TFT_touch_menu1
};

void (*TFT_display_static_cur_Menu__fptr_arr[])(void) = {
		&TFT_display_static_menu0,
		&TFT_display_static_menu1
};

#define TFT_MENU_SIZE (sizeof(TFT_display_cur_Menu__fptr_arr) / sizeof(*(TFT_display_cur_Menu__fptr_arr)))


/////////// Swipe feature (TFT_touch)
enum SwipeDetection{None, Up, Down, Left, Right};
enum SwipeDetection swipeDetect = None;
uint8_t swipeInProgress = 0;
uint8_t swipeEvokedBy = 0; 			  // The tag that initiated the swipe (needed to run an action based on which element was initially touched when the swipe began)
int32_t swipeInitialTouch_X = 32768; // X position of the initial touch of an swipe
int32_t swipeInitialTouch_Y = 32768;
int32_t swipeDistance_X = 0;		  // Distance (in px) between the initial touch and the current position of an swipe
int32_t swipeDistance_Y = 0;
uint8_t swipeEndOfTouch_Debounce = 0; // Counts the number of successive cycles without an touch (tag 0). Used to determine when an swipe is finished

/////////// Button states
extern uint8_t toggle_lock; // "Debouncing of touches" -> If something is touched, this is set to prevent button evaluations. As soon as the is nothing pressed any more this is reset to 0

/////////// Keypad feature (TFT_touch)
uint8_t keypadActive = 0;
uint8_t keypadShiftActive = 0; // Determines the shown set of characters
uint8_t keypadCurrentKey = 0; // Integer value (tag) of the currently pressed key. The function that uses it must reset it to 0!









void TFT_GraphStatic(uint8_t burst, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t padding, double amp_max, double t_max, double h_grid_lines, double v_grid_lines){
	/// Write the non-dynamic parts of an Graph to the TFT (axes & labels, grids and values, axis-arrows but no data). Can be used once during initStaticBackground() or at recurring display list build in TFT_display() completely coded by RS 02.01.2021.
	///
	///  burst	... determines if the normal or the burst version of the EVE Library is used to transmit DL command (0 = normal, 1 = burst). In full Pixels
	///  x		...	beginning of left edge of the graph (Note that the vertical axis starts at "x+padding" and that some Grid values might be at a position prior to x). In full Pixels
	///  y		... beginning of upper edge of the graph (Note this is the position of the axis-arrow point and that the horizontal axis label might be at a position prior to y). In full Pixels
	///  width	... width of the actual graph data area in full Pixels
	///  height	... height of the actual graph data area in full Pixels
	///  padding ... clearance from the outer corners (x,y) to the axes
	///  amp_max ... maximum represented value of amplitude (e.g. 10 Volts), will be used at 100% horizontal line
	///  t_max 	 ... maximum represented value of time (e.g. 2.2 Seconds), will be used at 100% horizontal line
	///  h_grid_lines ... number of horizontal grid lines
	///  v_grid_lines ... number of vertical grid lines
	///  Note: The predefined GRAPH_AXISCOLOR and GRAPH_GRIDCOLOR are used directly!

	// Define a array of function pointers for every used "EVE_cmd_dl..." function. First one is normal, second one is to be used with burst mode
	void (*EVE_cmd_dl__fptr_arr[])(uint32_t) = {EVE_cmd_dl, EVE_cmd_dl_burst};
	void (*EVE_cmd_text__fptr_arr[])(int16_t, int16_t, int16_t, uint16_t, const char*) = {EVE_cmd_text, EVE_cmd_text_burst};
	void (*EVE_cmd_number__fptr_arr[])(int16_t, int16_t, int16_t, uint16_t, int32_t) = {EVE_cmd_number, EVE_cmd_number_burst};


	// Internal offsets and sizes
	const uint8_t arrow_offset = 5;   // Offset of the ARROW HEAD corners

	const uint8_t grid_linewidth = 9; // linewidth of the grid in 1/16 pixel (16=1px)

	#define axis_lbl_txt_size 26 	  // Font for axis labels
	#define grid_lbl_txt_size 20 	  // Font for grid labels

	const uint8_t h_ax_lbl_comp_x = 6;  // Offset used to print the horizontal axis label at the right position (text width compensation)
	const uint8_t h_ax_lbl_comp_y = 20; // Offset used to print the horizontal axis label at the right position (text height compensation)
	const uint8_t v_ax_lbl_comp_x = 2;  // Offset used to print the vertical axis label at the right position (text width compensation)
	const uint8_t v_ax_lbl_comp_y = 22; // Offset used to print the vertical axis label at the right position (text height compensation)

	const uint8_t h_grid_lbl_comp_x = 3;  // Offset used to print the horizontal grid labels (numbers) at the right position (text width compensation)
	const uint8_t h_grid_lbl_comp_y = 10; // Offset used to print the horizontal grid labels (numbers) at the right position (text height compensation)
	const uint8_t v_grid_lbl_comp_x = 7;  // Offset used to print the vertical grid labels (numbers) at the right position (text width compensation)
	const uint8_t v_grid_lbl_comp_y = 0;  // Offset used to print the vertical grid labels (numbers) at the right position (text height compensation)

	/// Calculate pixels between lines and labels of the grid
	// Used by grid lines and labels (space between them)
	double widthPerSection = (double)(width)/v_grid_lines;
	double heightPerSection = (double)(height)/h_grid_lines;

	/// Axes LABELS
	(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_AXISCOLOR);
	(*EVE_cmd_text__fptr_arr[burst])(x + padding         + h_ax_lbl_comp_x, y + padding          - h_ax_lbl_comp_y, axis_lbl_txt_size, 0, "V");
	(*EVE_cmd_text__fptr_arr[burst])(x + padding + width + v_ax_lbl_comp_x, y + padding + height - v_ax_lbl_comp_y, axis_lbl_txt_size, 0, "t");

	/// AXES lines
	//(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_AXISCOLOR);
	(*EVE_cmd_dl__fptr_arr[burst])(DL_BEGIN | EVE_LINES);
	// left vertical line (Amplitude)
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(x + padding, y));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(x + padding, y + padding + height + padding));
	// bottom horizontal line (Time)
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(x                            , y + padding + height ));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(x + padding + width + padding, y + padding + height ));

	/// GRID lines
	(*EVE_cmd_dl__fptr_arr[burst])(LINE_WIDTH(grid_linewidth)); /* size is in 1/16 pixel */
	(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_GRIDCOLOR);
	// vertical grid
	for(int i=1; i<=(int)floor(v_grid_lines); i++){
		// y-position at upper and lower corner; x-position from left with padding and width of graph divided by number of gridlines - times current line
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(x + padding + (uint16_t)(widthPerSection*(double)i), y + padding ));
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(x + padding + (uint16_t)(widthPerSection*(double)i), y + padding + height ));
	}
	// horizontal grid
	for(int i=1; i<=(int)floor(h_grid_lines); i++){
		// x-position at left and right corner; y-position from top y, padding and height divided by number of gridlines - times current line
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(x + padding        , y + padding + height - (uint16_t)(heightPerSection*(double)i) ));
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(x + padding + width, y + padding + height - (uint16_t)(heightPerSection*(double)i) ));
	}
	(*EVE_cmd_dl__fptr_arr[burst])(DL_END);

	/// Grid VALUES
	(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_AXISCOLOR);
	// vertical grid (time)
	for(int i=1; i<=(int)ceil(v_grid_lines); i++){ // "ceil" and "i-1" at val -> print also the 0 value
		// Calc time at current vertical line
		double val = (t_max/v_grid_lines*(double)(i-1));

		// If its a pure integer write it as number, else convert and write it to string
		if((val - (double)((uint32_t)val)) == 0){ //val % 1.0 == 0
			(*EVE_cmd_number__fptr_arr[burst])(x + padding + (uint16_t)(widthPerSection*(double)(i-1)) + h_grid_lbl_comp_x, y + height + h_grid_lbl_comp_y, grid_lbl_txt_size, 0, (int32_t)val); //EVE_OPT_RIGHTX| + 18
		}
		else{
			char buffer[32]; // buffer for double to string conversion
			sprintf(buffer, "%.1lf", val); // double to string conversion
			(*EVE_cmd_text__fptr_arr[burst])(x + padding + (uint16_t)(widthPerSection*(double)(i-1)) + h_grid_lbl_comp_x, y + height + h_grid_lbl_comp_y, grid_lbl_txt_size, 0, buffer);
		}
	}
	// horizontal grid (amplitude)
	//(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_AXISCOLOR);
	for(int i=1; i<=(int)floor(h_grid_lines); i++){  // "floor" and "i" at val -> don't print the 0 value
		// Calc amplitude at current horizontal line
		double val = (amp_max/h_grid_lines*(double)i);

		// If its a pure integer write it as number, else convert and write it to string
		if((val - (double)((uint32_t)val)) == 0){ //val % 1.0 == 0
			(*EVE_cmd_number__fptr_arr[burst])(x - v_grid_lbl_comp_x, y + padding + height - (uint16_t)(heightPerSection*(double)i) + v_grid_lbl_comp_y, grid_lbl_txt_size, 0, (int32_t)val); //EVE_OPT_RIGHTX|
		}
		else{
			char buffer[32]; // buffer for double to string conversion
			sprintf(buffer, "%.1lf", val); // double to string conversion
			(*EVE_cmd_text__fptr_arr[burst])(x - v_grid_lbl_comp_x, y + padding + height - (uint16_t)(heightPerSection*(double)i) + v_grid_lbl_comp_y, grid_lbl_txt_size, 0, buffer);
		}
	}

	/// ARROWS on axes
	//(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_AXISCOLOR);
	// bottom vertical arrow (Amplitude)
	(*EVE_cmd_dl__fptr_arr[burst])(DL_BEGIN | EVE_LINE_STRIP);
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(x + padding + arrow_offset, y + arrow_offset ));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(x + padding               , y                ));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(x + padding - arrow_offset, y + arrow_offset ));
	(*EVE_cmd_dl__fptr_arr[burst])(DL_END);
	// bottom horizontal arrow (Time)
	(*EVE_cmd_dl__fptr_arr[burst])(DL_BEGIN | EVE_LINE_STRIP);
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(x + padding + width + padding - arrow_offset, y + padding + height + arrow_offset ));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(x + padding + width + padding               , y + padding + height                ));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(x + padding + width + padding - arrow_offset, y + padding + height - arrow_offset ));
	(*EVE_cmd_dl__fptr_arr[burst])(DL_END);

}

void TFT_GraphData(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t padding, double y_max, XMC_VADC_RESULT_SIZE_t SBuffer[], uint16_t size, uint16_t *SBuffer_curidx, uint8_t graphmode, uint32_t datacolor, uint32_t markercolor){
	/// Write the dynamic parts of an Graph to the TFT (data and markers). Used at recurring display list build in TFT_display() completely coded by RS 02.01.2021.
	///
	///  x		...	beginning of left edge of the graph (Note that the vertical axis starts at "x+padding" and that some Grid values might be at a position prior to x). In full Pixels
	///  y		... beginning of upper edge of the graph (Note this is the position of the axis-arrow point and that the horizontal axis label might be at a position prior to y). In full Pixels
	///  width	... width of the actual graph data area in full Pixels
	///  height	... height of the actual graph data area in full Pixels
	///  padding	...	clearance from the outer corners (x,y) to the axes
	///  y_max   ... maximum expected value of input (e.g. for 12bit ADC 4095), will represent 100%
	///  SBuffer[] 		...	Array of data values
	///  size	 		... size of array of data values
	///  *SBuffer_curidx ... current position (index of most recent value)
	///  graphmode 		... 0 = frame-mode, 1 = roll-mode
	///  datacolor 		... 24bit color (as 32 bit integer with leading 0's) used for the dataline
	///  markercolor		... 24bit color (as 32 bit integer with leading 0's) used for the current position line
	///  Note: No predefined graph settings are used direct (#define ...)!



	/// Display current DATA as line strip in frame or roll mode
	EVE_cmd_dl_burst(DL_COLOR_RGB | datacolor);
	EVE_cmd_dl_burst(DL_BEGIN | EVE_LINE_STRIP);
	/// Display graph frame-mode
	if(graphmode == 0){
		// Print values in the order they are stored
		for (int x_cur = 0; x_cur < size; ++x_cur) {
			EVE_cmd_dl_burst(VERTEX2F(x + padding + x_cur, y + padding + height - (uint16_t)(( ((double)SBuffer[x_cur]) / ((double)y_max) )*(double)(height)) )); //if(frameover==1) printf("%lf %lf\n", ((((double)(SBuffer[x_cur]))/((double)y_max))*(double)(height)), (double)SBuffer[x]);
		}
	}
	/// Display graph roll-mode
	else {
		// Print newest value always at the rightmost corner with all older values to the right
		// => Start Display    x position at rightmost corner and decrement till 0 (last run will make it -1 at the end but it isnt used after that)
		// => Start Arrayindex i at current index and decrement every loop. If i goes below 0, reset to max index and decrement further till
		//    value before current is hit.

		int16_t i = *SBuffer_curidx;
		for (int16_t x_cur = size-1; x_cur >= 0; x_cur--) {
			// if index goes below 0 set to highest buffer index
			if(i < 0){i = size-1;}

			// Send next point for EVE_LINE_STRIP at current x+padding and normalized buffer value
			EVE_cmd_dl_burst(VERTEX2F(x + padding + x_cur, y + padding + height - (uint16_t)(( ((double)SBuffer[i]) / ((double)y_max) )*(double)(height)) )); 				// EVE_cmd_dl_burst(VERTEX2F(x + padding + x_cur, EVE_VSIZE - ((uint16_t)(SBuffer[i]/y_div) + margin + padding)));

			// decrement index
			i--;
		}
	}
	// End EVE_LINE_STRIP and therefore DATA
	EVE_cmd_dl_burst(DL_END);


	/// Draw current POSITION MARKER in frame mode
	if(graphmode == 0){
		EVE_cmd_dl_burst(DL_COLOR_RGB | markercolor);
		EVE_cmd_dl_burst(DL_BEGIN | EVE_LINE_STRIP);
		EVE_cmd_dl_burst(VERTEX2F(x + padding + *SBuffer_curidx, y + padding - 5 ));
		EVE_cmd_dl_burst(VERTEX2F(x + padding + *SBuffer_curidx, y + padding + height + 5 ));
		EVE_cmd_dl_burst(DL_END);
	}
	/////////////// GRAPH END

}


void touch_calibrate(void) {
// Sends pre-recorded touch calibration values, depending on the display the code is compiled for. Created by Rudolph Riedel, adapted by RS @ MCI 2020/21
#if defined (EVE_RiTFT43)
	// Original library values
	//EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000062cd);
	//EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0xfffffe45);
	//EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff45e0a);
	//EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x000001a3);
	//EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00005b33);
	//EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xFFFbb870);

	// Actual values (measured with routine down below) by Rene S. at 20.12.2020 on Display SM-RVT43ULBNWC01 2031/20/609 00010
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000061c4);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x0000025d);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff14ab1);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0xffffff91);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00005b6b);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xFFFac7ab);
#endif

// All targets other than EVE_RiTFT43 where deleted -> see original lib 4

/* activate this if you are using a module for the first time or if you need to re-calibrate it */
/* write down the numbers on the screen and either place them in one of the pre-defined blocks above or make a new block */
#if 0
	/* calibrate touch and displays values to screen */
	EVE_cmd_dl(CMD_DLSTART);
	EVE_cmd_dl(DL_CLEAR_RGB | BLACK);
	EVE_cmd_dl(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG);
	EVE_cmd_text((EVE_HSIZE/2), 50, 26, EVE_OPT_CENTER, "Please tap on the dot.");
	EVE_cmd_calibrate();
	EVE_cmd_dl(DL_DISPLAY);
	EVE_cmd_dl(CMD_SWAP);
	EVE_cmd_execute();

	uint32_t touch_a, touch_b, touch_c, touch_d, touch_e, touch_f;

	touch_a = EVE_memRead32(REG_TOUCH_TRANSFORM_A);
	touch_b = EVE_memRead32(REG_TOUCH_TRANSFORM_B);
	touch_c = EVE_memRead32(REG_TOUCH_TRANSFORM_C);
	touch_d = EVE_memRead32(REG_TOUCH_TRANSFORM_D);
	touch_e = EVE_memRead32(REG_TOUCH_TRANSFORM_E);
	touch_f = EVE_memRead32(REG_TOUCH_TRANSFORM_F);

	EVE_cmd_dl(CMD_DLSTART);
	EVE_cmd_dl(DL_CLEAR_RGB | BLACK);
	EVE_cmd_dl(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG);
	EVE_cmd_dl(TAG(0));

	EVE_cmd_text(5, 15, 26, 0, "TOUCH_TRANSFORM_A:");
	EVE_cmd_text(5, 30, 26, 0, "TOUCH_TRANSFORM_B:");
	EVE_cmd_text(5, 45, 26, 0, "TOUCH_TRANSFORM_C:");
	EVE_cmd_text(5, 60, 26, 0, "TOUCH_TRANSFORM_D:");
	EVE_cmd_text(5, 75, 26, 0, "TOUCH_TRANSFORM_E:");
	EVE_cmd_text(5, 90, 26, 0, "TOUCH_TRANSFORM_F:");

	EVE_cmd_setbase(16L);
	EVE_cmd_number(310, 15, 26, EVE_OPT_RIGHTX|8, touch_a);
	EVE_cmd_number(310, 30, 26, EVE_OPT_RIGHTX|8, touch_b);
	EVE_cmd_number(310, 45, 26, EVE_OPT_RIGHTX|8, touch_c);
	EVE_cmd_number(310, 60, 26, EVE_OPT_RIGHTX|8, touch_d);
	EVE_cmd_number(310, 75, 26, EVE_OPT_RIGHTX|8, touch_e);
	EVE_cmd_number(310, 90, 26, EVE_OPT_RIGHTX|8, touch_f);

	EVE_cmd_dl(DL_DISPLAY);	/* instruct the graphics processor to show the list */
	EVE_cmd_dl(CMD_SWAP); /* make this list active */
	EVE_cmd_execute();

	while(1);
#endif
}

uint8_t TFT_init(void)
{
	/// Initializes EVE (or checks if its already initialized). Only at first sucessful EVE_init() the tft is set active, backlight is set to medium brightness, the pre-elaborated touch calibration is loaded and the static Background is initiated. Created by Rudolph Riedel, adapted by RS @ MCI 2020/21
	if(EVE_init() != 0)
	{
		#if defined (DEBUG_ENABLE)
			printf("TFT_init - EVE_init was ok, starting to init TFT!\n");
		#endif
		// Mark Display as active (TFT_display only does anythin after his)
		tft_active = 1;

		// Initial Backlight strength
		EVE_memWrite8(REG_PWM_DUTY, 0x30);	/* setup backlight, range is from 0 = off to 0x80 = max */

		// Write prerecorded touchscreen calibration back to display
		touch_calibrate();

		// Clear screen, set precision for VERTEX2F to 1 pixel and show DL for the first time
		EVE_start_cmd_burst(); /* start writing to the cmd-fifo as one stream of bytes, only sending the address once */
		EVE_cmd_dl_burst(CMD_DLSTART); /* start the display list */
		EVE_cmd_dl_burst(TAG(0)); /* do not use the following objects for touch-detection */
		EVE_cmd_bgcolor_burst(MAIN_BGCOLOR);
		EVE_cmd_dl_burst(VERTEX_FORMAT(0)); /* reduce precision for VERTEX2F to 1 pixel instead of 1/16 pixel default */
		EVE_cmd_dl_burst(DL_CLEAR_RGB | WHITE); /* set the default clear color to white */
		EVE_cmd_dl_burst(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG); /* clear the screen - this and the previous prevent artifacts between lists, Attributes are the color, stencil and tag buffers */
		EVE_cmd_dl_burst(DL_DISPLAY); /* instruct the graphics processor to show the list */
		EVE_cmd_dl_burst(CMD_SWAP); /* make this list active */
		EVE_end_cmd_burst(); /* stop writing to the cmd-fifo, the cmd-FIFO will be executed automatically after this or when DMA is done */
		return 1;
	}
	else{
		return 0;
	}
}

void TFT_display_init_screen(void)
{
	/// Display logo over full screen
	if(tft_active != 0)
	{
		EVE_cmd_inflate(MEM_LOGO, logo_init, sizeof(logo_init)); /* load logo into gfx-memory and de-compress it */

		EVE_start_cmd_burst(); /* start writing to the cmd-fifo as one stream of bytes, only sending the address once */

		EVE_cmd_dl_burst(CMD_DLSTART); /* start the display list */
		//EVE_cmd_dl_burst(VERTEX_FORMAT(0)); /* reduce precision for VERTEX2F to 1 pixel instead of 1/16 pixel default */
		EVE_cmd_dl_burst(DL_CLEAR_RGB | WHITE); /* set the default clear color to white */
		EVE_cmd_dl_burst(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG); /* clear the screen - this and the previous prevent artifacts between lists, Attributes are the color, stencil and tag buffers */
		EVE_cmd_dl_burst(TAG(0));

		EVE_cmd_setbitmap_burst(MEM_LOGO, EVE_ARGB1555, 308, 250);
		EVE_cmd_dl_burst(DL_BEGIN | EVE_BITMAPS);
		EVE_cmd_dl_burst(VERTEX2F(86*16, 11*16));  //105*16
		EVE_cmd_dl_burst(DL_END);


		EVE_cmd_dl_burst(DL_DISPLAY); /* instruct the graphics processor to show the list */
		EVE_cmd_dl_burst(CMD_SWAP); /* make this list active */

		EVE_end_cmd_burst(); /* stop writing to the cmd-fifo, the cmd-FIFO will be executed automatically after this or when DMA is done */
	}
}



void TFT_display_static(void)
{
	// Static portion of display-handling, meant to be called once at startup. Created by Rudolph Riedel, adapted by RS @ MCI 2020/21
	EVE_cmd_dl(CMD_DLSTART); // Start a new display list (resets REG_CMD_DL to 0)

	/// Main settings
	EVE_cmd_dl(TAG(1)); /* give everything considered background area tag 1 -> used for wipe feature*/
	EVE_cmd_bgcolor(MAIN_BGCOLOR); /* light grey */
	EVE_cmd_dl(VERTEX_FORMAT(0)); /* reduce precision for VERTEX2F to 1 pixel instead of 1/16 pixel default */
	// Main Rectangle
	EVE_cmd_dl(DL_COLOR_RGB | MAIN_BGCOLOR);
	EVE_cmd_dl(DL_BEGIN | EVE_RECTS);
	EVE_cmd_dl(VERTEX2F(0, 0));
	EVE_cmd_dl(VERTEX2F(EVE_HSIZE, EVE_VSIZE));
	EVE_cmd_dl(DL_END);


	/////////////// Execute current menu specific code
	(*TFT_display_static_cur_Menu__fptr_arr[TFT_cur_Menu])();



	// Wait for Display to be finished
	while (EVE_busy());

	// Get size of the current display list
	num_dl_static = EVE_memRead16(REG_CMD_DL); // REG_CMD_DL = Command display list offset

	// Copy "num_dl_static" bytes from pointer "EVE_RAM_DL" to pointer "MEM_DL_STATIC"
	EVE_cmd_memcpy(MEM_DL_STATIC, EVE_RAM_DL, num_dl_static);
	while (EVE_busy());

	// The menu is now established and can be set as last known menu
	TFT_last_Menu = TFT_cur_Menu;
}

void TFT_touch(void)
{
	/// Check for touch events and setup vars for TFT_display() (Buttons). Created by Rudolph Riedel, adapted by RS @ MCI 2020/21
	// Init vars
	uint8_t tag; // temporary store of received touched tag
	
	// If Display is still busy, skip this evaluation to prevent hanging, glitches and flickers
	if(EVE_busy()) { return; } /* is EVE still processing the last display list? */

	// Read the value for the first touch point
	tag = EVE_memRead8(REG_TOUCH_TAG);
	// Read currently touched position in pixels
	uint32_t TouchXY = EVE_memRead32(REG_TOUCH_SCREEN_XY); // Currently touched X and Y coordinate from register (1.byte=X, 2.byte=Y)
	uint16_t Y = TouchXY;
	uint16_t X = TouchXY >> 16;

	/// If a swipe is in progress save start coordinates (once at start), calculate swipe distance and detect the end-of-touch (debounced).
	/// A swipe gets started by a touch tag action (switch statement below) and lasts till no touch was detected for at least 5 executions (debounce).
	/// At an end-of-touch the evoking tag is run one again so it can execute final actions.
	if(swipeInProgress){
		// If the initial touched position is not set, remember it (used to determine how far the user swiped)
		if(swipeInitialTouch_X == 32768 && swipeInitialTouch_Y == 32768){
			swipeInitialTouch_X = X;
			swipeInitialTouch_Y = Y;
		}

		// Update the currently swiped distance and its direction if the background is still touched (tag==1) and the positions are valid
		// Note a positive effect: If the tag somehow changes or the position get invalid (happens at corners) the last valid distances and effects survive
		if(X < 32768 && Y < 32768){
			swipeDistance_X = swipeInitialTouch_X - X;
			swipeDistance_Y = swipeInitialTouch_Y - Y;
		}

		// Increase end-of-touch debounce timer
		if(tag == 0) swipeEndOfTouch_Debounce++;

		// Detect end-of-touch (Swipe ends if there was no touch detected for 5 cycles)
		if(swipeEndOfTouch_Debounce >= 5){
			// Execute the tag action that invoked the swipe process one last time
			tag = swipeEvokedBy;

			// Reset swipe feature variables
			swipeInProgress = 0;
			swipeEndOfTouch_Debounce = 0;
			swipeInitialTouch_X = 32768;
			swipeInitialTouch_Y = 32768;
		}
	}

	//
	if(keypadActive && tag >= 33){
		keypadCurrentKey = tag;
	}

	// Execute action based on touched tag
	switch(tag)
	{
		// nothing touched - reset states and locks
		case 0:
			toggle_lock = 0;

			// If an unprocessed keypadCurrentKey is registered and the key was released (which is why he landed here) execute current menu specific code to handle the input
			if(keypadCurrentKey){
				// Shift was pressed - change shift-active and reset current-key
				if(keypadCurrentKey == 94){
					keypadShiftActive = !keypadShiftActive;
					keypadCurrentKey = 0;
				}
				else{
					(*TFT_touch_cur_Menu__fptr_arr[TFT_cur_Menu])(tag, swipeInProgress, &swipeEvokedBy, &swipeDistance_X, &swipeDistance_Y);
				}
			}
			break;
		// Background elements are touched - detect swipes to left/right for menu changes
		case 1:
			// Deactivate Keypad
			keypadActive = 0;

			// Init a new swipe - if it isn't already running (and no end-of-touch of a previous swipe is detected)
			if(swipeInProgress == 0 && swipeEvokedBy == 0){
				// Initial touch on background was detected - init swipe and mark me as elicitor
				swipeInProgress = 1;
				swipeEvokedBy = 1;
			}
			// Evaluate current status of the swipe - if it is in progress and evoked by me
			else if(swipeInProgress == 1 && swipeEvokedBy == 1){
				// If the user swiped more on x than on y-axis he probably wants to swipe left/right
				if(abs(swipeDistance_X) > abs(swipeDistance_Y)){
					if(swipeDistance_X > 50)      	// swipe to left
						swipeDetect = Left;
					else if(swipeDistance_X < -50)	// swipe to right
						swipeDetect = Right;
					else
						swipeDetect = None;
				}
			}
			// Final actions after end-of-touch was detected - if the swipe is not in progress but swipeEvokedBy is still on me
			else if(swipeInProgress == 0 && swipeEvokedBy == 1){
				// Change menu if swipe was detected
				if(swipeDetect == Left && TFT_cur_Menu < (TFT_MENU_SIZE-1)) TFT_cur_Menu++;
				else if(swipeDetect == Right && TFT_cur_Menu > 0) TFT_cur_Menu--;

				// Finalize swipe by resetting swipeEvokedBy
				swipeEvokedBy = 0;
			}
			break;
		default:
			// Execute current menu specific code
			(*TFT_touch_cur_Menu__fptr_arr[TFT_cur_Menu])(tag, swipeInProgress, &swipeEvokedBy, &swipeDistance_X, &swipeDistance_Y);
			break;
	}

	//printf("%d %d %d %d-%d\n", swipeInProgress, (int)swipeDetect, TFT_cur_Menu, swipeInitialTouch_X, X);
}

void TFT_display(void)
{
	/// Dynamic portion of display-handling, meant to be called every 20ms or more. Created by Rudolph Riedel, extensively adapted by RS @ MCI 2020/21
	///

	if(tft_active != 0)
	{
		// Setup static part of the current menu - only needed once when the menu is changed
		if(TFT_last_Menu != TFT_cur_Menu)
			TFT_display_static();

		// Debug
		#if defined (EVE_DMA)
			uint16_t cmd_fifo_size;
			cmd_fifo_size = EVE_dma_buffer_index*4; /* without DMA there is no way to tell how many bytes are written to the cmd-fifo */
		#endif

		// Get values from display before burst starts (is not possible during!)
		TFT_display_get_values();


		// Start Burst (start writing to the cmd-fifo as one stream of bytes, only sending the address once)
		EVE_start_cmd_burst();


		/////////////// Start the actual display list
		EVE_cmd_dl_burst(CMD_DLSTART);
		EVE_cmd_dl_burst(DL_CLEAR_RGB | WHITE); /* set the default clear color to white */
		EVE_cmd_dl_burst(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG); /* clear the screen - this and the previous prevent artifacts between lists, Attributes are the color, stencil and tag buffers */
		EVE_cmd_dl_burst(TAG(0));

		// Insert static part of display-list from copy in gfx-mem
		EVE_cmd_append_burst(MEM_DL_STATIC, num_dl_static);


		/////////////// Execute current menu specific code
		(*TFT_display_cur_Menu__fptr_arr[TFT_cur_Menu])();

		// Keypad
		if(keypadActive){
			// Background Rectangle
			EVE_cmd_dl_burst(DL_COLOR_RGB | GREY);
			EVE_cmd_dl_burst(DL_BEGIN | EVE_RECTS);
			EVE_cmd_dl_burst(VERTEX2F(0, EVE_VSIZE-2-21-(24*4)-2));
			EVE_cmd_dl_burst(VERTEX2F(EVE_HSIZE, EVE_VSIZE));
			EVE_cmd_dl_burst(DL_END);

			// Keys
			EVE_cmd_dl_burst(TAG(0));
			if(keypadShiftActive == 0){
				EVE_cmd_keys_burst(2, EVE_VSIZE-2-21-(24*4), EVE_HSIZE-4-50-4-4, 21, 26, keypadCurrentKey, "1234567890");
				EVE_cmd_keys_burst(2, EVE_VSIZE-2-21-(24*3), EVE_HSIZE-4, 21, 26, keypadCurrentKey, "qwertyuiop");
				EVE_cmd_keys_burst(2, EVE_VSIZE-2-21-(24*2), EVE_HSIZE-4, 21, 26, keypadCurrentKey, "asdfghjkl");
				EVE_cmd_keys_burst(2, EVE_VSIZE-2-21-(24*1), EVE_HSIZE-4, 21, 26, keypadCurrentKey, "^yxcvbnm._");
			}
			else{
				EVE_cmd_keys_burst(2, EVE_VSIZE-2-21-(24*4), EVE_HSIZE-4-50-4-4, 21, 26, keypadCurrentKey, "!\"%&/()=?\\");
				EVE_cmd_keys_burst(2, EVE_VSIZE-2-21-(24*3), EVE_HSIZE-4, 21, 26, keypadCurrentKey, "QWERTYUIOP");
				EVE_cmd_keys_burst(2, EVE_VSIZE-2-21-(24*2), EVE_HSIZE-4, 21, 26, keypadCurrentKey, "ASDFGHJKL");
				EVE_cmd_keys_burst(2, EVE_VSIZE-2-21-(24*1), EVE_HSIZE-4, 21, 26, keypadCurrentKey, "^ZXCVBNM:-");
			}
			char BS[] = "0";
			BS[0] = (char)46; // Backspace caracter TODO COMMENT

			EVE_cmd_fgcolor_burst(MAIN_BTNCTSCOLOR);
			EVE_cmd_bgcolor_burst(MAIN_BTNCOLOR);
			EVE_cmd_keys_burst(EVE_HSIZE-4-50, EVE_VSIZE-2-21-(24*4), 50, 21, 19, keypadCurrentKey, &BS);
			//EVE_cmd_keys_burst(EVE_HSIZE-4-21-4-21, EVE_VSIZE-2-21, 46, 21, 20, keypadCurrentKey, "._");
			//EVE_cmd_keys_burst(2, EVE_VSIZE-2-21, EVE_HSIZE-4, 21, 20, 0, "^ ._");
			//EVE_cmd_dl_burst(TAG(94));
			//EVE_cmd_button_burst(2, EVE_VSIZE-2-21, 90, 21, 20, 0, "^");
			//EVE_cmd_dl_burst(TAG(39));
			EVE_cmd_keys_burst(2+30+4+60, EVE_VSIZE-2-21, 288, 21, 20, keypadCurrentKey, " ");
			//EVE_cmd_button_burst(2+30+4+60, EVE_VSIZE-2-21, 288, 21, 20, 0, " ");

		}


		/////////////// Finish Display list and burst
		EVE_cmd_dl_burst(DL_DISPLAY); /* instruct the graphics processor to show the list */
		EVE_cmd_dl_burst(CMD_SWAP); /* make this list active */

		EVE_end_cmd_burst(); /* stop writing to the cmd-fifo, the cmd-FIFO will be executed automatically after this or when DMA is done */
	}
}
