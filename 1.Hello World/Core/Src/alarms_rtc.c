/*
 * alarms_rtc.c
 *
 *  Created on: Aug 24, 2024
 *      Author: piotr
 */

#include "main.h"
#include "alarms_rtc.h"
#include "rtc.h"
#include "NEO_6M.h"


void SetGPSAlarmADataOk(void)
{
  RTC_TimeTypeDef sTime = { 0 };
  RTC_DateTypeDef sDate = { 0 };
  RTC_AlarmTypeDef sAlarm = { 0 };

  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  /** Enable the Alarm A
   */
  sAlarm.AlarmTime.Hours = 4;
  sAlarm.AlarmTime.Minutes = 0;
  sAlarm.AlarmTime.Seconds = 0;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.Alarm = RTC_ALARM_A;
  if (HAL_RTC_SetAlarm_IT (&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
    {
      Error_Handler ();
    }
}

void SetGPSAlarmADataNOk(void)
{
  RTC_TimeTypeDef sTime = { 0 };
  RTC_DateTypeDef sDate = { 0 };
  RTC_AlarmTypeDef sAlarm = { 0 };

  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  /** Enable the Alarm A
   */
//  if(sAlarm.AlarmTime.Minutes>58) {
//      sAlarm.AlarmTime.Minutes=0;
//    }else{
//      sAlarm.AlarmTime.Minutes=sTime.Minutes+1;
//    }
//  sAlarm.AlarmTime.Seconds = 0;

  //***** TEST ******
    if(sAlarm.AlarmTime.Seconds>58) {
        sAlarm.AlarmTime.Seconds= (sAlarm.AlarmTime.Seconds + 10) % 10;
      }else{
        sAlarm.AlarmTime.Seconds=sTime.Seconds+10;
      }
    //sAlarm.AlarmTime.Minutes = 0;
  //***** TEST ******

  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY|RTC_ALARMMASK_HOURS|RTC_ALARMMASK_MINUTES;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.Alarm = RTC_ALARM_A;
  if (HAL_RTC_SetAlarm_IT (&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
    {
      Error_Handler ();
    }
}
