/**
 * @file fatfs.c
 * @date 2017-07-05
 *
 * NOTE:
 * This file is generated by DAVE. Any manual modification done to this file will be lost when the code is regenerated.
 */
/**
 * @cond
 ***********************************************************************************************************************
 * FATFS v4.0.24 Helps the user to create his own file system
 *
 * Copyright (c) 2015-2018, Infineon Technologies AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,are permitted provided that the
 * following conditions are met:
 *
 *   Redistributions of source code must retain the above copyright notice, this list of conditions and the  following
 *   disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 *   following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 *   Neither the name of the copyright holders nor the names of its contributors may be used to endorse or promote
 *   products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE  FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY,OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * To improve the quality of the software, users are encouraged to share modifications, enhancements or bug fixes
 * with Infineon Technologies AG (dave@infineon.com).
 ***********************************************************************************************************************
 *
 * Change History
 * --------------
 *
 * 2016-02-05:
 *     - Initial version<br>
 * 2016-04-12:
 *     - Removed the array FATFS_devicefunc
 * 2018-07-05:
 *     - Updated to FatFS v0.13b_p1
 * @endcond
 *
 */


/***********************************************************************************************************************
 * HEADER FILES
 **********************************************************************************************************************/
#include "fatfs.h"
#include "ff_Src/ffconf.h"
#include "ff_Src/diskio.h"
#if (FATFS_STANDARDLIBRARY == 1U)
#include "sltha.h"
#endif
#if (FF_FS_NORTC == 1U)
#include <time.h>
#endif
/***********************************************************************************************************************
	* MACROS
 **********************************************************************************************************************/
DRESULT FATFS_errorcodes[5] =   {
                                  RES_OK ,
                                  RES_ERROR,
                                  RES_WRPRT,
                                  RES_NOTRDY,
                                  RES_PARERR
                                };
DSTATUS FATFS_statuscodes[4] =  {
                                  (DSTATUS)0,
                                  (DSTATUS)STA_NOINIT,
                                  (DSTATUS)STA_NODISK,
                                  (DSTATUS)STA_PROTECT
                                };

FATFS_DEVICEFUNCTYPE_t FATFS_devicefunc =  {
                                              SDMMC_BLOCK_Initialize,
                                              SDMMC_BLOCK_GetStatus,
                                              SDMMC_BLOCK_ReadBlock,
                                              SDMMC_BLOCK_WriteBlock,
                                              SDMMC_BLOCK_Ioctl
                                           };
/***********************************************************************************************************************
 * LOCAL DATA
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * LOCAL ROUTINES
 **********************************************************************************************************************/
extern SDMMC_BLOCK_t* const sdmmc_block[FF_VOLUMES];
/**********************************************************************************************************************
* API IMPLEMENTATION
**********************************************************************************************************************/
/*
 * API to retrieve the version of the FATFS APP
 */
DAVE_APP_VERSION_t FATFS_GetAppVersion(void)
{
  DAVE_APP_VERSION_t version;

  version.major = (uint8_t)FATFS_MAJOR_VERSION;
  version.minor = (uint8_t)FATFS_MINOR_VERSION;
  version.patch = (uint8_t)FATFS_PATCH_VERSION;

  return (version);
}

/*
 * API to initialize the FATFS APP
 */
FATFS_STATUS_t FATFS_Init(FATFS_t *const handle)
{
  XMC_ASSERT("FATFS_Init:HandlePtr NULL", (handle != NULL));
  FATFS_STATUS_t status = FATFS_STATUS_SUCCESS;
  if (handle->initialized == false)
  {
  for (int i = 0; i<FF_VOLUMES; i++)
  {
    status |= (FATFS_STATUS_t)SDMMC_BLOCK_Init(sdmmc_block[i]);
  }

#if (FATFS_STANDARDLIBRARY == 1U)
  SLTHA_Init();
#endif

#if (FF_FS_NORTC == 0U)
  if (status == FATFS_STATUS_SUCCESS)
  {
    status = (FATFS_STATUS_t)RTC_Init(handle->rtc_handle);
  }
#endif
    handle->initialized = true;
  }
  else
  {
    status = FATFS_STATUS_FAILURE;
  }
  return (status);
}

/*
 * The function performs the disk initialization.
 */
DSTATUS disk_initialize(BYTE drive) /* Physical drive number (0..) */
{
  DSTATUS diskstatus = (DSTATUS)0;
  uint32_t status;

  /* If drive number is greater than the maximum drives allowed  */
  if (drive >= FF_VOLUMES)
  {
    diskstatus = (DSTATUS)((uint8_t)STA_NODISK | (uint8_t)STA_NOINIT);
  }
  else
  {
    /* Call the Initialize function. */
    status = FATFS_devicefunc.InitializePtr(sdmmc_block[drive]);
    /* Fatfs to Device Abstraction Layer Error Code Mapping */
    diskstatus = FATFS_statuscodes[status];
  }
  return (diskstatus);
}

