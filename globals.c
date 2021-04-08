/*
 * globals.c
 *
 *  Created on: 20 Feb 2021
 *      Author: RS
 */

#include <DAVE.h>
#include <math.h>
#include <globals.h>

/*  MACROS - DEFINEs */

/* DEBUG */
#if defined (DEBUG_ENABLE)
	extern void initialise_monitor_handles(void);
#else
	#define printf(...) { ; }
#endif



///*  SYSTEM VARIABLEs */
// Counter used for function delay_ms
//volatile uint32_t _msCounter = 0;
volatile uint8_t tft_tick = 0; // Trigger tft display function. Is set every time by Adc_Measurement_Handler (used in main and measure)


///*  MEASUREMENTs */
volatile uint32_t measurementCounter = 0; // Count of executed measurements

// Sensor 1 Front
#define S1_BUF_SIZE (480-20-20) // =440 values stored, new every 5ms -> 2.2sec storage          //sizeof(InputBuffer1)/sizeof(InputBuffer1[0])
int_buffer_t   s1_buf_0raw   [S1_BUF_SIZE] = { 0 }; // all elements 0
float_buffer_t s1_buf_1filter[S1_BUF_SIZE] = { 0.0 };
float_buffer_t s1_buf_2conv  [S1_BUF_SIZE] = { 0.0 };
#define S1_FILENAME_CURLEN 6
char    s1_filename_spec[STR_SPEC_MAXLEN] = "S1.CAL"; // Note: File extension must be 3 characters long or an error will occur (fatfs lib?)
volatile sensor sensor1 = {
	.index = 1,
	.name = "Front",
	.bufIdx = 0,
	.bufMaxIdx = S1_BUF_SIZE-1,
	.bufRaw    = (int_buffer_t*)&s1_buf_0raw,
	.bufFilter = (float_buffer_t*)&s1_buf_1filter,
	.bufConv   = (float_buffer_t*)&s1_buf_2conv,
	.errorOccured = 0,
	.errorThreshold = 3900, // Raw value above this threshold will be considered invalid ( errorOccured=1 ). The stored value will be linear interpolated on the last Filter values.
	.avgFilterOrder = 5,
	.avgFilterSum = 0.0,
	.fitFilename = s1_filename_spec,
	.fitFilename_curLen = S1_FILENAME_CURLEN,
	.fitOrder = 2,
	.fitCoefficients[0] = 0,
	.fitCoefficients[1] = 0,
	.fitCoefficients[2] = 0,
	.fitCoefficients[3] = 0
};


//uint16_t InputBuffer1_idx = 0; // Current index in Buffer



///*  MENU AND USER INTERFACE */
// Input signal type used for measurement and GUI display
volatile uint8_t inputType = 0; // 0=Sensor5, 1=TestImpulse, 2=TestSawTooth, 3=TestSine

sdStates sdState = sdNone;

///* DEBUG */
// Debug value - is set to 1 if the sensor buffer gets full the first time (used to only run debugcode when actual data is there...)
volatile uint8_t frameover = 0; 		// Used by: measure.c, tft.c



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

void measure_movAvgFilter_clean(sensor* sens){
	/// Implementation of an moving average filter on an ring-buffer. This version is used to reevaluate the sum variable if the filter order is changed or the buffer is not 0'd out when starting the moving average filter.
	/// Note: This resets the sum and adds all elements between the oldest and newest entry to it before dividing.


	// Get index of oldest element, which shall be removed (current index minus filter order with roll-over check)
	int32_t oldIdx = sens->bufIdx - sens->avgFilterOrder;
	if(oldIdx < 0) oldIdx += sens->bufMaxIdx;

	// Subtract oldest element and add newest to sum
	sens->avgFilterSum = 0;
	for(int i = sens->bufIdx; i != oldIdx; i--){
		if(i<0) i += sens->bufMaxIdx+1;
		sens->avgFilterSum += sens->bufRaw[i];
	}

	// Calculate average and return it
	sens->bufFilter[sens->bufIdx] = sens->avgFilterSum / sens->avgFilterOrder;
}
