/*
 * globals.h
 *	This file keeps all variables that are used between the file -saying that are needed for collaboration between modules
 *  Created on: 20 Feb 2021
 *      Author: RS
 */
#include <DAVE.h>

#ifndef GLOBALS_H_
#define GLOBALS_H_

/*  MACROS - DEFINEs */
#define DEBUG_ENABLE // self implemented Debug flag
#define MEASUREMENT_INTERVAL (5.0) // Time between measurements in ms. Must be same as is set in TIMER_0 DAVE App!
#define S1_BUF_SIZE (480-20-20) // =440 values stored, next every 5ms -> 2.2sec storage          //sizeof(InputBuffer1)/sizeof(InputBuffer1[0])

// Function to convert a float to int16 with rounding // https://stackoverflow.com/questions/24723180/c-convert-floating-point-to-int
#ifndef FLOAT_TO_INT16
#define FLOAT_TO_INT16(x) ((x)>=0?(int16_t)((x)+0.5):(int16_t)((x)-0.5))
#endif

/*  MACROS - DEFINEs */
// These types are used to store measurement results and values throughout the program. If they need to be changed, this can be done here.
//#define INT_BUFFER_SIZE 2		// Size of int buffer in byte - needed to compare it
//#define FLOAT_BUFFER_SIZE 4		// Size of float buffer in byte - needed to compare it
typedef uint16_t int_buffer_t;
typedef float float_buffer_t;

/*  SYSTEM VARIABLEs */
volatile uint32_t _msCounter;
volatile uint8_t tft_tick;
volatile uint32_t measurementCounter;

/*  MEASUREMENTs */

// Sensor data definition
#define STR_SPEC_MAXLEN 20
typedef struct {
	uint8_t index;
	char*   name;
	uint16_t        bufIdx;
	uint16_t        bufMaxIdx;
	int_buffer_t*   bufRaw;
	float_buffer_t* bufFilter;
	float_buffer_t* bufConv;
	uint8_t	 	  errorOccured;	  		// Number of error-measurements that occurred since last valid value. If this is 0 the curent value is valid.
	int_buffer_t  errorThreshold; 		// Raw value above this threshold will be considered as invalid ( errorOccured=1 ). The stored value will be linear interpolated on the last Filter values.
	int32_t		  errorLastValidSlope;
	float     avgFilterSum;
	uint16_t  avgFilterOrder;
	char*   fitFilename;
	uint8_t  fitFilename_curLen;
	uint8_t fitOrder;
	float   fitCoefficients[4];

} sensor;

// Sensor 1 Front
char s1_filename_spec[STR_SPEC_MAXLEN];
volatile sensor sensor1;
extern int_buffer_t s1_buf_0raw[];
extern float_buffer_t s1_buf_1filter[];
extern float_buffer_t s1_buf_2conv[];


/*  MENU AND USER INTERFACE */
volatile uint8_t inputType;

// SD-Card handling
enum sdStates{sdNone=0, sdMounted, sdFileOpen, sdError};
typedef enum sdStates sdStates;
sdStates sdState;

/* DEBUG */
#if defined (DEBUG_ENABLE)
	extern void initialise_monitor_handles(void);
#else
	#define printf(...) { ; }
#endif
volatile uint8_t frameover;



/*  FUNCTIONS ---------------------------------------------------------------------------------------------------------------------------- */
void SysTick_Handler(); // Interrupt Routine - used for delay_ms
void delay_ms(uint32_t ms); // Delay execution by given milliseconds - used in tft.h->EVE.h->EVE_target.h

float poly_calc (float c_x, float* f_coefficients, uint8_t order);
void measure_movAvgFilter_clean(sensor* sens);

#endif /* GLOBALS_H_ */
