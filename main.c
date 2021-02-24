/*
 * main.c
 *  Created on: 2021 Feb 19 20:41:21
 *  Author: Rene Santeler @ MCI 2020/21
 *  Version: V0.2
 *  Comment:
 *  Parts are taken from DataAcquisitionAnalyzerPlatform_XMC4700_V1.6 MCI Project done with Stefan Reinmüller
 */

#include <DAVE.h>                 // Declarations from DAVE Code Generation (includes SFR declaration)
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <globals.h>
#include <FT800-FT813-5.x/tft.h> // Implementation of display communication using the EVE Library of Rudolph Riedel
#include <measure.h>

// This file is kept as clean as possible. All variables and functions used by more than one component are stated in the 'globals' files.
// See "globals" for details on how everything works together

///*  INTERRUPT HANDLER */
// SysTick_Handler in "globals"
// Adc_Measurement_Handler in "measure"


int main(void)
{
	// Make semihosting and printf back to DAVE possible if debug is enabled
	#if defined (DEBUG_ENABLE)
	initialise_monitor_handles();
	#endif

	// Initialize environment
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

	// Initial disable of CS pin
	DIGITAL_IO_SetOutputHigh(&IO_DIO_DIGOUT_CS_TFT);

	// Initialize Display
	if( TFT_init() ){ printf("TFT init done 1\n"); }
	else{ printf("TFT init failed 0\n"); }

	// Show initial logo
	TFT_display_init_screen();
	_msCounter = 0;
	while (_msCounter < 100) __NOP();

	// Init static Background (menu + basic graph)
	//initStaticGraphBackground();


	// Main loop
	printf("Start Main Loop -------------------------------------\n");
	while(1U) {
		if(tft_tick) { // Is set by Adc_Measurement_Handler at every interrupt of it
			// reset tick
			tft_tick = 0;

			// Evaluate touches
			TFT_touch();

			// Evaluate and rewrite display list
			display_delay++;
			if(MeasurementCounter % 4 == 0) { // 4*5ms=20ms,  1/20ms=50Hz refresh rate
				display_delay = 0;
				TFT_display();
			}

		}
	}


}
