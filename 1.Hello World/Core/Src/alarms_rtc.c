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

void SetGPSAlarmADataOk (void)
{
  RTC_TimeTypeDef sTime =
  { 0 };
  RTC_DateTypeDef sDate =
  { 0 };
  RTC_AlarmTypeDef sAlarm =
  { 0 };

  HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate (&hrtc, &sDate, RTC_FORMAT_BIN);

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

void SetGPSAlarmADataNOk (void)
{
  RTC_TimeTypeDef sTime =
  { 0 };
  RTC_DateTypeDef sDate =
  { 0 };
  RTC_AlarmTypeDef sAlarm =
  { 0 };

  HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate (&hrtc, &sDate, RTC_FORMAT_BIN);

  sAlarm.AlarmTime.Seconds = sTime.Seconds;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY | RTC_ALARMMASK_HOURS | RTC_ALARMMASK_MINUTES;
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

void SetGPSAlarmB (void)
{
  RTC_TimeTypeDef sTime =
  { 0 };
  RTC_DateTypeDef sDate =
  { 0 };
  RTC_AlarmTypeDef sAlarm =
  { 0 };

  HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate (&hrtc, &sDate, RTC_FORMAT_BIN);

  sAlarm.AlarmTime.Seconds = 0;

  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY | RTC_ALARMMASK_HOURS | RTC_ALARMMASK_MINUTES;

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
  wakeUpSource_t wakeUpSource = WKUP_SRC_BOOT_UP;
  // Sprawdź, czy MCU było w trybie STANDBY
  if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) == SET)
  {
    // Sprawdź, czy wybudzenie pochodzi od Wakeup Pin
            if (__HAL_PWR_GET_FLAG(PWR_FLAG_WU) == SET)
            {
                // Weryfikacja, czy Wakeup Pin spowodował wybudzenie
                if (HAL_GPIO_ReadPin(WKUP_BUTTON_GPIO_Port, WKUP_BUTTON_Pin) == GPIO_PIN_SET)
                {
                    wakeUpSource = WKUP_SRC_WKUP_PIN; // Wybudzenie przez Wakeup Pin
                }
            }

            // Sprawdź flagi RTC Alarm (ALRAF i ALRBF)
            if (__HAL_RTC_ALARM_GET_FLAG(&hrtc, RTC_FLAG_ALRAF))
            {
                wakeUpSource = WKUP_SRC_ALARM_A; // Wybudzenie przez RTC Alarm A
            }
            else if (__HAL_RTC_ALARM_GET_FLAG(&hrtc, RTC_FLAG_ALRBF))
            {
                wakeUpSource = WKUP_SRC_ALARM_B; // Wybudzenie przez RTC Alarm B
            }

    // Wyczyszczenie flag STANDBY i WAKEUP
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);// Kasowanie flagi Standby
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU); // Kasowanie flagi Wakeup

    // Wyczyszczenie flag RTC
    __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
    __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRBF);
    __HAL_GPIO_EXTI_CLEAR_IT(EXTI_LINE_0);    // Kasowanie flagi EXTI
  }
  return wakeUpSource;
}
