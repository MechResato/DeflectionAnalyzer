/*
@file    		tft.c
@brief   		Implementation of display communication using the EVE Library. Meant to display a menu and a dynamic graph (implemented for XMC4700 and DAVE)
@version 		2.0 (base lib version was 1.13)
@date    		2020-09-05
@initialauthor  Rudolph Riedel
@author 		RS (Rene Santeler & Stefan Reinmüller) @ MCI 2020/21 (initially created by Rudolph Riedel, completely reworked by RS )

@section History
2.0 (adapted from Rudolph Riedel base version 1.13 - below changes are from RS 2020/21)
- Added color scheme, adaptable banner, dynamic graph implementation (TFT_GraphStatic & TFT_GraphData), a display init which adds the static part of a graph to static DL (TFT_GraphStatic), a display init to show a bitmap (TFT_display_init_screen), ...
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
#include "tft.h"

// Memory-map definitions
#define MEM_LOGO 0x00000000 // start-address of logo, needs about 20228 bytes of memory. Will be written by TFT_display_init_screen and overwritten by TFT_GraphStatic (and everything else...)
#define MEM_DL_STATIC (EVE_RAM_G_SIZE - 4096) // 0xff000 -> Start-address of the static part of the display-list. Defined as the upper 4k of gfx-mem (WARNING: If static part gets bigger than 4k it gets clipped. IF dynamic part gets so big that it touches the last 4k (1MiB-4k) it overwrites the static part)
static uint32_t num_dl_static; // amount of bytes in the static part of our display-list



/////////// General Variables
static uint8_t tft_active = 0;  // Prevents TFT_display of doing anything if EVE_init isn't successful of TFT_init wasn't called
// Define a array of function pointers for every used "EVE_cmd_dl..." function. First one is normal, second one is to be used within a burst mode
static void (*EVE_cmd_dl__fptr_arr[])(uint32_t) = {EVE_cmd_dl, EVE_cmd_dl_burst};
static void (*EVE_cmd_text__fptr_arr[])(int16_t, int16_t, int16_t, uint16_t, const char*) = {EVE_cmd_text, EVE_cmd_text_burst};
static void (*EVE_cmd_number__fptr_arr[])(int16_t, int16_t, int16_t, uint16_t, int32_t) = {EVE_cmd_number, EVE_cmd_number_burst};
/////////// Button states
static uint8_t toggle_lock = 0; // "Debouncing of touches" -> If something is touched, this is set to prevent button evaluations. As soon as the is nothing pressed any more this is reset to 0



/////////// Menu function pointers - At the end of the TFT_display_static, TFT_display and TFT_touch the function referenced to this pointer is executed
extern menu* Menu_Objects[TFT_MENU_SIZE];
extern void (*TFT_display_cur_Menu__fptr_arr[TFT_MENU_SIZE])(void);
extern void (*TFT_touch_cur_Menu__fptr_arr[TFT_MENU_SIZE])(uint8_t tag, uint8_t* toggle_lock, uint8_t swipeInProgress, uint8_t *swipeEvokedBy, int32_t *swipeDistance_X, int32_t *swipeDistance_Y);
extern void (*TFT_display_static_cur_Menu__fptr_arr[TFT_MENU_SIZE])(void);
// TFT_MENU_SIZE is declared in menu.c and must be changed if menus are added or removed
// TFT_MAIN_MENU_SIZE is declared in menu.c. It states to where the main menus (accessible via swipe an background) are listed. All higher menus are considered submenus (control on how to get there is on menu.c)
static int8_t TFT_cur_MenuIdx = 2; // Index of currently used menu (TFT_display,TFT_touch).
static int8_t TFT_last_MenuIdx = -1; // Index of last used menu (TFT_display_static). If this differs from TFT_cur_MenuIdx the initial TFT_display_static function of the menu is executed. Also helpful to determine what was the last menu during the TFT_display_static.
#ifndef MAIN_BTNCTSCOLOR
#define MAIN_BTNCTSCOLOR	0xAD9666
#endif
#ifndef MAIN_BTNCOLOR
#define MAIN_BTNCOLOR		0xEAA92B
#endif
#ifndef MAIN_BGCOLOR
#define MAIN_BGCOLOR		0xF5F1EE
#endif
static uint32_t keypadControlKeyBgColor = MAIN_BTNCOLOR;
static uint32_t keypadControlKeyFgColor = MAIN_BTNCTSCOLOR;
static uint32_t mainBgColor = MAIN_BGCOLOR;

/////////// Scroll feature
static int16_t TFT_cur_ScrollV = 0;
static int16_t TFT_last_ScrollV = 0;
static int16_t TFT_UpperBond = EVE_VSIZE; 	// Defines up to which point elements are displayed. Needed to not scroll elements from main area into header or similar. Set by TFT_header, used by all element display functions.




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
uint8_t textbox_cursor_pos = 2;
#define TEXTBOX_ACTIVE_TARGET_HEIGTH 72	// target offset of the text from upper border of the TFT (position so that it can easily be edited/seen) in pixel
char buf[100] = ""; // Common string buffer for functions like str_insert

////// TODO: Early on all display_static functions were build to be able to use or not use burst mode. for now they are set to always use  burst. If no problems with this occur the functions can be stripped of this functionality




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
	strncpy(buf, target, pos);

	// Copy the rest of the text one character behind the new on
	strcpy(&buf[pos+1], &target[pos]);

	// Add new character
	buf[pos] = ch;

	// Copy buffer back to target
	strcpy(target,buf);

	// Increase length
	(*len)++;
}

void TFT_setMenu(uint8_t idx){
	// Set upper bond
	TFT_UpperBond = Menu_Objects[idx]->upperBond;
	//if(keypadActive) keypad_close();

	TFT_cur_MenuIdx = idx;

}

void TFT_setColor(uint8_t burst, uint32_t textColor, uint32_t fgColor, uint32_t bgColor, uint32_t gradColor){
	/// Write the to be used colors to the TFT. Used by all graphic elements.
	/// Parameters set to 1 will be ignored!
	///	TODO: This is not the best solution
	///
	///  textColor	...
	///  fgColor	...
	///	 bgColor	...
	///
	///	 Uses tft-global variables:
	///		EVE Library ...

	if(burst){
		if(textColor != 1)
			EVE_cmd_dl_burst(DL_COLOR_RGB | textColor);
		if(fgColor != 1)
			EVE_cmd_fgcolor_burst(fgColor);
		if(bgColor != 1)
			EVE_cmd_bgcolor_burst(bgColor);
		if(gradColor != 1)
			EVE_cmd_gradcolor_burst(gradColor);
	}
	else{
		if(textColor != 1)
			EVE_cmd_dl(DL_COLOR_RGB | textColor);
		if(fgColor != 1)
			EVE_cmd_fgcolor(fgColor);
		if(bgColor != 1)
			EVE_cmd_bgcolor(bgColor);
		if(gradColor != 1)
					EVE_cmd_gradcolor(gradColor);
	}
}
void TFT_control(control* ctrl){
	/// Write a user control element (button/toggle) to the TFT. Adapts the y coordinate automatically to current vertical scroll! (Use EVE_cmd_... command direct if wanted otherwise)
	/// Meant to be used at recurring display list build in TFT_display().
	/// Note: Use TFT_setColor(...) before calling this!
	///
	///  x
	///  y
	///	 ...
	///  font
	///  textColor	Color of the text
	///  text		Name of the current menu (Header)
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
	}
}

//void TFT_header_static(uint8_t burst, uint16_t layout[], uint32_t bannerColor, uint32_t dividerColor, uint32_t headerColor, char* headerText){
void TFT_header_static(uint8_t burst, menu* men){
	/// Write the non-dynamic parts of an textbox to the TFT (background & frame). Can be used once during init of a static background or at recurring display list build in TFT_display()
	///
	///  burst			Determines if the normal or the burst version of the EVE Library is used to transmit DL command (0 = normal, 1 = burst).
	///  layout			Banner line strip edge positions. Array of 4 elements [Y1,X1,Y2,X2] (from left to right: Y1 is held horizontal till X1, increasing till X2/Y2 and finally held horizontal at Y2 till EVE_HSIZE)
	///  bannerColor	Color of the banner surface
	///  dividerColor	Color of the line at the edge of the banner
	///  headerColor	Color of the text
	///  headerText		Name of the current menu (Header)
	///
	///	 Uses tft-global variables:
	///		EVE Library ...
	///		EVE_cmd_dl__fptr_arr, EVE_cmd_text__fptr_arr	Function pointer for "EVE_cmd_dl..." function with or without burst
	///		TEXTBOX_HEIGTH



	// Set upper bond
	//if(layout[0] > layout[2])
	//	TFT_UpperBond = layout[0];
	//else
	//	TFT_UpperBond = layout[2];

	/// Draw Banner and divider line on top
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

	(*EVE_cmd_dl__fptr_arr[burst])(TAG(0)); /* do not use the following objects for touch-detection */
}

