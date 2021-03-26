/*
@file    		tft.c
@brief   		Implementation of display communication using the EVE Library. Meant to display a menu and a dynamic graph (implemented for XMC4700 and DAVE)
@version 		2.0 (base lib version was 1.13)
@date    		2020-09-05
@initialauthor  Rudolph Riedel
@author 		RS (Rene Santeler & Stefan Reinmüller) @ MCI 2020/21 (initially created by Rudolph Riedel, completely reworked by RS )

@section History
2.0 (adapted from Rudolph Riedel base version 1.13 - below changes are from RS 2020/21)
- Added color scheme, adaptable banner, dynamic graph implementation (TFT_graph_static & TFT_graph_stepdata), a display init which adds the static part of a graph to static DL (TFT_graph_static), a display init to show a bitmap (TFT_display_init_screen), ...
- Adapted TFT_init to only do the most necessary thins for init (no static DL creation! you need to call one afterwards before using TFT_display!)
2.1
- Added menu structure (ToDo: explain changes)
- Added keypad and swipe feature
- Added TFT elements (TFT_label, TFT_header, TFT_textbox, TFT_control, TFT_graph_stepdata)

// See https://brtchip.com/eve-toolchains/ for helpful Tools
 */
////// TODO: Early on all display_static functions were build to be able to use or not use burst mode. For now they are set to always use burst. If no problems with this occur, the functions can be stripped of this functionality.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <globals.h>
#include "Libraries/FT800-FT813-5.x/EVE.h"
#include "tft_data.h"
#include "menu.h"
#include "tft.h"




/////////// General Variables
static uint8_t tft_active = 0;  // Prevents TFT_display of doing anything if EVE_init isn't successful of TFT_init wasn't called
static uint8_t toggle_lock = 0; // "Debouncing of touches" -> If something is touched, this is set to prevent button evaluations. As soon as the is nothing pressed any more this is reset to 0
static char str_buf[100] = ""; // Character buffer used by str_insert and TFT_label

// Memory-map definitions
#define MEM_LOGO 0x00000000 // Start-address of logo, needs about 20228 bytes of memory. Will be written by TFT_display_init_screen and overwritten by TFT_graph_static (and everything else...)
#define MEM_DL_STATIC (EVE_RAM_G_SIZE - 4096) // 0xff000 -> Start-address of the static part of the display-list. Defined as the upper 4k of gfx-mem (WARNING: If static part gets bigger than 4k it gets clipped. IF dynamic part gets so big that it touches the last 4k (1MiB-4k) it overwrites the static part)
static uint32_t num_dl_static; // Amount of bytes in the static part of our display-list


/////////// Menu function pointers - At the end of the TFT_display_static, TFT_display and TFT_touch the function referenced to this pointer is executed
extern menu* Menu_Objects[TFT_MENU_SIZE]; // An array of menu structs that hold information general about the menus (see menu struct in tft.h for definition and menu.c for initialization)
extern void (*TFT_display_cur_Menu__fptr_arr[TFT_MENU_SIZE])(void);
extern void (*TFT_touch_cur_Menu__fptr_arr[TFT_MENU_SIZE])(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y);
extern void (*TFT_display_static_cur_Menu__fptr_arr[TFT_MENU_SIZE])(void);
// TFT_MENU_SIZE is declared in menu.c and must be changed if menus are added or removed
// TFT_MAIN_MENU_SIZE is declared in menu.c. It states to where the main menus (accessible via swipe an background) are listed. All higher menus are considered submenus (control on how to get there is on menu.c)
static int8_t TFT_cur_MenuIdx = 0; // Index of currently used menu (TFT_display,TFT_touch).
static int8_t TFT_last_MenuIdx = -1; // Index of last used menu (TFT_display_static). If this differs from TFT_cur_MenuIdx the initial TFT_display_static function of the menu is executed. Also helpful to determine what was the last menu during the TFT_display_static.
static uint32_t keypadControlKeyBgColor = MAIN_BTNCOLOR;
static uint32_t keypadControlKeyFgColor = MAIN_BTNCTSCOLOR;
static uint32_t mainBgColor = MAIN_BGCOLOR;

/////////// Scroll feature
static int8_t TFT_refresh_static = 0;
static int16_t TFT_cur_ScrollV = 0;
static int16_t TFT_last_ScrollV = 0;
static int16_t TFT_UpperBond = 0; 	// Defines up to which point elements are displayed. Needed to not scroll elements from main area into header or similar. Set by TFT_header, used by all element display functions.


/////////// Swipe feature (TFT_touch)
static SwipeDetection swipeDetect = None;
static uint8_t swipeInProgress = 0;
static uint8_t swipeEvokedBy = 0; 			 // The tag that initiated the swipe (needed to run an action based on which element was initially touched when the swipe began)
static int32_t swipeInitialTouch_X = 32768;  // X position of the initial touch of an swipe
static int32_t swipeInitialTouch_Y = 32768;
static uint8_t swipeEndOfTouch_Debounce = 0; // Counts the number of successive cycles without an touch (tag 0). Used to determine when an swipe is finished
int32_t swipeDistance_X = 0;		  		 // Distance (in px) between the initial touch and the current position of an swipe
int32_t swipeDistance_Y = 0;


/////////// Keypad feature (TFT_touch)
static keypadTypes keypadType = Standard;
static uint8_t keypadActive = 0;
static uint8_t keypadEvokedBy = 0;					// The tag that initiated the keypad (needed to only edit this element with the keypad)
static uint8_t keypadKeypressFinished = 0;
static uint8_t keypadCurrentKey = 0; 				// Integer value (tag) of the currently pressed key. The function that uses it must reset it to 0!
static uint8_t keypadShiftActive = 0; 				// Determines the shown set of characters
static uint8_t keypadEndOfKeypress_Debounce = 0; 	// Counts the number of successive cycles without an touch (tag 0). Used to determine when an keypress is finished
static uint8_t keypadInitialLock = 0; 				// When keyboard is activated the keypadInitialLock is set so it only starts accepting input after the finger is first lifted (otherwise it would recognize the random button the aligns with the activating button as a keypress)
static const char backspace_char = 46; 				// The code that needs to be used for the selected backspace character. It is the offset of the extended ASCII code ('<<' = 174, minus the offset of 128 => 46). Need to use font 19 for extended ASCII characters!
#define KEYPAD_ACTIVE_TARGET_HEIGTH 72			// target offset of the text from upper border of the TFT (position so that it can easily be edited/seen) in pixel
// TAG ASSIGNMENT
//		0		No touch
//		1		Background (swipe area)
//		32-126	Keyboard Common Buttons
//		60		Keyboard Left <
//		62		Keyboard Right >
//		94		Keyboard Shift Key (^)
//		127		Keyboard Backspace (displayed character is different, see 'backspace_char'!)
//		128		Keyboard Enter

/////////// Textbox feature
uint8_t textbox_cursor_pos = 0;

// Array of function pointers for every used "EVE_cmd_dl..." function. First one is normal, second one is to be used within a burst mode.
static void (*EVE_cmd_dl__fptr_arr[])(uint32_t) = {EVE_cmd_dl, EVE_cmd_dl_burst};
static void (*EVE_cmd_text__fptr_arr[])(int16_t, int16_t, int16_t, uint16_t, const char*) = {EVE_cmd_text, EVE_cmd_text_burst};
static void (*EVE_cmd_text_var__fptr_arr[])(int16_t, int16_t, int16_t, uint16_t, const char*, uint8_t, ...) = {EVE_cmd_text_var, EVE_cmd_text_var_burst};
static void (*EVE_cmd_number__fptr_arr[])(int16_t, int16_t, int16_t, uint16_t, int32_t) = {EVE_cmd_number, EVE_cmd_number_burst};



void keypad_open(uint8_t evokedBy, enum keypadTypes type){
	/// Open a keypad for the according evoker (control element - e.g. tag of an textbox)


	// Only open keypad if it isn't open
	if(keypadActive == 0){
		// Activate Keypad and set type
		keypadActive = 1;
		keypadEvokedBy = evokedBy;
		keypadType = type;

		// When keyboard is activated the keypadInitialLock is set so it only starts accepting input after the finger is first lifted (otherwise it would recognize the random button the aligns with the activating button as a keypress)
		keypadInitialLock = 1;
	}
}

void keypad_close(){
	/// Close and reset the keypad


	// Deactivate Keypad and reset control variables
	keypadActive = 0;
	keypadEvokedBy = 0;
	keypadType = Standard;
	keypadKeypressFinished = 0;
}



void str_insert(char* target, int8_t* len, char ch, int8_t pos){
	/// Insert a character 'ch' at given position 'pos' of a string 'target'.
	/// Note: 'len' will be automatically increased (string gets longer)! Does not work for strings longer than 99 characters


	// Copy actual text in front of the new char to the buffer
	strncpy(str_buf, target, pos);

	// Copy the rest of the text one character behind the new on
	strcpy(&str_buf[pos+1], &target[pos]);

	// Add new character
	str_buf[pos] = ch;

	// Copy buffer back to target
	strcpy(target,str_buf);

	// Increase length
	(*len)++;
}

void TFT_setMenu(int16_t idx){
	/// Set the current menu to the one of given index and reset environment (keypad, scroll, upperbond)
	/// If used with already set menu index or -1 the display with refresh the static part


	//Let TFT_display_static() run on next TFT_display() cycle
	TFT_refresh_static = 1;

	// Set new index (used by all function pointers and menu based objects)
	if(idx >= 0)
		TFT_cur_MenuIdx = idx;

	// If the menu changed - reset features
	if(TFT_cur_MenuIdx != TFT_last_MenuIdx){ //keypadActive &&
		// Set upper bond
		TFT_UpperBond = Menu_Objects[idx]->upperBond;

		// Close keypad if necessary
		keypad_close();

		// Reset scroll
		TFT_cur_ScrollV = 0;
	}
}

