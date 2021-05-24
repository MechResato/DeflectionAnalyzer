/*
@file    		measure.c
@brief   		Implementation of an efficient measurement interrupt-routine with simple filtering and conversion of values (implemented for XMC4700 and DAVE)
@version 		1.0
@date    		2020-02-20
@author 		Rene Santeler @ MCI 2020/21
 */

#include <DAVE.h>
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <math.h>
#include "globals.h"
#include "measure.h"

/// Implemented in globals:
// struct's: sensor
// #define's: FIFO_LINE_SIZE_PAD, FIFO_BITS_ALL_BLOCK, FIFO_BITS_ONE_BLOCK, FIFO_BLOCKS, SENSOR_RAW_SIZE, SENSORS_SIZE and
//            POSTPROCESS_INTERPOLATE_ERRORS or POSTPROCESS_CHANGEORDER_AT_ERRORS,
//            INPUT_STANDARD or other STANDARD_...
extern volatile uint8_t main_trigger;			// triggers main slope execution
extern volatile sensor* sensors[];			// array of all sensors that need to be evaluated
extern volatile measureModes measureMode;	// state of the measurement (purpose: none, monitoring or recording)
/// FIFO-variables
extern volatile uint8_t volatile * volatile fifo_buf;	// FIFO buffer
extern volatile uint16_t fifo_writeBufIdx;				// index in FIFO buffer
extern volatile uint8_t fifo_writeBlock;				// current block to write
extern volatile uint8_t fifo_finBlock[];				// array of which block is finished an can be recorded


/// Implementation of an moving average filter on an ring-buffer. This version is very fast but it needs to be started on an 0'd out buffer and the filter interval sum must not be changed outside of this!!!
/// If this gets out of sync, use the slow version measure_movAvgFilter_clean before using this again (globals.h).
/// Note: This only subtracts the oldest and adds the newest entry to the stored sum before dividing.
#define MEASURE_MOVAVGFILTER(sens, divider)                       				\
	/* Get index of oldest element, which shall be removed
	 * (current index - filter interval with roll-over check) */					\
	int32_t oldIdx = sens->bufIdx - sens->avgFilterInterval;					\
	if(oldIdx < 0) oldIdx += sens->bufMaxIdx+1;									\
	/* Subtract oldest element and add newest to sum */							\
	sens->avgFilterSum += sens->bufRaw[sens->bufIdx] - sens->bufRaw[oldIdx];	\
	/* Calculate average and return it */										\
	sens->bufFilter[sens->bufIdx] = sens->avgFilterSum / divider;

/// Compute filtered raw value to converted value and save it
#define MEASURE_POLYCONVERSION(sens, sensBufIdx)			\
	/* Value of sum and result are stored in register to
	 *  enhance speed (multiple successive access). Use
	 *  first coefficient (constant) as init value.*/		\
	register float result = sens->fitCoefficients[0];		\
	register float pow_x = 1;								\
	/* Calculate every term and add it to the result*/		\
	for(register uint8_t i = 1; i < sens->fitOrder+1; i++){	\
		pow_x *= sens->bufFilter[sensBufIdx];				\
		result += sens->fitCoefficients[i] * pow_x; 		\
	}														\
	/* Save result*/										\
	sens->bufConv[sensBufIdx] = result;




