/*
 * record.c
 *
 *  Created on: 25 Feb 2021
 *      Author: Admin
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <DAVE.h>
#include "globals.h"


static FATFS fs; // File system object (volume work area)
static FIL fil; // File object

DSTATUS diskStatus;

extern sdStates sdState;


void record_mountDisk(uint8_t mount){
	/// Used to mount and unmount a disk.


	// API result code
	FRESULT res;

	// Get current status of the SD
	diskStatus = disk_status(fs.pdrv);
	printf("disk_status = %d\n", diskStatus);

	// Reinitialize disk if there was no disk till now
	if(diskStatus != 0){ // translation of these codes is a bit hidden in the lib. See "FATFS_statuscodes" array in fatfs.c
		// Reset SD state
		sdState = sdNone;

		// Initialize SD
		DSTATUS resDinit = disk_initialize(0);
		printf("Reinit sd = %d\n", resDinit);
	}


	// If an SD is mounted unmount it (always)
	if(sdState != sdNone){
		res = f_unmount("0:");
		printf("SD still open %d, unmounting %d\n", sdState, res);
		sdState = sdNone;
	}

	// If needed, mount the SD and mark state as mounted or error
	if(mount){
		/* Register work area */
		res = f_mount(&fs, "0:", 1);
		if (res == FR_OK) {
			printf("Mount OK\n");
			sdState = sdMounted;
		}
		else if (res == FR_NOT_READY){
			printf("Mount no disk\n");
			sdState = sdNone;
		}
		else {
			printf("Mount error: %d\n", res);
			sdState = sdError;
		}
	}

}


