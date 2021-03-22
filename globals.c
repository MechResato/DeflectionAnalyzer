/*
 * globals.c
 *
 *  Created on: 20 Feb 2021
 *      Author: RS
 */

#include <DAVE.h>
#include <globals.h>

/*  MACROS - DEFINEs */
#define DEBUG_ENABLE // self implemented Debug flag
#define INPUTBUFFER1_SIZE (480-20-20) // =440 values stored, next every 5ms -> 2.2sec storage          //sizeof(InputBuffer1)/sizeof(InputBuffer1[0])

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
volatile uint32_t MeasurementCounter = 0; // Count of executed measurements

// Buffer 1
uint16_t InputBuffer1_idx = 0; // Current index in Buffer
int_buffer_t InputBuffer1[INPUTBUFFER1_SIZE] = { 0 }; // all elements 0
float_buffer_t InputBuffer1_conv[INPUTBUFFER1_SIZE] = { 0.0 }; // all elements 0



///*  MENU AND USER INTERFACE */
// Input signal type used for measurement and GUI display
volatile uint8_t InputType = 0; // 0=Sensor5, 1=TestImpulse, 2=TestSawTooth, 3=TestSine



///* DEBUG */
// Debug value - is set to 1 if the sensor buffer gets full the first time (used to only run debugcode when actual data is there...)
volatile uint8_t frameover = 0; 		// Used by: measure.c, tft.c



///*  FUNCTIONS ---------------------------------------------------------------------------------------------------------------------------- */
//void SysTick_Handler(){ // Interrupt Routine - used for delay_ms
//	_msCounter++;
//}
void delay_ms(uint32_t ms){
	// Delay execution by given milliseconds - used in tft.h->EVE.h->EVE_target.h
	//uint32_t now = SYSTIMER_GetTickCount();
	//while(now+ms > SYSTIMER_GetTickCount())
	uint32_t targetMicroSec = SYSTIMER_GetTime() + (ms*1000);
	while(targetMicroSec > SYSTIMER_GetTime())
		__NOP(); // do nothing
}