//void TFT_label(uint8_t burst, uint16_t x, uint16_t y, uint8_t font, uint32_t textColor, char* text){
void TFT_label(uint8_t burst, label* lbl){
	/// Write a text/label to the TFT. Can be used once during init of a static background TFT_display_static() or at recurring display list build in TFT_display()
	///
	///  burst		Determines if the normal or the burst version of the EVE Library is used to transmit DL command (0 = normal, 1 = burst).
	///  x
	///  y
	///  font
	///  textColor	Color of the text
	///  text		Name of the current menu (Header)
	///
	///	 Uses tft-global variables:
	///		EVE Library ...
	///		EVE_cmd_dl__fptr_arr, EVE_cmd_text__fptr_arr	Function pointer for "EVE_cmd_dl..." function with or without burst
	///		TFT_cur_ScrollV, TFT_UpperBond


	uint16_t curY = lbl->y;
	if(lbl->ignoreScroll == 0){
		// Determine current position (with scroll value)
		curY -= TFT_cur_ScrollV;
	}

	// Only show label if it is inside display
	if(lbl->ignoreScroll || (curY > TFT_UpperBond && curY < EVE_VSIZE)){
		// Add text
		(*EVE_cmd_dl__fptr_arr[burst])(TAG(0)); /* do not use the following objects for touch-detection */
		//(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | textColor);
		(*EVE_cmd_text__fptr_arr[burst])(lbl->x, curY, lbl->font, lbl->options, lbl->text);
	}
}


