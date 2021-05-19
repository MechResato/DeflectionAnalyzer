/*
@file    		globals.c
@brief   		Shared variables and types to be used with this project (implemented for XMC4700 and DAVE)
@version 		1.0
@date    		2020-02-20
@author 		Rene Santeler @ MCI 2020/21
 */

#include <DAVE.h>
#include <math.h>
#include <globals.h>

/*  SYSTEM VARIABLEs */
volatile uint8_t main_trigger = 0; // Trigger tft display function. Is set every time by Adc_Measurement_Handler (used in main and measure)


/*  MEASUREMENTs */
volatile uint32_t measurementCounter = 0; // Count of executed measurements
volatile uint8_t monitorSensorIdx = 0;

// Sensor 1 Front
int_buffer_t   s1_buf_0raw   [S_BUF_SIZE] = { 0 }; // all elements 0
float_buffer_t s1_buf_1filter[S_BUF_SIZE] = { 0.0 };
float_buffer_t s1_buf_2conv  [S_BUF_SIZE] = { 0.0 };
#define S1_FILENAME_CURLEN 6
char    s1_filename_cal[STR_SPEC_MAXLEN] = "S1.CAL"; // Note: File extension must be 3 characters long or an error will occur (fatfs lib?)
volatile sensor sensor1 = {
	.index = 0,
	.name = "Front",
	.adcChannel = &ADC_MEASUREMENT_Channel_A,
	.bufIdx = 0,
	.bufMaxIdx = S_BUF_SIZE-1,
	.bufRaw    = (int_buffer_t*)&s1_buf_0raw,
	.bufFilter = (float_buffer_t*)&s1_buf_1filter,
	.bufConv   = (float_buffer_t*)&s1_buf_2conv,
	.originPoint = 0,
	.operatingPoint = 0,
	.errorOccured = 0,
	.errorThreshold = 3900, // Raw value above this threshold will be considered invalid ( errorOccured=1 ). The stored value will be linear interpolated on the last Filter values.
	.avgFilterInterval = 5,
	.avgFilterSum = 0.0,
	.fitFilename = s1_filename_cal,
	.fitFilename_curLen = S1_FILENAME_CURLEN,
	.fitOrder = 2,
	.fitCoefficients[0] = 0,
	.fitCoefficients[1] = 0,
	.fitCoefficients[2] = 0,
	.fitCoefficients[3] = 0
};

// Sensor 2 Rear
int_buffer_t   s2_buf_0raw   [S_BUF_SIZE] = { 0 }; // all elements 0
float_buffer_t s2_buf_1filter[S_BUF_SIZE] = { 0.0 };
float_buffer_t s2_buf_2conv  [S_BUF_SIZE] = { 0.0 };
#define S2_FILENAME_CURLEN 6
char    s2_filename_cal[STR_SPEC_MAXLEN] = "S2.CAL"; // Note: File extension must be 3 characters long or an error will occur (fatfs lib?)
volatile sensor sensor2 = {
	.index = 1,
	.name = "Rear",
	.adcChannel = &ADC_MEASUREMENT_Channel_B,
	.bufIdx = 0,
	.bufMaxIdx = S_BUF_SIZE-1,
	.bufRaw    = (int_buffer_t*)&s2_buf_0raw,
	.bufFilter = (float_buffer_t*)&s2_buf_1filter,
	.bufConv   = (float_buffer_t*)&s2_buf_2conv,
	.originPoint = 0,
	.operatingPoint = 0,
	.errorOccured = 0,
	.errorThreshold = 3900, // Raw value above this threshold will be considered invalid ( errorOccured=1 ). The stored value will be linear interpolated on the last Filter values.
	.avgFilterInterval = 5,
	.avgFilterSum = 0.0,
	.fitFilename = s2_filename_cal,
	.fitFilename_curLen = S2_FILENAME_CURLEN,
	.fitOrder = 2,
	.fitCoefficients[0] = 0,
	.fitCoefficients[1] = 0,
	.fitCoefficients[2] = 0,
	.fitCoefficients[3] = 0
};