void TFT_setColor(uint8_t burst, int32_t textColor, int32_t fgColor, int32_t bgColor, int32_t gradColor){
	/// Write the to be used colors to the TFT. Used by all graphic elements.
	/// Parameters set to -1 will be ignored! Use this to only set certain color.
	///
	///  textColor	... Textcolor used by every text
	///  fgColor	...	Foreground color used by control elements like buttons and such
	///	 bgColor	... Background color used by control elements like buttons and such
	///  gradColor	...	Gradient color used by control elements like buttons and such
	///
	///	 Uses tft-global variables:
	///		EVE Library ...


	if(burst){
		if(textColor >= 0)
			EVE_cmd_dl_burst(DL_COLOR_RGB | textColor);
		if(fgColor >= 0)
			EVE_cmd_fgcolor_burst(fgColor);
		if(bgColor >= 0)
			EVE_cmd_bgcolor_burst(bgColor);
		if(gradColor >= 0)
			EVE_cmd_gradcolor_burst(gradColor);
	}
	else{
		if(textColor >= 0)
			EVE_cmd_dl(DL_COLOR_RGB | textColor);
		if(fgColor >= 0)
			EVE_cmd_fgcolor(fgColor);
		if(bgColor >= 0)
			EVE_cmd_bgcolor(bgColor);
		if(gradColor >= 0)
			EVE_cmd_gradcolor(gradColor);
	}
}


void TFT_header(uint8_t burst, menu* men){
	/// Write the non-dynamic parts of a menu to the TFT (banner, divider line & header text). Can be used once during init of a static background or at recurring display list build in TFT_display_[menu]()
	///
	///  burst			Determines if the normal or the burst version of the EVE Library is used to transmit DL command (0 = normal, 1 = burst).
	///  layout			Banner line strip edge positions. Array of 4 elements [Y1,X1,Y2,X2] (from left to right: Y1 is held horizontal till X1, increasing till X2/Y2 and finally held horizontal at Y2 till EVE_HSIZE)
	///  bannerColor	Color of the banner surface
	///  dividerColor	Color of the line at the edge of the banner
	///  headerColor	Color of the text
	///  headerText		Name of the current menu (header text)
	///
	///	 Uses tft-global variables:
	///		EVE Library ...
	///		EVE_cmd_dl__fptr_arr, EVE_cmd_text__fptr_arr	Function pointer for "EVE_cmd_dl..." function with or without burst


	/// Draw Banner and divider line on top (line strip edge positions from left to right: Y1 is held horizontal till X1, increasing till X2/Y2 and finally held horizontal at Y2 till EVE_HSIZE)
	// Banner
	(*EVE_cmd_dl__fptr_arr[burst])(TAG(1)); /* give everything considered background area tag 1 -> used for wipe feature*/
	(*EVE_cmd_dl__fptr_arr[burst])(LINE_WIDTH(1*16)); /* size is in 1/16 pixel */
	(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | men->bannerColor);
	(*EVE_cmd_dl__fptr_arr[burst])(DL_BEGIN | EVE_EDGE_STRIP_A);
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(0                   , men->headerLayout[0]));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(men->headerLayout[1], men->headerLayout[0]));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(men->headerLayout[3], men->headerLayout[2]));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(EVE_HSIZE           , men->headerLayout[2]));
	(*EVE_cmd_dl__fptr_arr[burst])(DL_END);
	// Divider
	(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | men->dividerColor);
	(*EVE_cmd_dl__fptr_arr[burst])(DL_BEGIN | EVE_LINE_STRIP);
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(0                   , men->headerLayout[0]));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(men->headerLayout[1], men->headerLayout[0]));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(men->headerLayout[3], men->headerLayout[2]));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(EVE_HSIZE           , men->headerLayout[2]));
	(*EVE_cmd_dl__fptr_arr[burst])(DL_END);

	// Add header text
	(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | men->headerColor);
	(*EVE_cmd_text__fptr_arr[burst])(20, 15, 30, 0, men->headerText);

	// Reset current tag to prevent next elements from being considered background
	(*EVE_cmd_dl__fptr_arr[burst])(TAG(0));
}

void TFT_label(uint8_t burst, label* lbl){
	/// Write a text/label to the TFT. Can be used once during init of a static background TFT_display_static() or at recurring display list build in TFT_display()
	///	Use num_src to display values (see description below and at struct)
	///
	///  burst		Determines if the normal or the burst version of the EVE Library is used to transmit DL command (0 = normal, 1 = burst).
	///  x
	///  y
	///  font
	///  options
	///  text			Name of the current menu (Header)
	///  num_src		A pointer to the numeric source this label represents. Set to 0 if its a pure text label. If set to 1 the string in text is used to format the source
	///  ignoreScroll
	///
	///	 Uses tft-global variables:
	///		EVE Library ...
	///		EVE_cmd_dl__fptr_arr, EVE_cmd_text__fptr_arr	Function pointer for "EVE_cmd_dl..." function with or without burst
	///		TFT_cur_ScrollV, TFT_UpperBond
	///		str_buf
	///
	///	 Limitations: num_src conversion only works with strings shorter than 99 characters (using global buffer 'str_buf')


	// Determine desired y position based on scroll and ignore scroll
	uint16_t curY = lbl->y;
	if(lbl->ignoreScroll == 0){
		// Determine current position (with scroll value)
		curY -= TFT_cur_ScrollV;
	}

	// Only show label if it is inside display or ignores scroll
	if(lbl->ignoreScroll || (curY > TFT_UpperBond && curY < EVE_VSIZE)){
		// Do not recognize touches on label
		(*EVE_cmd_dl__fptr_arr[burst])(TAG(0));

		// If label is a number related one (srcType not 0 = srcTypeNone), convert number according to text and print string
		// Note: This could be more efficient if we only convert when needed and/or lower the refresh rate (store calculated values).
		if(lbl->numSrc.srcType != srcTypeNone){
			/// If a numerical source is assigned to the textbox, refresh the text
			// ... for integer source
			if(lbl->numSrc.srcType == srcTypeInt)
				// If the offset is not given use integer directly, else use offset to determine which value is to be used
				if(lbl->numSrc.srcOffset == NULL)
					(*EVE_cmd_text_var__fptr_arr[burst])(lbl->x, curY, 26, EVE_OPT_FORMAT | lbl->options, lbl->text, 1 ,(int32_t)*lbl->numSrc.intSrc);
				else
					(*EVE_cmd_text_var__fptr_arr[burst])(lbl->x, curY, 26, EVE_OPT_FORMAT | lbl->options, lbl->text, 1 ,(int32_t)( lbl->numSrc.intSrc[*lbl->numSrc.srcOffset]) );
			// ... for floating source
			else if(lbl->numSrc.srcType == srcTypeFloat){
				// Split float value into integral/fractional part and print them according to lbl->text
				float intPart, fracPart;
				/// If the offset is not given use floatSrc directly, else use offset to determine which value is to be used
				if(lbl->numSrc.srcOffset == NULL)
					fracPart = modff( *lbl->numSrc.floatSrc, &intPart);
				else
					fracPart = modff(lbl->numSrc.floatSrc[*lbl->numSrc.srcOffset], &intPart);
				// Print the string while using text as format and raising the fraction to the power of 10 given by fracExp
				(*EVE_cmd_text_var__fptr_arr[burst])(lbl->x, curY, 26, EVE_OPT_FORMAT | lbl->options, lbl->text, 2 ,FLOAT_TO_INT16(intPart), FLOAT_TO_INT16(fabsf( fracPart*(pow10f((float)lbl->fracExp)))) ); //"%d.%.2d"
			}
		}
		// No source given -> print pure text
		else{
			(*EVE_cmd_text__fptr_arr[burst])(lbl->x, curY, lbl->font, lbl->options, lbl->text);
		}
	}
}

void TFT_control(control* ctrl){
	/// Display a user control element (button/toggle) to the TFT. Adapts the y coordinate automatically to current vertical scroll (use ignoreScroll property of control struct if wanted otherwise)
	/// Meant to be used at recurring display list build in TFT_display().
	/// Note: Use TFT_setColor(...) before calling this!
	///
	///  x				Position of left control edge in full Pixels.
	///  y				Position of upper control edge in full Pixels.
	///  w0				Width of the control
	///  h0				Height of the control
	///  mytag			The touch tag of the control. Note: Use a global define to set this in object and use it inside TFT_touch_[menu]() inside tag switch.
	///  options		Options that can be applied to the control (For toggles: EVE_OPT_FLAT, EVE_OPT_FORMAT, for buttons: Same as for toggles but additionally EVE_OPT_FILL with standard EVE_OPT_3D)
	///  state			Toggles: state of the toggle - 0 off - 65535 on, Buttons: Bitwise or'ed with options
	///  font			Font of the control text
	///  textColor		Color of the text
	///  text			Text displayed in the control
	///	 ignoreScroll	If set to 1 the global TFT_cur_ScrollV is ignored
	///
	///	 Uses tft-global variables:
	///		EVE Library ...
	///		TFT_cur_ScrollV, TFT_UpperBond


	uint16_t curY = ctrl->y;
	if(ctrl->ignoreScroll == 0){
		// Determine current position (with scroll value)
		curY -= TFT_cur_ScrollV;
	}

	// Only show textbox if it is inside display
	if(ctrl->ignoreScroll || (curY > TFT_UpperBond && curY < EVE_VSIZE)){
		// Set assigned Tag
		EVE_cmd_dl_burst(TAG(ctrl->mytag)); /* do not use the following objects for touch-detection */

		// Draw Element
		switch (ctrl->controlType) {
			case Button:
				EVE_cmd_button_burst(ctrl->x, curY, ctrl->w0, ctrl->h0, ctrl->font, ctrl->options | ctrl->state, ctrl->text);
				break;
			case Toggle:
				EVE_cmd_toggle_burst(ctrl->x, curY, ctrl->w0, ctrl->font, ctrl->options, ctrl->state, ctrl->text);
				break;
		}

		/// Reset tag
		EVE_cmd_dl_burst(TAG(0));
	}
}



