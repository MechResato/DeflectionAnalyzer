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
#define MAIN_BGCOLOR		0xfdf9f6
#endif

// Function to convert a float to int16 with rounding // https://stackoverflow.com/questions/24723180/c-convert-floating-point-to-int
#ifndef FLOAT_TO_INT16
#define FLOAT_TO_INT16(x) ((x)>=0?(int16_t)((x)+0.5):(int16_t)((x)-0.5))
#endif

// int_buffer_t and float_buffer_t is defined in globals files

/// Source definition for display elements: Some Elements can be associated to a source via a pointer. This source might be an integer or an float. This is a way to achieve the possible use of both.
enum srcTypes{srcTypeNone=0, srcTypeInt, srcTypeFloat};
typedef enum srcTypes srcTypes;
//#define SRC_MAXSIZE 4 	// The size in bytes of the biggest value used in union below! This will be used to compare
typedef struct {
	srcTypes srcType;			// Type of the src union. 0=none (pure text tbx), 1=integer(int_buffer_t), 2=float
	union {						// A pointer to the numeric source this textbox represents.
		int_buffer_t* intSrc;
		float_buffer_t* floatSrc;
	};
	uint16_t* srcOffset;		// Offset (index) of src. Only needed for array sources (the actual byte offset will be calculated based on srcType)
	//char lastVal[SRC_MAXSIZE];
} srcDefinition;

/// Code to only refresh text if needed - might or might not be used in the future
// && memcmp(lbl->numSrc.intSrc, lbl->numSrc.lastVal, INT_BUFFER_SIZE) != 0
// Save current value
//memcpy(lbl->numSrc.lastVal, lbl->numSrc.intSrc, INT_BUFFER_SIZE);


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

void TFT_setMenu(int16_t idx);
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
	//float* num_src; 		// A pointer to the numeric source this label represents. Set to 0 if its a pure text label. If set to 1 the string in text is used to format the source
	srcDefinition numSrc;	// A struct containing a pointer to the numeric source this label represents. See srcDefinition for more info.
	uint8_t fracExp;		// Determines how many fractional digits are shown when using a float source e.g. 2->2digits, 3->3digits.  NOTE: You also need to write the format specifier to the text field (e.g. for 2 digits "%d.%.2d")
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
	int8_t mytag;
	char* text;
	uint16_t state;
	controlTypes controlType;
	uint8_t ignoreScroll;
} control;
void TFT_control(control* ctrl);


/// Textbox feature
/// Prints a label at x position and a textbox at x+labelOffsetX behind. Its connected to a text and, if srcType is not 0, to a numerical source that all are kept up-to-date.
typedef struct {
	uint16_t x;				// Beginning of left edge of the textbox .
	uint16_t y;				// Beginning of upper edge of the textbox.
	uint16_t width; 		// Width of the actual textbox. 11px per possible character (+some space on the left border) should be fine (A width of 190-200 is perfect for a worst case 16 character string in a textbox).
	uint16_t labelOffsetX;	// Distance between x and the actual start of the textbox (space needed for label).
	char* labelText;		// Text of the preceding label.
	int8_t mytag;			// The touch tag used for the textbox.
	char* text;				// Pointer to the text buffer or value the textbox is linked to. Must be set!
	int8_t text_maxlen;		// The size of the buffer or value the textbox is linked to. Must be set!
	int8_t* text_curlen;	// Pointer to a variable showing the current size of the string buffer or value the textbox is linked to. IMPORTANT: This must be set to the current size at beginning!
	int8_t active;			// Marker showing if textbox is being modified by user.
	char* numSrcFormat;		// Format used when converting the referenced numeric source to text (at display and at setStatus). Must be set if an numeric source is used and must contain one %d for integer and two for float source (e.g."%d.%.2d")!
	uint8_t fracExp;		// Determines how many fractional digits are shown when using a float source e.g. 2->2digits, 3->3digits.  NOTE: You also need to write the format specifier to the text field (e.g. for 2 digits "%d.%.2d")
	srcDefinition numSrc;
	//int8_t srcType;			// Type of the src union. 0=none (pure text tbx), 1=integer(int_buffer_t), 2=float
	//union {					// A pointer to the numeric source this textbox represents.
	//	int_buffer_t* intSrc;
	//	float* floatSrc;
	//} src;
	//uint16_t* srcOffset;		// Offset (index) of src. Only needed for array sources (the actual byte offset will be calculated based on srcType)
	//float* num_src; 		// A pointer to the numeric source this textbox represents. Set to 0 if tbx has no numeric source (pure text).
	keypadTypes keypadType;	// The type of keypad that shall be used if textbox gets active. This controls the type of characters that can be entered (see keypadTypes).
} textbox;
#define TEXTBOX_HEIGTH 31	// Overall height of the textbox in pixel.
#define TEXTBOX_PAD_V 8		// Offset of the text from upper border in pixel.
#define TEXTBOX_PAD_H 7		// Offset of the text from left border in pixel.
void TFT_textbox_static(uint8_t burst, textbox* tbx);
void TFT_textbox_touch(textbox* tbx);
void TFT_textbox_display(textbox* tbx);
void TFT_textbox_setStatus(textbox* tbx, uint8_t active, int16_t cursorPos);


