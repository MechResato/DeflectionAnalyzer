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
float s1_avgSum = 0;

static inline float measure_movAvgFilter(float* sum, int_buffer_t* rbuf, uint16_t maxIdx, uint16_t newIdx, uint16_t order);

void Adc_Measurement_Handler(void){
	/// Do measurements and store result in buffer. Allows to 'measure' self produced test signal based on value in global variable InputType
	/// uses global/extern variables: InputType, sensor1.bufferIdx, InputBuffer1_size, InputBuffer1, tft_tick

	DIGITAL_IO_SetOutputHigh(&IO_6_2);

	// Increment current Buffer index and set back to 0 if greater than size of array
	sensor1.bufferIdx++;
	if(sensor1.bufferIdx >= INPUTBUFFER1_SIZE){
		//printf("fo\n");
		frameover = 1;
		sensor1.bufferIdx = 0;
	}

	/// Read next value from sensor
	switch (InputType){
		// 0 ADC Sensor 5
		case 0:
			InputBuffer1[sensor1.bufferIdx] = ADC_MEASUREMENT_GetResult(&ADC_MEASUREMENT_Channel_A); //result = ADC_MEASUREMENT_GetResult(&ADC_MEASUREMENT_Channel_A);
			break;
		// 1 TestImpulse
		case 1:
			if(sensor1.bufferIdx % (INPUTBUFFER1_SIZE/5)) InputBuffer1[sensor1.bufferIdx] = 0;
			else InputBuffer1[sensor1.bufferIdx] = 4095;
			break;
		// 2 TestSawtooth
		case 2:
			if(lastval < 4095) InputBuffer1[sensor1.bufferIdx] = lastval+7;
			else InputBuffer1[sensor1.bufferIdx] = 0;
			lastval = InputBuffer1[sensor1.bufferIdx];
			break;
		// 3 TestSine (Note: The used time variable is not perfect for this purpose - just for test)
		case 3:
			InputBuffer1[sensor1.bufferIdx] = (uint32_t)((0.5*(1.0+sin(2.0 * M_PI * 11.725 * ((double)MeasurementCounter/3000))))*4095.0);
			break;
		default:
			InputBuffer1[sensor1.bufferIdx] = ADC_MEASUREMENT_GetResult(&ADC_MEASUREMENT_Channel_A); //result = ADC_MEASUREMENT_GetResult(&ADC_MEASUREMENT_Channel_A);
			// Convert raw value to adapted value and save it
			InputBuffer1_conv[sensor1.bufferIdx] = poly_calc(InputBuffer1[sensor1.bufferIdx], &sensor1.coefficients[0], sensor1.fitOrder); //5.2/4096.0*InputBuffer1[sensor1.bufferIdx];
			break;
	}

	//float fil1 = 0;
	//float sum1 = 0;
	//int32_t oldIdx = sensor1.bufferIdx - 5;
	//if(oldIdx < 0) oldIdx += INPUTBUFFER1_SIZE;
	//for(int i = sensor1.bufferIdx; i != oldIdx; i--){
	//	if(i<0) i += INPUTBUFFER1_SIZE+1;
	//	sum1 += InputBuffer1[i];
	//}
	//fil1 = sum1/5;
	//printf("-%d n%d %.2f %d\n", (int)oldIdx, (int)sensor1.bufferIdx, fil1, InputBuffer1[sensor1.bufferIdx]);


	float fil2 = measure_movAvgFilter(&s1_avgSum, &InputBuffer1[0], INPUTBUFFER1_SIZE, sensor1.bufferIdx, 5);
	//uint16_t order = 5; float* sum = &s1_avgSum; int_buffer_t* rbuf = &InputBuffer1[0]; uint16_t maxIdx = INPUTBUFFER1_SIZE; uint16_t newIdx = sensor1.bufferIdx;
	//// Get index of oldest element, which shall be removed (current index minus order with roll-over check)
	//oldIdx = newIdx - (order);
	//if(oldIdx < 0) oldIdx += maxIdx;
	//// Subtract oldest element and add newest to sum
	//*sum += rbuf[newIdx] - rbuf[oldIdx];
	//fil2 = *sum / order;
	//printf("%.2f %.2f\n", *sum, fil2);
	//printf("+%d n%d %.2f %d\n%.2f %.2f\n", (int)oldIdx, (int)sensor1.bufferIdx, fil1, InputBuffer1[sensor1.bufferIdx], *sum, fil2);

	//	if(sensor1.bufferIdx == 0 || sensor1.bufferIdx == 439 || sensor1.bufferIdx == 1)
	//		for(int i = sensor1.bufferIdx; i != oldIdx; i--){
	//			if(i<0) i += INPUTBUFFER1_SIZE+1;
	//			printf("i%d: %d (i<-%d)\n", i, InputBuffer1[i], i-1);
	//		}
	//
	//	if(sensor1.bufferIdx < 10 || oldIdx < 10 || sensor1.bufferIdx == 439 || oldIdx == 439)
	//		if(fil1==fil2)
	//			printf("+%d %d: s%.2f, n%d o%d, r%.2f\n\n", (int)oldIdx, (int)sensor1.bufferIdx, sum1, InputBuffer1[sensor1.bufferIdx], InputBuffer1[oldIdx], fil1 );
	//		else
	//			printf("-%d %d: s%.2f, n%d o%d, r%.2f\n\n", (int)oldIdx, (int)sensor1.bufferIdx, sum1, InputBuffer1[sensor1.bufferIdx], InputBuffer1[oldIdx], fil1 );
	//	else
	//		if(fil1==fil2)
	//			printf("+%d\n", (int)sensor1.bufferIdx);
	//		else
	//			printf("-%d\n", (int)sensor1.bufferIdx);


	if(fil2==0);

	// Trigger next main loop (with his it is running in synch with the measurement -> change if needed!)
	tft_tick = 42;

	// Increase count of executed measurements
	MeasurementCounter++;

	DIGITAL_IO_SetOutputLow(&IO_6_2);
}

// movin avg filter full loop -> 3.54us (@leap over 4.14us)
// movin avg filter function  -> 2.96us (@leap over 3.12us)
// movin avg filter function  -> 2.76us (@leap over 2.88us)

static inline float measure_movAvgFilter(float* sum, int_buffer_t* rbuf, uint16_t maxIdx, uint16_t newIdx, uint16_t order){
	/// Implementation of an moving average filter on an ring-buffer


	// Get index of oldest element, which shall be removed (current index minus order with roll-over check)
	int32_t oldIdx = newIdx - order;
	if(oldIdx < 0) oldIdx += maxIdx;

	// Subtract oldest element and add newest to sum
	*sum += rbuf[newIdx] - rbuf[oldIdx];


	// Calculate result and print debug
	//float res = *sum / order;
	//if(newIdx < 10 || oldIdx < 10 || newIdx == 439 || oldIdx == 439)
		//printf("+%d %d: s%.2f, n%d o%d, r%.2f\n", (int)oldIdx, (int)newIdx, *sum, rbuf[newIdx], rbuf[oldIdx], res);

	// Calculate average and return it
	return *sum / order;
}