void TFT_textbox_static(uint8_t burst, textbox* tbx){
	/// Write the non-dynamic parts of an textbox to the TFT (background & frame). Can be used once during init of a static background or at recurring display list build in TFT_display()
	/// If mytag is 0 the textbox is considered read only with grey background. Else its read/write with white background
	///
	///  burst			Determines if the normal or the burst version of the EVE Library is used to transmit DL command (0 = normal, 1 = burst).
	///  x				Position of left textbox edge. In full Pixels.
	///  y				Position of upper textbox edge. In full Pixels.
	///  labelOffsetX	Distance between x and the actual start of the textbox (space needed for label).
	///  width			Width of the actual textbox data area in full Pixels.
	///  labelText		Text of the preceding label.
	///  mytag			To be assigned tag for touch recognition.
	///
	///	 Uses tft-global variables:
	///		EVE Library ...
	///		EVE_cmd_dl__fptr_arr	Function pointer for "EVE_cmd_dl..." function with or without burst
	///		TEXTBOX_HEIGTH, TEXTBOX_PAD_V
	///		TFT_cur_ScrollV, TFT_UpperBond
	///
	/// Limitations: The frame is hard-coded black


	// Determine current position (with scroll value)
	uint16_t curY = tbx->y - TFT_cur_ScrollV;

	// Only show textbox if it is inside display
	if(curY > TFT_UpperBond && curY < EVE_VSIZE){

		/// Write label
		(*EVE_cmd_text__fptr_arr[burst])(tbx->x, curY + TEXTBOX_PAD_V, 26, 0, tbx->labelText); // +7 to get same

		/// Calculate coordinates
		uint16_t abs_x_left = 	tbx->x + tbx->labelOffsetX;
		uint16_t abs_x_right = 	tbx->x + tbx->labelOffsetX + tbx->width;
		uint16_t abs_y_top = 	curY;
		uint16_t abs_y_bottom = curY + TEXTBOX_HEIGTH;

		/// Set background color and tag based on mytag
		if(tbx->mytag == 0){ // read-only
			// Read-only: Set no touch tag and grey background color
			(*EVE_cmd_dl__fptr_arr[burst])(TAG(0));
			(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | LIGHTGREY);
		}
		else{
			// Read/Write: Set actual tag and white background color
			(*EVE_cmd_dl__fptr_arr[burst])(TAG(tbx->mytag));
			(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | WHITE);
		}


		/// Background rectangle
		(*EVE_cmd_dl__fptr_arr[burst])(DL_BEGIN | EVE_RECTS);
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(abs_x_left , abs_y_top   ));
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(abs_x_right, abs_y_bottom));
		(*EVE_cmd_dl__fptr_arr[burst])(DL_END);

		/// Frame
		(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | BLACK);
		(*EVE_cmd_dl__fptr_arr[burst])(DL_BEGIN | EVE_LINE_STRIP);
		// start point
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(abs_x_left,  abs_y_top));
		// left vertical
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(abs_x_left,  abs_y_bottom));
		// bottom horizontal
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(abs_x_right, abs_y_bottom));
		// right vertical
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(abs_x_right, abs_y_top));
		// bottom horizontal
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(abs_x_left,  abs_y_top));
		(*EVE_cmd_dl__fptr_arr[burst])(DL_END);

		/// Reset tag
		(*EVE_cmd_dl__fptr_arr[burst])(TAG(0));
	}
}

void TFT_textbox_touch(textbox* tbx){
	/// Manage input from keyboard and manipulate text. Used at recurring touch evaluation in TFT_touch().
	/// Note: Set textbox_cursor_pos initial before entering this the first time. Check TAG ASSIGNMENT above - this refers to the conventions given there!
	///
	///  mytag			The tag of the current control element (code only responds for this textbox if keypadEvokedBy is mytag)
	///  text			The string that shall be manipulated
	///  text_maxlen	Maximum length of the manipulated text (this will not add characters if length is at maximum)
	///  text_curlen	Current length of the manipulated text (this will change length according to input)
	///
	///	 Uses tft-global variables:
	///		EVE Library ...
	///		keypad:  keypadActive, keypadEvokedBy, keypadCurrentKey, keypadKeypressFinished
	///		textbox_cursor_pos


	/// If the keyboard is active and evoked by the current textbox, manipulate text according to input
	if(keypadActive && keypadEvokedBy == tbx->mytag){
		/// If a finished keypress is detected and the current key is in valid range - manipulate text according to keypad input
		if(keypadKeypressFinished && keypadCurrentKey >= 32 && keypadCurrentKey <= 128){
			// Enter key - finish textbox input
			if(keypadCurrentKey == 128){
				// Close/reset keypad
				TFT_textbox_setStatus(tbx, 0, 0);
			}
			else {
				// Backspace - Delete last character
				if(keypadCurrentKey == 127){
					// Remove last character (if cursor is inside of string)
					if(textbox_cursor_pos >= 1 && textbox_cursor_pos <= *(tbx->text_curlen)){
						// Overwrite previous char with rest of string
						strcpy(&(tbx->text[textbox_cursor_pos-1]), &(tbx->text[textbox_cursor_pos]));
						// Decrease text length and move cursor back
						(*(tbx->text_curlen))--;
						textbox_cursor_pos--;
					}
				}
				// Left - Move cursor back (if cursor is inside of string)
				else if(keypadCurrentKey == 60){
					if(textbox_cursor_pos > 0)
						textbox_cursor_pos--;
				}
				// Right - Move cursor forth (if cursor is inside of string)
				else if(keypadCurrentKey == 62){
					if(textbox_cursor_pos < *(tbx->text_curlen))
						textbox_cursor_pos++;
				}
				// Actual character - Add at position if the string isn't at max length
				else if(*(tbx->text_curlen) < ((tbx->text_maxlen)-1)){
						str_insert(tbx->text, (tbx->text_curlen), (char)keypadCurrentKey, textbox_cursor_pos);
						textbox_cursor_pos++;
				}
			}

			// Mark keypress as handled (keypress is now processed -> wait till the next one comes)
			keypadKeypressFinished = 0;
			keypadCurrentKey = 0;

		} // end if - keypadKeypressFinished && keypadCurrentKey in range

	} // end if - keyboard active && evoked by me

}


void TFT_textbox_display(textbox* tbx){
	/// Write the dynamic parts of an textbox to the TFT (text & cursor). Used at recurring display list build in TFT_display()
	///
	///  x				Position of left textbox edge. In full Pixels.
	///  y				Position of upper textbox edge. In full Pixels.
	///  labelOffsetX	Distance between x and the actual start of the textbox (space needed for label).
	///  mytag			The tag of the current control element (cursor is only shown if keypadEvokedBy is mytag).
	///  text			The string that shall be displayed.
	///
	///	 Uses tft-global variables:
	///		EVE Library ...
	///		keypad:  keypadActive, keypadEvokedBy
	///		textbox_cursor_pos
	///		TEXTBOX_PAD_V, TEXTBOX_PAD_H
	///		str_buf
	///
	///  Limitations: Made for font size 26. Make sure the text can't be longer than the textbox width!


	// Cursor and control variables (static: will keep value between function executions)
	static char cursor = '|';
	static uint32_t lastBlink = 0;

	// Determine current position (with scroll value)
	uint16_t curY = tbx->y + TEXTBOX_PAD_V - TFT_cur_ScrollV;

	// Only show text if it is inside display
	if(curY > TFT_UpperBond && curY < EVE_VSIZE){

		/// Set tag
		EVE_cmd_dl_burst(TAG(tbx->mytag));

		/// Set text color
		EVE_cmd_dl_burst(DL_COLOR_RGB | BLACK);

		/// If the keyboard is active, show textbox text with cursor - else just write text directly
		if(keypadActive && keypadEvokedBy == tbx->mytag){ //
			// Toggle cursor every 550ms
			if(SYSTIMER_GetTime() > (lastBlink + 550000)){
				lastBlink = SYSTIMER_GetTime();
				if(cursor == '|')
					cursor = ' ';
				else
					cursor = '|';
			}

			/// Copy text to buffer but add the cursor
			// Copy actual text previous to the cursor to the buffer
			strncpy(str_buf, tbx->text, textbox_cursor_pos);
			// Add cursor
			str_buf[textbox_cursor_pos] = cursor;
			// Copy rest of string past cursor to the buffer
			strcpy(&str_buf[textbox_cursor_pos+1], &(tbx->text[textbox_cursor_pos]));

			// Write buffer as text
			EVE_cmd_text_burst(tbx->x + tbx->labelOffsetX + TEXTBOX_PAD_H,curY, 26, 0, str_buf);
		}
		// textbox is inactive
		else{
			// If textbox is a number related one (srcType not 0 = srcTypeNone), convert number according to text and print string
			// Note: This could be more efficient if we only convert when needed and/or lower the refresh rate (store calculated values).
			if(tbx->numSrc.srcType != srcTypeNone){
				/// If a numerical source is assigned to the textbox, refresh the text
				// ... for integer source
				if(tbx->numSrc.srcType == srcTypeInt)
					// If the offset is not given, use integer directly, else use offset to determine which value is to be used
					if(tbx->numSrc.srcOffset == NULL){
						EVE_cmd_text_var_burst(tbx->x + tbx->labelOffsetX + TEXTBOX_PAD_H, curY, 26, EVE_OPT_FORMAT, tbx->numSrcFormat, 1 ,(int32_t)*tbx->numSrc.intSrc);
					}
					else
						EVE_cmd_text_var_burst(tbx->x + tbx->labelOffsetX + TEXTBOX_PAD_H, curY, 26, EVE_OPT_FORMAT, tbx->numSrcFormat, 1 ,(int32_t)( tbx->numSrc.intSrc[*tbx->numSrc.srcOffset]) );
				// ... for floating source
				else if(tbx->numSrc.srcType == srcTypeFloat){
					// Split float value into integral/fractional part and print them according to tbx->text
					float intPart, fracPart;
					/// If the offset is not given, use floatSrc directly, else use offset to determine which value is to be used
					if(tbx->numSrc.srcOffset == NULL)
						fracPart = modff(*tbx->numSrc.floatSrc, &intPart);
					else
						fracPart = modff(tbx->numSrc.floatSrc[*tbx->numSrc.srcOffset], &intPart);
					// Print the string while using text as format and raising the fraction to the power of 10 given by fracExp
					EVE_cmd_text_var_burst(tbx->x + tbx->labelOffsetX + TEXTBOX_PAD_H, curY, 26, EVE_OPT_FORMAT, tbx->numSrcFormat, 2 ,FLOAT_TO_INT16(intPart), FLOAT_TO_INT16(fabsf( fracPart*(pow10f((float)tbx->fracExp)))) ); //"%d.%.2d"
				}
			}
			// No source given -> print pure text
			else{
				EVE_cmd_text_burst(tbx->x + tbx->labelOffsetX + TEXTBOX_PAD_H, curY, 26, 0, tbx->text);
			}

		}

		/// Reset tag
		EVE_cmd_dl_burst(TAG(0));
	}
}

