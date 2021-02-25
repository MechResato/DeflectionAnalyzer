/*********************************************************************************************************************
* DAVE APP Name : RTC       APP Version: 4.1.14
*
* NOTE:
* This file is generated by DAVE. Any manual modification done to this file will be lost when the code is regenerated.
*********************************************************************************************************************/

/**
 * @cond
 ***********************************************************************************************************************
 *
 * Copyright (c) 2015, Infineon Technologies AG
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
 * 2015-02-16:
 *     - Initial version<br>
 * 2015-11-18:
 *     - When the timer/alarm event triggers NMI, the corresponding event callback from RTC config structure is removed.<br>
 *
 * @endcond
 *
 */


/***********************************************************************************************************************
 * HEADER FILES
 **********************************************************************************************************************/
#include "rtc.h"
	

/***********************************************************************************************************************
 * DATA STRUCTURES
 **********************************************************************************************************************/
const RTC_CONFIG_t RTC_0_config  = 
{

  /* RTC start at Initialization */
  .start = RTC_START_ENABLE,

  /* Timer Periodic seconds interrupt disabled */
  .periodic_sec_intr = RTC_INT_PERIODIC_SEC_DISABLE,

  /* Timer Periodic minutes interrupt disabled */
  .periodic_min_intr = RTC_INT_PERIODIC_MIN_DISABLE,

  /* Timer Periodic hours interrupt disabled */
  .periodic_hour_intr = RTC_INT_PERIODIC_HOUR_DISABLE,

  /* Timer Periodic days interrupt disabled */
  .periodic_day_intr = RTC_INT_PERIODIC_DAY_DISABLE,

  /* Timer Periodic months interrupt disabled */
  .periodic_month_intr = RTC_INT_PERIODIC_MONTH_DISABLE,

  /* Timer Periodic years interrupt disabled */
  .periodic_year_intr = RTC_INT_PERIODIC_YEAR_DISABLE,

  /* Alarm interrupt disabled */
  .alarm_intr = RTC_INT_ALARM_DISABLE,

};
const XMC_RTC_CONFIG_t RTC_0_time_alarm_config =
{
{
  .seconds = 0U,
  .minutes = 0U,
  .hours = 0U,
  /* To be in sync with RTC hardware, day of month entered in UI is 
     subtracted by 1 */
  .days = 0U,
  .daysofweek = XMC_RTC_WEEKDAY_THURSDAY,
  /* To be in sync with RTC hardware, month entered in UI is subtracted by 1 */
  .month = XMC_RTC_MONTH_JANUARY,
  .year = 1970U
},
{
  .seconds = 0U,
  .minutes = 1U,
  .hours = 0U,
  /* To be in sync with RTC hardware, day of month entered in UI is subtracted 
     by 1 */
  .days = 0U,
/* To be in sync with RTC hardware, month entered in UI is subtracted by 1 */
  .month = XMC_RTC_MONTH_JANUARY,
  .year = 1970U
},
.prescaler = DEFAULT_DIVIDERVALUE
};
RTC_t RTC_0 =
{
  .time_alarm_config = &RTC_0_time_alarm_config,
  .config = &RTC_0_config,
  .initialized = false
};
