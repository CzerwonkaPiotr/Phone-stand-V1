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
#include "stdio.h"


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
  printf ("-> ALARM_A SET (data OK)\n\r");
  printf ("Current time: %d:%d:%d\n\r",sTime.Hours, sTime.Minutes, sTime.Seconds);
  printf ("AlarmA OK time: %d:%d:%d\n\r",sAlarm.AlarmTime.Hours, sAlarm.AlarmTime.Minutes, sAlarm.AlarmTime.Seconds);
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

  //***** TEST co 10s ******
  int alarmIntervalSec = 20; // Max 45
    if(sTime.Seconds> 60 - alarmIntervalSec - 1) {
        sAlarm.AlarmTime.Seconds = (sTime.Seconds + alarmIntervalSec) % alarmIntervalSec;
      }else{
        sAlarm.AlarmTime.Seconds = sTime.Seconds + alarmIntervalSec;
      }
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
  printf ("-> ALARM_A SET (data NOK)\n\r");
  printf ("Current time: %d:%d:%d\n\r",sTime.Hours, sTime.Minutes, sTime.Seconds);
  printf ("AlarmA NOK time: %d:%d:%d\n\r",sAlarm.AlarmTime.Hours, sAlarm.AlarmTime.Minutes, sAlarm.AlarmTime.Seconds);
}

void SetGPSAlarmB(void)
{
  RTC_TimeTypeDef sTime = { 0 };
  RTC_DateTypeDef sDate = { 0 };
  RTC_AlarmTypeDef sAlarm = { 0 };

  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  /** Enable the Alarm B
   */
  if(sTime.Minutes>58) {
        sAlarm.AlarmTime.Minutes = 0;
      }else{
        sAlarm.AlarmTime.Minutes = sTime.Minutes+1;
      }
    sAlarm.AlarmTime.Seconds = 0;

  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY|RTC_ALARMMASK_HOURS;

  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.Alarm = RTC_ALARM_B;
  if (HAL_RTC_SetAlarm_IT (&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
    {
      Error_Handler ();
    }
  printf ("-> ALARM_B SET \n\r");
  printf ("Current time: %d:%d:%d\n\r",sTime.Hours, sTime.Minutes, sTime.Seconds);
  printf ("AlarmB time: %d:%d:%d\n\r",sAlarm.AlarmTime.Hours, sAlarm.AlarmTime.Minutes, sAlarm.AlarmTime.Seconds);
}

uint8_t Check_RTC_Alarm (void)
{
  RTC_TimeTypeDef sTime;
  HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
  RTC_DateTypeDef sDate;
  HAL_RTC_GetDate (&hrtc, &sDate, RTC_FORMAT_BIN);
  RTC_AlarmTypeDef sAlarmA;
  HAL_RTC_GetAlarm (&hrtc, &sAlarmA, RTC_ALARM_A, RTC_FORMAT_BIN);
  RTC_AlarmTypeDef sAlarmB;
  HAL_RTC_GetAlarm (&hrtc, &sAlarmB, RTC_ALARM_B, RTC_FORMAT_BIN);

  if ((sAlarmA.AlarmTime.Hours == sTime.Hours)
      && (sAlarmA.AlarmTime.Minutes == sTime.Minutes)
      && (sAlarmA.AlarmTime.Seconds == sTime.Seconds))
    {
      return 1; // Return 1 if MCU was woken up by an RTC alarmA
    }
  else if (sAlarmA.AlarmTime.Seconds == sTime.Seconds && sAlarmB.AlarmTime.Seconds == sTime.Seconds)
    {
      return 9; // Return 9 if MCU was woken up by an RTC alarmB or could be alarmA
    }
  else if (sAlarmA.AlarmTime.Seconds == sTime.Seconds)
    {
      return 2; // Return 2 if MCU was woken up by an RTC alarmA data NOK
    }
  else if (sAlarmB.AlarmTime.Seconds == sTime.Seconds)
    {
      return 3; // Return 3 if MCU was woken up by an RTC alarmB
    }
  else
    {
      return 0; // Return 0 if MCU was woken up by something else than alarmc like wkup button
    }
}
