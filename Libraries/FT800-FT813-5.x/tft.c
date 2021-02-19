/*
@file    		tft.c
@brief   		Implementation of display communication using the EVE Library. Meant to display a menu and a dynamic graph (implemented for XMC4700 and DAVE)
@version 		2.0 (base lib version was 1.13)
@date    		2020-09-05
@initialauthor  Rudolph Riedel
@author 		RS (Rene Santeler & Stefan Reinmüller) @ MCI 2020/21 (initially created by Rudolph Riedel, completely reworked by RS )

@section History
2.0 (adapted from Rudolph Riedel base version 1.13 - below changes are from RS 2020/21)
- Added color scheme, adaptable banner, dynamic graph implementation (TFT_GraphStatic & TFT_GraphData), a display init wit which addas the static part of a graph to static DL (initStaticGraphBackground), a display init to show a bitmap (TFT_display_init_screen),
- Adapted TFT_init to only do the most necessary thins for init (no static DL creation! you need to call one afterwards before using TFT_display!)

// See https://brtchip.com/eve-toolchains/ for helpful Tools
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "EVE.h"
#include "tft_data.h"


/////////// Banner line strip edge positions (from left to right: Y1 is held horizontal till X1, increasing till X2/Y2 and finally held horizontal at Y2 till EVE_HSIZE)
#define LAYOUT_Y1 66
#define LAYOUT_Y2 50
#define LAYOUT_X1 280
#define LAYOUT_X2 320


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

/* simple pre-definded colors */
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


// Memory-map definitions
#define MEM_LOGO 0x00000000 // start-address of logo, needs about 20228 bytes of memory. Will be written by TFT_display_init_screen and overwritten by initStaticGraphBackground (and everything else...)
#define MEM_DL_STATIC (EVE_RAM_G_SIZE - 4096) // 0xff000 -> Start-address of the static part of the display-list. Defined as the upper 4k of gfx-mem (WARNING: If static part gets bigger than 4k it gets clipped. IF dynamic part gets so big that it touches the last 4k (1MiB-4k) it overwrites the static part)
uint32_t num_dl_static; // amount of bytes in the static part of our display-list


/////////// General Variables
uint8_t tft_active = 0;  // Prevents TFT_display of doing anything if EVE_init isn't successful of TFT_init wasn't called
volatile uint8_t frameover = 0; // Debug value - is set to one if the sensor buffer gets full the first time (used to only run debugcode when actual data is there...)
// Input signal type used in main for measurement and here for swithcen on touch
volatile uint8_t InputType = 3; // 0=Sensor5, 1=TestImpulse, 2=TestSawTooth, 3=TestSine


/////////// Button states
uint16_t toggle_state_graphmode = 0;
uint16_t toggle_state_dimmer = 0;











