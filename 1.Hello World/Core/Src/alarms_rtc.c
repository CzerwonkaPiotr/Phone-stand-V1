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

//#define USB_CDC_IS_ACTIVE // Turn on and off printf functionality


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
  sAlarm.AlarmTime.Seconds = 30;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY;

  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.Alarm = RTC_ALARM_A;
  if (HAL_RTC_SetAlarm_IT (&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
    {
      Error_Handler ();
    }
#ifdef USB_CDC_IS_ACTIVE
  printf ("-> ALARM_A SET (data OK)\n\r");
  printf ("Current time: %d:%d:%d\n\r",sTime.Hours, sTime.Minutes, sTime.Seconds);
  printf ("AlarmA OK set time: %d:%d:%d\n\r",sAlarm.AlarmTime.Hours, sAlarm.AlarmTime.Minutes, sAlarm.AlarmTime.Seconds);
#endif
}

void SetGPSAlarmADataNOk(void)
{
  RTC_TimeTypeDef sTime = { 0 };
  RTC_DateTypeDef sDate = { 0 };
  RTC_AlarmTypeDef sAlarm = { 0 };

  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);


  sAlarm.AlarmTime.Seconds = sTime.Seconds;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY|RTC_ALARMMASK_HOURS|RTC_ALARMMASK_MINUTES;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.Alarm = RTC_ALARM_A;
  if (HAL_RTC_SetAlarm_IT (&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
    {
      Error_Handler ();
    }

#ifdef USB_CDC_IS_ACTIVE
  printf ("-> ALARM_A SET (data NOK)\n\r");
  printf ("Current time: %d:%d:%d\n\r",sTime.Hours, sTime.Minutes, sTime.Seconds);
  printf ("AlarmA NOK set time: %d:%d:%d\n\r",sAlarm.AlarmTime.Hours, sAlarm.AlarmTime.Minutes, sAlarm.AlarmTime.Seconds);
#endif
}

void SetGPSAlarmB(void)
{
  RTC_TimeTypeDef sTime = { 0 };
  RTC_DateTypeDef sDate = { 0 };
  RTC_AlarmTypeDef sAlarm = { 0 };

  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  sAlarm.AlarmTime.Seconds = 0;

  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY|RTC_ALARMMASK_HOURS|RTC_ALARMMASK_MINUTES;

  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.Alarm = RTC_ALARM_B;
  if (HAL_RTC_SetAlarm_IT (&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
    {
      Error_Handler ();
    }

#ifdef USB_CDC_IS_ACTIVE
  printf ("-> ALARM_B SET \n\r");
  printf ("Current time: %d:%d:%d\n\r",sTime.Hours, sTime.Minutes, sTime.Seconds);
  printf ("AlarmB set time: %d:%d:%d\n\r",sAlarm.AlarmTime.Hours, sAlarm.AlarmTime.Minutes, sAlarm.AlarmTime.Seconds);
#endif
}

uint8_t Check_RTC_Alarm (void)
{
  wakeUpSource_t wakeUpSource = 0;
  RTC_TimeTypeDef sTime;
  HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
  RTC_DateTypeDef sDate;
  HAL_RTC_GetDate (&hrtc, &sDate, RTC_FORMAT_BIN);
  RTC_AlarmTypeDef sAlarmA;
  HAL_RTC_GetAlarm (&hrtc, &sAlarmA, RTC_ALARM_A, RTC_FORMAT_BIN);
  RTC_AlarmTypeDef sAlarmB;
  HAL_RTC_GetAlarm (&hrtc, &sAlarmB, RTC_ALARM_B, RTC_FORMAT_BIN);

  if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) == SET && __HAL_PWR_GET_FLAG(PWR_FLAG_WU) == SET && (sAlarmA.AlarmTime.Hours == sTime.Hours)
      && (sAlarmA.AlarmTime.Minutes == sTime.Minutes) && (sAlarmA.AlarmTime.Seconds == sTime.Seconds))
  {
    wakeUpSource = WKUP_SRC_ALARM_A_DATA_OK; // MCU was woken up by an RTC alarmA
  }
  else if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) == SET && __HAL_PWR_GET_FLAG(PWR_FLAG_WU) == SET && sAlarmA.AlarmTime.Seconds == sTime.Seconds
      && sAlarmB.AlarmTime.Seconds == sTime.Seconds)
  {
    wakeUpSource = WKUP_SRC_ALARM_A_OR_B; // MCU was woken up by an RTC alarmB or could be alarmA
  }
  else if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) == SET && __HAL_PWR_GET_FLAG(PWR_FLAG_WU) == SET && sAlarmA.AlarmTime.Seconds == sTime.Seconds)
  {
    wakeUpSource = WKUP_SRC_ALARM_A_DATA_NOK; // MCU was woken up by an RTC alarmA data NOK
  }
  else if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) == SET && __HAL_PWR_GET_FLAG(PWR_FLAG_WU) == SET && sAlarmB.AlarmTime.Seconds == sTime.Seconds)
  {
    wakeUpSource = WKUP_SRC_ALARM_B; // MCU was woken up by an RTC alarmB
  }
  else if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) == SET && __HAL_PWR_GET_FLAG(PWR_FLAG_WU) == SET) // Check and handle if the system was resumed from StandBy mode
  {
    wakeUpSource = WKUP_SRC_WKUP_PIN; // MCU was woken up by WKUP PIN
  }
  else
  {
    wakeUpSource = WKUP_SRC_BOOT_UP; // MCU was woken up by something else than alarm like initial startup
  }
  return wakeUpSource;
}
