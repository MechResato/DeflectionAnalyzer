/*
 * main.c
 *  Created on: 2021 Feb 19 20:41:21
 *  Author: Rene Santeler @ MCI 2020/21
 *  Version: V1.0
 *  Comment: This project aims to design an extensible microcontroller system to support the user in calibrating, monitoring and
 *  recording of sensor data at the example of a motorcycle suspension. To do so, a highly modular menu-framework for
 *  touchscreens as well as multiple concepts for efficient sensor data management and SD-Card retention are implemented.
 *  The port of the EVE Library is taken from DataAcquisitionAnalyzerPlatform_XMC4700_V1.6 MCI Project done by Stefan Reinmüller and Rene Santeler.
 */

#include <DAVE.h> // Declarations from DAVE Code Generation (includes SFR declaration)
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <math.h>
#include <globals.h>
#include <measure.h>	// Everything related to the measurement, filtering and conversion of data by Rene Santeler
#include <record.h>		// Everything related to SD-Card handling and read/write by Rene Santeler
#include <tft.h> 		// Implementation of a display menu framework by Rene Santeler using the EVE Library of Rudolph Riedel

// This file is kept as clean as possible. All variables and functions used by more than one component are stated in the 'globals' files.
// See "globals" for details on how everything works together

///*  INTERRUPT HANDLER */
// SysTick_Handler in "globals"
// Adc_Measurement_Handler in "measure"

extern volatile uint8_t volatile * volatile fifo_buf;

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

	// Counter for TFT_display init
	uint8_t display_ticker = 0;

	// Initial disable of CS pin
	DIGITAL_IO_SetOutputHigh(&IO_DIO_DIGOUT_CS_TFT);

	// Initialize Display
	if( TFT_init() ){ printf("TFT init done 1\n"); }
	else{ printf("TFT init failed 0\n"); }

	// Show initial menu/logo
	TFT_display_init_screen();
	uint32_t now = SYSTIMER_GetTime();
	while (SYSTIMER_GetTime() < now + (1500*1000)) __NOP();

	// Load Values from SD-Card if possible
	for (uint8_t i = 0; i < SENSORS_SIZE; i++){
		// Allocate memory for data points (curveset) - will only be realloc'ed after this!
		sensors[i]->dp_y = (float*)malloc(1*sizeof(float));
		sensors[i]->dp_x = (float*)malloc(1*sizeof(float));
		// Load sensor calibration
		record_readCalFile(sensors[i]);
	}

	// Start ADC measurement interrupt routine
	TIMER_Start(&TIMER_0);

	// Main loop
	printf("Start Main Loop -------------------------------------\n");
	while(1U) {
		if(main_trigger) {
			/// Main tick is set by Adc_Measurement_Handler at every interrupt of it and triggers this
			// Reset tick
			main_trigger = 0;

			// Check if screenshot is requested
			#ifdef DEBUG_ENABLE
				if(DIGITAL_IO_GetInput(&IO_3_3_SC) == 1)
					menu_doScreenshot = 1;
			#endif

			/// RECORD HANDLING
			// If recording mode is active and there is something to record (current block is finished) write block to SD-Card
			if(measureMode == measureModeRecording && fifo_finBlock[fifo_recordBlock] == 1){
				// Timing measurement pin high
				DIGITAL_IO_SetOutputHigh(&IO_6_4);

				// Record current block
				record_block();

				// Mark Block as processed (if a block isn't processed before the measurement handler tries to write to it again the record fails)
				fifo_finBlock[fifo_recordBlock] = 0;

				// Mark next block as current record block
				fifo_recordBlock++;
				// Overleap correction of the current block index
				if(fifo_recordBlock == FIFO_BLOCKS)
					fifo_recordBlock = 0;

				// Timing measurement pin low
				DIGITAL_IO_SetOutputLow(&IO_6_4);
			}
			// If an error was detected during the measurement handler (usually block crash)
			else if(measureMode == measureModeRecordError)
				// Stop recording
				record_stop(1);


			/// Menu and HMI HANDLING
			// Timing measurement pin high
			DIGITAL_IO_SetOutputHigh(&IO_6_6);

			// Evaluate touches
			TFT_touch(); // ~100us with no touch

			// Evaluate and rewrite display content
			display_ticker++;
			if(measurementCounter % 4 == 0) { // 4*5ms=20ms,  1/20ms=50Hz refresh rate
				display_ticker = 0;
				TFT_display(); // ~9000us at Monitoring, 800us at Dashboard(empty), 1440us at Setup
			}

			// Timing measurement pin low
			DIGITAL_IO_SetOutputLow(&IO_6_6);
		} // End of main_tick
	} // End of mail loop


	// Free allocated memory (never needed - just to be clean)
	free(sensor1.dp_x);
	free(sensor1.dp_y);
	free(sensor2.dp_x);
	free(sensor2.dp_y);
}