// Array of all sensor objects to be used in measurement handler
volatile sensor* sensors[SENSORS_SIZE] = {&sensor1, &sensor2};


/* LOG FIFO */
char filename_rec[FILENAME_REC_MAXLEN] = "log.csv";
uint8_t filename_rec_curLength = 8;
// Also see FIFO_BLOCK_SIZE, FIFO_BLOCKS and FIFO_WRITEBUFIDX_BITS -> defined in header file
volatile uint8_t volatile * volatile fifo_buf = NULL;	// The pointer to the FIFO buffer itself. Will be allocated by malloc at start of log
volatile uint16_t fifo_writeBufIdx = 0;					// The index inside the fifo_buf where the next value will be written to by the measurement handler
volatile uint8_t fifo_writeBlock = 0;					// The current block (multiple of BLOCK_SIZE) inside the fifo_buf the measurement handler is writing to
volatile uint8_t fifo_recordBlock = 0;					// The current block (multiple of BLOCK_SIZE) inside the fifo_buf that shall be written to SD-Card (as soon as finBlock==logBlock)
volatile uint8_t fifo_finBlock[FIFO_BLOCKS] = {0};		// An array with an element for every Block in fifo_buf. Each corresponding element represents if a block is ready to recorded


///*  MENU AND USER INTERFACE */
sdStates sdState = sdNone;
volatile measureModes measureMode = measureModeMonitoring;


///*  FUNCTIONS ---------------------------------------------------------------------------------------------------------------------------- */
void delay_ms(uint32_t ms){
	uint32_t targetMicroSec = SYSTIMER_GetTime() + (ms*1000);
	while(targetMicroSec > SYSTIMER_GetTime())
		__NOP(); // do nothing
}

float poly_calc (float c_x, float* f_Coefficients, uint8_t order){
	/// Calculate the result of an polynomial based on the x value, the coefficients and the order (1=linear(2 coefficients used), 2=square(3 coefficients used)...)
	/// NOTE: A specialized version (inline and only for sensor struct's) of this code is implemented in measure.c because inline functions must be static to avoid compiler error (https://stackoverflow.com/questions/28606847/global-inline-function-between-two-c-files)


	// Temporary sum and result value, marked to be stored in a register to enhance speed (multiple successive access!). Use first coefficient (constant) as init value.
	register float result = f_Coefficients[0];
	// Calculate every term and add it to the result
	for(uint8_t i = 1; i < order+1; i++)
		result += f_Coefficients[i] * powf(c_x, i);
	// return result
	return result;
}

void measure_movAvgFilter_clean(sensor* sens, uint16_t filterOrder, uint8_t compFilterOrder){
	/// Implementation of an moving average filter on an ring-buffer. This version is used to reevaluate the sum variable if the filter order is changed or the buffer is not 0'd when the filter starts.
	/// It can also be used to calculate the sum over the filter while ignoring zeroed out values (decrease order/divider with every 0 found during sum).
	/// Note: This resets the sum and adds all elements between the oldest and newest entry to it before dividing.


	// Get index of oldest element, which shall be removed (current index minus filter order with roll-over check)
	int32_t oldIdx = sens->bufIdx - sens->avgFilterInterval;
	if(oldIdx < 0) oldIdx += sens->bufMaxIdx;

	// Subtract oldest element and add newest to sum
	sens->avgFilterSum = 0;
	for(int i = sens->bufIdx; i != oldIdx; i--){
		if(i<0) i += sens->bufMaxIdx+1;
		sens->avgFilterSum += sens->bufRaw[i];
	}

	// Calculate average and return it
	if(compFilterOrder != 0)
		sens->bufFilter[sens->bufIdx] = sens->avgFilterSum / compFilterOrder;
	else if(filterOrder != 0)
		sens->bufFilter[sens->bufIdx] = sens->avgFilterSum / filterOrder;
	else
		sens->bufFilter[sens->bufIdx] = 0;
}
