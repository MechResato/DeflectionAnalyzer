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



/*  SYSTEM VARIABLEs */
// Counter used for function delay_ms
volatile uint32_t _msCounter;
volatile uint8_t tft_tick; // Trigger tft display function. Is set every time by Adc_Measurement_Handler (used in main and measure)



/*  MEASUREMENTs */
volatile uint32_t MeasurementCounter; // Count of executed measurements

// Buffer 1
uint16_t InputBuffer1_idx; // Current index in Buffer
extern XMC_VADC_RESULT_SIZE_t InputBuffer1[];



/*  MENU AND USER INTERFACE */
// Input signal type used for measurement and GUI display
volatile uint8_t InputType; // 0=Sensor5, 1=TestImpulse, 2=TestSawTooth, 3=TestSine



/* DEBUG */
#if defined (DEBUG_ENABLE)
	extern void initialise_monitor_handles(void);
#else
	#define printf(...) { ; }
#endif
// Debug value - is set to 1 if the sensor buffer gets full the first time (used to only run debugcode when actual data is there...)
volatile uint8_t frameover; 		// Used by: measure.c, tft.c



/*  FUNCTIONS ---------------------------------------------------------------------------------------------------------------------------- */

void SysTick_Handler(); // Interrupt Routine - used for delay_ms
void delay_ms(uint32_t ms); // Delay execution by given milliseconds - used in tft.h->EVE.h->EVE_target.h

#endif /* GLOBALS_H_ */
