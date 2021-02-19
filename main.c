/*
 * main.c
 *  Created on: 2021 Feb 19 20:41:21
 *  Author: Rene Santeler @ MCI 2020/21
 *  Version: V0.1 (Abgabeversion Projekt)
 *  Comment:
 *  Parts are taken from DataAcquisitionAnalyzerPlatform_XMC4700_V1.6 MCI Project done with Stefan Reinmüller
 */

#include <DAVE.h>                 // Declarations from DAVE Code Generation (includes SFR declaration)
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <FT800-FT813-5.x/tft.h> // Implementation of display communication using the EVE Library of Rudolph Riedel


//// Debug
//#define DEBUG_ENABLE // self implemented Debug flag
#if defined (DEBUG_ENABLE)
	extern void initialise_monitor_handles(void);
#else
	#define printf(...) { ; }
#endif

//// Input Buffer
#define InputBuffer1_size (480-20-20) // =440 values stored, next every 5ms -> 2.2sec storage          //sizeof(InputBuffer1)/sizeof(InputBuffer1[0])
uint16_t InputBuffer1_idx = 0;
XMC_VADC_RESULT_SIZE_t InputBuffer1[InputBuffer1_size] = { 0 }; // all elements 0

//// System, counter and debug variables
volatile XMC_VADC_RESULT_SIZE_t result;
volatile uint8_t tft_tick = 0;
volatile uint32_t _msCounter = 0;
volatile uint32_t MeasurementCounter = 0; // Count of executed measurements

//// Adc_Measurement_Handler
extern volatile uint8_t InputType; // defined in tft.c, used to select signal reorded by  Adc_Measurement_Handler
uint32_t lastval = 0; // just for TestTriangle signal





void Adc_Measurement_Handler(void){
	/// Do measurements and store result in buffer. Allows to 'measure' self produced test signal based on value in global variable InputType
	/// uses global/extern variables: InputType, InputBuffer1_idx, InputBuffer1_size, InputBuffer1, tft_tick

	// Increment current Buffer index and set back to 0 if greater than size of array
	InputBuffer1_idx++;
	if(InputBuffer1_idx >= InputBuffer1_size){
		//printf("fo\n");
		frameover = 1;
		InputBuffer1_idx = 0;
	}

	/// Read next value from sensor
	 switch (InputType){
		// 0 ADC Sensor 5
		case 0:
			InputBuffer1[InputBuffer1_idx] = ADC_MEASUREMENT_GetResult(&ADC_MEASUREMENT_Channel_A); //result = ADC_MEASUREMENT_GetResult(&ADC_MEASUREMENT_Channel_A);
			break;
		// 1 TestImpulse
		case 1:
			if(InputBuffer1_idx % (InputBuffer1_size/5)) InputBuffer1[InputBuffer1_idx] = 0;
			else InputBuffer1[InputBuffer1_idx] = 4095;
			break;
		// 2 TestSawtooth
		case 2:
			if(lastval < 4095) InputBuffer1[InputBuffer1_idx] = lastval+14;
			else InputBuffer1[InputBuffer1_idx] = 0;
			lastval = InputBuffer1[InputBuffer1_idx];
			break;
		// 3 TestSine (Note: The used time variable is not perfect for this purpose)
		case 3:
			InputBuffer1[InputBuffer1_idx] = (uint32_t)((0.5*(1.0+sin(2.0 * M_PI * 11.725 * ((double)MeasurementCounter/3000))))*4095.0);
			break;
	 }

	// Trigger next main loop (with his it is running in synch with the measurement -> change if needed!)
	tft_tick = 42;

	// Increase count of executed measurements
	MeasurementCounter++;
}



void SysTick_Handler(){
	// Interrupt Routine - used for delay_ms
	_msCounter++;
}

// F
void delay_ms(uint32_t ms){
	// Delay execution by given milliseconds - used in tft.h->EVE.h->EVE_target.h
	uint32_t now = _msCounter;
	while(now+ms > _msCounter)
		__NOP(); // do nothing
}



int main(void)
{
	#if defined (DEBUG_ENABLE)
	initialise_monitor_handles();
	#endif

	DAVE_STATUS_t status;
	status = DAVE_Init();           /* Initialization of DAVE APPs  */
	if(status != DAVE_STATUS_SUCCESS) {
		/* Placeholder for error handler code. The while loop below can be replaced with an user error handler. */
		printf("DAVE APPs initialization failed\n");

		while(1U){ }
	}
	else{ printf("DAVE APPs initialization successful\n"); }

	// Arm-M internal interrupt init
	SysTick_Config(SystemCoreClock / 144);

	// Counter for TFT_display init
	uint8_t display_delay = 0;

	// Initial deactivation of CS pin
	DIGITAL_IO_SetOutputHigh(&IO_DIO_DIGOUT_CS_TFT);

	// Initialize Display
	if(TFT_init()){ printf("TFT init done 1\n"); }
	else{ printf("TFT init failed 0\n"); }


	// Show Logo for 1 seconds
	TFT_display_init_screen();
	_msCounter = 0;
	while (_msCounter < 600){__NOP();}

	// Init static Background (menu + basic graph)
	initStaticGraphBackground();


	// Main loop
	printf("Start Main Loop -------------------------------------\n");
	while(1U) {
		if(tft_tick) { // Is set every 5ms by Adc_Measurement_Handler
			// reset tick
			tft_tick = 0;

			// Evaluate touches
			TFT_touch();

			// Evaluate and rewrite display list
			display_delay++;
			if(MeasurementCounter%4 == 0) { // 4*5ms=20ms,  1/20ms=50Hz refresh rate
				display_delay = 0;
				TFT_display(&InputBuffer1[0], InputBuffer1_size, &InputBuffer1_idx);
			}

		}
	}


}
