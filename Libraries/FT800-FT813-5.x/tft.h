// Initially created by Rudolph Riedel, completely reworked by RS @ MCI 2020/21

#ifndef TFT_H_
#define TFT_H_

// Simple pre-defined colors
#define RED			0xff0000UL
#define ORANGE		0xffa500UL
#define GREEN		0x00ff00UL
#define BLUE		0x0000ffUL
#define BLUE_1		0x5dade2UL
#define YELLOW		0xffff00UL
#define PINK		0xff00ffUL
#define PURPLE		0x800080UL
#define WHITE		0xffffffUL
#define BLACK		0x000000UL
#define GREY		0x222222UL
#define LIGHTGREY	0xccccccUL

// If available use colors set at menu.h to keep color scheme. But if not, define own ones
#ifndef MAIN_BTNCTSCOLOR
#define MAIN_BTNCTSCOLOR	0xAD9666
#endif
#ifndef MAIN_BTNCOLOR
#define MAIN_BTNCOLOR		0xEAA92B
#endif
#ifndef MAIN_BGCOLOR
#define MAIN_BGCOLOR		0xF5F1EE
#endif

// INPUT_BUFFER_SIZE_t is defined in globals files

// Menu definition
typedef struct {
	uint8_t index;
	char* headerText;
	int16_t upperBond; 		  // TFT elements will only be shown below this coordinate, usually deepest y from headerLayout
	uint16_t headerLayout[4]; // Banner line strip edge positions. Array of 4 elements [Y1,X1,Y2,X2] (from left to right: Y1 is held horizontal till X1, increasing till X2/Y2 and finally held horizontal at Y2 till EVE_HSIZE)
	uint32_t bgColor;
	uint32_t bannerColor;
	uint32_t dividerColor;
	uint32_t headerColor;
} menu;

void TFT_setMenu(uint8_t idx);
void TFT_setColor(uint8_t burst, int32_t textColor, int32_t fgColor, int32_t bgColor, int32_t gradColor);
void TFT_header(uint8_t burst, menu* men);

// Swipe feature
enum SwipeDetection{None=0, Up, Down, Left, Right};
typedef enum SwipeDetection SwipeDetection;
extern int32_t swipeDistance_X;		  // Distance (in px) between the initial touch and the current position of an swipe
extern int32_t swipeDistance_Y;

// Keypad feature
enum keypadTypes{Standard=0, Numeric, Filename};
typedef enum keypadTypes keypadTypes;
void keypad_open(uint8_t evokedBy, enum keypadTypes type);
void keypad_close();


// Label
typedef struct {
	uint16_t x;
	uint16_t y;
	uint16_t font;
	uint16_t options;
	char* text;
	double* num_src; 		// A pointer to the numeric source this label represents. Set to 0 if its a pure text label. If set to 1 the string in text is used to format the source
	uint8_t ignoreScroll;
} label;
void TFT_label(uint8_t burst, label* lbl);


// Control (buttons, toggles, ...)
enum controlTypes{Button=0, Toggle};
typedef enum controlTypes controlTypes;
typedef struct {
	uint16_t x;
	uint16_t y;
	uint16_t w0;
	uint16_t h0;
	uint16_t font;
	uint16_t options;
	const int8_t mytag;
	char* text;
	uint16_t state;
	controlTypes controlType;
	uint8_t ignoreScroll;
} control;
void TFT_control(control* ctrl);


/// Textbox feature
/// Prints a label at x position and a textbox at x+labelOffsetX behind. Its connected to a text and, if num_src is not NULL, to a numerical source that all are kept up-to-date.
typedef struct {
	uint16_t x;				// Beginning of left edge of the textbox .
	uint16_t y;				// Beginning of upper edge of the textbox.
	uint16_t width; 		// Width of the actual textbox. 11px per possible character (+some space on the left border) should be fine (A width of 190-200 is perfect for a worst case 16 character string in a textbox).
	uint16_t labelOffsetX;	// Distance between x and the actual start of the textbox (space needed for label).
	char* labelText;		// Text of the preceding label.
	const int8_t mytag;		// The touch tag used for the textbox.
	double* num_src; 	// A pointer to the numeric source this textbox represents. Set to 0 if tbx has no numeric source (pure text).
	char* text;				// Pointer to the text buffer or value the textbox is linked to. Must be set!
	int8_t text_maxlen;		// The size of the buffer or value the textbox is linked to. Must be set!
	int8_t* text_curlen;	// Pointer to a variable showing the current size of the string buffer or value the textbox is linked to. IMPORTANT: This must be set to the current size at beginning!
	int8_t active;			// Marker showing if textbox is being modified by user.
	keypadTypes keypadType;	// The type of keypad that shall be used if textbox gets active. This controls the type of characters that can be entered (see keypadTypes).
} textbox;
#define TEXTBOX_HEIGTH 31	// Overall height of the textbox in pixel.
#define TEXTBOX_PAD_V 8		// Offset of the text from upper border in pixel.
#define TEXTBOX_PAD_H 7		// Offset of the text from left border in pixel.
void TFT_textbox_static(uint8_t burst, textbox* tbx);
void TFT_textbox_touch(textbox* tbx);
void TFT_textbox_display(textbox* tbx);
void TFT_textbox_setStatus(textbox* tbx, uint8_t active, uint8_t cursorPos);


// Graph feature
typedef struct {
	uint16_t x; 	  // Beginning of left edge of the graph in full pixels (Note that the vertical axis starts at "x+padding" and that some grid values might be at a position prior to x).
	uint16_t y; 	  // Beginning of upper edge of the graph in full pixels (Note that this is the position of the axis-arrow point and that the horizontal axis label might be at a position prior to y).
	uint16_t width;   // Width of the actual graph data area in full Pixels
	uint16_t height;  // Height of the actual graph data area in full Pixels
	uint16_t padding; // Clearance from the outer corners (x,y) to the axes
	char* x_label;	  // Text that will be written at the end of x Axis (like "x" or "t")
	char* y_label;	  // Text that will be written at the top of y Axis (like "y" or "V")
	double y_max; 	  // Maximum expected value of input (e.g. for 12bit ADC 4095), will represent 100% of y-Axis
	double amp_max;   // Maximum represented value of amplitude (e.g. 10 Volts), will be used at 100% of y-Axis
	double cx_initial;// NOT USED YET! (works for TFT_graph_stepdata but not TFT_graph_static) ToDo
	double cx_max; 	  // Maximum represented value of x-Axis (e.g. time 2.2 Seconds), will be used at 100% of x-Axis
	double h_grid_lines; 	// Number of horizontal grid lines
	double v_grid_lines; 	// Number of vertical grid lines
	uint8_t graphmode;		// 0 = frame-mode, 1 = roll-mode
} graph;
void TFT_graph_static(uint8_t burst, graph* gph);
void TFT_graph_pixeldata(graph* gph, INPUT_BUFFER_SIZE_t buf[], uint16_t buf_size, uint16_t *buf_curidx, uint32_t datacolor);
void TFT_graph_stepdata(graph* gph, INPUT_BUFFER_SIZE_t cy_buf[], uint16_t cy_buf_size, double cx_step, uint32_t datacolor);
void TFT_graph_XYdata(graph* gph, double cy_buf[], uint16_t cy_buf_size, double cx_buf[], uint16_t cx_buf_size, uint32_t datacolor);

// General
uint8_t TFT_init(void);
void TFT_display_init_screen(void);

void TFT_display_static(void);
void TFT_touch(void);
void TFT_display(void);


#endif /* TFT_H_ */
