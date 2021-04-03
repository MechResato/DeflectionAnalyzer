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

static inline void measure_movAvgFilter(volatile sensor* sens);


void Adc_Measurement_Handler(void){
	/// Interrupt handler - Do measurements, filter/convert them and store result in buffers. Allows to 'measure' self produced test signal based on value in global variable InputType
	/// Uses global/extern variables: InputType, tft_tick, sensor1, s1_buf_...

	// Timing measurement pin high
	DIGITAL_IO_SetOutputHigh(&IO_6_2);

	// Increment current Buffer index and set back to 0 if greater than size of array
	sensor1.bufIdx++;
	if(sensor1.bufIdx >= S1_BUF_SIZE){
		frameover = 1;
		sensor1.bufIdx = 0;
	}

	/// Read next value from sensor
	switch (inputType){
		// 0 ADC Sensor 5
		case 0:
			s1_buf_0raw[sensor1.bufIdx] = ADC_MEASUREMENT_GetResult(&ADC_MEASUREMENT_Channel_A); //result = ADC_MEASUREMENT_GetResult(&ADC_MEASUREMENT_Channel_A);
			break;
		// 1 TestImpulse
		case 1:
			if(sensor1.bufIdx % (S1_BUF_SIZE/5)) s1_buf_0raw[sensor1.bufIdx] = 0;
			else s1_buf_0raw[sensor1.bufIdx] = 4095;
			break;
		// 2 TestSawtooth
		case 2:
			if(lastval < 4095) s1_buf_0raw[sensor1.bufIdx] = lastval+7;
			else s1_buf_0raw[sensor1.bufIdx] = 0;
			lastval = s1_buf_0raw[sensor1.bufIdx];
			break;
		// 3 TestSine (Note: The used time variable is not perfect for this purpose - just for test)
		case 3:
			s1_buf_0raw[sensor1.bufIdx] = (uint32_t)((0.5*(1.0+sin(2.0 * M_PI * 11.725 * ((double)measurementCounter/3000))))*4095.0);
			break;
		default:
			// Get raw input from ADC
			s1_buf_0raw[sensor1.bufIdx] = ADC_MEASUREMENT_GetResult(&ADC_MEASUREMENT_Channel_A);
			break;
	}

	// TODO: (Consideration) Everything past here could be moved to the main slope. It would require a "last processed value" index and had to process every missed value between. The interrupt here would get much shorter though...


	// Check for sensor errors and try to interpolate a raw value. Note: This is only necessary because the filter would be confused otherwise and to print "clean" lines on the graph.
	// This should be OK as long as the frequency of the to be measured event is much lower than the sampling time, the average filter order adjusted to the application and only few error occur at a time.
	// Square interpolation was ruled out due to performance issues (on first tests this made the slope only ~400ns slower when errors occur).
	// Still, for precise results recalculate value in post-processing.
	if(s1_buf_0raw[sensor1.bufIdx] > sensor1.errorThreshold){
		// Mark current measurement as error
		sensor1.errorOccured++;

		// Calculate last index and check for over leap
		int32_t pre1Idx = sensor1.bufIdx - 1;
		if(pre1Idx < 0) pre1Idx += sensor1.bufMaxIdx;

		// If this is the the first error after an valid value, calculate/store last OK value and slope for linear interpolation
		if(sensor1.errorOccured == 1){
			// Calculate second to last index and check for over leap
			int32_t pre2Idx = sensor1.bufIdx - 2;
			if(pre2Idx < 0) pre2Idx += sensor1.bufMaxIdx;

			// Store last valid value and slope, to be used till the next valid value comes
			sensor1.errorLastValidSlope = s1_buf_1filter[pre1Idx] - s1_buf_1filter[pre2Idx];
		}
		// Linear interpolation of current value (needed to satisfy filter, otherwise the missing value would interfere for [avgFilterOrder] measurements)
		s1_buf_0raw[sensor1.bufIdx] = s1_buf_0raw[pre1Idx] + sensor1.errorLastValidSlope;
	}
	else {
		sensor1.errorOccured = 0;
	}

	// Calculate current filtered value
	measure_movAvgFilter(&sensor1);

	/// Convert raw value to adapted value and save it
	//s1_buf_2conv[sensor1.bufIdx] =  poly_calc_m(s1_buf_0raw[sensor1.bufIdx], &sensor1.fitCoefficients[0], sensor1.fitOrder); //5.2/4096.0*InputBuffer1[sensor1.bufIdx]; //sens->bufFilter[sens->bufIdx] // poly_calc_sensor_i(&sensor1, sensor1.bufRaw);
	// Temporary sum and result value, marked to be stored in a register to enhance speed (multiple successive access!). Use first coefficient (constant) as init value.
	register float result = sensor1.fitCoefficients[0];
	register float pow_x = 1;
	// Calculate every term and add it to the result
	for(uint8_t i = 1; i < sensor1.fitOrder+1; i++){
		pow_x *= sensor1.bufFilter[sensor1.bufIdx];
		result += sensor1.fitCoefficients[i] * pow_x; //result += sensor1.fitCoefficients[i] * ipow(sensor1.bufFilter[sensor1.bufIdx], i); //result += sensor1.fitCoefficients[i] * ipow(s1_buf_0raw[sensor1.bufIdx], i);
	}
	// Save result
	s1_buf_2conv[sensor1.bufIdx] = result;


	// Trigger next main loop (with this it is running in sync with the measurement -> change if needed!)
	tft_tick = 42;

	// Increase count of executed measurements
	measurementCounter++;

	// Timing measurement pin low
	DIGITAL_IO_SetOutputLow(&IO_6_2);
}

