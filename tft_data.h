// Basic idea by Rudolph Riedel, created by RS @ MCI 2020/21
// Used to store graphics in flash and load them to the display when necessary
// See https://brtchip.com/eve-toolchains/ for helpful Tools

#ifndef TFT_DATA_H_
#define TFT_DATA_H_

#if	defined (__AVR__)
	#include <avr/pgmspace.h>
#else
	#define PROGMEM
#endif

extern const uint8_t logo_init[6664] PROGMEM;

// Original data removed by RS 03.01.2021
//extern const uint8_t logo[239] PROGMEM;
//extern const uint8_t pic[3844] PROGMEM;

#endif /* TFT_DATA_H_ */
