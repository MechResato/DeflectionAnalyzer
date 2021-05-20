/*
@file    		record.c
@brief   		Implementation of SD-Card management, calibration file read/write, screenshot write, write of data with a FIFO to BIN and conversion to CSV files (implemented for XMC4700 and DAVE)
@version 		1.0
@date    		2020-02-25
@author 		Rene Santeler @ MCI 2020/21
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

//// External variables

/// Implemented in globals:
// struct's:  sensor
// type's:	  int_buffer_t (e.g. uint16_t), float_buffer_t (e.g. float),
// #define's: FIFO_LINE_SIZE_PAD, FIFO_BLOCKS, SENSOR_RAW_SIZE, SENSORS_SIZE and
//			  FILENAME_BUFFER_LENGTH
extern sdStates sdState;				  // state of sd-card (purpose: none=0, mounted, open or error)
extern volatile measureModes measureMode; // state of the measurement (purpose: none, monitoring or recording)
// FIFO-variables
extern volatile uint8_t volatile * volatile fifo_buf;	// FIFO buffer
extern volatile uint16_t fifo_writeBufIdx;				// index in FIFO buffer
extern volatile uint8_t fifo_writeBlock;				// current block to write to RAM
extern volatile uint8_t fifo_recordBlock;				// current block to be recorded to sd-card
extern volatile uint8_t fifo_finBlock[];				// array of which block is finished an can be recorded
/// Implemented in measure:
extern void measure_postProcessing(volatile sensor* sens);


//// Internal variables

/// FATFS variables
DSTATUS diskStatus;
static FATFS fs; 	// File system object (volume work area)
static FIL fil_r; 	// File object used for read only
static FIL fil_w; 	// File object used for write only

//// Internal functions
static FRESULT record_openFile(const char* path, objFIL objFILrw, uint8_t accessMode);
static FRESULT record_closeFile(objFIL objFILrw);
static int8_t record_checkEndOfFile(objFIL objFILrw);
static uint8_t record_writeCalFile_pair (char* comment, char* val_buff);
static int8_t record_backupFile(const char* path);



void record_mountDisk(uint8_t mount){
	/// Used to mount and unmount a disk.
	///
	/// mount ... 1 = mount, 0 = unmount SD-Card
	///
	/// Uses multiple ff.h defines (FATFS Lib)
	///	Uses record-global variables: fil_w, fil_r
	///	Uses globals variables: sdState


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
		DSTATUS resDiskInit = disk_initialize(0);
		printf("Reinit sd = %d\n", resDiskInit);
	}


	// If an SD is mounted unmount it (always)
	if(sdState != sdNone){
		res = f_unmount("0:");
		printf("SD still open, state: %d. Unmounting: %d\n", sdState, res);
		sdState = sdNone;
	}

	// If needed, mount the SD and mark state as mounted or error
	if(mount){
		// Register work area
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
	else{
		// Unregister work area
		res = f_unmount("0:");
		if (res == FR_OK) {
			printf("Unmount OK\n");
			sdState = sdNone;
		}
		else {
			printf("Unmount error: %d\n", res);
			sdState = sdError;
		}
	}

}

static FRESULT record_openFile(const char* path, objFIL objFILrw, uint8_t accessMode){
	/// Used to open a file on the read or write FIL struct for read/write access. If another file is still open it will be closed automatically!
	/// Note: This implementation allows only one read and one write file to be open at the same time!
	///
	/// path	   ... Path to the file to be opened
	/// objFILrw   ... Choose between read or write file
	/// accessMode ... File access mode and open method (see ff.h)
	///
	/// Uses multiple ff.h defines (FATFS Lib)
	///	Uses record-global variables: fil_w, fil_r
	///	Uses globals variables: sdState

	// FATFS result code
	FRESULT res = FR_OK;

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
				printf("\tClose File OK\n");
				sdState = sdMounted;
			}
			else{
				printf("\tClose File error: %d\n", res);
				sdState = sdError;
			}
		}
	}

	// If SD is mounted now, try to open the file/path
	if(sdState != sdError && sdState != sdNone) {
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
			printf("\tFile opened: %s\n", path);
			sdState = sdFileOpen;
		}
		else if (res == FR_INVALID_NAME){
			printf("\tFile open error: FR_INVALID_NAME '%s'\n", path);
			sdState = sdError;
		}
		else{
			printf("\tFile open error: %d\n", res);
			sdState = sdError;
		}
	}

	// Return last result
	return res;
}

static FRESULT record_closeFile(objFIL objFILrw){
	/// Used to close the file of the write or read FIL struct.
	///
	/// Uses multiple ff.h defines (FATFS Lib)
	///	Uses record-global variables: fil_w, fil_r
	///	Uses globals variables: sdState


	// FATFS result code
	FRESULT res = FR_OK;

	// If SD is mounted and the corresponding file is still open close it
	if(sdState != sdError && sdState != sdNone){
		// Close already open file if needed
		if(objFILrw == objFILread && fil_r.obj.fs != NULL){
			res = f_close(&fil_r);
		}
		else if(objFILrw == objFILwrite && fil_w.obj.fs != NULL){
			res = f_close(&fil_w);
		}

		// Check result
		if (res == FR_OK) {
			printf("\tClose File OK\n");

			// Check if one of the files is still open, if not mark sd state to "no file open" = "sd mounted"
			if(fil_r.obj.fs != NULL && fil_w.obj.fs != NULL)
				sdState = sdMounted;
		}
		else if(res == FR_INVALID_OBJECT){ // Note: Unsure if needed - check this again
			printf("\tFile close warning: %d\n", res);
			sdState = sdMounted;
		}
		else{
			printf("\tClose File error: %d\n", res);
			sdState = sdError;
		}
	}

	// Return last result
	return res;
}

static int8_t record_checkEndOfFile(objFIL objFILrw){
	/// Check and return end of file status of given file
	/// Returns 1 if EOF is reached, 0 if its not reached and -1 if the requested file is wrong or not open
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

static int8_t record_backupFile(const char* path){
	/// Check if the given file (in root directory) already exists. If yes, it renames the existing file to a pattern "[oldName]00.[fileExtesion]" where 00 is the next not used number between 1 and 99
	/// Limitations: Only use 3 character file extensions! No sub-folders are supported because the length is checked (except LFN (Long File Names -> FF_USE_LFN) are activated)
	/// Returns 1 if OK, 0 = error
	///
	/// path	...	The path/filename to be checked with extension
	///
	///	Uses globals variables: FF_USE_LFN, FILENAME_BUFFER_LENGTH
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



static uint8_t record_writeCalFile_pair (char* comment, char* val_buff){
	/// Writes a comment and an actual value as two separate lines to the file
	/// Returns 1 if there was an error, 0 means success.
	///
	///	Uses record-global variables: fil_w


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

void record_writeCalFile(sensor* sens){
	/// Write the calibration/specification data of the given sensor to the file stated in the sensor struct.
	/// If the File already exists, the current version will be backed up (means: only 2 version are saved current and last file!)
	/// Returns nothing.
	///
	///	sens	...	A struct of type sensor which holds all sensor data
	/// dp_x	... A float array holding all x-values (nominal/ ADC output) used to do the curve fit (sorted!)
	/// dp_y	... A float array holding all y-values (actual value in units e.g mm) used to do the curve fit (corresponding to x-values!)
	/// dp_size	... Number of data points (elements in dp_x and dp_y)
	///
	///	Uses record-global variables: fil_w
	///	Uses globals variables: sdState


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
		res = f_stat((char*)&sens->fitFilename[0], NULL);
		if(res == FR_OK){
			sprintf(buff, "S%dBAK.CAL", (sens->index+1));
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
				f_printf(&fil_w, "# Specification of sensor %d '%s'. Odd lines are comments, even lines are values. Float values are converted to 32bit integer hex (memory content). ", (sens->index+1), sens->name);
				printf("Write header\n");

				// Write fit order comment and value in separate lines
				sprintf(buff,"%d\n", sens->fitOrder);
				if( record_writeCalFile_pair("Curve fit function order:\n", &buff[0]) ) break;

				// Write coefficients comment and value in separate lines. Note: Floating point precision is taken from FLT_DIG (=here 6). See https://www.h-schmidt.net/FloatConverter/IEEE754.html for an online converter.
				sprintf(c_buff,"# Coefficients (%.8f, %.8f, %.8f, %.8f):\n", sens->fitCoefficients[0], sens->fitCoefficients[1], sens->fitCoefficients[2], sens->fitCoefficients[3]);
				sprintf(buff,"%08lX,%08lX,%08lX,%08lX\n", *(unsigned long*)&sens->fitCoefficients[0], *(unsigned long*)&sens->fitCoefficients[1], *(unsigned long*)&sens->fitCoefficients[2], *(unsigned long*)&sens->fitCoefficients[3]);
				if( record_writeCalFile_pair(&c_buff[0], &buff[0]) ) break;

				// Write avg filter interval comment and value in separate lines
				sprintf(buff,"%d\n", sens->avgFilterInterval);
				if( record_writeCalFile_pair("# Average filter interval:\n", &buff[0]) ) break;

				// Write error threshold comment and value in separate lines
				sprintf(buff,"%d\n", sens->errorThreshold);
				if( record_writeCalFile_pair("# Error threshold:\n", &buff[0]) ) break;

				// Write converted origin point (unloaded) comment and value in separate lines
				sprintf(c_buff,"# Sensor origin point/unloaded (%.8f):\n", sens->originPoint);
				sprintf(buff,"%08lX\n", *(unsigned long*)&sens->originPoint);
				if( record_writeCalFile_pair(&c_buff[0], &buff[0]) ) break;

				// Write converted operating point (offset/sag) comment and value in separate lines
				sprintf(c_buff,"# Sensor operating point offset/sag (%.8f):\n", sens->operatingPoint);
				sprintf(buff,"%08lX\n", *(unsigned long*)&sens->operatingPoint);
				if( record_writeCalFile_pair(&c_buff[0], &buff[0]) ) break;

				// Write numDataPoints comment and value in separate lines
				sprintf(buff,"%d\n", sens->dp_size);
				if( record_writeCalFile_pair("# Number of data points:\n", &buff[0]) ) break;

				// DataPoints x-value comment
				sprintf(c_buff,"# Datapoints x (%.8f", sens->dp_x[0]);
				for (uint8_t i = 1; i < sens->dp_size; i++)
					sprintf(&c_buff[0] + (strlen(c_buff)),",%.8f", sens->dp_x[i]);
				sprintf(&c_buff[0] + (strlen(c_buff)),"):\n");
				// DataPoints x-values
				sprintf(buff,"%08lX", *(unsigned long*)&sens->dp_x[0]);
				for (uint8_t i = 1; i < sens->dp_size; i++)
					sprintf(&buff[0] + (strlen(buff)),",%08lX", *(unsigned long*)&sens->dp_x[i]);
				sprintf(&buff[0] + (strlen(buff)),"\n%c", '\0');
				// Write comment and value
				if( record_writeCalFile_pair(&c_buff[0], &buff[0]) ) break;

				// DataPoints y-value comment
				sprintf(c_buff,"# Datapoints y (%.8f", sens->dp_y[0]);
				for (uint8_t i = 1; i < sens->dp_size; i++)
					sprintf(&c_buff[0] + (strlen(c_buff)),",%.8f", sens->dp_y[i]);
				sprintf(&c_buff[0] + (strlen(c_buff)),"):\n");
				// DataPoints y-values
				sprintf(buff,"%08lX", *(unsigned long*)&sens->dp_y[0]);
				for (uint8_t i = 1; i < sens->dp_size; i++)
					sprintf(&buff[0] + (strlen(buff)),",%08lX", *(unsigned long*)&sens->dp_y[i]);
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

void record_readCalFile(volatile sensor* sens){
	/// Read the calibration/specification data of the given sensor from the file stated in the sensor struct.
	/// Note: This function is not optimized for high speed. It should only be used when performance is not top priority (setup before actual start of record, not during).
	/// On 03.04.2021 this function measured to take about 266ms to complete.
	///
	///	sens	...	A struct of type sensor which will get all sensor data
	/// dp_x	... Optional. A float array holding all x-values (nominal/ ADC output) used to do the curve fit (sorted!)
	/// dp_y	... Optional. A float array holding all y-values (actual value in units e.g mm) used to do the curve fit (corresponding to x-values!)
	/// dp_size	... Optional. Number of data points (elements in dp_x and dp_y)
	///
	///	Uses record-global variables: fil_r
	///	Uses globals variables: sdState

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
		res = f_stat((char*)&sens->fitFilename[0], NULL);
		if(res == FR_OK){
			// Open/Create File
			printf("Open file\n");
			record_openFile((char*)&sens->fitFilename[0], objFILread, 0);

			// If file is ready ...
			if(sdState == sdFileOpen){
				/// ... read every second line and write it to its corresponding value
				// Single loop do-while slope to handle errors clean with break (inspired by Infineon "FATFS_EXAMPLE_XMC47": https://www.infineon.com/cms/en/product/promopages/aim-mc/dave_downloads.html)
				do{
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

					/// Read filter interval
					// Read comment line (ignore it) then read actual data line into buffer and stop process if the result isn't OK
					res_buf = f_gets(buff, 400, &fil_r);
					res_buf = f_gets(buff, 400, &fil_r);
					if (res_buf == 0) break;
					// Convert read string and write back to sensor struct
					sens->avgFilterInterval = strtoul(buff, NULL, 10);
					printf("filterInterval %d: %s", sens->avgFilterInterval, buff);

					/// Read error threshold
					// Read comment line (ignore it) then read actual data line into buffer and stop process if the result isn't OK
					res_buf = f_gets(buff, 400, &fil_r);
					res_buf = f_gets(buff, 400, &fil_r);
					if (res_buf == 0) break;
					// Convert read string and write back to sensor struct
					sens->errorThreshold = strtoul(buff, NULL, 10);
					printf("errorThreshold %d: %s", sens->errorThreshold, buff);

					/// Read origin point
					// Read comment line (ignore it) then read actual data line into buffer and stop process if the result isn't OK
					res_buf = f_gets(buff, 400, &fil_r);
					res_buf = f_gets(buff, 400, &fil_r);
					if (res_buf == 0) break;
					// Read as hex long
					unsigned long hexToFloatTmp1 = strtoul(buff, NULL, 16);
					// Convert to float and write to sensor struct
					sens->originPoint = *(float*)&hexToFloatTmp1;
					printf("originPoint %.8f: %s", sens->originPoint, buff);

					/// Read operating point
					// Read comment line (ignore it) then read actual data line into buffer and stop process if the result isn't OK
					res_buf = f_gets(buff, 400, &fil_r);
					res_buf = f_gets(buff, 400, &fil_r);
					if (res_buf == 0) break;
					// Read as hex long
					unsigned long hexToFloatTmp2 = strtoul(buff, NULL, 16);
					// Convert to float and write to sensor struct
					sens->operatingPoint = *(float*)&hexToFloatTmp2;
					printf("operatingPoint %.8f: %s", sens->operatingPoint, buff);

					// Read numDataPoints
					res_buf = f_gets(buff, 400, &fil_r);
					res_buf = f_gets(buff, 400, &fil_r);
					sens->dp_size = (uint8_t)strtoul(buff, NULL, 10);
					printf("numDataPoints %d: %s\n", sens->dp_size, buff);
					if (res_buf != 0 && sens->dp_size >= 1){
						// Allocate Memory for data points
						if(sens->dp_size >= 1){
							sens->dp_y = (float*)realloc(sens->dp_y, (sens->dp_size+1)*sizeof(float));
							sens->dp_x = (float*)realloc(sens->dp_x, (sens->dp_size+1)*sizeof(float));
							char *ptr = &buff[0];

							// Check for allocation errors
							if(sens->dp_y == NULL || sens->dp_x == NULL)
								printf("Read CAL: Memory realloc failed!\n");

							// Read DataPoints x-value
							res_buf = f_gets(buff, 100, &fil_r);
							res_buf = f_gets(buff, 100, &fil_r);
							if (res_buf == 0){
								buff[0] = '-';
								buff[1] = '1';
								buff[2] = '\0';
							}
							ptr = &buff[0];
							for (uint8_t i = 0; i < (sens->dp_size); i++) {
								printf("x %d: ",i );
								// Read as hex long
								unsigned long tmp = strtoul(ptr, &ptr, 16);
								// Convert to float and write to sensor struct
								(sens->dp_x)[i] = *(float*)&tmp;
								printf("= %.8f\n", (sens->dp_x)[i]);
								// Ignore separator between values
								if( (ptr - &buff[0]) < strlen(&buff[0]) )
									ptr++;
							}

							// Read DataPoints y-value
							res_buf = f_gets(buff, 100, &fil_r);
							res_buf = f_gets(buff, 100, &fil_r);
							if (res_buf == 0){
								buff[0] = '-';
								buff[1] = '1';
								buff[2] = '\0';
							}
							ptr = &buff[0];
							for (uint8_t i = 0; i < (sens->dp_size); i++) {
								printf("y %d: ",i );
								// Read as hex long
								unsigned long tmp = strtoul(ptr, &ptr, 16);
								// Convert to float and write to sensor struct
								(sens->dp_y)[i] = *(float*)&tmp;
								printf("= %.8f\n", (sens->dp_y)[i]);
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
		else{
			printf("File not found: %d", res);
		}


	}
	else{
		printf("SD-Card not ready: %d", res);
	}

	// Clean update of filter (Interval might be changed)
	measure_movAvgFilter_clean((sensor*)sens, sens->avgFilterInterval, 0);

	// Add a line break to console
	printf("\n");
}



uint8_t record_openBMP(const char* path){
	/// Creates a bitmap file at given path and adds an 480x272 pixel, 32bit header.
	/// Returns 1 if OK, 0 = error
	///
	/// path ... Path to the bmp-file to be created (with extension)
	///
	///	Uses globals variables: sdState, fil_w


	// FATFS result code and two string buffers for comment and values
	FRESULT res = 0;
	UINT bw;
	// The Header of an 480x272 pixel sized 32bit bitmap
	#define BMP_HEADER_ARGB8_32BIT_SIZE 54
	const uint8_t bmp_header_argb8_32bit[BMP_HEADER_ARGB8_32BIT_SIZE] =
	{
		0x42, 0x4D, 0x38, 0xF8, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, // Header itself - 14Byte: 2B FileType, 4B FileSize, 2B Reserved, 2B Reserved, 4B PixelDataOffset
		0x28, 0x00, 0x00, 0x00,	0xE0, 0x01,	0x00, 0x00, 0xF0, 0xFE, 0xFF, 0xFF, 0x01, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,  // Info Header - 40Byte (see)
		0x02, 0xF8,	0x07, 0x00, 0x12, 0x0B,	0x00, 0x00,	0x12, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	// Initial log line
	printf("\nrecord_openBMP:\n");

	// Try to mount disk
	record_mountDisk(1);

	// If the SD card is ready, backup existing file, try to open the new one and write specifications
	if(sdState == sdMounted || sdState == sdFileOpen){
		// Check filename for uniqueness and rename existing file if needed
		int8_t fil_OK = record_backupFile(path);

		// If everything is OK open file
		if(fil_OK){
			// Open/Create File
			record_openFile(path, objFILwrite, 0);

			// If file is ready to be written to ...
			if(sdState == sdFileOpen){
				// Write Header
				res = f_write(&fil_w, bmp_header_argb8_32bit, BMP_HEADER_ARGB8_32BIT_SIZE, &bw);
				if (res != FR_OK || bw <= 0)
					printf("\t BMP write failed %d (bw=%d)\n", res, bw);
				printf("Write BMP File open\n");
				return 1;
			}
			else{
				printf("Write BMP File not open\n");
				return 0;
			}
		}
		else{
			printf("File Backup failed\n");
			return 0;
		}
	}
	else{
		printf("No SD-Card mounted\n");
		return 0;
	}

}

void record_writeBMP(uint32_t* data, uint16_t size){
	/// Fills a bitmap file with 32bit pixels (EVE ARGB8 Format of EVE_cmd_snapshot2).
	///
	/// data ... Array of pixels
	/// size ... Size of the array in byte
	///
	///	Uses globals variables: fil_w


	// FATFS result code and two string buffers for comment and values
	FRESULT res = 0;
	UINT bw;

	// Write pixel
	res = f_write(&fil_w, data, size, &bw);
	if (res != FR_OK || bw <= 0)
		printf("\t BMP write failed %d (bw=%d)\n", res, bw);
}

void record_closeBMP(){
	/// Closes the bitmap file.
	///
	/// No input
	///
	///	Uses globals variables: sdState


	// Initial log line
	printf("\nrecord_closeBMP:\n");

	// If the SD card is ready, backup existing file, try to open the new one and write specifications
	if(sdState == sdMounted || sdState == sdFileOpen){

		// Close file
		record_closeFile(objFILwrite);

		// Add a line break to console
		printf("\n");
	}
}



int8_t record_start(){
	/// Check if ready for recording, rename existing record file, open new file, allocate memory for the FIFO and change measuring mode.
	/// This needs to be executed ONCE before record_block() is used!
	/// Returns 1 if OK, 0 = error
	///
	/// No Inputs.
	///
	///	Uses globals variables: filename_rec, FILENAME_BUFFER_LENGTH, sdState, measureMode, fifo_buf, FIFO_BLOCK_SIZE, FIFO_BLOCKS, fifo_finBlock, fifo_recordBlock, fifo_writeBufIdx, fifo_writeBlock, fifo_recordBlock
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

				// Reset fifo control variables
				fifo_writeBufIdx = 0;
				fifo_writeBlock = 0;
				fifo_recordBlock = 0;
				for(uint8_t i = 0; i < FIFO_BLOCKS; i++)
					fifo_finBlock[i] = 0;

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

void record_block(){
	/// Record the current block (fifo_recordBlock) of the FIFO to the SD-card
	/// No inputs or output (performance)
	///
	///	Uses record-global variables: fil_w
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

int8_t record_stop(uint8_t flushData){
	/// Check if file is open, flush remaining data to SD-card, free memory of the FIFO and change measuring mode.
	/// This needs to be executed ONCE after the last record_block() execution!
	/// Returns 1 if OK, 0 = error
	///
	/// flushData	...	If 1 the remaining finished blocks will be written to the SD-card
	///
	///	Uses globals variables: sdState, measureMode, fifo_buf, fifo_finBlock, fifo_recordBlock, FIFO_BLOCKS
	///


	// Initial log line
	printf("\nrecord_stop(flushData=%d):\n", flushData);

	// If a file is open, close it
	if(sdState == sdFileOpen){
		// Everything is OK - change mode (this enables actual storing and flushing of values)
		measureMode = measureModeMonitoring;

		// Flush remaining Blocks and data to SD-card
		if(flushData){
			// Write blocks till an unfinished block is found
			while(1){
				// No finished block left - stop
				if(fifo_finBlock[fifo_recordBlock] == 1){
					printf("Write finished block %d\n", fifo_recordBlock);

					// Record current block
					record_block();

					// Mark Block as processed (if a block isn't processed before the measurement handler tries to write to it again the record fails)
					fifo_finBlock[fifo_recordBlock] = 0;

					// Mark next block as current record block
					fifo_recordBlock++;
					// Overleap correction of the current block index
					if(fifo_recordBlock == FIFO_BLOCKS)
						fifo_recordBlock = 0;
				}
				// No finished block left - stop
				else
					break;
			}

		}

		// Free Memory
		free((uint8_t*)fifo_buf);

		// Close File
		record_closeFile(objFILwrite);

		// If everything is OK
		if(sdState != sdError){
			// Return 1 - Stop successful!
			printf("recording stopped\n");
			return 1;
		}

	}

	// Return 0 - Stop failed!
	printf("recording stop failed\n");
	return 0;
}

void record_convertBinFile(const char* filename, sensor** sensArray){
	/// Read the .BIN file (path) and write a corresponding .CSV file. The base name of both files will be same (error if not possible).
	/// Therefore only the base name of 'filename' is used, extensions are changed as needed (a parameter "test.csv" or "test.bin" will lead to the same result!).
	/// Note: This function is not optimized for high speed. It should only be used when performance is not top priority (after end of record, not during).
	/// Returns nothing. Note: This would be much faster if used with the FIFO, but for current situation there is no need.
	///
	///	filename	...	Path to the .BIN file.
	/// dp_x		... Optional. A float array holding all x-values (nominal/ ADC output) used to do the curve fit (sorted!)
	///
	///	Uses record-global variables: objFILread, objFILwrite
	///	Uses globals variables: sdState, measureMode, CSVLINE_BUFFER_LENGTH, FILENAME_BUFFER_LENGTH, MEASUREMENT_INTERVAL, SENSORS_SIZE, SENSOR_RAW_SIZE, RECORD_CSV_HEADER, RECORD_CSV_FORMAT, RECORD_CSV_ARGUMENTS, FIFO_LINE_SIZE_PAD


	// FATFS result code, Bytes written and a string buffer
	FRESULT res = 0;
	UINT bw,br;
	char csv_line_buff[CSVLINE_BUFFER_LENGTH]; // Consideration: dynamic allocation based on line length?

	// Initial log line
	printf("\nrecord_convertBinFile:\n");

	// Stop measuring - needed because same buffers are used to convert (also enhances performance a bit)
	measureMode = measureModeNone;

	// Wait a whole interval to make sure the buffers are'nt being written to anymore
	delay_ms(MEASUREMENT_INTERVAL);

	// Reset buffers of all sensors
	for (uint8_t i = 0; i < SENSORS_SIZE; i++){
		printf("Reset sensor struct %d\n", i);

		// Reset index counter
		sensArray[i]->bufIdx = sensArray[i]->bufMaxIdx;
		sensArray[i]->errorLastValid = 0;
		sensArray[i]->avgFilterSum = 0;

		// Set the current error count to the filter interval. This lets the filter ignore that the entry's before the first one are still empty
		sensArray[i]->errorOccured = sensArray[i]->avgFilterInterval;

		// Memset all elements of all buffers to 0
		memset((int_buffer_t*)sensArray[i]->bufRaw     , 0, (sensArray[i]->bufMaxIdx+1)*sizeof(int_buffer_t));
		memset((float_buffer_t*)sensArray[i]->bufFilter, 0, (sensArray[i]->bufMaxIdx+1)*sizeof(float_buffer_t));
		memset((float_buffer_t*)sensArray[i]->bufConv  , 0, (sensArray[i]->bufMaxIdx+1)*sizeof(float_buffer_t));
	}

	// Try to mount disk
	record_mountDisk(1);

	// If the SD card is ready, backup existing file, try to open the .BIN and .CSV file and convert data
	if(sdState == sdMounted || sdState == sdFileOpen){
		/// Determine right .BIN and .CSV filename
		// Buffer to store and modify the filename
		char filename_CSV[FILENAME_BUFFER_LENGTH];
		char filename_BIN[FILENAME_BUFFER_LENGTH];

		// Copy filename and make sure file extension is .BIN
		sprintf(filename_BIN, filename);
		filename_BIN[strlen(filename_BIN)-1] = 'N';
		filename_BIN[strlen(filename_BIN)-2] = 'I';
		filename_BIN[strlen(filename_BIN)-3] = 'B';

		// Copy filename and change file extension to .CSV
		sprintf(filename_CSV, filename);
		filename_CSV[strlen(filename_CSV)-1] = 'V';
		filename_CSV[strlen(filename_CSV)-2] = 'S';
		filename_CSV[strlen(filename_CSV)-3] = 'C';

		/// Check filename for uniqueness and rename existing file if needed
		int8_t fil_OK = record_backupFile(filename_CSV);

		/// Check if file exists - if so, open it
		res = f_stat(filename_BIN, NULL);
		printf("Checked if file exists: %d\n", res);
		if(fil_OK == 1 && res == FR_OK){
			// Get dynamic line size, line padding size, block size and size of every value to be used while reading binary
			// Note: This will not be done because we assume that this will run straight after the recording, and therefore the global variables must be equal

			// Open/Create Files
			res = record_openFile(filename_BIN, objFILread, 0);
			printf("Tried open BIN file: Error=%d\n", res);

			res |= record_openFile(filename_CSV, objFILwrite, 0);
			printf("Tried open CSV file: Error=%d\n", res);

			// If files are ready ...
			if(res == FR_OK){
				// Set file cursor
				res = f_lseek(&fil_r, 0);
				res |= f_lseek(&fil_w, 0);
				printf("\tReset file cursors (res%d)\n", res);

				// Generate header string, write it to file and reset buffer
				sprintf(csv_line_buff, "Time;"   RECORD_CSV_HEADER     "\n");
				res = f_write(&fil_w, csv_line_buff, strlen(csv_line_buff), &bw);
				csv_line_buff[0] = '\0';

				// Read line by line and convert to CSV, as long as end of file isn't reached
				uint16_t linCount = 0;
				float curTimeO = 0;
				while(record_checkEndOfFile(objFILread) == 0  ){ //linCount < 10
					// Reset buff
					char seperator = ';';
					int_buffer_t raw = 0;

					// Add current time to buffer
					sprintf( csv_line_buff, "%.3f;", curTimeO);

					// For each sensor - read corresponding bits to sensor buffer, apply filter, convert value and write to CSV file
					for (uint8_t i = 0; i < SENSORS_SIZE; i++){

						// Read raw value from file
						raw = 0;
						res = f_read(&fil_r, &raw, SENSOR_RAW_SIZE, &br);
						//printf("\tRead raw %d, read %d (res%d)", raw, br, res);

						// Increment current Buffer index and set back to 0 if greater than size of array
						sensArray[i]->bufIdx++;
						if(sensArray[i]->bufIdx > sensArray[i]->bufMaxIdx)
							sensArray[i]->bufIdx = 0;

						// Set current raw value
						sensArray[i]->bufRaw[sensArray[i]->bufIdx] = raw;

						// Error handling and calculation of raw/filtered/converted value
						measure_postProcessing(sensArray[i]);

						// On last value of line - change separator to newline
						if(i == SENSORS_SIZE-1)
							seperator = '\n';

						// Write current value to the buffer
						sprintf(
							csv_line_buff+strlen(csv_line_buff), // Add sensor data to end of buffer
							RECORD_CSV_FORMAT     "%c",		// Concatenate format ([values]separator[; or \n])
							RECORD_CSV_ARGUMENTS, seperator	// Arguments	->	  ([values]separator[; or \n])
						);
					}

					// Write Line
					res = f_write(&fil_w, csv_line_buff, strlen(csv_line_buff), &bw);

					// Read padding bytes to move cursor
					f_read(&fil_r, &raw, FIFO_LINE_SIZE_PAD, &br);

					// Reset Buffer
					csv_line_buff[0] = '\0';

					// Increment line and time counter
					curTimeO += (MEASUREMENT_INTERVAL/1000.0);
					linCount++;
				}

				printf("End of BIN file! %d lines written = %.2fs\n", linCount, MEASUREMENT_INTERVAL*linCount/1000);
			}
			else{
				printf("Error: Files are not ready or not open!\n");
			}

			// Close Files
			record_closeFile(objFILread);
			record_closeFile(objFILwrite);
		}
		else{
			printf("Error: BIN file not existent or CSV filename not unique/backuped!\n");
		}
	}

	// Restart measuring
	measureMode = measureModeMonitoring;


}
