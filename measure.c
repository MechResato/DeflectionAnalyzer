/*
 * measure.c
 *
 *  Created on: 20 Feb 2021
 *      Author: RS
 */

#include <DAVE.h>
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <math.h>
#include <globals.h>
#include <record.h>

uint32_t lastval = 0; // just for TestTriangle signal

extern volatile uint8_t volatile * volatile fifo_buf;



//static inline void measure_movAvgFilter(volatile sensor* sens);

volatile uint16_t testCount = 0;

/// Implementation of an moving average filter on an ring-buffer. This version is very fast but it needs to be started on an 0'd out buffer and the filter order sum must not be changed outside of this!!!
/// If this gets out of sync, use the slow version measure_movAvgFilter_clean before using this again (globals.h).
/// Note: This only subtracts the oldest and adds the newest entry to the stored sum before dividing.
#define MEASURE_MOVAVGFILTER(sens, filterOrder)                       			\
	/* Get index of oldest element, which shall be removed
	 * (current index - filter order with roll-over check) */					\
	int32_t oldIdx = sens->bufIdx - sens->avgFilterOrder;						\
	if(oldIdx < 0) oldIdx += sens->bufMaxIdx+1;									\
	/* Subtract oldest element and add newest to sum */							\
	sens->avgFilterSum += sens->bufRaw[sens->bufIdx] - sens->bufRaw[oldIdx];	\
	/* Calculate average and return it */										\
	sens->bufFilter[sens->bufIdx] = sens->avgFilterSum / filterOrder;

/// Convert raw value to adapted value and save it
// Temporary sum and result value, marked to be stored in a register to enhance speed (multiple successive access!). Use first coefficient (constant) as init value.
#define MEASURE_POLYCONVERSION(sens, sensBufIdx)		\
	register float result = sens->fitCoefficients[0];	\
	register float pow_x = 1;							\
	/* Calculate every term and add it to the result*/	\
	for(uint8_t i = 1; i < sens->fitOrder+1; i++){		\
		pow_x *= sens->bufFilter[sensBufIdx];			\
		result += sens->fitCoefficients[i] * pow_x; 	\
	}													\
	/* Save result*/									\
	sens->bufConv[sensBufIdx] = result;