static void TFT_textbox_setCursor(const textbox* tbx, int16_t position){
	/// Sets the position of the global textbox cursor.
	/// Internal function - use TFT_textbox_setStatus to set this from outside.
	/// -1	... Set to end
	/// 0	...	Set to begin
	/// x	... Set to position x if "x <= len"
	/// TODO: interpret e.g. -4 as 4th char from behind

	// Set cursor based on parameter position
	switch (position) {
		case -1:
			// Set to end
			textbox_cursor_pos = *tbx->text_curlen;
			break;
		default:
			// if position less than length - set to given position
			if(position <= *tbx->text_curlen)
				textbox_cursor_pos = position;
			break;
	}
}

void TFT_textbox_setStatus(textbox* tbx, uint8_t active, int16_t cursorPos){
	/// Controls the state of an textbox (open, set cursor & close). Meant to be called by an menu specific TFT_touch_[menu]() on touch of the textbox tag.
	/// If called with an inactive textbox and active=1 this opens the wanted keypad and sets the scroll so the textbox is visible above the keypad.
	/// If called with an already active textbox, this changes the cursor position.
	/// If called with an active textbox and active=0 this writes the input back to the source (if numeric source is linked), resets the scroll and closes the keypad.


	// If textbox shall be active but isn't - set scroll, activate keypad and set cursor
	if(active == 1 && tbx->active == 0){
		/// If a numeric source is given, write the source to the text (so it can be edited as text)
		// Integer source
		if(tbx->numSrc.srcType == srcTypeInt){
			// If the offset is not given, use integer directly, else use offset to determine which value is to be used
			if(tbx->numSrc.srcOffset == NULL)
				sprintf(tbx->text, tbx->numSrcFormat, *tbx->numSrc.intSrc); // string to integer conversion
			else
				sprintf(tbx->text, tbx->numSrcFormat, tbx->numSrc.intSrc[*tbx->numSrc.srcOffset]); // string to integer conversion with offset

			// Refresh text length
			*tbx->text_curlen = strlen(tbx->text);
		}
		// Fractional source
		else if(tbx->numSrc.srcType == srcTypeFloat){
			// Split float value into integral/fractional part and print them according to tbx->text
			float intPart, fracPart;
			/// If the offset is not given, use floatSrc directly, else use offset to determine which value is to be used
			if(tbx->numSrc.srcOffset == NULL)
				fracPart = modff( *tbx->numSrc.floatSrc, &intPart);
			else
				fracPart = modff(tbx->numSrc.floatSrc[*tbx->numSrc.srcOffset], &intPart);
			// Print the string while using text as format and raising the fraction to the power of 10 given by fracExp
			sprintf(tbx->text, tbx->numSrcFormat, FLOAT_TO_INT16(intPart), FLOAT_TO_INT16(fabsf( fracPart*(pow10f((float)tbx->fracExp)))) ); // string to integer conversion
			//,FLOAT_TO_INT16(intPart), FLOAT_TO_INT16(fabsf( fracPart*(pow10f((float)tbx->fracExp)))) ); //"%d.%.2d"

			// Refresh text length
			*tbx->text_curlen = strlen(tbx->text);
		}

		// Set vertical scroll so that the textbox can be seen
		TFT_cur_ScrollV = tbx->y - KEYPAD_ACTIVE_TARGET_HEIGTH;
		tbx->active = 1;

		// Activate Keypad and set cursor to given position
		keypad_open(tbx->mytag, tbx->keypadType);
		TFT_textbox_setCursor(tbx, cursorPos);
	}
	// If textbox shall be active and already is - update cursor position
	if(active == 1 && tbx->active == 1){
		// Activate Keypad and set cursor to given position
		TFT_textbox_setCursor(tbx, cursorPos);
	}
	// If textbox shall be deactivated - Write back content and reset scroll/keypad
	else{
		// If a numeric source is given, write the text back to source (save)
		if(tbx->numSrc.srcType == srcTypeInt){
			if(tbx->numSrc.srcOffset == NULL)
				*tbx->numSrc.intSrc = FLOAT_TO_INT16(atoff(tbx->text));//strtof(tbx->text, 0);
			else
				tbx->numSrc.intSrc[*tbx->numSrc.srcOffset] = FLOAT_TO_INT16(atoff(tbx->text));//strtof(tbx->text, 0);

		}
		else if(tbx->numSrc.srcType == srcTypeFloat){
			if(tbx->numSrc.srcOffset == NULL)
				*tbx->numSrc.floatSrc = atoff(tbx->text); //strtof(tbx->text, 0);  //(float_buffer_t)
			else
				tbx->numSrc.floatSrc[*tbx->numSrc.srcOffset] = atoff(tbx->text);

		}

		// Reset vertical scroll to normal
		TFT_cur_ScrollV = 0;
		tbx->active = 0;

		// Deactivate Keypad and set cursor to end
		keypad_close();
	}


}






void TFT_graph_static(uint8_t burst, graph* gph){
	/// Write the non-dynamic parts of an Graph to the TFT (axes & labels, grids and values, axis-arrows but no data). Can be used once during init of a static background or at recurring display list build in TFT_display() completely coded by RS 02.01.2021.
	///
	///  burst	... determines if the normal or the burst version of the EVE Library is used to transmit DL command (0 = normal, 1 = burst).
	///  x		...	beginning of left edge of the graph (Note that the vertical axis starts at "x+padding" and that some Grid values might be at a position prior to x). In full Pixels
	///  y		... beginning of upper edge of the graph (Note this is the position of the axis-arrow point and that the horizontal axis label might be at a position prior to y). In full Pixels
	///  width	... width of the actual graph data area in full Pixels
	///  height	... height of the actual graph data area in full Pixels
	///  padding ... clearance from the outer corners (x,y) to the axes
	///  amp_max ... maximum represented value of amplitude (e.g. 10 Volts), will be used at 100% horizontal line
	///  cx_max 	 ... maximum represented value of time (e.g. 2.2 Seconds), will be used at 100% horizontal line
	///  h_grid_lines ... number of horizontal grid lines
	///  v_grid_lines ... number of vertical grid lines
	///
	///  Note: The predefined GRAPH_AXISCOLOR and GRAPH_GRIDCOLOR are used directly!


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


	// Determine current position (with scroll value)
	uint16_t curY = gph->y - TFT_cur_ScrollV;


	/// Calculate pixels between lines and labels of the grid
	// Used by grid lines and labels (space between them)
	float widthPerSection = (float)(gph->width)/gph->v_grid_lines;
	float heightPerSection = (float)(gph->height)/gph->h_grid_lines;

	/// Axes LABELS
	(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_AXISCOLOR);
	(*EVE_cmd_text__fptr_arr[burst])(gph->x + gph->padding              + h_ax_lbl_comp_x, gph->y + gph->padding               - h_ax_lbl_comp_y, axis_lbl_txt_size, 0, gph->y_label);
	(*EVE_cmd_text__fptr_arr[burst])(gph->x + gph->padding + gph->width + v_ax_lbl_comp_x, gph->y + gph->padding + gph->height - v_ax_lbl_comp_y, axis_lbl_txt_size, EVE_OPT_RIGHTX, gph->x_label);

	/// AXES lines
	//(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_AXISCOLOR);
	(*EVE_cmd_dl__fptr_arr[burst])(DL_BEGIN | EVE_LINES);
	// left vertical line (Amplitude)
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding, curY));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding, curY + gph->padding + gph->height + gph->padding));
	// bottom horizontal line (Time)
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x                            				 , curY + gph->padding + gph->height ));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding + gph->width + gph->padding, curY + gph->padding + gph->height ));

	/// GRID lines
	(*EVE_cmd_dl__fptr_arr[burst])(LINE_WIDTH(grid_linewidth)); /* size is in 1/16 pixel */
	(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_GRIDCOLOR);
	// vertical grid
	for(int i=1; i<=(int)floor(gph->v_grid_lines); i++){
		// y-position at upper and lower corner; x-position from left with padding and width of graph divided by number of gridlines - times current line
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding + (uint16_t)(widthPerSection*(float)i), curY + gph->padding ));
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding + (uint16_t)(widthPerSection*(float)i), curY + gph->padding + gph->height ));
	}
	// horizontal grid
	for(int i=1; i<=(int)floor(gph->h_grid_lines); i++){
		// x-position at left and right corner; y-position from top y, padding and height divided by number of gridlines - times current line
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding        	  , curY + gph->padding + gph->height - (uint16_t)(heightPerSection*(float)i) ));
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding + gph->width, curY + gph->padding + gph->height - (uint16_t)(heightPerSection*(float)i) ));
	}
	(*EVE_cmd_dl__fptr_arr[burst])(DL_END);

	/// Grid VALUES
	(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_AXISCOLOR);
	// vertical grid (x)
	for(int i=1; i<=(int)ceil(gph->v_grid_lines); i++){ // "ceil" and "i-1" at val -> print also the 0 value
		// Calc time at current vertical line
		float val = (gph->cx_max/gph->v_grid_lines*(float)(i-1));

		// If its a pure integer write it as number, else convert and write it to string
		if((val - (float)((uint32_t)val)) == 0){ //val % 1.0 == 0
			(*EVE_cmd_number__fptr_arr[burst])(gph->x + gph->padding + (uint16_t)(widthPerSection*(float)(i-1)) + h_grid_lbl_comp_x, curY + gph->height + h_grid_lbl_comp_y, grid_lbl_txt_size, 0, (int32_t)val); //EVE_OPT_RIGHTX| + 18
		}
		else{
			char buffer[32]; // buffer for float to string conversion
			sprintf(buffer, "%.1lf", val); // float to string conversion
			(*EVE_cmd_text__fptr_arr[burst])(gph->x + gph->padding + (uint16_t)(widthPerSection*(float)(i-1)) + h_grid_lbl_comp_x, curY + gph->height + h_grid_lbl_comp_y, grid_lbl_txt_size, 0, buffer);
		}
	}
	// horizontal grid (y)
	//(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_AXISCOLOR);
	for(int i=1; i<=(int)floor(gph->h_grid_lines); i++){  // "floor" and "i" at val -> don't print the 0 value
		// Calc amplitude at current horizontal line
		float val = (gph->amp_max/gph->h_grid_lines*(float)i);

		// If its a pure integer write it as number, else convert and write it to string
		if((val - (float)((uint32_t)val)) == 0){ //val % 1.0 == 0
			(*EVE_cmd_number__fptr_arr[burst])(gph->x - v_grid_lbl_comp_x, curY + gph->padding + gph->height - (uint16_t)(heightPerSection*(float)i) + v_grid_lbl_comp_y, grid_lbl_txt_size, 0, (int32_t)val); //EVE_OPT_RIGHTX|
		}
		else{
			char buffer[32]; // buffer for float to string conversion
			sprintf(buffer, "%.1lf", val); // float to string conversion
			(*EVE_cmd_text__fptr_arr[burst])(gph->x - v_grid_lbl_comp_x, curY + gph->padding + gph->height - (uint16_t)(heightPerSection*(float)i) + v_grid_lbl_comp_y, grid_lbl_txt_size, 0, buffer);
		}
	}

	/// ARROWS on axes
	//(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_AXISCOLOR);
	// bottom vertical arrow (Amplitude)
	(*EVE_cmd_dl__fptr_arr[burst])(DL_BEGIN | EVE_LINE_STRIP);
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding + arrow_offset, curY + arrow_offset ));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding               , curY                ));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding - arrow_offset, curY + arrow_offset ));
	(*EVE_cmd_dl__fptr_arr[burst])(DL_END);
	// bottom horizontal arrow (Time)
	(*EVE_cmd_dl__fptr_arr[burst])(DL_BEGIN | EVE_LINE_STRIP);
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding + gph->width + gph->padding - arrow_offset, curY + gph->padding + gph->height + arrow_offset ));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding + gph->width + gph->padding               , curY + gph->padding + gph->height                ));
	(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding + gph->width + gph->padding - arrow_offset, curY + gph->padding + gph->height - arrow_offset ));
	(*EVE_cmd_dl__fptr_arr[burst])(DL_END);

}

