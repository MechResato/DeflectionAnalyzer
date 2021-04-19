/*********************************************************************************************************************
* DAVE APP Name : FATFS       APP Version: 4.0.26
*
* NOTE:
* This file is generated by DAVE. Any manual modification done to this file will be lost when the code is regenerated.
*********************************************************************************************************************/

/**
 * @cond
 ***********************************************************************************************************************
 *
 * Copyright (c) 2018, Infineon Technologies AG
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
 * 2015-12-10:
 *     - Initial version<br>
 *
 * @endcond
 *
 */

#ifndef FATFS_CONF_H
#define FATFS_CONF_H

/***********************************************************************************************************************
 * HEADER FILES
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * MACROS
 **********************************************************************************************************************/
#define FATFS_STANDARDLIBRARY    (0U)
#define FF_FS_READONLY           (0U)
#define FF_FS_LOCK               (2U)
#define FF_USE_FIND               (0U)
#define FF_USE_MKFS               (0U)
#define FF_USE_FASTSEEK           (0U)
#define FF_USE_EXPAND           (0U)
#define FF_USE_CHMOD           (0U)
#define FF_USE_LABEL              (0U)
#define FF_USE_FORWARD            (0U)
#define FF_MULTI_PARTITION    (0U)
#define FF_USE_TRIM               (0U)
#define FF_FS_TINY               (0U)
#define FF_FS_NORTC              (0U)
#define FF_FS_REENTRANT          (0U)
#define FF_FS_MINIMIZE          (0U)
#define FF_USE_STRFUNC            (2U)
#define FF_CODE_PAGE           850
#define FF_FS_RPATH              (2U)
#define FF_VOLUMES            (1U)
#define FF_MIN_SS             (512U)
#define FF_MAX_SS             (512U)
#define FF_FS_NOFSINFO           (2U)

#define FATFS_MAJOR_VERSION (4U)
#define FATFS_MINOR_VERSION (0U)
#define FATFS_PATCH_VERSION (26U)

#endif  /* ifndef FATFS_CONF_H */