void Adc_Measurement_Handler(void){
	/// Interrupt handler - Do measurements, filter/convert them and store result in buffers. Allows to 'measure' self produced test signal based on value in global variable InputType
	/// Start Timer after init and make sure initial conversion in ADC_MEASUREMENT APP is deactivated
	/// Uses global/extern variables: InputType, tft_tick, sensor[...], ...

	// Timing measurement pin high
	//DIGITAL_IO_SetOutputHigh(&IO_6_2);



	uint8_t sensIdx = 0;
	volatile sensor* sens;
	uint16_t sensBufIdx;
	do{
		// Store current sensor pointer (looks cleaner and may be faster without the additional indexing every time)
		sens = sensors[sensIdx];

		// Increment buffer index and store current ADC value if a measuring mode is active
		if(measureMode != measureModeNone){
			// Increment current Buffer index and set back to 0 if greater than size of array
			sensBufIdx = sens->bufIdx = sens->bufIdx + 1;
			if(sensBufIdx > sens->bufMaxIdx){
				frameover = 1;
				sensBufIdx = sens->bufIdx = 0;
			}


			// Read value from sensor (or generated test if defined)
			#if defined(INPUT_STANDARD)
				// Get raw input from ADC
				sens->bufRaw[sensBufIdx] = ADC_MEASUREMENT_GetResult(sens->adcChannel);
			#elif defined(INPUT_TESTIMPULSE)
			// 1 TestImpulse
				if(sensBufIdx % (S1_BUF_SIZE/5)) sens->bufRaw[sensBufIdx] = 0;
				else sens->bufRaw[sensBufIdx] = 4095;
			#elif defined(INPUT_TESTSAWTOOTH)
				// 2 TestSawtooth
				if(lastval < 4095) sens->bufRaw[sensBufIdx] = lastval+7;
				else sens->bufRaw[sensBufIdx] = 0;
				lastval = sens->bufRaw[sensBufIdx];
			#elif defined(INPUT_TESTSINE)
				// 3 TestSine (Note: The used time variable is not perfect for this purpose - just for test)
				sens->bufRaw[sensBufIdx] = (uint32_t)((0.5*(1.0+sin(2.0 * M_PI * 11.725 * ((double)measurementCounter/3000))))*4095.0);
			#endif
		}
		// TODO: (Consideration) Everything past here could be moved to the main slope. It would require a "last processed value" index and had to process every missed value between. The interrupt here would get much shorter though...


		// If system is in monitoring mode, filter/convert row values and fill corresponding buffers (+ error recognition)
		if(measureMode == measureModeMonitoring){
			// Check for sensor errors and try to interpolate a raw value. Note: This is only necessary because the filter would be confused otherwise and to print "clean" lines on the graph.
			// This should be OK as long as the frequency of the to be measured event is much lower than the sampling time, the average filter order adjusted to the application and only few error occur at a time.
			// Square interpolation was ruled out due to performance issues (on first tests this made the slope only ~400ns slower when errors occur).
			// Still, for precise results recalculate value in post-processing.
			if(sens->bufRaw[sensBufIdx] > sens->errorThreshold){
				// Mark current measurement as error
				sens->errorOccured++;

				// Calculate last index and check for over leap
				int32_t pre1Idx = sensBufIdx - 1;
				if(pre1Idx < 0) pre1Idx += sens->bufMaxIdx + 1;

				// If this is the the first error after an valid value, calculate/store last OK value and slope for linear interpolation
				if(sens->errorOccured == 1){
					// Calculate second to last index and check for over leap
					int32_t pre2Idx = sensBufIdx - 2;
					if(pre2Idx < 0) pre2Idx += sens->bufMaxIdx + 1;

					// Store last valid value and slope, to be used till the next valid value comes
					sens->errorLastValid = sens->bufFilter[pre1Idx] - sens->bufFilter[pre2Idx];
				}
				// Linear interpolation of current value (needed to satisfy filter, otherwise the missing value would interfere for [avgFilterOrder] measurements)
				sens->bufRaw[sensBufIdx] = sens->bufRaw[pre1Idx] + sens->errorLastValid;
			}
			else {
				sens->errorOccured = 0;
			}

			// Calculate current filtered value and save it in sensor struct
			MEASURE_MOVAVGFILTER(sens, sens->avgFilterOrder);

			/// Convert raw value to adapted value and save it in sensor struct
			MEASURE_POLYCONVERSION(sens, sensBufIdx);
		}

		// If system is in recording mode store current raw value in FIFO
		if(measureMode == measureModeRecording){
			//sens->bufRaw[sensBufIdx] = testCount;
			testCount++;

			// Copy current value to FIFO
			memcpy((void*)(fifo_buf + fifo_writeBufIdx), &(sens->bufRaw[sensBufIdx]), SENSOR_RAW_SIZE);

			//if(sensBufIdx % 150 == 0)
				//printf("%d %d %d\n", fifo_buf + fifo_writeBufIdx, &(sens->bufRaw[sensBufIdx]), SENSOR_RAW_SIZE);

			// Increase index
			fifo_writeBufIdx += SENSOR_RAW_SIZE;
		}

		// Check next sensor
		sensIdx++;
	} while(sensIdx != SENSORS_SIZE);


	// If system is in recording mode, finish and fill up current line (lines need to)
	if(measureMode == measureModeRecording){
		// Ignore rest of space on the current "line" defined by FIFO_LINE_SIZE. This is done to have the values of a measurement line inside a defined width, which must be a divider of the block size (a block must perfectly be fillable with n lines!)
		fifo_writeBufIdx += FIFO_LINE_SIZE_PAD;

		// Overleap check and correction -> ignore all bits that are higher than the used ones
		fifo_writeBufIdx &= FIFO_BITS_ALL_BLOCK;

		// Check for block end -> happens if the index is a multiple of the block size (e.g. 1024)
		// -> for powers of 2 this is every time the bits representing e.g 1023 (001111111111) are all 1's
		// Note: This only holds if the block size is a power of 2!!!
		if((fifo_writeBufIdx & FIFO_BITS_ONE_BLOCK) == 0){ //fifo_writeBufIdx % 1024 == 0
			// Mark current block as finished (ready to be written)
			fifo_finBlock[fifo_writeBlock] = 1;
			// Mark next block as current write block
			fifo_writeBlock++;
			// Overleap correction of the current block index
			if(fifo_writeBlock == FIFO_BLOCKS)
				fifo_writeBlock = 0;
			// Check if the write block has been recorded, if not a "crash" occurs and the process must be stopped
			if(fifo_finBlock[fifo_writeBlock] == 1){
				// Stop recording
				record_stop(1);
			}
		}
	}

	//if(fifo_writeBufIdx > 4094)
		//printf("%d\n",fifo_writeBufIdx);

	// Trigger next main loop (with this it is running in sync with the measurement -> change if needed!)
	main_tick = 42;

	// Increase count of executed measurements
	measurementCounter++;

	// Timing measurement pin low
	//DIGITAL_IO_SetOutputLow(&IO_6_2);
}