void TFT_graph_pixeldata_i(graph* gph, int_buffer_t buf[], uint16_t buf_size, uint16_t *buf_curidx, uint32_t datacolor){
	/// Write the dynamic parts of an Graph to the TFT (data and markers). Used at recurring display list build in TFT_display() completely coded by RS 02.01.2021.
	/// Every data-point is assumed to be at one pixel of the screen. Make this as wide as there are elements in the data-array.
	///
	///  x		...	beginning of left edge of the graph (Note that the vertical axis starts at "x+padding" and that some Grid values might be at a position prior to x). In full Pixels
	///  y		... beginning of upper edge of the graph (Note this is the position of the axis-arrow point and that the horizontal axis label might be at a position prior to y). In full Pixels
	///  width	... width of the actual graph data area in full Pixels
	///  height	... height of the actual graph data area in full Pixels
	///  padding	 ... clearance from the outer corners (x,y) to the axes
	///  y_max   	 ... maximum expected value of input (e.g. for 12bit ADC 4095), will represent 100%
	///  buf[] 		 ... Array of data values
	///  buf_size	 ... size of array of data values
	///  *buf_curidx ... current position (index of most recent value)
	///  graphmode 	 ... 0 = frame-mode, 1 = roll-mode
	///  datacolor 	 ... 24bit color (as 32 bit integer with leading 0's) used for the dataline
	///  markercolor ... 24bit color (as 32 bit integer with leading 0's) used for the current position line
	///  Note: No predefined graph settings are used direct (#define ...)!


	// Determine current position (with scroll value)
	uint16_t curY = gph->y - TFT_cur_ScrollV;


	/// Display current DATA as line strip in frame or roll mode
	EVE_cmd_dl_burst(DL_COLOR_RGB | datacolor);
	EVE_cmd_dl_burst(DL_BEGIN | EVE_LINE_STRIP);
	/// Display graph frame-mode
	if(gph->graphmode == 0){
		// Print values in the order they are stored
		for (int x_cur = 0; x_cur < buf_size; ++x_cur) {
			EVE_cmd_dl_burst(VERTEX2F(gph->x + gph->padding + x_cur, curY + gph->padding + gph->height - (uint16_t)(( ((float)buf[x_cur]) / ((float)gph->y_max) )*(float)(gph->height)) )); //if(frameover==1) printf("%lf %lf\n", ((((float)(buf[x_cur]))/((float)gph->y_max))*(float)(gph->height)), (float)buf[gph->x]);
		}
	}
	/// Display graph roll-mode
	else {
		// Print newest value always at the rightmost corner with all older values to the right
		// => Start Display    x position at rightmost corner and decrement till 0 (last run will make it -1 at the end but it isn't used after that)
		// => Start Arrayindex i at current index and decrement every loop. If i goes below 0, reset to max index and decrement further till
		//    value before current is hit.

		int16_t i = *buf_curidx;
		for (int16_t x_cur = buf_size-1; x_cur >= 0; x_cur--) {
			// if index goes below 0 set to highest buffer index
			if(i < 0){i = buf_size-1;}

			// Send next point for EVE_LINE_STRIP at current x+padding and normalized buffer value
			EVE_cmd_dl_burst(VERTEX2F(gph->x + gph->padding + x_cur, curY + gph->padding + gph->height - (uint16_t)(( ((float)buf[i]) / ((float)gph->y_max) )*(float)(gph->height)) )); 				// EVE_cmd_dl_burst(VERTEX2F(gph->x + gph->padding + x_cur, EVE_VSIZE - ((uint16_t)(buf[i]/y_div) + margin + gph->padding)));

			// decrement index
			i--;
		}
	}
	// End EVE_LINE_STRIP and therefore DATA
	EVE_cmd_dl_burst(DL_END);


	/// Draw current POSITION MARKER in frame mode
	if(gph->graphmode == 0){
		EVE_cmd_dl_burst(DL_COLOR_RGB | 0xff0000);
		EVE_cmd_dl_burst(DL_BEGIN | EVE_LINE_STRIP);
		EVE_cmd_dl_burst(VERTEX2F(gph->x + gph->padding + *buf_curidx, curY + gph->padding - 5 ));
		EVE_cmd_dl_burst(VERTEX2F(gph->x + gph->padding + *buf_curidx, curY + gph->padding + gph->height + 5 ));
		EVE_cmd_dl_burst(DL_END);
	}
	/////////////// GRAPH END

}


void TFT_graph_pixeldata_f(graph* gph, float_buffer_t buf[], uint16_t buf_size, uint16_t *buf_curidx, uint32_t datacolor){
	/// This is a copy of the above function! It taken a float buffer and will later be merged to its origin.
	/// TODO: Make this a "generic" function (C equivalent)


	// Determine current position (with scroll value)
	uint16_t curY = gph->y - TFT_cur_ScrollV;


	/// Display current DATA as line strip in frame or roll mode
	EVE_cmd_dl_burst(DL_COLOR_RGB | datacolor);
	EVE_cmd_dl_burst(DL_BEGIN | EVE_LINE_STRIP);
	/// Display graph frame-mode
	if(gph->graphmode == 0){
		// Print values in the order they are stored
		for (int x_cur = 0; x_cur < buf_size; ++x_cur) {
			EVE_cmd_dl_burst(VERTEX2F(gph->x + gph->padding + x_cur, curY + gph->padding + gph->height - (uint16_t)(( ((float)buf[x_cur]) / ((float)gph->y_max) )*(float)(gph->height)) )); //if(frameover==1) printf("%lf %lf\n", ((((float)(buf[x_cur]))/((float)gph->y_max))*(float)(gph->height)), (float)buf[gph->x]);
		}
	}
	/// Display graph roll-mode
	else {
		// Print newest value always at the rightmost corner with all older values to the right
		// => Start Display    x position at rightmost corner and decrement till 0 (last run will make it -1 at the end but it isn't used after that)
		// => Start Arrayindex i at current index and decrement every loop. If i goes below 0, reset to max index and decrement further till
		//    value before current is hit.

		int16_t i = *buf_curidx;
		for (int16_t x_cur = buf_size-1; x_cur >= 0; x_cur--) {
			// if index goes below 0 set to highest buffer index
			if(i < 0){i = buf_size-1;}

			// Send next point for EVE_LINE_STRIP at current x+padding and normalized buffer value
			EVE_cmd_dl_burst(VERTEX2F(gph->x + gph->padding + x_cur, curY + gph->padding + gph->height - (uint16_t)(( ((float)buf[i]) / ((float)gph->y_max) )*(float)(gph->height)) )); 				// EVE_cmd_dl_burst(VERTEX2F(gph->x + gph->padding + x_cur, EVE_VSIZE - ((uint16_t)(buf[i]/y_div) + margin + gph->padding)));

			// decrement index
			i--;
		}
	}
	// End EVE_LINE_STRIP and therefore DATA
	EVE_cmd_dl_burst(DL_END);


	/// Draw current POSITION MARKER in frame mode
	if(gph->graphmode == 0){
		EVE_cmd_dl_burst(DL_COLOR_RGB | 0xff0000);
		EVE_cmd_dl_burst(DL_BEGIN | EVE_LINE_STRIP);
		EVE_cmd_dl_burst(VERTEX2F(gph->x + gph->padding + *buf_curidx, curY + gph->padding - 5 ));
		EVE_cmd_dl_burst(VERTEX2F(gph->x + gph->padding + *buf_curidx, curY + gph->padding + gph->height + 5 ));
		EVE_cmd_dl_burst(DL_END);
	}
	/////////////// GRAPH END

}