void TFT_GraphStatic(uint8_t burst, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t padding, double amp_max, double t_max, double h_grid_lines, double v_grid_lines){
	/// Write the non-dynamic parts of an Graph to the TFT (axes & labels, grids and values, axis-arrows but no data). Can be used once during initStaticBackground() or at recurring display list build in TFT_display() completely coded by RS 02.01.2021.
	///
	///  burst	... determines if the normal or the burst version of the EVE Library is used to transmit DL command (0 = normal, 1 = burst). In full Pixels
	///  x		...	beginning of left edge of the graph (Note that the vertical axis starts at "x+padding" and that some Grid values might be at a position prior to x). In full Pixels
	///  y		... beginning of upper edge of the graph (Note this is the position of the axis-arrow point and that the horizontal axis label might be at a position prior to y). In full Pixels
	///  width	... width of the actual graph data area in full Pixels
	///  height	... height of the actual graph data area in full Pixels
	///  padding	...	clearance from the outer corners (x,y) to the axes
	///  amp_max ... maximum represented value of amplitude (e.g. 10 Volts), will be used at 100% horizontal line
	///  t_max 	... maximum represented value of time (e.g. 2.2 Seconds), will be used at 100% horizontal line
	///  h_grid_lines ... number of horizontal grid lines
	///  v_grid_lines ... number of vertical grid lines
	///  Note: The predefined GRAPH_AXISCOLOR and GRAPH_GRIDCOLOR are used directly!

	// Define a array of function pointers for every used "EVE_cmd_dl..." function. First on is normal, second on is to be used with burst mode
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

#if defined (EVE_CFAF240400C1_030SC)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x0000ed11);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x00001139);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff76809);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x00000000);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00010690);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xfffadf2e);
#endif

#if defined (EVE_CFAF320240F_035T)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x00005614);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x0000009e);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff43422);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x0000001d);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0xffffbda4);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0x00f8f2ef);
#endif

#if defined (EVE_CFAF480128A0_039TC)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x00010485);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x0000017f);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfffb0bd3);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x00000073);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x0000e293);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0x00069904);
#endif

#if defined (EVE_CFAF800480E0_050SC)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000107f9);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0xffffff8c);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff451ae);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x000000d2);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x0000feac);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xfffcfaaf);
#endif

#if defined (EVE_PAF90)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x00000159);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x0001019c);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff93625);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x00010157);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00000000);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0x0000c101);
#endif

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

#if defined (EVE_EVE2_38)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x00007bed);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x000001b0);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff60aa5);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x00000095);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0xffffdcda);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0x00829c08);
#endif

#if defined (EVE_EVE2_35G) ||  defined (EVE_EVE3_35G)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000109E4);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x000007A6);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xFFEC1EBA);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x0000072C);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x0001096A);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xFFF469CF);
#endif

#if defined (EVE_EVE2_43G) ||  defined (EVE_EVE3_43G)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x0000a1ff);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x00000680);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xffe54cc2);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0xffffff53);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x0000912c);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xfffe628d);
#endif

#if defined (EVE_EVE2_50G) || defined (EVE_EVE3_50G)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000109E4);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x000007A6);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xFFEC1EBA);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x0000072C);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x0001096A);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xFFF469CF);
#endif

#if defined (EVE_EVE2_70G)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000105BC);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0xFFFFFA8A);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0x00004670);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0xFFFFFF75);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00010074);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xFFFF14C8);
#endif

#if defined (EVE_NHD_35)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x0000f78b);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x00000427);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfffcedf8);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0xfffffba4);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x0000f756);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0x0009279e);
#endif

#if defined (EVE_RVT70)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000074df);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x000000e6);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfffd5474);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x000001af);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00007e79);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xffe9a63c);
#endif

#if defined (EVE_FT811CB_HY50HD)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 66353);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 712);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 4293876677);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 4294966157);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 67516);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 418276);
#endif

#if defined (EVE_ADAM101)
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000101E3);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x00000114);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xFFF5EEBA);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0xFFFFFF5E);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00010226);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0x0000C783);
#endif

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

