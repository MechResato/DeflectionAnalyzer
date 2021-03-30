/*
 * record.h
 *
 *  Created on: 25 Feb 2021
 *      Author: Admin
 */

#ifndef RECORD_H_
#define RECORD_H_



void record_mountDisk(uint8_t mount);
void record_openFile(const char* path);
void record_closeFile();
void record_printf(const char* path);


void record_buffer(void);



#endif /* RECORD_H_ */