void TFT_graph_stepdata(graph* gph, int_buffer_t cy_buf[], uint16_t cy_buf_size, float cx_step, uint32_t datacolor){
	/// Write the dynamic parts of an Graph to the TFT (data and markers). Used at recurring display list build in TFT_display() completely coded by RS 02.01.2021.
	///
	///
	///  x		...	beginning of left edge of the graph (Note that the vertical axis starts at "x+padding" and that some Grid values might be at a position prior to x). In full Pixels
	///  y		... beginning of upper edge of the graph (Note this is the position of the axis-arrow point and that the horizontal axis label might be at a position prior to y). In full Pixels
	///  width	... width of the actual graph data area in full Pixels
	///  height	... height of the actual graph data area in full Pixels
	///  padding	 ... clearance from the outer corners (x,y) to the axes
	///  y_max   	 ... maximum expected value of input (e.g. for 12bit ADC 4095), will represent 100%
	///  y_buf[] 		 ... Array of data values
	///  y_buf_size	 ... size of array of data values
	///  graphmode 	 ... 0 = frame-mode, 1 = roll-mode
	///  datacolor 	 ... 24bit color (as 32 bit integer with leading 0's) used for the dataline
	///  markercolor ... 24bit color (as 32 bit integer with leading 0's) used for the current position line
	///  Note: No predefined graph settings are used direct (#define ...)!


	// Determine current position (with scroll value)
	uint16_t curY = gph->y - TFT_cur_ScrollV;


	// Convert coordinate x_step to actual pixels per step
	uint16_t cx_step_px = gph->width / (gph->cx_max - gph->cx_initial) * cx_step;

	/// Display current DATA as line strip in frame or roll mode
	EVE_cmd_dl_burst(DL_COLOR_RGB | datacolor);
	EVE_cmd_dl_burst(DL_BEGIN | EVE_LINE_STRIP);
	/// Display graph
	// Print values in the order they are stored
	//for (int x_cur = 0; x_cur < y_buf_size; ++x_cur) {
	uint16_t x_cur = 0;
	for (int i = 0; i < cy_buf_size; i++) {
		// Write point
		EVE_cmd_dl_burst(
			VERTEX2F(
				gph->x + gph->padding + x_cur,
				  curY + gph->padding + gph->height - (uint16_t)(( ((float)cy_buf[i]) / ((float)gph->y_max) )*(float)(gph->height))
			)
		);
		// Add a step in x direction
		x_cur += cx_step_px;
	}

	// End EVE_LINE_STRIP and therefore DATA
	EVE_cmd_dl_burst(DL_END);
	/////////////// GRAPH END

}

void TFT_graph_XYdata(graph* gph, float cy_buf[], float cx_buf[], uint16_t buf_size, int16_t *buf_curidx, graphTypes gphTyp, uint32_t datacolor){
	/// Write the dynamic parts of an Graph to the TFT (data and markers). Used at recurring display list build in TFT_display() completely coded by RS 02.01.2021.
	///
	///
	///  x		...	beginning of left edge of the graph (Note that the vertical axis starts at "x+padding" and that some Grid values might be at a position prior to x). In full Pixels
	///  y		... beginning of upper edge of the graph (Note this is the position of the axis-arrow point and that the horizontal axis label might be at a position prior to y). In full Pixels
	///  width	... width of the actual graph data area in full Pixels
	///  height	... height of the actual graph data area in full Pixels
	///  padding	 ... clearance from the outer corners (x,y) to the axes
	///  y_max   	 ... maximum expected value of input (e.g. for 12bit ADC 4095), will represent 100%
	///  y_buf[] 	 ... Array of data values
	///  y_buf_size	 ... size of array of data values
	///  graphmode 	 ... 0 = frame-mode, 1 = roll-mode
	///  datacolor 	 ... 24bit color (as 32 bit integer with leading 0's) used for the dataline
	///  markercolor ... 24bit color (as 32 bit integer with leading 0's) used for the current position line
	///  Note: No predefined graph settings are used direct (#define ...)!


	// Determine current position (with scroll value)
	uint16_t curY = gph->y - TFT_cur_ScrollV;

	/// Display current DATA as line strip or as points based on graph type
	EVE_cmd_dl_burst(DL_COLOR_RGB | datacolor);
	switch(gphTyp){
		case graphLine:
			EVE_cmd_dl_burst(DL_BEGIN | EVE_LINE_STRIP);
			break;
		case graphPoint:
			EVE_cmd_dl_burst(POINT_SIZE(3*16)); // Size is in 1/16 pixel
			EVE_cmd_dl_burst(DL_BEGIN | EVE_POINTS);
			break;
	}

	/// Display graph
	// Print values in the order they are stored
	uint16_t cx_cur = 0;
	uint16_t cy_cur = 0;
	for (int i = 0; i < buf_size; i++) {
		//
		cx_cur = FLOAT_TO_INT16((float)gph->width / (gph->cx_max - gph->cx_initial) * cx_buf[i]);
		cy_cur = gph->height - FLOAT_TO_INT16(( (cy_buf[i]) / (gph->y_max) )*(float)(gph->height));

		// Write point
		EVE_cmd_dl_burst(
			VERTEX2F(
				gph->x + gph->padding + cx_cur,
				  curY + gph->padding + cy_cur
			)
		);
	}

	// Mark current point if graph mode point and index is given
	if(gphTyp == graphPoint && *buf_curidx != -1){
		EVE_cmd_dl_burst(DL_COLOR_RGB | RED);
		EVE_cmd_dl_burst(POINT_SIZE(4*16)); // Size is in 1/16 pixel
		cx_cur = FLOAT_TO_INT16((float)gph->width / (gph->cx_max - gph->cx_initial) * cx_buf[*buf_curidx]);
		cy_cur = gph->height - FLOAT_TO_INT16(( (cy_buf[*buf_curidx]) / (gph->y_max) )*(float)(gph->height));

		// Print current point
		EVE_cmd_dl_burst(
			VERTEX2F(
				gph->x + gph->padding + cx_cur,
				  curY + gph->padding + cy_cur
			)
		);
	}

	// End EVE_LINE_STRIP and therefore DATA
	EVE_cmd_dl_burst(DL_END);
	/////////////// GRAPH END

}

void TFT_graph_function(graph* gph, float* f_coefficients, uint8_t order, uint16_t step_divider, graphTypes gphTyp, uint32_t datacolor){
	/// Write the dynamic parts of an Graph to the TFT (data based on polynomial function coefficients). Used at recurring display list build in TFT_display() completely coded by RS 02.01.2021.
	/// WARNING!: Width must be divisible by step_divider! Otherwise the x-axis scale will be incorrect due to lost fractional pixel!!!
	///			  Convention: Always use a even width and exponents of 2 as step_divider (e.g. 200px or 440px with 2, 4, 8, ...)
	///
	///  gph			... The corresponding graph object
	///  f_coefficients	... A float array of coefficients (^0 to ^n) used as polynomial
	///  order			... Order/size of polynomial/f_coefficients
	///  datacolor 		... 24bit color (as 32 bit integer with leading 0's) used for the dataline
	///
	///  x		...	beginning of left edge of the graph (Note that the vertical axis starts at "x+padding" and that some Grid values might be at a position prior to x). In full Pixels
	///  y		... beginning of upper edge of the graph (Note this is the position of the axis-arrow point and that the horizontal axis label might be at a position prior to y). In full Pixels
	///  width	... width of the actual graph data area in full Pixels
	///  height	... height of the actual graph data area in full Pixels
	///  padding	 ... clearance from the outer corners (x,y) to the axes
	///  y_max   	 ... maximum expected value of input (e.g. for 12bit ADC 4095), will represent 100%
	///  y_buf[] 	 ... Array of data values
	///  y_buf_size	 ... size of array of data values



	// Determine current position (with scroll value)
	uint16_t curY = gph->y - TFT_cur_ScrollV;

	// Calculate actual pixels per step
	float steps = gph->width/step_divider;
	float cx_step = (gph->cx_max - gph->cx_initial) / steps;
	uint16_t cx_step_px = gph->width / steps;

	/// Display current DATA as line strip or as points based on graph type
	EVE_cmd_dl_burst(DL_COLOR_RGB | datacolor);
	switch(gphTyp){
		case graphLine:
			EVE_cmd_dl_burst(DL_BEGIN | EVE_LINE_STRIP);
			break;
		case graphPoint:
			EVE_cmd_dl_burst(POINT_SIZE(5*16)); // Size is in 1/16 pixel
			EVE_cmd_dl_burst(DL_BEGIN | EVE_POINTS);
			break;
	}

	/// Display graph
	// Print values in the order they are stored
	uint16_t cx_cur = 0;
	uint16_t cy_cur = 0;
	uint16_t tft_x_cur = 0;
	uint16_t tft_y_cur = 0;
	for (int i = 0; i < steps+1; i++) {
		// Calculate relative coordinates
		cx_cur = cx_step_px * i;
		cy_cur = gph->height - FLOAT_TO_INT16( (gph->height / (gph->y_max)) * poly_calc(cx_step*i, f_coefficients, order) );

		// Calculate absolute coordinates
		tft_x_cur = gph->x + gph->padding + cx_cur;
		tft_y_cur = curY + gph->padding + cy_cur;

		// Only print point if it is inside the graph and inside the TFT area
		if((cy_cur > 0 && cy_cur < gph->height) && (tft_y_cur > TFT_UpperBond && tft_y_cur < EVE_VSIZE) ){
			// Write point
			EVE_cmd_dl_burst(
				VERTEX2F(
					tft_x_cur,
					tft_y_cur
				)
			);
		}

	}

	// End EVE_LINE_STRIP and therefore DATA
	EVE_cmd_dl_burst(DL_END);
	/////////////// GRAPH END

}




