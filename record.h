/*
 * record.h
 *
 *  Created on: 25 Feb 2021
 *      Author: Admin
 */

#ifndef RECORD_H_
#define RECORD_H_


enum objFIL{objFILwrite=0, objFILread};
typedef enum objFIL objFIL;

void record_mountDisk(uint8_t mount);
FRESULT record_openFile(const char* path, objFIL objFILrw, uint8_t accessMode);
FRESULT record_closeFile(objFIL objFILrw);
//void record_printf(const char* path);

void record_writeCalFile(sensor* sens, float dp_x[], float dp_y[], uint8_t dp_size);
void record_readCalFile(volatile sensor* sens, float** dp_x, float** dp_y, uint16_t* dp_size);




void record_buffer(void);


int8_t record_start();
int8_t record_stop(uint8_t flushData);
void record_line();
void record_block();

#endif /* RECORD_H_ */
