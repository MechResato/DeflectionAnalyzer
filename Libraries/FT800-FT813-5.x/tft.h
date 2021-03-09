// Initially created by Rudolph Riedel, completely reworked by RS @ MCI 2020/21

#ifndef TFT_H_
#define TFT_H_

// Made globally available in order to be shown as debug values in menu.c
int32_t swipeDistance_X;		  // Distance (in px) between the initial touch and the current position of an swipe
int32_t swipeDistance_Y;

uint8_t keypadActive;
uint8_t keypadEvokedBy;

void TFT_textbox_static(uint8_t burst, uint16_t x, uint16_t y, uint16_t width, int8_t tag);
void TFT_textbox_touch(int8_t mytag, char* text, int8_t text_maxlen, int8_t* text_curlen);
void TFT_textbox_display(uint16_t x, uint16_t y, int8_t mytag, char* text, int8_t text_maxlen, int8_t* text_curlen);

void TFT_GraphStatic(uint8_t burst, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t padding, double amp_max, double t_max, double h_grid_lines, double v_grid_lines);
void TFT_GraphData(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t padding, double y_max, XMC_VADC_RESULT_SIZE_t SBuffer[], uint16_t size, uint16_t *SBuffer_curidx, uint8_t graphmode, uint32_t datacolor, uint32_t markercolor);



uint8_t TFT_init(void);
void touch_calibrate(void);
void TFT_display_init_screen(void);

void TFT_display_static(void);
void TFT_touch(void);
void TFT_display(void);


#endif /* TFT_H_ */