void touch_calibrate(uint8_t startCalibrate){
	/// Sends pre-recorded touch calibration values. Customized for EVE_RiTFT43. Created by Rudolph Riedel, adapted by RS @ MCI 2020/21
	/// Note: All targets other than EVE_RiTFT43 where deleted -> see original lib


	// Actual values (measured with routine down below) by Rene S. at 20.12.2020 on Display SM-RVT43ULBNWC01 2031/20/609 00010
	EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000061c4);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0x0000025d);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff14ab1);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0xffffff91);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00005b6b);
	EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xFFFac7ab);
	// Original library values
	//EVE_memWrite32(REG_TOUCH_TRANSFORM_A, 0x000062cd);
	//EVE_memWrite32(REG_TOUCH_TRANSFORM_B, 0xfffffe45);
	//EVE_memWrite32(REG_TOUCH_TRANSFORM_C, 0xfff45e0a);
	//EVE_memWrite32(REG_TOUCH_TRANSFORM_D, 0x000001a3);
	//EVE_memWrite32(REG_TOUCH_TRANSFORM_E, 0x00005b33);
	//EVE_memWrite32(REG_TOUCH_TRANSFORM_F, 0xFFFbb870);

	// Used to re-calibrate touch display
	// Write down the numbers on the screen and either place them in pre-defined block above or make a new block
	if(startCalibrate){
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
	}
}

