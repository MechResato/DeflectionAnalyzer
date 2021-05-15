/*
 * measure.h
 *
 *  Created on: 20 Feb 2021
 *      Author: RS
 */

#ifndef MEASURE_H_
#define MEASURE_H_

extern volatile uint8_t* volatile fifo_buf;


void measure_IRQ_handler(void);

void measure_postProcessing(volatile sensor* sens);

#endif /* MEASURE_H_ */