// Measurements at non leap-over cycle!
// poly_calc_sensor_i 5.80us max
// poly_calc_sensor   5.58us max
// no function, with struct and powf 5.36us max
// no function, with struct and ipow 3.80us max
// no function, with struct and improved ipow 3.68us max
// no function, with struct and recursive pow_x 3.6us max
// TODO: First measurements imply that functions as inline, the sensor struct only when needed and integer (ipow) usage are the fastest here. To be continued! (Note: signs that inline in combination with struct's might be slower than with actual variables were found!)




// movin avg filter full loop -> 3.54us (@leap over 4.14us)
// movin avg filter function  -> 2.96us (@leap over 3.12us)
// movin avg filter inline function  -> 2.76us (@leap over 2.88us)

static inline void measure_movAvgFilter(volatile sensor* sens){
	/// Implementation of an moving average filter on an ring-buffer. This version is very fast but it needs to be started on an 0'd out buffer and the filter order sum must not be changed outside of this!!!
	/// If this gets out of sync, use the slow version measure_movAvgFilter_clean before using this again (globals.h).
	/// Note: This only subtracts the oldest and adds the newest entry to the stored sum before dividing.


	// Get index of oldest element, which shall be removed (current index minus filter order with roll-over check)
	int32_t oldIdx = sens->bufIdx - sens->avgFilterOrder;
	if(oldIdx < 0) oldIdx += sens->bufMaxIdx;

	// Subtract oldest element and add newest to sum
	sens->avgFilterSum += sens->bufRaw[sens->bufIdx] - sens->bufRaw[oldIdx];

	// Calculate average and return it
	sens->bufFilter[sens->bufIdx] = sens->avgFilterSum / sens->avgFilterOrder;
}







//static inline uint32_t ipow(register int_buffer_t base, register uint8_t exp) {
//	/// Efficient integer power function by John Zwinck on Apr 4 '18 at 7:26 at "https://stackoverflow.com/questions/101439/the-most-efficient-way-to-implement-an-integer-based-power-function-powint-int"
//	/// Adapted by Rene Santeler: Marked both parameters and result as register (all are used often) made function inline and changed int to uint32_t. Performance increases significant with this
//
//	register uint32_t result = 1;
//    for (;;) {
//        if (exp & 1)
//            result *= base;
//        exp >>= 1;
//        if (!exp)
//            break;
//        base *= base;
//    }
//
//    return result;
//}

//static inline uint32_t ipow(int_buffer_t base, uint8_t exp) {
//	/// Efficient integer power function by Rene Santeler (slightly slower than the one from John Zwinck)
//	///
//	register uint32_t result = 1;
//
//	if(exp!=0)
//		do{
//			result *= base;
//			exp--;
//		}while(exp!=0);
//
//	return result;
//}

//static inline float poly_calc_r (float c_x, float* f_Coefficients, uint8_t order){
//	/// Calculate the result of an polynomial based on the x value, the coefficients and the order (1=linear(2 coefficients used), 2=square(3 coefficients used)...)
//	/// NOTE: A specialized version (inline and only for sensor struct's) of this code is implemented in measure.c because inline functions must be static to avoid compiler error (https://stackoverflow.com/questions/28606847/global-inline-function-between-two-c-files)
//	/// UNTESTED!
//
//	// Temporary sum and result value, marked to be stored in a register to enhance speed (multiple successive access!). Use first coefficient (constant) as init value.
//	register float result = f_Coefficients[0];
//	register float pow_x = 1;
//	// Calculate every term and add it to the result
//	for(uint8_t i = 1; i < order+1; i++){
//		pow_x *= c_x;
//		result += f_Coefficients[i] * pow_x;
//	}
//	// return result
//	return result;
//}

