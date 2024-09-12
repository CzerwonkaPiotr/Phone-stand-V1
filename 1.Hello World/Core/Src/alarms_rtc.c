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
  if(sAlarm.AlarmTime.Minutes>58) {
      sAlarm.AlarmTime.Minutes=0;
    }else{
      sAlarm.AlarmTime.Minutes=sTime.Minutes+1;
    }
  sAlarm.AlarmTime.Seconds = 0;

  //***** TEST ******
//    if(sAlarm.AlarmTime.Seconds>58) {
//        sAlarm.AlarmTime.Seconds= (sAlarm.AlarmTime.Seconds + 10) % 10;
//      }else{
//        sAlarm.AlarmTime.Seconds=sTime.Seconds+10;
//      }
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

void SetGPSAlarmB(void)
{
  RTC_TimeTypeDef sTime = { 0 };
  RTC_DateTypeDef sDate = { 0 };
  RTC_AlarmTypeDef sAlarm = { 0 };

  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  /** Enable the Alarm B
   */
  if(sAlarm.AlarmTime.Minutes>58) {
        sAlarm.AlarmTime.Minutes=0;
      }else{
        sAlarm.AlarmTime.Minutes=sTime.Minutes+1;
      }
    sAlarm.AlarmTime.Seconds = 0;

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
  else if ((sAlarmB.AlarmTime.Hours == sTime.Hours)
      && (sAlarmB.AlarmTime.Minutes == sTime.Minutes)
      && (sAlarmB.AlarmTime.Seconds == sTime.Seconds))
    {
      return 2; // Return 1 if MCU was woken up by an RTC alarmB
    }
  else
    {
      return 0; // Return 0 if MCU was woken up by something else than alarmc like wkup button
    }
}