uint8_t TFT_init(void) {
	/// Initializes EVE (or checks if its already initialized). Only at first successful EVE_init() the tft is set active, backlight is set to medium brightness, the pre-elaborated touch calibration is loaded and the static Background is initiated. Created by Rudolph Riedel, adapted by RS @ MCI 2020/21

	// Initialize EVE and initialize TFT if that is OK
	if(EVE_init() != 0)
	{
		#if defined (DEBUG_ENABLE)
			printf("TFT_init - EVE_init was ok, starting to init TFT!\n");
		#endif
		// Mark Display as active (TFT_display only does anything after his)
		tft_active = 1;

		// Initial Backlight strength
		EVE_memWrite8(REG_PWM_DUTY, 0x30);	/* setup backlight, range is from 0 = off to 0x80 = max */

		// Write prerecorded touchscreen calibration back to display
		touch_calibrate(0);

		// Clear screen, set precision for VERTEX2F to 1 pixel and show DL for the first time
		EVE_start_cmd_burst(); /* start writing to the cmd-fifo as one stream of bytes, only sending the address once */
		EVE_cmd_dl_burst(CMD_DLSTART); /* start the display list */
		EVE_cmd_dl_burst(TAG(0)); /* do not use the following objects for touch-detection */
		EVE_cmd_bgcolor_burst(mainBgColor);
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

void TFT_display_init_screen(void) {
	/// Display logo over full screen.

	// If tft is active (TFT_init was executed and didn't fail) load initial logo and show DL
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



void TFT_display_static(void) {
	/// Static portion of display-handling, meant to be called once at every change of menu. Created by Rudolph Riedel, adapted by RS @ MCI 2020/21
	/// It creates a fresh display list, sets the background, loads the static part of the currently to be shown menu and copy's the display list to the rear end of the TFT RAM.
	/// This copy can than be used during TFT_display(). Purpose: The parts of the display that do not change must only be written to the display once, which decreases the main uC time cost.
	///
	///	 Uses tft-global variables:
	///		EVE Library ...
	///		Colors:  mainBgColor
	///		TFT_display_static_cur_Menu__fptr_arr:	Function pointer for menu specific TFT_display_static function
	///		TFT_last_MenuIdx, TFT_cur_MenuIdx				Index of last and current menu
	///		num_dl_static							Current size of display list
	///		MEM_DL_STATIC							Start position where static DL shall be stored



	// Start Burst (start writing to the cmd-fifo as one stream of bytes, only sending the address once)
	EVE_start_cmd_burst();

	// Start a new display list (resets REG_CMD_DL to 0)
	EVE_cmd_dl_burst(CMD_DLSTART);

	/// Main settings
	EVE_cmd_dl_burst(TAG(1)); /* give everything considered background area tag 1 -> used for wipe feature*/
	EVE_cmd_bgcolor_burst(mainBgColor); /* light grey */
	EVE_cmd_dl_burst(VERTEX_FORMAT(0)); /* reduce precision for VERTEX2F to 1 pixel instead of 1/16 pixel default */
	// Main Rectangle
	EVE_cmd_dl_burst(DL_COLOR_RGB | mainBgColor);
	EVE_cmd_dl_burst(DL_BEGIN | EVE_RECTS);
	EVE_cmd_dl_burst(VERTEX2F(0, 0));
	EVE_cmd_dl_burst(VERTEX2F(EVE_HSIZE, EVE_VSIZE));
	EVE_cmd_dl_burst(DL_END);



	/////////////// Execute current menu specific code
	(*TFT_display_static_cur_Menu__fptr_arr[TFT_cur_MenuIdx])();



	EVE_end_cmd_burst(); /* stop writing to the cmd-fifo, the cmd-FIFO will be executed automatically after this or when DMA is done */

	// Wait for Display to be finished
	while (EVE_busy());

	// Get size of the current display list
	num_dl_static = EVE_memRead16(REG_CMD_DL); // REG_CMD_DL = Command display list offset

	// Copy "num_dl_static" bytes from pointer "EVE_RAM_DL" to pointer "MEM_DL_STATIC"
	EVE_cmd_memcpy(MEM_DL_STATIC, EVE_RAM_DL, num_dl_static);
	while (EVE_busy());

	// The menu is now established and can be set as last known menu
	TFT_last_MenuIdx = TFT_cur_MenuIdx;
	TFT_last_ScrollV = TFT_cur_ScrollV;
	TFT_refresh_static = 0;
}

void TFT_touch(void)
{
	/// Check for touch events and evaluate them. Manages swipe/keyboard feature and control vars for TFT_display() (therefore it must initially run before it).
	/// Meant to be called more often that TFT_display() from main. Divided into 5 parts:
	/// 	Read from TFT, evaluate swipe feature, evaluate keypad feature, evaluate global tags and run menu specific code.
	///
	///	 Uses tft-global variables:
	///		EVE Library ...
	///		Swipe feature:  	swipeInProgress, swipeEvokedBy, swipeInitialTouch_X, swipeInitialTouch_Y, swipeDistance_X, swipeDistance_Y, swipeEndOfTouch_Debounce. (all of them)
	///		Keypad feature: 	keypadActive, keypadEvokedBy, keypadKeypressFinished, keypadCurrentKey, keypadShiftActive, keypadEndOfKeypress_Debounce. (all of them)
	///		toggle_lock:		Used to react to an button-press (not keypress!) only after it is fished and animate the button accordingly
	///		swipeDetect:		Used to detect a swipe and react to it (ain't listed in 'Swipe feature' because this can be used by any code block that evaluates swipes)
	///		TFT_touch_cur_Menu__fptr_arr: Function pointer for menu specific TFT_touch function
	///		TFT_MAIN_MENU_SIZE:	States to where the main menus (accessible via swipe an background) are listed (see menu.h!)



	// Init vars
	uint8_t tag; // temporary store of received touched tag
	
	// If Display is still busy, skip this evaluation to prevent hanging, glitches and flickers
	if(EVE_busy()) { return; } /* is EVE still processing the last display list? */

	/// Read values from TFT
	// Read the value for the first touch point
	tag = EVE_memRead8(REG_TOUCH_TAG);
	// Read currently touched position in pixels
	uint32_t TouchXY = EVE_memRead32(REG_TOUCH_SCREEN_XY); // Currently touched X and Y coordinate from register (1.byte=X, 2.byte=Y)
	uint16_t Y = TouchXY;
	uint16_t X = TouchXY >> 16;


	/// If a swipe is in progress, save start coordinates (once at start), calculate swipe distance and detect the end-of-touch (debounced).
	/// A swipe gets started by a touch tag action (switch statement below) and lasts till no touch was detected for at least 5 executions (debounce).
	/// At an end-of-touch the evoking tag is run one again so it can execute final actions.
	/// This code block manages the swipe itself but the effect of it must be implemented inside the tag-switch (here at global TFT_touch or in menu specific TFT_touch)
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

		// If nothing is touched detect possible end-of-touch (debounceed)
		if(tag == 0){
			// Increase end-of-touch debounce timer
			swipeEndOfTouch_Debounce++;

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
	}


	/// If the keypad is active, check if a keypress is finished or a new keypress must be registered
	/// The Keyboard gets started by menu specific code (calling keypad_open([tag of evoker],[type of keypad]) and lasts till it's disabled by keypad_close().
	/// Any function that deals with the keypad output MUST mark a keypress as handled afterwards by reseting keypadKeypressFinished and keypadCurrentKey to 0!
	// When keyboard is activated the keypadInitialLock is set so it only starts accepting input after the finger is first lifted (otherwise it would recognize the random button the aligns with the activating button as a keypress)
	if(keypadActive && keypadInitialLock == 0){
		// If there is an unprocessed current key but nothing is touched anymore (key was just released) - evaluate if it was shift or run menu specific code
		if(tag == 0 && keypadCurrentKey != 0){
			// Increase end-of-keypress debounce timer
			keypadEndOfKeypress_Debounce++;

			// Detect end-of-keypress (keypress ends if there was no touch detected for 5 cycles)
			if(keypadEndOfKeypress_Debounce >= 5){
				// Shift was pressed - change shift-active and reset current-key
				if(keypadCurrentKey == 94){
					keypadShiftActive = !keypadShiftActive;
					keypadCurrentKey = 0;
				}
				// Every key but shift
				else{
					// Mark the last keypress as finished (will make the menu specific touch code run) and reset debounce counter
					keypadKeypressFinished = 1;
					keypadEndOfKeypress_Debounce = 0;
				}
			}
		}
		// Register current keypress if the tag is in character range
		else if(tag >= 32 && tag <= 128){
			// Reset debounce counter
			keypadEndOfKeypress_Debounce = 0;

			// Register new keypress
			keypadCurrentKey = tag;
			tag = 0;
		}
	}


	/// Execute action based on touched tag
	switch(tag)
	{
		// nothing touched - reset states and locks
		case 0:
			toggle_lock = 0;

			// When keyboard is activated the keypadInitialLock is set so it only starts accepting input after the finger is first lifted (otherwise it would recognize the random button the aligns with the activating button as a keypress)
			keypadInitialLock = 0;
			break;
		// Background elements are touched - detect swipes to left/right for menu changes
		case 1:
			// Change menu on swipe (only if current menu is a main menu - no not allow swipe in submenu's)
			if(TFT_cur_MenuIdx < TFT_MAIN_MENU_SIZE){
				/// Init a new swipe - if it isn't already running (and no end-of-touch of a previous swipe is detected)
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
					if(swipeDetect == Left && TFT_cur_MenuIdx < (TFT_MAIN_MENU_SIZE-1))
						TFT_setMenu(TFT_cur_MenuIdx+1);
					else if(swipeDetect == Right && TFT_cur_MenuIdx > 0)
						TFT_setMenu(TFT_cur_MenuIdx-1);

					// Finalize swipe by resetting swipeEvokedBy
					swipeEvokedBy = 0;
				}
			}
			break;
	}

	/////////////// Execute current menu specific code if a non global tag is touched or a Keypress is finished and can be processed
	if(tag > 1 || keypadKeypressFinished)
		(*TFT_touch_cur_Menu__fptr_arr[TFT_cur_MenuIdx])(tag, &toggle_lock, swipeInProgress, &swipeEvokedBy, &swipeDistance_X, &swipeDistance_Y);
}

void TFT_display(void)
{
	/// Dynamic portion of display-handling, meant to be called less often that TFT_touch() from main (at least every 20ms for min 50FPS). Created by Rudolph Riedel, extensively adapted by RS @ MCI 2020/21
	/// It manages the creates a fresh display list, manages the static part (let it be build or uses it) of the display list and shows the menu specific code as well as the keyboard.
	/// It is divided into following parts:
	///		Let the static part be created if necessary, get values from TFT (extern), copy's the previously saved static part of the current menu from the rear end of the TFT RAM to the beginning,
	///		let menu specific code run, draw keypad in its current state if needed and finish display list.
	///
	///	 Uses tft-global variables:
	///		EVE Library ...
	///		Colors:  WHITE, GREY, keypadControlKeyFgColor, keypadControlKeyBgColor
	///		Keypad feature: keypadActive, keypadCurrentKey, keypadShiftActive
	///		tft_active							Marker if library was initialized successfully
	///		TFT_display_cur_Menu__fptr_arr:		Function pointer for menu specific TFT_display function
	///		TFT_last_MenuIdx, TFT_cur_MenuIdx			Index of last and current menu
	///		num_dl_static						Current size of display list
	///		MEM_DL_STATIC						Start position where static DL shall be stored

	// If tft is active (TFT_init was executed and didn't fail) load initial logo and show DL
	if(tft_active != 0)
	{
		// Setup static part of the current menu - only needed once when the menu is changed
		// ToDo: Do menu changes and scroll only with TFT_setMenu (or similar) and set TFT_refresh_static there. This way the additional 2 ORs dont have to be determined every time
		if(TFT_refresh_static || TFT_last_MenuIdx != TFT_cur_MenuIdx || TFT_last_ScrollV != TFT_cur_ScrollV)
			TFT_display_static();

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
		(*TFT_display_cur_Menu__fptr_arr[TFT_cur_MenuIdx])();

		// Keypad
		if(keypadActive){
			// Background Rectangle
			EVE_cmd_dl_burst(DL_COLOR_RGB | GREY);
			EVE_cmd_dl_burst(DL_BEGIN | EVE_RECTS);
			EVE_cmd_dl_burst(VERTEX2F(0, EVE_VSIZE-2-29-(32*4)-2));
			EVE_cmd_dl_burst(VERTEX2F(EVE_HSIZE, EVE_VSIZE));
			EVE_cmd_dl_burst(DL_END);

			// Keys
			EVE_cmd_dl_burst(TAG(0));
			// Show different keypads based on type
			int8_t showShiftKey;
			int8_t showSpaceKey;
			switch (keypadType) {
				case Filename:
					// keypadType = Filename
					if(keypadShiftActive == 0){
						EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*4), EVE_HSIZE-4-50-4, 29, 26, keypadCurrentKey, "1234567890");
						EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*3), EVE_HSIZE-4, 29, 26, keypadCurrentKey, "qwertyuiop");
						EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*2), EVE_HSIZE-4, 29, 26, keypadCurrentKey, "asdfghjkl");
						EVE_cmd_keys_burst(2+50+4, EVE_VSIZE-2-29-(32*1), EVE_HSIZE-4-50-4, 29, 26, keypadCurrentKey, "zxcvbnm._");
					}
					else{
						EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*4), EVE_HSIZE-4-50-4, 29, 26, keypadCurrentKey, "1234567890");
						EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*3), EVE_HSIZE-4, 29, 26, keypadCurrentKey, "QWERTYUIOP");
						EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*2), EVE_HSIZE-4, 29, 26, keypadCurrentKey, "ASDFGHJKL");
						EVE_cmd_keys_burst(2+50+4, EVE_VSIZE-2-29-(32*1), EVE_HSIZE-4-50-4, 29, 26, keypadCurrentKey, "ZXCVBNM#-");
					}
					showShiftKey = 1;
					showSpaceKey = 1;
					break;
				case Numeric:
					// keypadType = Numeric
					EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*4), EVE_HSIZE-4-50-4, 29, 26, keypadCurrentKey, "789");
					EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*3), EVE_HSIZE-4-50-4, 29, 26, keypadCurrentKey, "456");
					EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*2), EVE_HSIZE-4-50-4, 29, 26, keypadCurrentKey, "123");
					EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*1), 138+138+4, 29, 26, keypadCurrentKey, "0");
					EVE_cmd_keys_burst(2+138+4+138+4-1, EVE_VSIZE-2-29-(32*1), 138+1, 29, 26, keypadCurrentKey, ".");
					showShiftKey = 0;
					showSpaceKey = 0;
					break;
				default:
					// keypadType = Standard
					if(keypadShiftActive == 0){
						EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*4), EVE_HSIZE-4-50-4, 29, 26, keypadCurrentKey, "1234567890");
						EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*3), EVE_HSIZE-4, 29, 26, keypadCurrentKey, "qwertyuiop");
						EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*2), EVE_HSIZE-4, 29, 26, keypadCurrentKey, "asdfghjkl");
						EVE_cmd_keys_burst(2+50+4, EVE_VSIZE-2-29-(32*1), EVE_HSIZE-4-50-4, 29, 26, keypadCurrentKey, "zxcvbnm._");
					}
					else{
						EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*4), EVE_HSIZE-4-50-4, 29, 26, keypadCurrentKey, "!\"%&/()=?\\");
						EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*3), EVE_HSIZE-4, 29, 26, keypadCurrentKey, "QWERTYUIOP");
						EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*2), EVE_HSIZE-4, 29, 26, keypadCurrentKey, "ASDFGHJKL");
						EVE_cmd_keys_burst(2+50+4, EVE_VSIZE-2-29-(32*1), EVE_HSIZE-4-50-4, 29, 26, keypadCurrentKey, "ZXCVBNM:-");
					}
					showShiftKey = 1;
					showSpaceKey = 1;
					break;
			}


			// Control Keys in different color
			EVE_cmd_fgcolor_burst(keypadControlKeyFgColor); //MAIN_BTNCTSCOLOR
			EVE_cmd_bgcolor_burst(keypadControlKeyBgColor); //MAIN_BTNCOLOR

			// Left/Right, Space and Shift Key
			EVE_cmd_keys_burst(2, EVE_VSIZE-2-29, 90, 29, 26, keypadCurrentKey, "<>");
			if(showSpaceKey == 1)
				EVE_cmd_keys_burst(2+30+4+60, EVE_VSIZE-2-29, 288, 29, 26, keypadCurrentKey, " ");
			if(showShiftKey == 1)
				EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*1), 50, 29, 26, keypadCurrentKey, "^");

			// Backspace
			EVE_cmd_dl_burst(TAG(127));
			if(keypadCurrentKey == 127)
				EVE_cmd_button_burst(EVE_HSIZE-50, EVE_VSIZE-2-29-(32*4), 50, 29, 19, EVE_OPT_FLAT, &backspace_char);
			else
				EVE_cmd_button_burst(EVE_HSIZE-2-50, EVE_VSIZE-2-29-(32*4), 50, 29, 19, 0, &backspace_char);

			// Enter Button
			EVE_cmd_dl_burst(TAG(128));
			EVE_cmd_button_burst(2+30+4+60+288+4, EVE_VSIZE-2-29, EVE_HSIZE-2-30-4-60-288-2-4, 29, 20, 0, "OK");
		}


		/////////////// Finish Display list and burst
		EVE_cmd_dl_burst(DL_DISPLAY); /* instruct the graphics processor to show the list */
		EVE_cmd_dl_burst(CMD_SWAP); /* make this list active */

		EVE_end_cmd_burst(); /* stop writing to the cmd-fifo, the cmd-FIFO will be executed automatically after this or when DMA is done */
	}
}