void TFT_textbox_static(uint8_t burst, textbox* tbx){
	/// Write the non-dynamic parts of an textbox to the TFT (background & frame). Can be used once during init of a static background or at recurring display list build in TFT_display()
	///
	///  burst		Determines if the normal or the burst version of the EVE Library is used to transmit DL command (0 = normal, 1 = burst).
	///  x			Position of left textbox edge. In full Pixels
	///  y			Position of upper textbox edge. In full Pixels
	///  width		Width of the actual textbox data area in full Pixels
	///  tag		To be assigned tag for touch recognition
	///
	///	 Uses tft-global variables:
	///		EVE Library ...
	///		EVE_cmd_dl__fptr_arr	Function pointer for "EVE_cmd_dl..." function with or without burst
	///		TEXTBOX_HEIGTH
	///		TFT_cur_ScrollV, TFT_UpperBond


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


		/// Set tag
		(*EVE_cmd_dl__fptr_arr[burst])(TAG(tbx->mytag));

		/// Background
		(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | 0xFFFFFFUL);
		(*EVE_cmd_dl__fptr_arr[burst])(DL_BEGIN | EVE_RECTS);
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(abs_x_left , abs_y_top   ));
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(abs_x_right, abs_y_bottom));
		(*EVE_cmd_dl__fptr_arr[burst])(DL_END);

		/// Frame
		(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | 0x000000UL);
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
	///
	///  mytag			The tag of the current control element (code only responds if keypadEvokedBy is mytag)
	///  text			The string that shall be manipulated
	///  text_maxlen	Maximum length of the manipulated text (this will not add characters if length is at maximum)
	///  text_curlen	Current length of the manipulated text (this will change length according to input)
	///
	///	 Uses tft-global variables:
	///		EVE Library ...
	///		keypad:  keypadActive, keypadEvokedBy, keypadCurrentKey, keypadKeypressFinished
	///		textbox_cursor_pos
	///
	///  Limitations: TFT_touch() must run prior to this! Check TAG ASSIGNMENT above - this refers to the convention given there! Set textbox_cursor_pos initial before entering this the first time.



	/// If the keyboard is active and evoked by the current textbox, manipulate text according to input
	if(keypadActive && keypadEvokedBy == tbx->mytag){
		/// Manipulate text according to keypad input
		if(keypadKeypressFinished && keypadCurrentKey >= 32 && keypadCurrentKey <= 128){
			// Enter key
			if(keypadCurrentKey == 128){
				// Close/reset keypad
				//keypad_close();
				TFT_textbox_setStatus(tbx, 0, 0);
			}
			else {
				// Backspace key
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
				// Move cursor Left (if cursor is inside of string)
				else if(keypadCurrentKey == 60){
					if(textbox_cursor_pos > 0)
						textbox_cursor_pos--;
				}
				// Move cursor Right (if cursor is inside of string)
				else if(keypadCurrentKey == 62){
					if(textbox_cursor_pos < *(tbx->text_curlen))
						textbox_cursor_pos++;
				}
				// Add character if the string isn't at max length
				else if(*(tbx->text_curlen) < ((tbx->text_maxlen)-1)){
						str_insert(tbx->text, (tbx->text_curlen), (char)keypadCurrentKey, textbox_cursor_pos);
						textbox_cursor_pos++;
				}
			}

			// Mark keypress as handled (we did what we had to do, now we wait till the next one comes)
			keypadKeypressFinished = 0;
			keypadCurrentKey = 0;
		}

	}
	//else if(tbx->active == 1){
	//	// Keypad was aborted - deactivate textbox
	//	TFT_textbox_setStatus(tbx, 0, 0);
	//}
}

