// Initially created by Rudolph Riedel, completely reworked by RS @ MCI 2020/21

#ifndef TFT_H_
#define TFT_H_

// Simple pre-defined colors
#define RED		0xff0000UL
#define ORANGE	0xffa500UL
#define GREEN	0x00ff00UL
#define BLUE	0x0000ffUL
#define BLUE_1	0x5dade2UL
#define YELLOW	0xffff00UL
#define PINK	0xff00ffUL
#define PURPLE	0x800080UL
#define WHITE	0xffffffUL
#define BLACK	0x000000UL
#define GREY	0x222222UL

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

// Header
void TFT_header_static(uint8_t burst, uint16_t layout[], uint32_t bannerColor, uint32_t dividerColor, uint32_t headerColor, char* headerText);

void TFT_label(uint8_t burst, uint16_t x, uint16_t y, uint8_t font, uint32_t textColor, char* text);

void TFT_control(uint16_t x, uint16_t y, uint16_t w0, uint16_t h0, uint16_t font, uint16_t options, uint32_t textColor, uint32_t btnColor, uint32_t btnContrastColor, char* text);

// Textbox feature
typedef struct {
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t labelOffsetX;
	char* labelText;
	int8_t mytag;
	char* text;
	int8_t text_maxlen;
	int8_t* text_curlen;
	int8_t active;
	keypadTypes keypadType;
} textbox;
void TFT_textbox_static(uint8_t burst, textbox* tbx);
void TFT_textbox_touch(textbox* tbx);
void TFT_textbox_display(textbox* tbx);
//void TFT_textbox_static(uint8_t burst, uint16_t x, uint16_t y, uint16_t width, int8_t tag, char* labelText, uint16_t labelOffsetX);
//void TFT_textbox_touch(int8_t mytag, char* text, int8_t text_maxlen, int8_t* text_curlen);
//void TFT_textbox_display(uint16_t x, uint16_t y, int8_t mytag, char* text);
void TFT_textbox_setCursor(int16_t position, int16_t len);
void TFT_textbox_setStatus(textbox* tbx, uint8_t active, uint8_t cursorPos);

// Graph feature
void TFT_GraphStatic(uint8_t burst, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t padding, double amp_max, double t_max, double h_grid_lines, double v_grid_lines);
void TFT_GraphData(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t padding, double y_max, XMC_VADC_RESULT_SIZE_t SBuffer[], uint16_t size, uint16_t *SBuffer_curidx, uint8_t graphmode, uint32_t datacolor, uint32_t markercolor);



// General
uint8_t TFT_init(void);
void touch_calibrate(void);
void TFT_display_init_screen(void);

void TFT_display_static(void);
void TFT_touch(void);
void TFT_display(void);


#endif /* TFT_H_ */