// Graph feature
enum graphTypes{graphLine=0, graphPoint};
typedef enum graphTypes graphTypes;
typedef struct {
	uint16_t x; 	  // Beginning of left edge of the graph in full pixels (Note that the vertical axis starts at "x+padding" and that some grid values might be at a position prior to x).
	uint16_t y; 	  // Beginning of upper edge of the graph in full pixels (Note that this is the position of the axis-arrow point and that the horizontal axis label might be at a position prior to y).
	uint16_t width;   // Width of the actual graph data area in full pixels.  WARNING: This affects correctness of x-axis scale! Convention: Always use a even width and exponents of 2 as step_divider (e.g. 200px or 440px with 2, 4, 8, ...)
	uint16_t height;  // Height of the actual graph data area in full Pixels
	uint16_t padding; // Clearance from the outer corners (x,y) to the axes
	char* x_label;	  // Text that will be written at the end of x Axis (like "x" or "t")
	char* y_label;	  // Text that will be written at the top of y Axis (like "y" or "V")
	float y_max; 	  // Maximum expected value of input (e.g. for 12bit ADC 4095), will represent 100% of y-Axis
	float amp_max;   // Maximum represented value of amplitude (e.g. 10 Volts), will be used at 100% of y-Axis
	float cx_initial;// NOT USED YET! (works for TFT_graph_stepdata but not TFT_graph_static) ToDo
	float cx_max; 	  // Maximum represented value of x-Axis (e.g. time 2.2 Seconds), will be used at 100% of x-Axis
	float h_grid_lines; 	// Number of horizontal grid lines
	float v_grid_lines; 	// Number of vertical grid lines
	uint8_t graphmode;		// 0 = frame-mode, 1 = roll-mode
} graph;
void TFT_graph_static(uint8_t burst, graph* gph);
void TFT_graph_pixeldata(graph* gph, int_buffer_t buf[], uint16_t buf_size, uint16_t *buf_curidx, uint32_t datacolor);
void TFT_graph_stepdata(graph* gph, int_buffer_t cy_buf[], uint16_t cy_buf_size, float cx_step, uint32_t datacolor);
void TFT_graph_XYdata(graph* gph, float cy_buf[], float cx_buf[], uint16_t buf_size, int16_t *buf_curidx, graphTypes gphTyp, uint32_t datacolor);
void TFT_graph_function(graph* gph, float* f_coefficients, uint8_t order, uint16_t step_divider, graphTypes gphTyp, uint32_t datacolor);

// General
uint8_t TFT_init(void);
void TFT_display_init_screen(void);

void TFT_display_static(void);
void TFT_touch(void);
void TFT_display(void);


#endif /* TFT_H_ */