// Check for error and set raw to 0 if detected
//if(sensArray[i]->bufRaw[sensArray[i]->bufIdx] > sensArray[i]->errorThreshold){
//	sensArray[i]->errorOccured++;
//	sensArray[i]->bufRaw[sensArray[i]->bufIdx] = 0;
//
//	if(sensArray[i]->errorOccured > sensArray[i]->avgFilterOrder){
//
//	}
//}
//else if(sensArray[i]->errorOccured != 0){
//	sensArray[i]->errorOccured--;
//}


void measure_movAvgFilter(volatile sensor* sens, uint16_t filterOrder){
	// See MEASURE_MOVAVGFILTER() macro for details. This is only made to be used outside of this module.
	MEASURE_MOVAVGFILTER(sens, filterOrder);
}

void measure_polyConversion(volatile sensor* sens, uint16_t sensBufIdx){
	// See MEASURE_POLYCONVERSION() macro for details. This is only made to be used outside of this module.
	MEASURE_POLYCONVERSION(sens, sensBufIdx);
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

//static inline void measure_movAvgFilter(volatile sensor* sens){
//	/// Implementation of an moving average filter on an ring-buffer. This version is very fast but it needs to be started on an 0'd out buffer and the filter order sum must not be changed outside of this!!!
//	/// If this gets out of sync, use the slow version measure_movAvgFilter_clean before using this again (globals.h).
//	/// Note: This only subtracts the oldest and adds the newest entry to the stored sum before dividing.
//
//
//	// Get index of oldest element, which shall be removed (current index minus filter order with roll-over check)
//	int32_t oldIdx = sens->bufIdx - sens->avgFilterOrder;
//	if(oldIdx < 0) oldIdx += sens->bufMaxIdx+1;
//
//	// Subtract oldest element and add newest to sum
//	sens->avgFilterSum += sens->bufRaw[sens->bufIdx] - sens->bufRaw[oldIdx];
//
//	// Calculate average and return it
//	sens->bufFilter[sens->bufIdx] = sens->avgFilterSum / sens->avgFilterOrder;
//}







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
//float fil2 = measure_movAvgFilter(&s1_avgSum, &InputBuffer1[0], INPUTBUFFER1_SIZE, sensBufIdx, sens->avgFilterOrder);
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
//int32_t oldIdx = sensBufIdx - 5;
//if(oldIdx < 0) oldIdx += INPUTBUFFER1_SIZE;
//for(int i = sensBufIdx; i != oldIdx; i--){
//	if(i<0) i += INPUTBUFFER1_SIZE+1;
//	sum1 += InputBuffer1[i];
//}
//fil1 = sum1/5;
//printf("-%d n%d %.2f %d\n", (int)oldIdx, (int)sensBufIdx, fil1, InputBuffer1[sensBufIdx]);