void TFT_textbox_display(textbox* tbx){
	/// Write the dynamic parts of an textbox to the TFT (text & cursor). Used at recurring display list build in TFT_display()
	///
	///  x				Position of left textbox edge. In full Pixels
	///  y				Position of upper textbox edge. In full Pixels
	///  mytag			The tag of the current control element (cursor is only shown if keypadEvokedBy is mytag)
	///  text			The string that shall be displayed
	///
	///	 Uses tft-global variables:
	///		EVE Library ...
	///		keypad:  keypadActive, keypadEvokedBy
	///		textbox_cursor_pos
	///		TEXTBOX_PAD_V, TEXTBOX_PAD_H
	///
	///  Limitations: Only made for font size 26. Make sure the text can't be longer than the textbox width



	// Buffers and control variables (static: will keep value between function executions)
	static char outputBuffer[100] = ""; // Copy of 'text' with added cursor
	static char cursor = '|';
	static uint32_t lastBlink = 0;

	// Determine current position (with scroll value)
	uint16_t curY = tbx->y + TEXTBOX_PAD_V - TFT_cur_ScrollV;

	// Only show text if it is inside display
	if(curY > TFT_UpperBond && curY < EVE_VSIZE){

		/// Set tag
		EVE_cmd_dl_burst(TAG(tbx->mytag));

		/// Set text color
		EVE_cmd_dl_burst(DL_COLOR_RGB | 0x000000UL);

		/// If the keyboard is active, show textbox text with cursor - else just write text directly
		if(keypadActive && keypadEvokedBy == tbx->mytag){ //
			if(SYSTIMER_GetTime() > (lastBlink + 550000)){
				lastBlink = SYSTIMER_GetTime();
				if(cursor == '|')
					cursor = ' ';
				else
					cursor = '|';
			}

			/// Copy text to buffer but add the cursor
			// Copy actual text previous to the new char to the buffer
			strncpy(outputBuffer, tbx->text, textbox_cursor_pos);
			// Add cursor
			outputBuffer[textbox_cursor_pos] = cursor;
			// Copy rest of string
			strcpy(&outputBuffer[textbox_cursor_pos+1], &(tbx->text[textbox_cursor_pos]));

			// Write current text
			EVE_cmd_text_burst(tbx->x + tbx->labelOffsetX + TEXTBOX_PAD_H,curY, 26, 0, outputBuffer);
		}
		else{
			// Write current text
			EVE_cmd_text_burst(tbx->x + tbx->labelOffsetX + TEXTBOX_PAD_H,curY, 26, 0, tbx->text);
		}

		/// Reset tag
		EVE_cmd_dl_burst(TAG(0));
	}
}

