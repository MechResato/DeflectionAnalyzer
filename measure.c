/*
 * measure.c
 *
 *  Created on: 20 Feb 2021
 *      Author: RS
 */

#include <DAVE.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <globals.h>

uint32_t lastval = 0; // just for TestTriangle signal

void Adc_Measurement_Handler(void){
	/// Do measurements and store result in buffer. Allows to 'measure' self produced test signal based on value in global variable InputType
	/// uses global/extern variables: InputType, InputBuffer1_idx, InputBuffer1_size, InputBuffer1, tft_tick

	// Increment current Buffer index and set back to 0 if greater than size of array
	InputBuffer1_idx++;
	if(InputBuffer1_idx >= INPUTBUFFER1_SIZE){
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
			if(InputBuffer1_idx % (INPUTBUFFER1_SIZE/5)) InputBuffer1[InputBuffer1_idx] = 0;
			else InputBuffer1[InputBuffer1_idx] = 4095;
			break;
		// 2 TestSawtooth
		case 2:
			if(lastval < 4095) InputBuffer1[InputBuffer1_idx] = lastval+7;
			else InputBuffer1[InputBuffer1_idx] = 0;
			lastval = InputBuffer1[InputBuffer1_idx];
			break;
		// 3 TestSine (Note: The used time variable is not perfect for this purpose - just for test)
		case 3:
			InputBuffer1[InputBuffer1_idx] = (uint32_t)((0.5*(1.0+sin(2.0 * M_PI * 11.725 * ((double)MeasurementCounter/3000))))*4095.0);
			break;
		default:
			InputBuffer1[InputBuffer1_idx] = ADC_MEASUREMENT_GetResult(&ADC_MEASUREMENT_Channel_A); //result = ADC_MEASUREMENT_GetResult(&ADC_MEASUREMENT_Channel_A);
			// Convert raw value to adapted value and save it
			InputBuffer1_conv[InputBuffer1_idx] = poly_calc(InputBuffer1[InputBuffer1_idx], &s1_coefficients[0], s1_fit_order); //5.2/4096.0*InputBuffer1[InputBuffer1_idx];

			break;
	 }


	// Trigger next main loop (with his it is running in synch with the measurement -> change if needed!)
	tft_tick = 42;

	// Increase count of executed measurements
	MeasurementCounter++;
}



