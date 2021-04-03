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
//static FILINFO fno; // File information object

DSTATUS diskStatus;

extern sdStates sdState;

/// Note: This implementation allows only one open file at a time!
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
		printf("SD still open, state: %d. Unmounting: %d\n", sdState, res);
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
		res = f_open(&fil, path, FA_OPEN_ALWAYS | FA_WRITE | FA_READ);

		if ((res == FR_OK) || (res == FR_EXIST)){
			printf("File opened: %s\n", path);
			sdState = sdFileOpen;
		}
		else if (res == FR_INVALID_NAME){
			printf("File open error: FR_INVALID_NAME '%s'\n", path);
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
	/// TODO: Check write results and written bytes for consistency

	FRESULT res = 0; /* API result code */
	UINT bw,br; /* Bytes written */
	char buff[400];

	// Try to mount disk
	record_mountDisk(1);

	// If the SD card is ready backup existing file, try to open the new one and write specifications
	if(sdState == sdMounted || sdState == sdFileOpen){

		/// Check if file already exists - if so rename it
		// Get file-info to check if it exists
		res = f_stat((char*)&sens->fitFilename[0], NULL); // Use &fno if actual file-info is needed
		if(res == FR_OK){
			sprintf(buff,"s%d_backup.spec", sens->index);
			f_rename((char*)&sens->fitFilename[0], buff);
		}
		else{
			printf("File not found: %d\n", res);
		}

		// Open/Create File
		record_openFile((char*)&sens->fitFilename[0]);

		// If file is ready to be written to ...
		if(sdState == sdFileOpen){
			/// ... write comment and content lines alternating to file
			// Single loop do-while slope to handle errors clean with break; (inspired by Infineon "FATFS_EXAMPLE_XMC47": https://www.infineon.com/cms/en/product/promopages/aim-mc/dave_downloads.html)
			do{
				printf("Writing spec file\n");

				// Write header
				f_printf(&fil, "Specification of sensor %d '%s'. Odd lines are comments, even lines are values. Float values are converted to 32bit integer hex (memory content). ", sens->index, sens->name);
				printf("Write header\n");

				// Write fit order header and value in separate lines
				f_printf(&fil, "Curve fit function order:\n");
				printf("Write fit order comment\n");
				sprintf(buff,"%d\n", sens->fitOrder);
				res = f_write(&fil, buff, strlen(buff), &bw);
				if (res != FR_OK) break;
				printf("Write fit order\n");

				// Write coefficients header and value in separate lines. Note: Floating point precision is taken from FLT_DIG (=here 6). See https://www.h-schmidt.net/FloatConverter/IEEE754.html for an online converter.
				sprintf(buff,"Coefficients (%.8f, %.8f, %.8f, %.8f):\n", sens->fitCoefficients[0], sens->fitCoefficients[1], sens->fitCoefficients[2], sens->fitCoefficients[3]);
				res = f_write(&fil, buff, strlen(buff),&bw);
				printf("Write coefficients comment\n");
				sprintf(buff,"%08lX,%08lX,%08lX,%08lX\n", *(unsigned long*)&sens->fitCoefficients[0], *(unsigned long*)&sens->fitCoefficients[1], *(unsigned long*)&sens->fitCoefficients[2], *(unsigned long*)&sens->fitCoefficients[3]);
				res = f_write(&fil, buff, strlen(buff),&bw);
				if (res != FR_OK) break;
				printf("Write coefficients\n");

				// numDataPoints header and value in separate lines
				f_printf(&fil, "Number of data points:\n");
				sprintf(buff,"%d\n", dp_size);
				res = f_write(&fil, buff, strlen(buff),&bw);
				if (res != FR_OK) break;
				printf("Write numDataPoints\n");

				// DataPoints x-value header and value in separate lines
				sprintf(buff,"Data points x (%.8f", dp_x[0]);
				res = f_write(&fil, buff, strlen(buff),&bw);
				if (res != FR_OK) break;
				for (uint8_t i = 1; i < dp_size; i++){
					sprintf(buff,",%.8f", dp_x[i]);
					res = f_write(&fil, buff, strlen(buff),&bw);
					if (res != FR_OK) break;
				}
				printf("DataPoints x Comment\n");

				sprintf(buff,"):\n%08lX", *(unsigned long*)&dp_x[0]);
				res = f_write(&fil, buff, strlen(buff),&bw);
				if (res != FR_OK) break;
				for (uint8_t i = 1; i < dp_size; i++){
					sprintf(buff,",%08lX", *(unsigned long*)&dp_x[i]);
					res = f_write(&fil, buff, strlen(buff),&bw);
					if (res != FR_OK) break;
				}
				printf("DataPoints x\n");

				// DataPoints y-value header and value in separate lines
				sprintf(buff,"\nData points y (%.8f", dp_y[0]);
				res = f_write(&fil, buff, strlen(buff),&bw);
				if (res != FR_OK) break;
				for (uint8_t i = 1; i < dp_size; i++){
					sprintf(buff,",%.8f", dp_y[i]);
					res = f_write(&fil, buff, strlen(buff),&bw);
					if (res != FR_OK) break;
				}
				printf("DataPoints y Comment\n");

				sprintf(buff,"):\n%08lX", *(unsigned long*)&dp_y[0]);
				res = f_write(&fil, buff, strlen(buff),&bw);
				if (res != FR_OK) break;
				for (uint8_t i = 1; i < dp_size; i++){
					sprintf(buff,",%08lX", *(unsigned long*)&dp_y[i]);
					res = f_write(&fil, buff, strlen(buff),&bw);
					if (res != FR_OK) break;
				}
				printf("DataPoints y\n");
			} while(false);

			// Close file
			record_closeFile();
		}
		else{
			printf("Write Spec File not open\n");
		}
	}


	br = 0;
	bw = 0;
	br = bw;
	bw = br;

}