void TFT_textbox_setCursor(int16_t position, int16_t len){
	/// Sets the position of the textbox cursor.
	/// -1	... Set to end
	/// 0	...	Set to begin
	/// x	... Set to position x if "x <= len"

	switch (position) {
		case -1:
			textbox_cursor_pos = len;
			break;
		default:
			if(position <= len)
				textbox_cursor_pos = position;
			break;
	}
}

void TFT_textbox_setStatus(textbox* tbx, uint8_t active, uint8_t cursorPos){

	if(active == 1){
		// Set vertical scroll so that the textbox can be seen
		TFT_cur_ScrollV = tbx->y - TEXTBOX_ACTIVE_TARGET_HEIGTH;
		tbx->active = 1;

		// Activate Keypad and set cursor to end
		keypad_open(tbx->mytag, tbx->keypadType);
		TFT_textbox_setCursor(cursorPos, *tbx->text_curlen);

		printf("activate tbx\n");
	}
	else{
		// Reset vertical scroll to normal
		TFT_cur_ScrollV = 0;
		tbx->active = 0;

		// Deactivate Keypad and set cursor to end
		keypad_close();

		printf("deactivate tbx\n");
	}
}




//void TFT_GraphData(graph* gph, INPUT_BUFFER_SIZE_t buf[], uint16_t buf_size, uint16_t *buf_curidx);
//void TFT_GraphStatic(uint8_t burst, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t padding, double amp_max, double t_max, double h_grid_lines, double v_grid_lines){
void TFT_GraphStatic(uint8_t burst, graph* gph){
	/// Write the non-dynamic parts of an Graph to the TFT (axes & labels, grids and values, axis-arrows but no data). Can be used once during init of a static background or at recurring display list build in TFT_display() completely coded by RS 02.01.2021.
	///
	///  burst	... determines if the normal or the burst version of the EVE Library is used to transmit DL command (0 = normal, 1 = burst).
	///  x		...	beginning of left edge of the graph (Note that the vertical axis starts at "x+padding" and that some Grid values might be at a position prior to x). In full Pixels
	///  y		... beginning of upper edge of the graph (Note this is the position of the axis-arrow point and that the horizontal axis label might be at a position prior to y). In full Pixels
	///  width	... width of the actual graph data area in full Pixels
	///  height	... height of the actual graph data area in full Pixels
	///  padding ... clearance from the outer corners (x,y) to the axes
	///  amp_max ... maximum represented value of amplitude (e.g. 10 Volts), will be used at 100% horizontal line
	///  t_max 	 ... maximum represented value of time (e.g. 2.2 Seconds), will be used at 100% horizontal line
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
	double widthPerSection = (double)(gph->width)/gph->v_grid_lines;
	double heightPerSection = (double)(gph->height)/gph->h_grid_lines;

	/// Axes LABELS
	(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_AXISCOLOR);
	(*EVE_cmd_text__fptr_arr[burst])(gph->x + gph->padding              + h_ax_lbl_comp_x, gph->y + gph->padding               - h_ax_lbl_comp_y, axis_lbl_txt_size, 0, "V");
	(*EVE_cmd_text__fptr_arr[burst])(gph->x + gph->padding + gph->width + v_ax_lbl_comp_x, gph->y + gph->padding + gph->height - v_ax_lbl_comp_y, axis_lbl_txt_size, 0, "t");

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
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding + (uint16_t)(widthPerSection*(double)i), curY + gph->padding ));
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding + (uint16_t)(widthPerSection*(double)i), curY + gph->padding + gph->height ));
	}
	// horizontal grid
	for(int i=1; i<=(int)floor(gph->h_grid_lines); i++){
		// x-position at left and right corner; y-position from top y, padding and height divided by number of gridlines - times current line
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding        	  , curY + gph->padding + gph->height - (uint16_t)(heightPerSection*(double)i) ));
		(*EVE_cmd_dl__fptr_arr[burst])(VERTEX2F(gph->x + gph->padding + gph->width, curY + gph->padding + gph->height - (uint16_t)(heightPerSection*(double)i) ));
	}
	(*EVE_cmd_dl__fptr_arr[burst])(DL_END);

	/// Grid VALUES
	(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_AXISCOLOR);
	// vertical grid (time)
	for(int i=1; i<=(int)ceil(gph->v_grid_lines); i++){ // "ceil" and "i-1" at val -> print also the 0 value
		// Calc time at current vertical line
		double val = (gph->t_max/gph->v_grid_lines*(double)(i-1));

		// If its a pure integer write it as number, else convert and write it to string
		if((val - (double)((uint32_t)val)) == 0){ //val % 1.0 == 0
			(*EVE_cmd_number__fptr_arr[burst])(gph->x + gph->padding + (uint16_t)(widthPerSection*(double)(i-1)) + h_grid_lbl_comp_x, curY + gph->height + h_grid_lbl_comp_y, grid_lbl_txt_size, 0, (int32_t)val); //EVE_OPT_RIGHTX| + 18
		}
		else{
			char buffer[32]; // buffer for double to string conversion
			sprintf(buffer, "%.1lf", val); // double to string conversion
			(*EVE_cmd_text__fptr_arr[burst])(gph->x + gph->padding + (uint16_t)(widthPerSection*(double)(i-1)) + h_grid_lbl_comp_x, curY + gph->height + h_grid_lbl_comp_y, grid_lbl_txt_size, 0, buffer);
		}
	}
	// horizontal grid (amplitude)
	//(*EVE_cmd_dl__fptr_arr[burst])(DL_COLOR_RGB | GRAPH_AXISCOLOR);
	for(int i=1; i<=(int)floor(gph->h_grid_lines); i++){  // "floor" and "i" at val -> don't print the 0 value
		// Calc amplitude at current horizontal line
		double val = (gph->amp_max/gph->h_grid_lines*(double)i);

		// If its a pure integer write it as number, else convert and write it to string
		if((val - (double)((uint32_t)val)) == 0){ //val % 1.0 == 0
			(*EVE_cmd_number__fptr_arr[burst])(gph->x - v_grid_lbl_comp_x, curY + gph->padding + gph->height - (uint16_t)(heightPerSection*(double)i) + v_grid_lbl_comp_y, grid_lbl_txt_size, 0, (int32_t)val); //EVE_OPT_RIGHTX|
		}
		else{
			char buffer[32]; // buffer for double to string conversion
			sprintf(buffer, "%.1lf", val); // double to string conversion
			(*EVE_cmd_text__fptr_arr[burst])(gph->x - v_grid_lbl_comp_x, curY + gph->padding + gph->height - (uint16_t)(heightPerSection*(double)i) + v_grid_lbl_comp_y, grid_lbl_txt_size, 0, buffer);
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

//void TFT_GraphData(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t padding, double y_max, INPUT_BUFFER_SIZE_t SBuffer[], uint16_t size, uint16_t *SBuffer_curidx, uint8_t graphmode, uint32_t datacolor, uint32_t markercolor){
void TFT_GraphData(graph* gph, INPUT_BUFFER_SIZE_t buf[], uint16_t buf_size, uint16_t *buf_curidx, uint32_t datacolor){
	/// Write the dynamic parts of an Graph to the TFT (data and markers). Used at recurring display list build in TFT_display() completely coded by RS 02.01.2021.
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
			EVE_cmd_dl_burst(VERTEX2F(gph->x + gph->padding + x_cur, curY + gph->padding + gph->height - (uint16_t)(( ((double)buf[x_cur]) / ((double)gph->y_max) )*(double)(gph->height)) )); //if(frameover==1) printf("%lf %lf\n", ((((double)(buf[x_cur]))/((double)gph->y_max))*(double)(gph->height)), (double)buf[gph->x]);
		}
	}
	/// Display graph roll-mode
	else {
		// Print newest value always at the rightmost corner with all older values to the right
		// => Start Display    x position at rightmost corner and decrement till 0 (last run will make it -1 at the end but it isnt used after that)
		// => Start Arrayindex i at current index and decrement every loop. If i goes below 0, reset to max index and decrement further till
		//    value before current is hit.

		int16_t i = *buf_curidx;
		for (int16_t x_cur = buf_size-1; x_cur >= 0; x_cur--) {
			// if index goes below 0 set to highest buffer index
			if(i < 0){i = buf_size-1;}

			// Send next point for EVE_LINE_STRIP at current x+padding and normalized buffer value
			EVE_cmd_dl_burst(VERTEX2F(gph->x + gph->padding + x_cur, curY + gph->padding + gph->height - (uint16_t)(( ((double)buf[i]) / ((double)gph->y_max) )*(double)(gph->height)) )); 				// EVE_cmd_dl_burst(VERTEX2F(gph->x + gph->padding + x_cur, EVE_VSIZE - ((uint16_t)(buf[i]/y_div) + margin + gph->padding)));

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



void touch_calibrate(void) {
	/// Sends pre-recorded touch calibration values. Made for EVE_RiTFT43. Created by Rudolph Riedel, adapted by RS @ MCI 2020/21
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


/* activate this if you are using a module for the first time or if you need to re-calibrate it */
/* write down the numbers on the screen and either place them in pre-defined block above or make a new block */
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

uint8_t TFT_init(void) {
	/// Initializes EVE (or checks if its already initialized). Only at first sucessful EVE_init() the tft is set active, backlight is set to medium brightness, the pre-elaborated touch calibration is loaded and the static Background is initiated. Created by Rudolph Riedel, adapted by RS @ MCI 2020/21

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
		touch_calibrate();

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
	/// Display logo over full screen

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
			/// Deactivate Keypad
			//keypad_close();


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
		if(TFT_last_MenuIdx != TFT_cur_MenuIdx || TFT_last_ScrollV != TFT_cur_ScrollV)
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
					break;
				case Numeric:
					// keypadType = Numeric
					EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*4), EVE_HSIZE-4-50-4, 29, 26, keypadCurrentKey, "789");
					EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*3), EVE_HSIZE-4-50-4, 29, 26, keypadCurrentKey, "456");
					EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*2), EVE_HSIZE-4-50-4, 29, 26, keypadCurrentKey, "123");
					EVE_cmd_keys_burst(2, EVE_VSIZE-2-29-(32*1), 138+138+4, 29, 26, keypadCurrentKey, "0");
					EVE_cmd_keys_burst(2+138+4+138+4-1, EVE_VSIZE-2-29-(32*1), 138+1, 29, 26, keypadCurrentKey, ".");
					showShiftKey = 0;
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
					break;
			}


			// Control Keys in different color
			EVE_cmd_fgcolor_burst(keypadControlKeyFgColor); //MAIN_BTNCTSCOLOR
			EVE_cmd_bgcolor_burst(keypadControlKeyBgColor); //MAIN_BTNCOLOR

			// Space, Left/Right and Shift Key
			EVE_cmd_keys_burst(2+30+4+60, EVE_VSIZE-2-29, 288, 29, 26, keypadCurrentKey, " ");
			EVE_cmd_keys_burst(2, EVE_VSIZE-2-29, 90, 29, 26, keypadCurrentKey, "<>");
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