void record_openFile(const char* path){
	/// Used to open a file for read/write access. If another file is still open it will be closed automatically!

	// API result code
	FRESULT res;

	// If SD is not mounted or a file is still open mount/close it
	if(sdState == sdNone)
		record_mountDisk(1);
	else if(sdState == sdFileOpen){
		res = f_close(&fil);
		if (res == FR_OK) {
			printf("Close File OK\n");
			sdState = sdMounted;
		}
		else{
			printf("Close File error: %d\n", res);
			sdState = sdError;
		}
	}

	// If SD is mounted now, try to open the file/path
	if(sdState == sdMounted) {
		res = f_open(&fil, path, FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
		if ((res == FR_OK) || (res == FR_EXIST)){
			printf("File opened\n");
			sdState = sdFileOpen;
		}
		else{
			printf("File open error: %d\n", res);
			sdState = sdError;
		}
	}
}

void record_closeFile(){
	/// Used to close a file.

	// API result code
	FRESULT res;

	// If SD is mounted, try to open the file/path
	if(sdState == sdFileOpen) {
		res = f_close(&fil);
		if (res == FR_OK){
			printf("File closed\n");
			sdState = sdMounted;
		}
		else if(res == FR_INVALID_OBJECT){
			printf("File close warning: %d\n", res);
			sdState = sdMounted;
		}
		else{
			printf("File close error: %d\n", res);
			sdState = sdError;
		}
	}
}






void record_writeSpecFile(sensor* sens, float dp_x[], float dp_y[], uint8_t dp_size){
	///


	FRESULT res = 0; /* API result code */
	UINT bw,br; /* Bytes written */
	char buff[75];

	if(sdState == sdMounted || sdState == sdFileOpen){

		// Check if file already exists - if so rename it

		// Open/Create File

		// If file is ready to be written to ...
		if(sdState == sdFileOpen){

			// Write header
			f_printf(&fil, "Specification file for Sensor %d '%s'. Odd lines are comments, even lines are values. Float values are converted to 32bit integer hex values (memory content).", sens->index, sens->sensorText);

			// Write fit order header and value in separate lines
			f_printf(&fil, "Curve fit function order: %d", sens->fitOrder);
			sprintf(buff,"%d\n", sens->fitOrder);
			res = f_write(&fil, buff, strlen(buff), &bw);

			// Write coefficients header and value in separate lines
			f_printf(&fil, "Coefficients: %.*f, %.*f, %.*f, %.*f\n", FLT_DIG, sens->coefficients[0], FLT_DIG, sens->coefficients[1], FLT_DIG, sens->coefficients[2], FLT_DIG, sens->coefficients[3]);
			sprintf(buff,"%08lX,%08lX,%08lX,%08lX\n", *(unsigned long*)&sens->coefficients[0], *(unsigned long*)&sens->coefficients[1], *(unsigned long*)&sens->coefficients[2], *(unsigned long*)&sens->coefficients[3]);
			res = f_write(&fil, buff, strlen(buff), &bw);

			// numDataPoints header and value in separate lines
			f_printf(&fil, "Number of data points: %d\n", dp_size);
			sprintf(buff,"%d\n", dp_size);
			res = f_write(&fil, buff, strlen(buff), &bw);

			// DataPoints x-value header and value in separate lines
			f_printf(&fil, "Data points x: %.*f", FLT_DIG, dp_x[0]);
			for (uint8_t i = 1; i < dp_size-1; i++)
				f_printf(&fil, ",%.*f", FLT_DIG, dp_x[i]);

			f_printf(&fil, "\n%08lX", *(unsigned long*)&dp_x[0]);
			for (uint8_t i = 1; i < dp_size-1; i++)
				f_printf(&fil, ",%08lX", *(unsigned long*)&dp_x[i]);

			// DataPoints y-value header and value in separate lines
			f_printf(&fil, "\nData points y: %.*f", FLT_DIG, dp_y[0]);
			for (uint8_t i = 1; i < dp_size-1; i++)
				f_printf(&fil, ",%.*f", FLT_DIG, dp_y[i]);

			f_printf(&fil, "\n%08lX", *(unsigned long*)&dp_y[0]);
			for (uint8_t i = 1; i < dp_size-1; i++)
				f_printf(&fil, ",%08lX", *(unsigned long*)&dp_y[i]);

		}
	}

}








void record_buffer(void)
{
	// Example from Infineon for test purpose. TODO

	float testf_w = 1.0/7.0;
	float testf_r = 0.0;


	FRESULT res; /* API result code */
	UINT bw,br; /* Bytes written */
	char buff[50];

	record_mountDisk(1);

	if (sdState == sdMounted) {
		/* Create a new directory */
		res = f_mkdir("Record");
		if ((res == FR_OK) || (res == FR_EXIST))
		{
			/* Create a file as new */
			res = f_open(&fil, "Record/log.txt", FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
			if ((res == FR_OK) || (res == FR_EXIST))
			{
				/* Write a message */
				sprintf(buff,"%08lX\n", *(unsigned long*)&testf_w);
				printf("write: %s\n", buff);
				res = f_write(&fil, buff, 13, &bw);
				//sprintf(buff,"Current value 2 %5d\n", InputBuffer1[InputBuffer1_idx]);
				//res = f_write(&fil, buff, 22, &bw);
				if (res == FR_OK )
				{
					res = f_lseek(&fil, 0);

					//sprintf(Buffer2,"%08lX",*(unsigned long*)&ADCValue2_float2);
					printf("ori: %08lX, %f\n", *(unsigned long*)&testf_w, testf_w);
					f_gets(buff, 25, &fil);
					printf("gets: %s\n", buff);

					uint32_t tmp = strtol(buff, NULL, 16);
					testf_r = *(float*)&tmp;
					printf("strtof: %08lX, %f, %d\n", tmp, testf_r, testf_r==testf_w);

					//f_gets(buff, 25, &fil);
					//printf("2 %s\n", buff);

					/* Read the buffer from file */
					//res = f_read(&fil, &buff[0], 15, &br);
					//if(res == FR_OK)
					//{
					//	/* Go to location 15 */
					//	res = f_lseek(&fil, 15);
					//	if(res == FR_OK)
					//	{
					//		/* Add some more content to the file */
					//		res = f_write(&fil, "\nWelcome to Infineon", 20, &bw);
					//		if(res == FR_OK)
					//		{
					//
					//		}
					//	}
					//}
				}
				else{
					printf("Write Failed\n");
				}
			}
			diskStatus = disk_status(fs.pdrv);
			printf("disk_status open = %d\n", diskStatus);


			res = f_close(&fil);
			printf("f_close 1: %d\n", res);
			res = f_close(&fil);
			printf("f_close 2: %d\n", res);
		}
		diskStatus = disk_status(fs.pdrv);
		printf("disk_status mount = %d\n", diskStatus);


		res = f_unmount("0:");
	}



	//res = f_stat("Record", NULL); //FR_NOT_ENABLED
	//printf("f_stat = %d\n", res);
}




/* Register work area */
//res = f_mount(&fs, "0:", 1);
//if (res == FR_OK)
//{
//	/* Create a new directory */
//	res = f_mkdir("Record");
//	if ((res == FR_OK) || (res == FR_EXIST))
//	{
//		/* Create a file as new */
//		res = f_open(&fil, "Record/log.txt", FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
//		if ((res == FR_OK) || (res == FR_EXIST))
//		{
//			/* Write a message */
//			sprintf(buff,"Current value %d\n", InputBuffer1[InputBuffer1_idx]);
//			res = f_write(&fil, buff, 15, &bw);
//			if (res == FR_OK )
//			{
//				res = f_lseek(&fil, 0);
//				/* Read the buffer from file */
//				res = f_read(&fil, &buff[0], 15, &br);
//				if(res == FR_OK)
//				{
//					/* Go to location 15 */
//					res = f_lseek(&fil, 15);
//					if(res == FR_OK)
//					{
//						/* Add some more content to the file */
//						res = f_write(&fil, "\nWelcome to Infineon", 20, &bw);
//						if(res == FR_OK)
//						{
//
//						}
//					}
//				}
//			}
//		}
//		f_close(&fil);
//	}
//	res = f_unmount("0:");
//}