void record_readSpecFile(volatile sensor* sens, float** dp_x, float** dp_y, uint16_t* dp_size){
	///
	/// This function is not optimized for high speed. It should only be used when performance is not top priority (setup before actual start of record, not during).
	/// On 03.04.2021 this function measured to take about 266ms to complete...

	FRESULT res = 0; /* API result code */

	//UINT bw,br; /* Bytes written */
	char buff[400];
	TCHAR* res_buf;

	// Try to mount disk
	record_mountDisk(1);

	// If the SD card is ready backup existing file, try to open the new one and write specifications
	if(sdState == sdMounted || sdState == sdFileOpen){

		/// Check if file exists - if so open it
		printf("Check if file exists\n");
		res = f_stat((char*)&sens->fitFilename[0], NULL); // Use &fno if actual file-info is needed
		if(res == FR_OK){
			// Open/Create File
			printf("Open file\n");
			record_openFile((char*)&sens->fitFilename[0]);

			// If file is ready ...
			if(sdState == sdFileOpen){
				/// ... read every second line and write it to its corresponding value
				// Single loop do-while slope to handle errors clean with break (inspired by Infineon "FATFS_EXAMPLE_XMC47": https://www.infineon.com/cms/en/product/promopages/aim-mc/dave_downloads.html)
				do{
					printf("do while\n");
					res = f_lseek(&fil, 0);
					if (res != FR_OK) break;

					/// Read fit order
					// Read comment line (ignore it) then read actual data line into buffer and stop process if the result isn't OK
					res_buf = f_gets(buff, 400, &fil);
					printf("Spec header: '%s'\n", buff);
					res_buf = f_gets(buff, 400, &fil);
					if (res_buf == 0) break;
					// Convert read string to unsigned long and write back to sensor struct
					sens->fitOrder = strtoul(buff, NULL, 10);
					printf("fitOrder %d: '%s'\n", sens->fitOrder, buff);

					/// Read coefficients
					// Read comment line (ignore it) then read actual data line into buffer and stop process if the result isn't OK
					res_buf = f_gets(buff, 100, &fil);
					res_buf = f_gets(buff, 100, &fil);
					if (res_buf == 0) break;
					// Convert read string to long and write back to sensor struct
					char *ptr = &buff[0];
					for (uint8_t i = 0; i < 4; i++) {
						printf("coefficients %d: '%s' ",i , ptr);
						// Read as hex long
						unsigned long tmp = strtoul(ptr, &ptr, 16);
						// Convert to float and write to sensor struct
						sens->fitCoefficients[i] = *(float*)&tmp;
						printf("= %.8f\n", sens->fitCoefficients[i]);
						// Ignore separator between values
						if( (ptr - &buff[0]) < strlen(&buff[0]) )
							ptr++;
					}

					printf("After coefficients\n");

					// Read numDataPoints
					res_buf = f_gets(buff, 400, &fil);
					res_buf = f_gets(buff, 400, &fil);
					printf("buff: '%s' res %d\n", buff, (int)res_buf);
					if (res_buf != 0 && dp_size != NULL){
						printf("Reading DPs\n");
						*dp_size = (uint8_t)strtoul(buff, NULL, 10);
						printf("dp_size %d\n", *dp_size);


						// Allocate Memory for data points
						if(*dp_size >= 1){
							*dp_y = (float*)malloc(*dp_size*sizeof(float));
							*dp_x = (float*)malloc(*dp_size*sizeof(float));
							char *ptr = &buff[0];

							// Check for allocation errors
							if(*dp_y == NULL || *dp_x == NULL)
								printf("Read spec: Memory malloc failed!\n");

							// Read DataPoints y-value
							res_buf = f_gets(buff, 100, &fil);
							res_buf = f_gets(buff, 100, &fil);
							if (res_buf == 0){
								buff[0] = '-';
								buff[1] = '1';
								buff[2] = '\0';
							}
							ptr = &buff[0];
							for (uint8_t i = 0; i < *dp_size; i++) {
								printf("y %d: ",i );
								// Read as hex long
								unsigned long tmp = strtoul(ptr, &ptr, 16);
								// Convert to float and write to sensor struct
								(*dp_y)[i] = *(float*)&tmp; // TODO
								printf("= %.8f\n", (*dp_y)[i]);
								// Ignore separator between values
								if( (ptr - &buff[0]) < strlen(&buff[0]) )
									ptr++;
							}

							// Read DataPoints x-value
							res_buf = f_gets(buff, 100, &fil);
							res_buf = f_gets(buff, 100, &fil);
							if (res_buf == 0){
								buff[0] = '-';
								buff[1] = '1';
								buff[2] = '\0';
							}
							ptr = &buff[0];
							for (uint8_t i = 0; i < *dp_size; i++) {
								printf("x %d: ",i );
								// Read as hex long
								unsigned long tmp = strtoul(ptr, &ptr, 16);
								// Convert to float and write to sensor struct
								(*dp_x)[i] = *(float*)&tmp;
								printf("= %.8f\n", (*dp_x)[i]);
								// Ignore separator between values
								if( (ptr - &buff[0]) < strlen(&buff[0]) )
									ptr++;
							}
						}
					}
					else{
						printf("No num data points or no pointers given\n");
					}
					// Close file
					record_closeFile();
				} while(false);
			}
			else{
				printf("Read Spec File not open\n");
			}
		}

	}
	else{
		printf("File not found: %d\n", res);
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


	br = 0;
	bw = 0;
	br = bw;
	bw = br;

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
