/*
 * record.h
 *
 *  Created on: 25 Feb 2021
 *      Author: RS
 */

#ifndef RECORD_H_
#define RECORD_H_


enum objFIL{objFILwrite=0, objFILread};
typedef enum objFIL objFIL;

void record_mountDisk(uint8_t mount);
void record_convertBinFile(const char* filename_BIN, sensor** sensArray);
//FRESULT record_openFile(const char* path, objFIL objFILrw, uint8_t accessMode);
//FRESULT record_closeFile(objFIL objFILrw);


void record_writeCalFile(sensor* sens);
void record_readCalFile(volatile sensor* sens);


uint8_t record_openBMP(const char* path);
void record_writeBMP(uint32_t* data, uint16_t size);
void record_closeBMP();


int8_t record_start();
void record_block();
int8_t record_stop(uint8_t flushData);


#endif /* RECORD_H_ */
