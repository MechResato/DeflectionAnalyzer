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


FATFS fs; /* File system object (volume work area) */
FIL fil; /* File object */


void record_buffer(void)
{
	FRESULT res; /* API result code */
	UINT bw,br; /* Bytes written */
	char buff[20];

	/* Register work area */
	res = f_mount(&fs, "0:", 1);
	if (res == FR_OK)
	{
		/* Create a new directory */
		res = f_mkdir("XMC4500");
		if ((res == FR_OK) || (res == FR_EXIST))
		{
			/* Create a file as new */
			res = f_open(&fil, "XMC4500/hello.txt", FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
			if ((res == FR_OK) || (res == FR_EXIST))
			{
				/* Write a message */
				res = f_write(&fil, "Hello, World!\r\n", 15, &bw);
				if (res == FR_OK )
				{
					res = f_lseek(&fil, 0);
					/* Read the buffer from file */
					res = f_read(&fil, &buff[0], 15, &br);
					if(res == FR_OK)
					{
						/* Go to location 15 */
						res = f_lseek(&fil, 15);
						if(res == FR_OK)
						{
							/* Add some more content to the file */
							res = f_write(&fil, "\nWelcome to Infineon", 20, &bw);
							if(res == FR_OK)
							{

							}
						}
					}
				}
			}
			f_close(&fil);
		}
		res = f_unmount("0:");
	}


}
