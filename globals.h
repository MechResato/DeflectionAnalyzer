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
#define DEBUG_ENABLE   // self implemented Debug flag
#define INPUT_STANDARD // Defines what signal shall be retrieved in ADC_measurement_handler. INPUT_STANDARD = current adc value, INPUT_TESTIMPULSE = generated impulse signal, INPUT_TESTSAWTOOTH = generated saw signal, INPUT_TESTSINE = generated sine signal

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
volatile uint8_t main_tick;
volatile uint32_t measurementCounter;
volatile uint8_t measurementCurSensor;

// Record screenshot marker, can only be used in debug mode
#ifdef DEBUG_ENABLE
	uint8_t menu_doScreenshot;
#endif

/*  MEASUREMENTs */

// Setting to decide which error handling strategy is used. ONLY ONE OF THE FOLLOWING MUST BE ACTIVATED AT A TIME! the
// See measure.c measure_postProcessing() for more details. When changing this you will also need to change the way the BIN->CSV conversion is done!
#define POSTPROCESS_CHANGEORDER_AT_ERRORS 1
#define POSTPROCESS_INTERPOLATE_ERRORS 0
// If POSTPROCESS_CHANGEORDER_AT_ERRORS is activated this decides if a filtered/converted value for measurements considered errors shall be calculated (if filter order is greater than occurred errors)
#define POSTPROCESS_BUGGED_VALUES 1

// Sensor data definition
#define SENSOR_RAW_SIZE sizeof(int_buffer_t) // Bytes. Size of the a variable that represents the raw value. FIFO_BLOCK_SIZE MUST BE DIVISIBLE BY THIS!
#define STR_SPEC_MAXLEN 20
typedef struct {
	uint8_t index;
	char*   name;
	ADC_MEASUREMENT_CHANNEL_t* adcChannel;
	uint16_t        bufIdx;
	uint16_t        bufMaxIdx;
	int_buffer_t*   bufRaw;
	float_buffer_t* bufFilter;
	float_buffer_t* bufConv;
	uint8_t	 	  errorOccured;	  		// Number of error-measurements that occurred since last valid value. If this is 0 the curent value is valid.
	int_buffer_t  errorThreshold; 		// Raw value above this threshold will be considered as invalid ( errorOccured=1 ). The stored value will be linear interpolated on the last Filter values.
	int32_t		  errorLastValid;
	float     avgFilterSum;
	uint16_t  avgFilterOrder;
	char*   fitFilename;
	uint8_t  fitFilename_curLen;
	uint8_t fitOrder;
	float   fitCoefficients[4];

} sensor;

// Sensor 1 Front
char s1_filename_cal[STR_SPEC_MAXLEN];
volatile sensor sensor1;
extern int_buffer_t s1_buf_0raw[];
extern float_buffer_t s1_buf_1filter[];
extern float_buffer_t s1_buf_2conv[];

// Sensor 2 Rear
char s2_filename_cal[STR_SPEC_MAXLEN];
volatile sensor sensor2;
extern int_buffer_t s2_buf_0raw[];
extern float_buffer_t s2_buf_1filter[];
extern float_buffer_t s2_buf_2conv[];

// Array of all sensor objects to be used in measurement handler
#define SENSORS_SIZE 2
extern volatile sensor* sensors[];

/*  RECORDING FIFO AND FILENAME */
#define FILENAME_REC_MAXLEN 10
extern char filename_rec[FILENAME_REC_MAXLEN];
uint8_t filename_rec_curLength;
// Size of buffers used for filename handling (change this if Long File Names - LFN is activated)
#define FILENAME_BUFFER_LENGTH 20
// Size of buffers used to generate a line for the CSV File. Adapt if line gets longer (more sensors, values, etc).
#define CSVLINE_BUFFER_LENGTH 200

// Note: FIFO_BLOCK_SIZE times FIFO_BLOCKS must always be a power of 2! Otherwise the overleap check with &= doesn't work anymore
#define FIFO_BLOCK_SIZE 1024			// Number of bytes in one block
#define FIFO_BLOCKS 	4				// Number of blocks that are used (total RAM usage = FIFO_BLOCK_SIZE*FIFO_BLOCKS)
#define FIFO_BITS_ONE_BLOCK (FIFO_BLOCK_SIZE-1)		         // = 0b000 0111 1111 111 for 1024BS. Represents the used bits of the uint16_t which represents the index in one block. Use '&' to check if a number is a multiple of the block size or to ignore higher bits
#define FIFO_BITS_ALL_BLOCK	((FIFO_BLOCK_SIZE*FIFO_BLOCKS)-1)// = 0b000 0011 1111 1111 for 1024BS and 4B. Represents the used bits of the uint16_t which represents the index in whole buffer. Use '&' to ignore higher bits
#define FIFO_LINE_SIZE 		4				// Number of bytes that represent one measurement line. This MUST be a clean divider of the FIFO_BLOCK_SIZE, and must be adapted if more or less sensors are recorded.
#define FIFO_LINE_SIZE_PAD 	2				// Number of bytes that are added after the content of each measurement line. Might or might not be needed to fill a line to FIFO_LINE_SIZE. This MUST be adapted if more or less sensors are recorded or the size changes.
volatile uint8_t volatile * volatile fifo_buf;
volatile uint16_t fifo_writeBufIdx;
volatile uint8_t fifo_writeBlock;
volatile uint8_t fifo_recordBlock;
extern volatile uint8_t fifo_finBlock[];

/// BIN to CSV conversion
// The header text to be written once at first line of CSV file. Must include alle coumns of all sensors! Do not add the "Time" column or the line break at the end (will be automatically added).
#define RECORD_CSV_HEADER		"S1_RAW;S1_FILTERED;S1_CONVERTED;EO"
// ... used for every sensor ... TODO
#define RECORD_CSV_ARGUMENTS	sensArray[i]->bufRaw[sensArray[i]->bufIdx], sensArray[i]->bufFilter[sensArray[i]->bufIdx], sensArray[i]->bufConv[sensArray[i]->bufIdx], sensArray[i]->errorOccured
// ... used for every sensor ... TODO
#define RECORD_CSV_FORMAT		"%d;%.1f;%.2f;%d"

/*  MENU AND USER INTERFACE */
// Data Acquisition Mode
enum measureModes{measureModeNone=0, measureModeMonitoring, measureModeRecording};
typedef enum measureModes measureModes;
measureModes measureMode;

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
void measure_movAvgFilter_clean(sensor* sens, uint16_t filterOrder, uint8_t compFilterOrder);

#endif /* GLOBALS_H_ */