void measure_IRQ_handler(void){
	/// Interrupt handler - Do measurements, filter/convert them and store result in buffers. Allows to 'measure' self produced test signal based on value in global variable InputType
	/// Start Timer after init and make sure initial conversion in ADC_MEASUREMENT APP is deactivated
	///
	/// Uses global/externs: ADC_MEASUREMENT APP, INPUT_[...], InputType, tft_tick, measureMode, sensor[...], measurementCounter, fifo_[...]

	// Timing measurement pin high
	DIGITAL_IO_SetOutputHigh(&IO_6_2_TIMING);


	// Check
	uint8_t sensIdx = 0;
	volatile sensor* sens;
	uint16_t sensBufIdx;

	do{
		// Store current sensor pointer (looks cleaner and may be faster without the additional indexing every time)
		sens = sensors[sensIdx];

		// Increment buffer index and store current ADC value if a measuring mode is active
		if(measureMode != measureModeNone){
			// Note: Use bufRaw as buffer in order to not have execution-time varying commands before the measurement (index overleap when using "sens->bufRaw[sensBufIdx]" directly)
			int_buffer_t bufRaw;

			// Read value from sensor (or generated test if defined)
			#if defined(INPUT_STANDARD)
				// Get raw input from ADC
				bufRaw = ADC_MEASUREMENT_GetResult(sens->adcChannel);//(&ADC_MEASUREMENT_Channel_A);
			#elif defined(INPUT_TESTIMPULSE)
			// 1 TestImpulse
				if(sensBufIdx % (S1_BUF_SIZE/5)) bufRaw[sensBufIdx] = 0;
				else bufRaw[sensBufIdx] = 4095;
			#elif defined(INPUT_TESTSAWTOOTH)
				// 2 TestSawtooth
				static uint32_t lastval; // just for TestTriangle signal
				if(lastval < 4095) bufRaw[sensBufIdx] = lastval+7;
				else bufRaw[sensBufIdx] = 0;
				lastval = bufRaw[sensBufIdx];
			#elif defined(INPUT_TESTSINE)
				// 3 TestSine (Note: The used time variable is not perfect for this purpose - just for test)
				bufRaw[sensBufIdx] = (uint32_t)((0.5*(1.0+sin(2.0 * M_PI * 11.725 * ((double)measurementCounter/3000))))*4095.0);
			#endif

			// Increment current Buffer index and set back to 0 if greater than size of array
			// Note: This is designed to take about the same amount of time either case but it still has potential to make the sampling time a bit async when overleaping. If even higher accuracy is needed move this to a second slope after all sensor values are retrieved.
			sensBufIdx = sens->bufIdx + 1;
			if(sensBufIdx > sens->bufMaxIdx)
				sens->bufIdx = sensBufIdx = 0;
			else
				sens->bufIdx = sensBufIdx;

			// Store raw value
			sens->bufRaw[sensBufIdx] = bufRaw;


			// (Consideration) Everything past here could be moved to the main slope. It would require a "last processed value" index and had to process every missed value between. The interrupt here would get much shorter though...


			// If system is in monitoring mode, filter/convert row values and fill corresponding buffers (+ error recognition)
			if(measureMode == measureModeMonitoring){
				// Error handling and calculation of raw/filtered/converted value
				measure_postProcessing(sens);
			}
			// If system is in recording mode store current raw value in FIFO
			else if(measureMode == measureModeRecording){
				// Copy current value to FIFO
				memcpy((void*)(fifo_buf + fifo_writeBufIdx), &(sens->bufRaw[sensBufIdx]), SENSOR_RAW_SIZE);

				// Increase index (this is OK without overleap check because buffer is designed to fill up perfectly!)
				fifo_writeBufIdx += SENSOR_RAW_SIZE;
			}
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
				measureMode = measureModeRecordError;
			}
		}
	}

	// Trigger next main loop (with this it is running in sync with the measurement)
	main_trigger = 42;

	// Increase count of executed measurements
	measurementCounter++;

	// Timing measurement pin low
	DIGITAL_IO_SetOutputLow(&IO_6_2_TIMING);
}