void TFT_display_init_screen(void)
{

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

void initStaticGraphBackground(void)
{
	// Static portion of display-handling, meant to be called once at startup. Created by Rudolph Riedel, adapted by RS @ MCI 2020/21
	EVE_cmd_dl(CMD_DLSTART); // Start a new display list (resets REG_CMD_DL to 0)

	/// Main settings
	EVE_cmd_dl(TAG(0)); /* do not use the following objects for touch-detection */
	EVE_cmd_bgcolor(MAIN_BGCOLOR); /* light grey */
	EVE_cmd_dl(VERTEX_FORMAT(0)); /* reduce precision for VERTEX2F to 1 pixel instead of 1/16 pixel default */

	/// Draw Banner and divider line on top
	// Banner
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
	EVE_cmd_dl(DL_COLOR_RGB | MAIN_TEXTCOLOR);
	#if defined (EVE_DMA)
		EVE_cmd_text(10, EVE_VSIZE - 65, 26, 0, "Bytes: ");
	#endif
	EVE_cmd_text(360, 10, 26, 0, "DL-size:");
	EVE_cmd_text(360, 25, 26, 0, "Sensor:");

	/// Write the static part of the Graph to the display list
	TFT_GraphStatic(0, G_x, G_y, G_width, G_height, G_PADDING, G_amp_max, G_t_max, G_h_grid_lines, G_v_grid_lines);

	// Wait for Display to be finished
	while (EVE_busy());

	// Get size of the current display list
	num_dl_static = EVE_memRead16(REG_CMD_DL); // REG_CMD_DL = Command display list offset

	// Copy "num_dl_static" bytes from pointer "EVE_RAM_DL" to pointer "MEM_DL_STATIC"
	EVE_cmd_memcpy(MEM_DL_STATIC, EVE_RAM_DL, num_dl_static);
	while (EVE_busy());

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

void TFT_touch(void)
{
	/// Check for touch events and setup vars for TFT_display() (Buttons). Created by Rudolph Riedel, adapted by RS @ MCI 2020/21
	// Init vars
	uint8_t tag; // temporary store of received touched tag
	static uint8_t toggle_lock = 0; // "Debouncing of touches" -> If something is touched, this is set to prevent button evaluations. As soon as the is nothing pressed any more this is reset to 0
	
	// If Display is still busy, skip this evaluation to prevent hanging, glitches and flickers
	if(EVE_busy()) { return; } /* is EVE still processing the last display list? */

	// Read the value for the first touch point
	tag = EVE_memRead8(REG_TOUCH_TAG);

	// Determine which tag was touched
	switch(tag)
	{
		// nothing touched - reset states and locks
		case 0:
			toggle_lock = 0;
			break;
		// dimmer button on top as on/off radio-switch
		case 10:
			if(toggle_lock == 0) {
				printf("Button Dimmer touched\n");
				toggle_lock = 42;
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
			if(toggle_lock == 0) {
				printf("Toggle Roll touched\n");
				toggle_lock = 42;
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
			if(toggle_lock == 0) {
				printf("Switch Signal\n");
				toggle_lock = 42;
				InputType++;
				if(InputType > 3){ InputType = 0; }
			}
			break;
	}
}

void TFT_display(XMC_VADC_RESULT_SIZE_t SBuffer[], uint16_t size, uint16_t *SBuffer_curidx)
{
	/// Dynamic portion of display-handling, meant to be called every 20ms or more. Created by Rudolph Riedel, extensively adapted by RS @ MCI 2020/21
	///
	/// The inputs are used to draw the Graph data. Note that also some predefined graph settings are used direct (#define G_... )

	//static int32_t rotate = 0;
	uint16_t display_list_size = 0;

	if(tft_active != 0)
	{
		#if defined (EVE_DMA)
			uint16_t cmd_fifo_size;
			cmd_fifo_size = EVE_dma_buffer_index*4; /* without DMA there is no way to tell how many bytes are written to the cmd-fifo */
		#endif

		// get size of last display list to be printed on screen (section "Debug Values")
		display_list_size = EVE_memRead16(REG_CMD_DL);

		// start Burst (start writing to the cmd-fifo as one stream of bytes, only sending the address once)
		EVE_start_cmd_burst();



		/////////////// Start the actual display list
		EVE_cmd_dl_burst(CMD_DLSTART);
		EVE_cmd_dl_burst(DL_CLEAR_RGB | WHITE); /* set the default clear color to white */
		EVE_cmd_dl_burst(DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG); /* clear the screen - this and the previous prevent artifacts between lists, Attributes are the color, stencil and tag buffers */
		EVE_cmd_dl_burst(TAG(0));

		// insert static part of display-list from copy in gfx-mem
		EVE_cmd_append_burst(MEM_DL_STATIC, num_dl_static);



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
		sprintf(buffer, "%.2lf", (G_amp_max/G_y_max)*SBuffer[*SBuffer_curidx]); // double to string conversion
		strcat(buffer, unit_Sensor);
		EVE_cmd_text_burst(470, 25, 26, EVE_OPT_RIGHTX, buffer);



		/////////////// GRAPH
		///// Print static part of the Graph ---> Moved to initStaticBackground()
		//TFT_GraphStatic(1, G_x, G_y, G_width, G_height, G_PADDING, G_amp_max, G_t_max, G_h_grid_lines, G_v_grid_lines);

		///// Print dynamic part of the Graph (data & marker)
		TFT_GraphData(G_x, G_y, G_width, G_height, G_PADDING, G_y_max, SBuffer, size, SBuffer_curidx, toggle_state_graphmode, GRAPH_DATA1COLOR, GRAPH_POSMARKCOLOR);



		/////////////// Finish Display list and burst
		EVE_cmd_dl_burst(DL_DISPLAY); /* instruct the graphics processor to show the list */
		EVE_cmd_dl_burst(CMD_SWAP); /* make this list active */

		EVE_end_cmd_burst(); /* stop writing to the cmd-fifo, the cmd-FIFO will be executed automatically after this or when DMA is done */
	}
}