//static inline float poly_calc_sensor_f (sensor* sens, float_buffer_t* x_buf){
//	/// Calculate the result of an polynomial based on a FLOAT x-value, the coefficients and the order (1=linear(2 coefficients used), 2=square(3 coefficients used)...)
//	/// NOTE: This is a specialized version (inline and only for sensor struct's) of the common implementation in globals.c because inline functions must be static to avoid compiler error (https://stackoverflow.com/questions/28606847/global-inline-function-between-two-c-files)
//
//
//	// Temporary sum and result value, marked to be stored in a register to enhance speed (multiple successive access!). Use first coefficient (constant) as init value.
//	register float result = sens->fitCoefficients[0];
//	// Calculate every term and add it to the result
//	for(uint8_t i = 1; i < sens->fitOrder+1; i++)
//		result += sens->fitCoefficients[i] * powf(x_buf[sens->bufIdx], i);
//	// return result
//	return result;
//}
//
//static inline float poly_calc_sensor_i (sensor* sens, int_buffer_t* x_buf){
//	/// Calculate the result of an polynomial based on a INTEGER x-value, the coefficients and the order (1=linear(2 coefficients used), 2=square(3 coefficients used)...)
//	/// NOTE: This is a specialized version (inline and only for sensor struct's) of the common implementation in globals.c because inline functions must be static to avoid compiler error (https://stackoverflow.com/questions/28606847/global-inline-function-between-two-c-files)
//
//
//	// Temporary sum and result value, marked to be stored in a register to enhance speed (multiple successive access!). Use first coefficient (constant) as init value.
//	register float result = sens->fitCoefficients[0];
//	// Calculate every term and add it to the result
//	for(uint8_t i = 1; i < sens->fitOrder+1; i++)
//		result += sens->fitCoefficients[i] * powf(x_buf[sens->bufIdx], i); //(float)
//	// return result
//	return result;
//}
//
//static inline float poly_calc_m (float c_x, float* f_Coefficients, uint8_t order){
//	/// Calculate the result of an polynomial based on the x value, the coefficients and the order (1=linear(2 coefficients used), 2=square(3 coefficients used)...)
//	/// NOTE: A specialized version (inline and only for sensor struct's) of this code is implemented in measure.c because inline functions must be static to avoid compiler error (https://stackoverflow.com/questions/28606847/global-inline-function-between-two-c-files)
//
//	// Temporary sum and result value, marked to be stored in a register to enhance speed (multiple successive access!). Use first coefficient (constant) as init value.
//	register float result = f_Coefficients[0];
//	// Calculate every term and add it to the result
//	for(uint8_t i = 1; i < order+1; i++)
//		result += f_Coefficients[i] * powf(c_x, i);
//	// return result
//	return result;
//}


//float s1_avgSum = 0;
//float fil2 = measure_movAvgFilter(&s1_avgSum, &InputBuffer1[0], INPUTBUFFER1_SIZE, sensor1.bufIdx, sensor1.avgFilterOrder);
//static inline float measure_movAvgFilter(float* sum, int_buffer_t* rbuf, uint16_t maxIdx, uint16_t newIdx, uint16_t order);
//static inline float measure_movAvgFilter(float* sum, int_buffer_t* rbuf, uint16_t maxIdx, uint16_t newIdx, uint16_t order){
//	/// Implementation of an moving average filter on an ring-buffer
//
//
//	// Get index of oldest element, which shall be removed (current index minus order with roll-over check)
//	int32_t oldIdx = newIdx - order;
//	if(oldIdx < 0) oldIdx += maxIdx;
//
//	// Subtract oldest element and add newest to sum
//	*sum += rbuf[newIdx] - rbuf[oldIdx];
//
//	// Calculate average and return it
//	return *sum / order;
//}




//float fil1 = 0;
//float sum1 = 0;
//int32_t oldIdx = sensor1.bufIdx - 5;
//if(oldIdx < 0) oldIdx += INPUTBUFFER1_SIZE;
//for(int i = sensor1.bufIdx; i != oldIdx; i--){
//	if(i<0) i += INPUTBUFFER1_SIZE+1;
//	sum1 += InputBuffer1[i];
//}
//fil1 = sum1/5;
//printf("-%d n%d %.2f %d\n", (int)oldIdx, (int)sensor1.bufIdx, fil1, InputBuffer1[sensor1.bufIdx]);
