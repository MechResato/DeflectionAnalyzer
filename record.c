/*
 * record.c
 *
 *  Created on: 25 Feb 2021
 *      Author: Admin
 */


// NOTE: Inside the fatfs DAVE App "LFN - Long File Names" are disabled per default and can only be enabled manually (must be redone after every "Generate code") at "ffconf.h" line 114 value "#define FF_USE_LFN		1"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <DAVE.h>
#include "globals.h"
#include "record.h"

DSTATUS diskStatus;
static FATFS fs; // File system object (volume work area)
static FIL fil_r; // File object used for read only
static FIL fil_w; // File object used for write only
//static FILINFO fno; // File information object

/// External variables
extern sdStates sdState;
extern measureModes measureMode;

/// Internal functions
static FRESULT record_openFile(const char* path, objFIL objFILrw, uint8_t accessMode);
static FRESULT record_closeFile(objFIL objFILrw);
static int8_t record_checkEndOfFile(objFIL objFILrw);
static uint8_t record_writeCalFile_pair (char* comment, char* val_buff);
static int8_t record_backupFile(const char* path);



void record_mountDisk(uint8_t mount){
	/// Used to mount and unmount a disk.


	// FATFS result code
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

/// Note: This implementation allows only one read and one write file to be open at the same time!
static FRESULT record_openFile(const char* path, objFIL objFILrw, uint8_t accessMode){
	/// Used to open a file on the read or write FIL struct for read/write access. If another file is still open it will be closed automatically!
	/// Todo

	// FATFS result code
	FRESULT res;

	// If SD is not mounted or a file is still open mount/close it
	if(sdState == sdNone)
		record_mountDisk(1);
	else{
		// Initialize result variable to an undefined value to detect if a result was written
		res = 127;

		// Close already open file if needed
		if(objFILrw == objFILread && fil_r.obj.fs != NULL){
			res = f_close(&fil_r);
		}
		else if(objFILrw == objFILwrite && fil_w.obj.fs != NULL){
			res = f_close(&fil_w);
		}

		// Check result if it has changed
		if(res != 127){
			if (res == FR_OK) {
				printf("Close File OK\n");
				sdState = sdMounted;
			}
			else{
				printf("Close File error: %d\n", res);
				sdState = sdError;
			}
		}
	}

	// If SD is mounted now, try to open the file/path
	if(sdState == sdMounted) {
		// Check if a accessMode is given, else use FA_OPEN_ALWAYS
		if(accessMode == 0)
			accessMode = FA_OPEN_ALWAYS;

		// Open file
		if(objFILrw == objFILread)
			res = f_open(&fil_r, path, accessMode | FA_READ);
		else if (objFILrw == objFILwrite)
			res = f_open(&fil_w, path, accessMode | FA_WRITE | FA_READ);

		// Check if open was successful
		if ((res == FR_OK) || (res == FR_EXIST)){
			printf("File opened: %s\n", path);
			sdState = sdFileOpen;
		}
		else if (res == FR_INVALID_NAME){
			printf("File open error: FR_INVALID_NAME '%s'\n", path);
			sdState = sdError;
		}
		else{
			printf("File open error: %d\n", res);
			sdState = sdError;
		}
	}

	// Return last result
	return res;
}

static FRESULT record_closeFile(objFIL objFILrw){
	/// Used to close the file of the write or read FIL struct.

	// FATFS result code
	FRESULT res;

	// If SD is mounted and the corresponding file is still open close it
	if(sdState == sdFileOpen){
		// Close already open file if needed
		if(objFILrw == objFILread && fil_r.obj.fs != NULL){
			res = f_close(&fil_r);
		}
		else if(objFILrw == objFILwrite && fil_w.obj.fs != NULL){
			res = f_close(&fil_w);
		}

		// Check result
		if (res == FR_OK) {
			printf("Close File OK\n");
			sdState = sdMounted;
		}
		else if(res == FR_INVALID_OBJECT){ // Note: Unsure if needed - check this again
			printf("File close warning: %d\n", res);
			sdState = sdMounted;
		}
		else{
			printf("Close File error: %d\n", res);
			sdState = sdError;
		}
	}

	// Return last result
	return res;
}

static int8_t record_checkEndOfFile(objFIL objFILrw){
	/// Check and return end of file status of given file
	/// Return 1 if if EOF is reached, 0 if its not reached and -1 if the requested file is wrong or not open
	///
	/// objFILrw	...	Requested file (see objFIL, write or read file)


	// Return end of file status of given file
	if(objFILrw == objFILread && fil_r.obj.fs != NULL)
		return f_eof(&fil_r);
	else if(objFILrw == objFILwrite && fil_w.obj.fs != NULL)
		return f_eof(&fil_w);

	// Return error if the requested file is wrong or not open
	return -1;
}




static uint8_t record_writeCalFile_pair (char* comment, char* val_buff){
	/// Writes a comment and an actual value as two separate lines to the file
	/// Returns 1 if there was an error, 0 means success.


	// FATFS result code and bytes written
	FRESULT res = 0;
	UINT bw;

	// Write comment
	printf("Comment: %s", comment);
	f_printf(&fil_w, comment);

	// Write value
	printf("Value: %s", val_buff);
	res = f_write(&fil_w, val_buff, strlen(val_buff), &bw);

	// Return 1 if there was an error, else 0
	if (res != FR_OK || bw <= 0)
		return 1;
	else
		return 0;
}

void record_writeCalFile(sensor* sens, float dp_x[], float dp_y[], uint8_t dp_size){
	/// Write the calibration/specification data of the given sensor to the file stated in the sensor struct.
	// If the File already exists, the current version will be backed up (means: only 2 version are saved current and last file!)
	/// Returns nothing.
	///
	///	sens	...	A struct of type sensor which holds all sensor data
	/// dp_x	... A float array holding all x-values (nominal/ ADC output) used to do the curve fit (sorted!)
	/// dp_y	... A float array holding all y-values (actual value in units e.g mm) used to do the curve fit (corresponding to x-values!)
	/// dp_size	... Number of data points (elements in dp_x and dp_y)


	// FATFS result code and two string buffers for comment and values
	FRESULT res = 0;
	char c_buff[400];
	char buff[400];

	// Initial log line
	printf("\nrecord_writeSpecFile:\n");

	// Try to mount disk
	record_mountDisk(1);

	// If the SD card is ready, backup existing file, try to open the new one and write specifications
	if(sdState == sdMounted || sdState == sdFileOpen){

		/// Check if file already exists - if so rename it
		// Get file-info to check if it exists
		res = f_stat((char*)&sens->fitFilename[0], NULL); // Use &fno if actual file-info is needed
		if(res == FR_OK){
			sprintf(buff, "S%dBAK.CAL", sens->index);
			res = f_rename((char*)&sens->fitFilename[0], buff);
			printf("Backup CAL file to %s: %d\n", buff, res);
		}
		else{
			printf("File not found: %d\n", res);
		}

		// Open/Create File
		record_openFile((char*)&sens->fitFilename[0], objFILwrite, 0);

		// If file is ready to be written to ...
		if(sdState == sdFileOpen){
			/// ... write comment and content lines alternating to file
			// Single loop do-while slope to handle errors clean with break; (inspired by Infineon "FATFS_EXAMPLE_XMC47": https://www.infineon.com/cms/en/product/promopages/aim-mc/dave_downloads.html)
			do{
				// Write header
				f_printf(&fil_w, "Specification of sensor %d '%s'. Odd lines are comments, even lines are values. Float values are converted to 32bit integer hex (memory content). ", sens->index, sens->name);
				printf("Write header\n");

				// Write fit order comment and value in separate lines
				sprintf(buff,"%d\n", sens->fitOrder);
				if( record_writeCalFile_pair("Curve fit function order:\n", &buff[0]) ) break;

				// Write coefficients comment and value in separate lines. Note: Floating point precision is taken from FLT_DIG (=here 6). See https://www.h-schmidt.net/FloatConverter/IEEE754.html for an online converter.
				sprintf(c_buff,"Coefficients (%.8f, %.8f, %.8f, %.8f):\n", sens->fitCoefficients[0], sens->fitCoefficients[1], sens->fitCoefficients[2], sens->fitCoefficients[3]);
				sprintf(buff,"%08lX,%08lX,%08lX,%08lX\n", *(unsigned long*)&sens->fitCoefficients[0], *(unsigned long*)&sens->fitCoefficients[1], *(unsigned long*)&sens->fitCoefficients[2], *(unsigned long*)&sens->fitCoefficients[3]);
				if( record_writeCalFile_pair(&c_buff[0], &buff[0]) ) break;

				// Write avg filter order comment and value in separate lines
				sprintf(buff,"%d\n", sens->avgFilterOrder);
				if( record_writeCalFile_pair("Average filter order:\n", &buff[0]) ) break;

				// Write avg filter order comment and value in separate lines
				sprintf(buff,"%d\n", sens->errorThreshold);
				if( record_writeCalFile_pair("Error threshold:\n", &buff[0]) ) break;

				// Write numDataPoints comment and value in separate lines
				sprintf(buff,"%d\n", dp_size);
				if( record_writeCalFile_pair("Number of data points:\n", &buff[0]) ) break;

				// DataPoints x-value comment
				sprintf(c_buff,"Data Points x (%.8f", dp_x[0]);
				for (uint8_t i = 1; i < dp_size; i++)
					sprintf(&c_buff[0] + (strlen(c_buff)),",%.8f", dp_x[i]);
				sprintf(&c_buff[0] + (strlen(c_buff)),"):\n");
				// DataPoints x-values
				sprintf(buff,"%08lX", *(unsigned long*)&dp_x[0]);
				for (uint8_t i = 1; i < dp_size; i++)
					sprintf(&buff[0] + (strlen(buff)),",%08lX", *(unsigned long*)&dp_x[i]);
				sprintf(&buff[0] + (strlen(buff)),"\n%c", '\0');
				// Write comment and value
				if( record_writeCalFile_pair(&c_buff[0], &buff[0]) ) break;

				// DataPoints y-value comment
				sprintf(c_buff,"Data points y (%.8f", dp_y[0]);
				for (uint8_t i = 1; i < dp_size; i++)
					sprintf(&c_buff[0] + (strlen(c_buff)),",%.8f", dp_y[i]);
				sprintf(&c_buff[0] + (strlen(c_buff)),"):\n");
				// DataPoints y-values
				sprintf(buff,"%08lX", *(unsigned long*)&dp_y[0]);
				for (uint8_t i = 1; i < dp_size; i++)
					sprintf(&buff[0] + (strlen(buff)),",%08lX", *(unsigned long*)&dp_y[i]);
				sprintf(&buff[0] + (strlen(buff)),"\n%c", '\0');
				// Write comment and value
				if( record_writeCalFile_pair(&c_buff[0], &buff[0]) ) break;

				printf("Write of CAL file successful!\n");
			} while(false);

			// Close file
			record_closeFile(objFILwrite);
		}
		else{
			printf("Write CAL File not open");
		}
	}

	// Add a line break to console
	printf("\n");
}

void record_readCalFile(volatile sensor* sens, float** dp_x, float** dp_y, uint16_t* dp_size){
	/// Read the calibration/specification data of the given sensor from the file stated in the sensor struct.
	/// Note: This function is not optimized for high speed. It should only be used when performance is not top priority (setup before actual start of record, not during).
	/// On 03.04.2021 this function measured to take about 266ms to complete...
	/// Returns nothing.
	///
	///	sens	...	A struct of type sensor which will get all sensor data
	/// dp_x	... Optional. A float array holding all x-values (nominal/ ADC output) used to do the curve fit (sorted!)
	/// dp_y	... Optional. A float array holding all y-values (actual value in units e.g mm) used to do the curve fit (corresponding to x-values!)
	/// dp_size	... Optional. Number of data points (elements in dp_x and dp_y)


	// FATFS result code and two string buffers for comment and values
	FRESULT res = 0; /* API result code */
	char buff[400];
	TCHAR* res_buf;

	// Initial log line
	printf("\nrecord_readSpecFile:\n");

	// Try to mount disk
	record_mountDisk(1);

	// If the SD card is ready, backup existing file, try to open the new one and read in data to sensor struct
	if(sdState == sdMounted || sdState == sdFileOpen){

		/// Check if file exists - if so, open it
		printf("Check if file exists\n");
		res = f_stat((char*)&sens->fitFilename[0], NULL); // Use &fno if actual file-info is needed
		if(res == FR_OK){
			// Open/Create File
			printf("Open file\n");
			record_openFile((char*)&sens->fitFilename[0], objFILread, 0);

			// If file is ready ...
			if(sdState == sdFileOpen){
				/// ... read every second line and write it to its corresponding value
				// Single loop do-while slope to handle errors clean with break (inspired by Infineon "FATFS_EXAMPLE_XMC47": https://www.infineon.com/cms/en/product/promopages/aim-mc/dave_downloads.html)
				do{
					//printf("do while\n");
					res = f_lseek(&fil_r, 0);
					if (res != FR_OK) break;

					/// Read fit order
					// Read comment line (ignore it) then read actual data line into buffer and stop process if the result isn't OK
					res_buf = f_gets(buff, 400, &fil_r);
					if (res_buf == 0) break;
					printf("CAL header: %s", buff);
					res_buf = f_gets(buff, 400, &fil_r);
					if (res_buf == 0) break;
					// Convert read string to unsigned long and write back to sensor struct
					sens->fitOrder = strtoul(buff, NULL, 10);
					printf("fitOrder %d: %s", sens->fitOrder, buff);

					/// Read coefficients
					// Read comment line (ignore it) then read actual data line into buffer and stop process if the result isn't OK
					res_buf = f_gets(buff, 100, &fil_r);
					res_buf = f_gets(buff, 100, &fil_r);
					if (res_buf == 0) break;
					// Convert read string to long and write back to sensor struct
					char *ptr = &buff[0];
					for (uint8_t i = 0; i < 4; i++) {
						printf("coefficients %d: %s ",i , ptr);
						// Read as hex long
						unsigned long tmp = strtoul(ptr, &ptr, 16);
						// Convert to float and write to sensor struct
						sens->fitCoefficients[i] = *(float*)&tmp;
						printf("= %.8f\n", sens->fitCoefficients[i]);
						// Ignore separator between values
						if( (ptr - &buff[0]) < strlen(&buff[0]) )
							ptr++;
					}

					/// Read filter order
					// Read comment line (ignore it) then read actual data line into buffer and stop process if the result isn't OK
					res_buf = f_gets(buff, 400, &fil_r);
					res_buf = f_gets(buff, 400, &fil_r);
					if (res_buf == 0) break;
					// Convert read string to unsigned long and write back to sensor struct
					sens->avgFilterOrder = strtoul(buff, NULL, 10);
					printf("filterOrder %d: %s", sens->avgFilterOrder, buff);

					/// Read error threshold
					// Read comment line (ignore it) then read actual data line into buffer and stop process if the result isn't OK
					res_buf = f_gets(buff, 400, &fil_r);
					res_buf = f_gets(buff, 400, &fil_r);
					if (res_buf == 0) break;
					// Convert read string to unsigned long and write back to sensor struct
					sens->errorThreshold = strtoul(buff, NULL, 10);
					printf("errorThreshold %d: %s", sens->errorThreshold, buff);

					// Read numDataPoints
					res_buf = f_gets(buff, 400, &fil_r);
					res_buf = f_gets(buff, 400, &fil_r);
					printf("numDataPoints: %s", buff);
					if (res_buf != 0 && dp_size != NULL){
						printf("Reading DPs:\n");
						*dp_size = (uint8_t)strtoul(buff, NULL, 10);
						printf("dp_size %d\n", *dp_size);


						// Allocate Memory for data points
						if(*dp_size >= 1){
							*dp_y = (float*)malloc((*dp_size+1)*sizeof(float));
							*dp_x = (float*)malloc((*dp_size+1)*sizeof(float));
							char *ptr = &buff[0];

							// Check for allocation errors
							if(*dp_y == NULL || *dp_x == NULL)
								printf("Read CAL: Memory malloc failed!\n");

							// Read DataPoints y-value
							res_buf = f_gets(buff, 100, &fil_r);
							res_buf = f_gets(buff, 100, &fil_r);
							if (res_buf == 0){
								buff[0] = '-';
								buff[1] = '1';
								buff[2] = '\0';
							}
							ptr = &buff[0];
							for (uint8_t i = 0; i < (*dp_size); i++) {
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
							res_buf = f_gets(buff, 100, &fil_r);
							res_buf = f_gets(buff, 100, &fil_r);
							if (res_buf == 0){
								buff[0] = '-';
								buff[1] = '1';
								buff[2] = '\0';
							}
							ptr = &buff[0];
							for (uint8_t i = 0; i < (*dp_size); i++) {
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
						printf("No num data points in file or no parameters called\n");
					}

					printf("Read of CAL file successful!\n");
				} while(false);

				// Close file
				record_closeFile(objFILread);

			} // end of if "file ready"
			else{
				printf("CAL file not open!");
			}
		}

	}
	else{
		printf("File not found: %d", res);
	}

	// Clean update of filter (the order might be changed)
	measure_movAvgFilter_clean((sensor*)sens);

	// Add a line break to console
	printf("\n");
}



void record_convertBinFile(const char* filename_BIN, sensor** sensArray){
	/// Read the .BIN file (path) and write a corresponding .CSV file.
	/// Note: This function is not optimized for high speed. It should only be used when performance is not top priority (after end of record, not during).
	/// Returns nothing.
	///
	///	path	...	Path to the .BIN file.
	/// dp_x	... Optional. A float array holding all x-values (nominal/ ADC output) used to do the curve fit (sorted!)
	///
	/// globals TODO


	// FATFS result code, Bytes written and a string buffer
	FRESULT res = 0;
	UINT bw,br;
	char buff[CSVLINE_BUFFER_LENGTH]; // Consideration: dynamic allocation based on line length?

	// Initial log line
	printf("\nrecord_convertBinFile:\n");

	// Stop measuring - needed because same buffers are used to convert (also enhances performance a bit)
	measureMode = measureModeNone;

	// Wait a whole interval to make sure the buffers are'nt being written to anymore
	delay_ms(MEASUREMENT_INTERVAL);

	// Reset buffers of all sensors
	for (uint8_t i = 0; i < SENSORS_SIZE; i++){
		// Reset index counter
		sensArray[i]->bufIdx = 0;
		sensArray[i]->errorOccured = 0;
		sensArray[i]->avgFilterSum = 0;

		// Memset all elements of all buffers to 0
		memset((int_buffer_t*)sensArray[i]->bufRaw   , 0, sensArray[i]->bufMaxIdx+1);
		memset((int_buffer_t*)sensArray[i]->bufFilter, 0, sensArray[i]->bufMaxIdx+1);
		memset((int_buffer_t*)sensArray[i]->bufConv  , 0, sensArray[i]->bufMaxIdx+1);

		// Clean update of filter (sum must be reset)
		measure_movAvgFilter_clean((sensor*)sensArray[i]);
	}

	// Try to mount disk
	record_mountDisk(1);

	// If the SD card is ready, backup existing file, try to open the .BIN and .CSV file and convert data
	if(sdState == sdMounted || sdState == sdFileOpen){
		/// Change file extension to .CSV
		// Buffer to store and modify the filename
		char filename_CSV[FILENAME_BUFFER_LENGTH];

		// Copy filename and change file extension to .CSV
		sprintf(filename_CSV, filename_BIN);
		filename_CSV[strlen(filename_CSV)-1] = 'V';
		filename_CSV[strlen(filename_CSV)-2] = 'S';
		filename_CSV[strlen(filename_CSV)-3] = 'C';

		/// Check filename for uniqueness and rename existing file if needed
		int8_t fil_OK = record_backupFile(filename_CSV);

		/// Check if file exists - if so, open it
		printf("Check if file exists\n");
		res = f_stat(filename_CSV, NULL);
		if(fil_OK == 1 && res == FR_OK){
			// Get line size, line padding size, block size and size of every value to be used while reading binary
			// Todo list variables
			// Note: This will not be done because we assume that this will run straight after the recording, and therefore the global variables must be equal

			// Open/Create Files
			printf("Open BIN file\n");
			res &= record_openFile(filename_BIN, objFILread, 0);

			printf("Open CSV file\n");
			res &= record_openFile(filename_CSV, objFILwrite, 0);

			// If files are ready ...
			if(res == FR_OK){
				// Generate line format (not needed?)
				//char lineFormat[100] = "%d\n";

				// As long as end of file isn't reached - read line by line and convert to csv
				while(record_checkEndOfFile(objFILread)){
					// Reset buff
					char seperator = ';';

					// Read next line

					// For each sensor - read corresponding bits to sensor buffer, apply filter, convert value and write to CSV file
					for (uint8_t i = 0; i < SENSORS_SIZE; i++){
						// Set current raw value

						// Check for Error

						// Set current filter value

						// Set current converted value

						// On last value of line - change separator to newline
						if(i == SENSORS_SIZE-1)
							seperator = '\n';

						// Write current value to the buffer
						sprintf(buff+strlen(buff), "%d%c", sensArray[i]->bufRaw[sensArray[i]->bufIdx], seperator);
					}

					// Write Line
					res = f_write(&fil_w, buff, 13, &bw);
					printf("Writing line: %s\n", buff);

				}

			}
		}
	}

	// Restart measuring
	measureMode = measureModeMonitoring;


}








static int8_t record_backupFile(const char* path){
	/// Check if the given file (in root directory) already exists. If yes, it renames the existing file to a pattern "[oldName]00.[fileExtesion]" where 00 is the next not used number between 1 and 99
	/// Limitations: Only use 3 character file extensions! No sub-folders are supported because the length is checked (except LFN (Long File Names -> FF_USE_LFN) are activated)
	/// Returns 1 if OK, 0 = error
	///
	/// path	...	The path/filename to be checked with extension
	///
	///	Uses globals variables: ToDo
	///


	FRESULT res = 0; /* API result code */

	// Initial log line
	printf("\trecord_backupFile:\n");

	// Only start if given name is longer than 4 characters (e.g. 'n.csv')
	if(strlen(path) > 4 && (strlen(path) <= 10 || FF_USE_LFN == 1)){

		// Marker used to detect problems while generating the filename/renaming
		char new_filename[FILENAME_BUFFER_LENGTH]; //
		char base_rename[FILENAME_BUFFER_LENGTH];  //
		char extension[4];

		// Copy extension
		extension[3] = '\0';
		extension[2] = path[strlen(path)-1];
		extension[1] = path[strlen(path)-2];
		extension[0] = path[strlen(path)-3];

		// Copy path to new_filename
		sprintf(new_filename, path);

		/// Check if file already exists - if so rename it
		// Get file-info to check if it exists
		res = f_stat(new_filename, NULL);
		if(res == FR_OK){
			/// File already exists, we need to find a new name and rename the old file

			// Extract base of filename (without extension)
			snprintf(base_rename, strlen(new_filename)-3, new_filename);

			// Add two numbers '01' and file extension
			sprintf(new_filename, "%s01.%s", base_rename, extension);

			// Debug message
			printf("\tFile already exists, try name: %s (%s)\n", new_filename, base_rename);

			// Check if a not yet used name is found - if not (still exists) increase file numbers and try again
			uint8_t i = 1;
			do{
				// Check if file exists
				res = f_stat(new_filename, NULL);
				if(res == FR_OK){
					// File already exists, generate filename with next number and try again
					i++;
					sprintf(new_filename, "%s%02d.%s", base_rename, i, extension);
					printf("\tFile already exists, try name: %s\n", new_filename);
				}
				else if (res == FR_NO_FILE){
					/// A unique name was found - rename!

					// Add extension to the base name in order to use it for renaming
					sprintf(&base_rename[strlen(base_rename)], ".%s", extension);

					// Rename file
					printf("\tNew filename found, rename %s to %s\n", base_rename, new_filename);
					res = f_rename(base_rename, new_filename);
					if(res == FR_OK){
						// Rename successful
						printf("\t Renamed file\n");

						// The actual new record file shall be named without numbers,
						// therefore just use the base name as new filename
						sprintf(new_filename, base_rename);

						// Renaming was successful
						return 1;
					}
					else{
						// Rename failed - start not successful
						printf("\t Rename failed %d\n", res);
					}

					// Tried renaming - now stop while loop
					break;
				}
				else{
					// Actual errors at file check only occur if all is lost - stop while loop
					printf("\t Check file error: %d\n", res);
					break;
				}
			}while(i < 100);
		}
		else if(res == FR_NO_FILE){
			// File doesn't exist - no renaming necessary
			printf("\t Filename is already unique - OK\n");
			return 1;
		}
		else{
			// Actual errors at file check only occur if all is lost - stop while loop
			printf("\t Check file error: %d\n", res);
		}



	}


	// Return 0 - Failed!
	printf("record_backupFile failed\n");
	return 0;
}

int8_t record_start(){
	/// Check if ready for recording, rename existing record file, open new file, allocate memory for the FIFO and change measuring mode.
	/// This needs to be executed ONCE before record_block() is used!
	/// Returns 1 if OK, 0 = error
	///
	/// No Inputs.
	///
	///	Uses globals variables: filename_rec, ToDo
	///


	// Initial log line
	printf("\nrecord_start:\n");

	// Try to mount disk
	record_mountDisk(1);

	//// -------------------------------------------------
	//// If the SD card is ready, backup possible existing file, try to open the new one, allocate fifo_buf and mark everything accordingly
	if(sdState == sdMounted){

		// Buffer to store and modify the filename
		char filename[FILENAME_BUFFER_LENGTH];

		// Copy filename and change file extension to .BIN
		sprintf(filename, filename_rec);
		filename[strlen(filename)-1] = 'N';
		filename[strlen(filename)-2] = 'I';
		filename[strlen(filename)-3] = 'B';

		// Check filename for uniqueness and rename existing file if needed
		int8_t fil_OK = record_backupFile(filename);

		// If the filename was already unique or the existing file was renamed - actually start/init the recording
		if(fil_OK){
			// Open/Create File
			record_openFile(filename, objFILwrite, 0);

			// If file is successfully opened...
			if(sdState == sdFileOpen){
				// Allocate memory for the log FIFO
				fifo_buf = (volatile uint8_t volatile * volatile)malloc(FIFO_BLOCK_SIZE*FIFO_BLOCKS);
				// Check for allocation errors
				if(fifo_buf == NULL){
					printf("Memory allocation failed!\n");
				}
				else{
					printf("Memory allocated!\n");

					// Set whole buffer null to avoid padding bytes being random
					memset((uint8_t*)fifo_buf, 0, FIFO_BLOCK_SIZE*FIFO_BLOCKS);

					// Everything is OK - change mode (this enables actual storing and flushing of values)
					measureMode = measureModeRecording;

					// Return 1 - Success!
					printf("recording started\n");
					return 1;
				}
			}
		}

	}

	// Return 0 - Failed!
	printf("recording start failed\n");
	return 0;
}

int8_t record_stop(uint8_t flushData){
	/// Check if file is open, flush remaining data to SD-card, free memory of the FIFO and change measuring mode.
	/// This needs to be executed ONCE after the last record_block() execution!
	/// Returns 1 if OK, 0 = error
	///
	/// flushData	...	If this is 1 the function will try to write the not yet written blocks to SD-card
	///
	///	Uses globals variables: sdState, measureMode, fifo_buf, ToDo
	///


	// Initial log line
	printf("\nrecord_stop(flushData=%d):\n", flushData);

	// If a file is open, close it
	if(sdState == sdFileOpen){
		// Everything is OK - change mode (this enables actual storing and flushing of values)
		measureMode = measureModeMonitoring;

		// Flush remaining Blocks and data to SD-card
		if(flushData){
			; // Todo Write last flush code
		}

		// Free Memory
		free((uint8_t*)fifo_buf);

		// Close File
		record_closeFile(objFILwrite);

		// If everything is OK
		if(sdState == sdMounted){
			// Return 1 - Stop successful!
			printf("recording stopped\n");
			return 1;
		}

	}

	// Return 0 - Stop failed!
	printf("recording stop failed\n");
	return 0;
}

void record_block(){
	/// Record the current block (fifo_recordBlock) of the FIFO to the SD-card
	/// No inputs or output (performance)
	///
	///	Uses record-global variables: fil
	///	Uses globals variables: fifo_buf, fifo_recordBlock, FIFO_BLOCK_SIZE


	FRESULT res = 0; /* API result code */
	UINT bw; /* Bytes written */

	// Write a FIFO_BLOCK_SIZE sized block to the beginning of the current block
	res = f_write(
			&fil_w,
			(void*)(fifo_buf + (fifo_recordBlock*FIFO_BLOCK_SIZE)),
			FIFO_BLOCK_SIZE,
			&bw
		  );

	// If error occurred or there are less bytes written that should be - stop recording
	if (res != FR_OK || bw != FIFO_BLOCK_SIZE){
		printf("Recording of block %d failed! Stopping record\n", fifo_recordBlock);
		record_stop(0);
	}

}





void record_line(){
	///  Used to speed-test sd-cards with oscilloscope. TODO delete this
	///

	// All writes with transcent sd that are not equal to 512byte need 2ms every 512byte that are written! Exactly 512 needs this every time

	//    1 chars const	transcent		 4.72us		res = f_write(&fil, "1" , 1, &bw);
	//    2 chars const	transcent		 4.92us		res = f_write(&fil, "1.", 2, &bw);
	//   10 chars const	transcent		 6.31us		res = f_write(&fil, "0123456789", 10, &bw);
	//  100 chars const	transcent		21.96us		res = f_write(&fil, "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789", 100, &bw);
	//  512 chars const transcent		1985us  	but every 424ms (~85*512 Byte) the write takes 16150us once. Every 2.6s the write takes there are 3 to 4 subsequent very slow writes (e.g 6ms, 32ms, 25ms, 10ms = 76.4ms till return to normal operation, this varies from 70ms, 81ms up to 157ms)
	//  512 chars const sandisk			1036.00us	but every 310ms (~62*512 Byte) the write takes 5500us once and another 2264us next before again doing 62 1ms cycles
	// 1024 chars const transcent		1996us   	...
	// 1024 chars const sandisk			1ms~1.4ms  	...
	// 2048 chars const transcent		2112us   	...
	// 2048 chars const sandisk			1ms~1.4ms  	...
	//  256 chars const	transcent		abwechselnd 2.45ms und 49us


	FRESULT res = 0; /* API result code */
	UINT bw; /* Bytes written */
	//char buff[512] = "1.14";

	//float testF = 1.14;
	//char* testC = "1.14";

	//sprintf(buff,"%.2f", testF);
	//res = f_write(&fil, "1" , 1, &bw);
	res = f_write(&fil_w, "01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123450123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234501234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123450123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234501234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123450123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345", 2048, &bw);
	if (res != FR_OK){
		sdState = sdMounted;
		printf("record line failed\n");
	}

}

void record_buffer(void)
{
	// Example from Infineon for test purpose. TODO delete this

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
			res = f_open(&fil_w, "Record/log.txt", FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
			if ((res == FR_OK) || (res == FR_EXIST))
			{
				/* Write a message */
				sprintf(buff,"%08lX\n", *(unsigned long*)&testf_w);
				printf("write: %s\n", buff);
				res = f_write(&fil_w, buff, 13, &bw);
				//sprintf(buff,"Current value 2 %5d\n", InputBuffer1[InputBuffer1_idx]);
				//res = f_write(&fil_w, buff, 22, &bw);
				if (res == FR_OK )
				{
					res = f_lseek(&fil_w, 0);

					//sprintf(Buffer2,"%08lX",*(unsigned long*)&ADCValue2_float2);
					printf("ori: %08lX, %f\n", *(unsigned long*)&testf_w, testf_w);
					f_gets(buff, 25, &fil_w);
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
					//		res = f_write(&fil_w, "\nWelcome to Infineon", 20, &bw);
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


			res = f_close(&fil_w);
			printf("f_close 1: %d\n", res);
			res = f_close(&fil_w);
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
