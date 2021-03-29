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
#include <DAVE.h>
#include "globals.h"


FATFS fs; /* File system object (volume work area) */
FIL fil; /* File object */


void record_buffer(void)
{
	// Example from Infineon for test purpose. TODO

	float testf_w = 1.0/7.0;
	float testf_r = 0.0;


	FRESULT res; /* API result code */
	UINT bw,br; /* Bytes written */
	char buff[50];

	/* Register work area */
	res = f_mount(&fs, "0:", 1);
	if (res == FR_OK)
	{
		/* Create a new directory */
		res = f_mkdir("Record");
		if ((res == FR_OK) || (res == FR_EXIST))
		{
			/* Create a file as new */
			res = f_open(&fil, "Record/log.txt", FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
			if ((res == FR_OK) || (res == FR_EXIST))
			{
				/* Write a message */
				sprintf(buff,"%a\n", testf_w);
				printf("init: %s\n", buff);
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

					testf_r = strtof(buff, NULL);
					printf("strtof: %a, %f\n", testf_r, testf_r);

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
			f_close(&fil);
		}
		res = f_unmount("0:");
	}


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