/*
 * The function gets the disk status information.
 */
DSTATUS disk_status(BYTE drive)		/* Physical drive number (0..) */
{
  DSTATUS diskstatus;
  uint32_t status;
  /* If drive number is greater than the maximum drives allowed  */
  if (drive >= (uint8_t)FF_VOLUMES)
  {
    diskstatus = (DSTATUS)((uint8_t)STA_NODISK | (uint8_t)STA_NOINIT);
  }
  else
  {
    /* Call the Initialize function.*/
    status = FATFS_devicefunc.GetStatusPtr(sdmmc_block[drive]);
    /* Fatfs to Block Layer Error Code Mapping */
    diskstatus = FATFS_statuscodes[status];
  }
  return (diskstatus);
}

/*
 * The function reads the blocks of data from the disk.
 */
DRESULT disk_read(
  BYTE drive,		  /* Physical drive number (0..) */
  BYTE *buffer,	      /* Data buffer to store read data */
  DWORD sectornumber, /* Sector address (LBA) */
  UINT sectorcount    /* Number of sectors to read */
)
{
  DRESULT diskresult;
  uint32_t result;
  /* If drive number is greater than the maximum drives allowed  */
  if (drive >= (uint8_t)FF_VOLUMES )
  {
    diskresult = RES_PARERR;
  }
  /* If sector count is less than 1. Minimum 1 sector is needed*/
  else if (sectorcount < (uint8_t)1)
  {
    diskresult = RES_PARERR;
  }
  /*Call the ReadBlkPtr function.*/
  else
  {
    result = (uint32_t)FATFS_devicefunc.ReadBlkPtr(sdmmc_block[drive], (uint8_t *)buffer,
                                                          (uint32_t)sectornumber, sectorcount);

    /* FatFs to Device Abstraction Layer Error Code Mapping */
    diskresult = FATFS_errorcodes[result];
  }
  return (diskresult);
}

/*
 * The function writes the blocks of data on the disk.
 */
DRESULT disk_write(
  BYTE drive,			/* Physical drive number (0..) */
  const BYTE *buffer,	/* Data to be written */
  DWORD sectornumber,	/* Sector address (LBA) */
  UINT sectorcount	    /* Number of sectors to write */
)
{
  DRESULT diskresult;
  uint32_t result;
  /* If drive number is greater than the maximum drives allowed  */
  if (drive >= (uint8_t)FF_VOLUMES)
  {
    diskresult = RES_PARERR;
  }
  /* If sector count is less than 1. Minimum 1 sector is needed*/
  else if (sectorcount < (uint8_t)1)
  {
    diskresult = RES_PARERR;
  }
  /*Call the WriteBlkPtr function.*/
  else
  {
    result = (uint32_t)FATFS_devicefunc.WriteBlkPtr(sdmmc_block[drive],(uint8_t *)buffer, 
                                                           (uint32_t)sectornumber, sectorcount);
    /* FatFs to Device Abstraction Layer Error Code Mapping */
    diskresult = FATFS_errorcodes[result];
  }
  return (diskresult);
}

/*
 * The function performs the various IOCTL operation.
 */
DRESULT disk_ioctl(
  BYTE drive,		/* Physical drive number (0..) */
  BYTE command,	    /* Control code */
  void *buffer      /* Buffer to send/receive control data */
)
{
  DRESULT diskresult;
  uint32_t result;
  if (drive >= (uint8_t)FF_VOLUMES)
  {
    diskresult = RES_PARERR;
  }
  /*Call the Ioctl function.*/
  else
  {
    result = FATFS_devicefunc.IoctlPtr(sdmmc_block[drive],command, buffer);
    /* FatFs to Block Layer Error Code Mapping */
    diskresult = FATFS_errorcodes[result];
  }
  return (diskresult);
}

/**
 * This is a real time clock service to be called from FatFs module.
 */
DWORD get_fattime()
{
#if ((FF_FS_NORTC == 0U) && (FF_FS_READONLY == 0U))
  XMC_RTC_TIME_t Time = {{0UL}};

  RTC_GetTime(&Time);

  /* Pack date and time into a DWORD variable */
  return (((DWORD)(Time.year - 1980UL) << 25UL) | ((DWORD)Time.month << 21UL) | ((DWORD)Time.days << 16UL)
          | ((DWORD)Time.hours << 11UL)
          | ((DWORD)Time.minutes << 5UL)
          | ((DWORD)Time.seconds >> 1UL));
#endif

#if (FF_FS_NORTC == 1U)
  struct tm stdtime;
  DWORD current_time;

  current_time = (((stdtime.tm_year - 1980UL) << 25UL)  | (stdtime.tm_mon << 21UL) | (stdtime.tm_mday << 16UL) |
                   (stdtime.tm_hour << 11UL) | (stdtime.tm_min << 5UL) | (stdtime.tm_sec >> 1UL));

  return (current_time);
#endif
}


