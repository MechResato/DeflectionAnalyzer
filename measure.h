/*
 * measure.h
 *
 *  Created on: 20 Feb 2021
 *      Author: RS
 */

#ifndef MEASURE_H_
#define MEASURE_H_

void Adc_Measurement_Handler(void);

extern void measure_movAvgFilter_clean(sensor* sens);

#endif /* MEASURE_H_ */
