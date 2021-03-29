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
#define INPUTBUFFER1_SIZE (480-20-20) // =440 values stored, next every 5ms -> 2.2sec storage          //sizeof(InputBuffer1)/sizeof(InputBuffer1[0])

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

/*  MEASUREMENTs */
volatile uint32_t MeasurementCounter;
uint16_t InputBuffer1_idx;
extern int_buffer_t InputBuffer1[];
extern float_buffer_t InputBuffer1_conv[];
extern uint8_t s1_fit_order;
extern float s1_coefficients[4];

/*  MENU AND USER INTERFACE */
volatile uint8_t InputType;


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

#endif /* GLOBALS_H_ */
