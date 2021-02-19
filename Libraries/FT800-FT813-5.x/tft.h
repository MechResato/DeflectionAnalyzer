// Initially created by Rudolph Riedel, completely reworked by RS @ MCI 2020/21

#ifndef TFT_H_
#define TFT_H_


volatile uint8_t InputType; // Input signal type used in main for measurement and here for swithcen on touch -> see tft.c!
volatile uint8_t frameover;

void TFT_GraphStatic(uint8_t burst, uint16_t x, uint16_t y, uint16_t width, uint16_t height, double amp_max, double t_max, double h_grid_lines, double v_grid_lines);
void TFT_GraphData(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t padding, double y_max, XMC_VADC_RESULT_SIZE_t SBuffer[], uint16_t size, uint16_t *SBuffer_curidx, uint8_t graphmode, uint32_t datacolor, uint32_t markercolor);
void touch_calibrate(void);
void TFT_display_init_screen(void);
void initStaticGraphBackground(void);
uint8_t TFT_init(void);
void TFT_touch(void);
void TFT_display(XMC_VADC_RESULT_SIZE_t SBuffer[], uint16_t size, uint16_t *SBuffer_curidx);

#endif /* TFT_H_ */