void measure_postProcessing(volatile sensor* sens){
	/// Uses the raw buffer and current raw-value to detect errors, filter the data and convert. The processing of the
	/// filtered and converted value is designed to be fast and accurate enough for monitoring. However for actual
	/// precise results the raw value must be post-processed externally! The error handling is somewhat complicated
	/// and can be controlled by global defined macros. In this context this is only necessary because the filter would
	/// otherwise get confused by faulty values. This also allows "clear" lines to be printed to on the monitoring graph.
	///
	/// Input: Takes the array of sensors to be processed. Make sure the newest raw is already in buffer.
	///
	///	 Uses measure-global macros:
	///		MEASURE_MOVAVGFILTER, MEASURE_POLYCONVERSION
	///
	///	 Uses global variables macros:
	///		POSTPROCESS_CHANGEORDER_AT_ERRORS, POSTPROCESS_INTERPOLATE_ERRORS, POSTPROCESS_BUGGED_VALUES


#if POSTPROCESS_CHANGEORDER_AT_ERRORS == 1
	/// Check for sensor errors and try to reduce filter interval on every encounter. Detects errors that are entering or
	/// leaving the filter interval and changes the filter interval accordingly. Set sens.errorOccured to sens.avgFilterOrder
	/// at the beginning of the measurement to ignore not yet written values and allow correct values to be written (no
	/// ramp up). Errors are represented by a 0 value in the raw buffer. Make sure this can never be an actual value.

	// The compensated filter interval
	int16_t compFilterInterval;

	/// Check if oldest value in filter interval (to be removed) was an error, if so decrease error counter
	if(sens->errorOccured != 0){
		// Get oldest index
		int32_t oldIdx = sens->bufIdx - sens->avgFilterInterval;
		if(oldIdx < 0) oldIdx += sens->bufMaxIdx+1;
		// Decrease error counter
		if(sens->bufRaw[oldIdx] == 0)
			sens->errorOccured--;
	}

	/// Check if newest value in filter interval (to be added) is an error, if so increase error counter, null raw value
	/// null raw value and limit max amount of errors possible
	if(sens->bufRaw[sens->bufIdx] > sens->errorThreshold){
		// Increment Count of errors in filter interval
		sens->errorOccured++;

		// Set raw value to 0
		sens->bufRaw[sens->bufIdx] = 0;

		// If more errors occurred than can be compensated by the avg filter, limit it to stay in interval
		if(sens->errorOccured > sens->avgFilterInterval)
			sens->errorOccured = sens->avgFilterInterval;
	}

	// If no errors in filter interval - use avgFilterOrder as average divider
	// Else if errors in filter interval - use filter interval compensated by number of errors occurred (number of actual values in interval, used) as divider
	if(sens->errorOccured == 0)
		compFilterInterval = sens->avgFilterInterval;
	else
		compFilterInterval = sens->avgFilterInterval - sens->errorOccured;

#elif POSTPROCESS_INTERPOLATE_ERRORS == 1
	/// Check for sensor errors and try to interpolate a raw value. This should be OK as long as the frequency of the,
	/// to be measured, event is much lower than the frequency time, the average filter interval adjusted to the application
	/// and only few errors occur at a time. Square interpolation was ruled out due to performance issues (on first tests
	/// the linear approach made the slope only ~400ns slower when errors occur). However with this method the raw value
	/// can't be used to detect errors (it might be interpolated!) therefore the errorOccured property of the sensor
	/// struct must be recorded as well!

	// Filter interval is never changed with this approach
	static int16_t compFilterInterval = sens->avgFilterInterval;

	/// Check if newest value in filter interval (to be added) is an error
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
#else
#error "Error: Global macro POSTPROCESS_INTERPOLATE_ERRORS or POSTPROCESS_CHANGEORDER_AT_ERRORS must be set to 1!"
#endif

	/// Post processing: Calculate filtered/converted value and fill corresponding buffers
#if POSTPROCESS_BUGGED_VALUES == 1
	// Always calculate filtered and converted value if possible (even if current value was an error)
	if(compFilterInterval){
#else
	// Only calculate filtered and converted value if no error are in filter interval
	if(compFilterInterval && sens->bufRaw[sens->bufIdx] != 0){
#endif
		// Set current filter value
		MEASURE_MOVAVGFILTER(sens, compFilterInterval);

		// Set current converted value
		MEASURE_POLYCONVERSION(sens, sens->bufIdx);
	}
	// compFilterOrder is 0 - no data available
	else{
		// Set current filter value to 0 (current value is unusable)
		MEASURE_MOVAVGFILTER(sens, 1); // Only to keep sum up to date - result must be ignored
		sens->bufFilter[sens->bufIdx] = 0;

		// Set current converted value to 0 (current value is unusable)
		sens->bufConv[sens->bufIdx] = 0;
	}
}

