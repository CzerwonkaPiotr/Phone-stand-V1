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

// Uncomment to enable USB CDC printf functionality
//#define USB_CDC_IS_ACTIVE

/**
  * @brief  Set RTC Alarm A for GPS Data OK scenario.
  * @note   This function sets Alarm A to trigger at a fixed time (04:00:30).
  */
void SetGPSAlarmADataOk (void)
{
  RTC_TimeTypeDef sTime = {0};   // Structure to hold current time
  RTC_DateTypeDef sDate = {0};  // Structure to hold current date
  RTC_AlarmTypeDef sAlarm = {0}; // Structure to configure Alarm A

  // Get the current time and date from the RTC
  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  // Configure Alarm A to trigger at 04:00:30
  sAlarm.AlarmTime.Hours = 4;
  sAlarm.AlarmTime.Minutes = 0;
  sAlarm.AlarmTime.Seconds = 30;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY; // Ignore date in alarm triggering
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL; // Disable subsecond precision
  sAlarm.Alarm = RTC_ALARM_A; // Configure Alarm A

  // Set Alarm A and enable interrupt
  if (HAL_RTC_SetAlarm_IT (&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler ();
  }

#ifdef USB_CDC_IS_ACTIVE
    // Debug information (via USB CDC)
    printf("-> ALARM_A SET (Data OK)\n\r");
    printf("Current time: %02d:%02d:%02d\n\r", sTime.Hours, sTime.Minutes, sTime.Seconds);
    printf("AlarmA OK set time: %02d:%02d:%02d\n\r", sAlarm.AlarmTime.Hours, sAlarm.AlarmTime.Minutes, sAlarm.AlarmTime.Seconds);
#endif
}

/**
  * @brief  Set RTC Alarm A for GPS Data NOT OK scenario.
  * @note   This function sets Alarm A to trigger every second.
  */
void SetGPSAlarmADataNOk (void)
{
  RTC_TimeTypeDef sTime = {0};   // Structure to hold current time
  RTC_DateTypeDef sDate = {0};  // Structure to hold current date
  RTC_AlarmTypeDef sAlarm = {0}; // Structure to configure Alarm A

  // Get the current time and date from the RTC
  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  // Configure Alarm A to trigger every second
  sAlarm.AlarmTime.Seconds = sTime.Seconds; // Set alarm to trigger on the current second
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY | RTC_ALARMMASK_HOURS | RTC_ALARMMASK_MINUTES; // Trigger every second
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL; // Disable subsecond precision
  sAlarm.Alarm = RTC_ALARM_A; // Configure Alarm A

  // Set Alarm A and enable interrupt
  if (HAL_RTC_SetAlarm_IT (&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler (); // Handle errors during alarm setup
  }

#ifdef USB_CDC_IS_ACTIVE
    // Debug information (via USB CDC)
    printf("-> ALARM_A SET (Data NOT OK)\n\r");
    printf("Current time: %02d:%02d:%02d\n\r", sTime.Hours, sTime.Minutes, sTime.Seconds);
    printf("AlarmA NOT OK set time: %02d:%02d:%02d\n\r", sAlarm.AlarmTime.Hours, sAlarm.AlarmTime.Minutes, sAlarm.AlarmTime.Seconds);
#endif
}

/**
  * @brief  Set RTC Alarm B.
  * @note   This function sets Alarm B to trigger at the beginning of every minute.
  */
void SetGPSAlarmB (void)
{
  RTC_TimeTypeDef sTime = {0};   // Structure to hold current time
  RTC_DateTypeDef sDate = {0};  // Structure to hold current date
  RTC_AlarmTypeDef sAlarm = {0}; // Structure to configure Alarm B

  // Get the current time and date from the RTC
  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  // Configure Alarm B to trigger at the start of the next minute
  sAlarm.AlarmTime.Seconds = 0; // Trigger at the beginning of the next minute
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY | RTC_ALARMMASK_HOURS | RTC_ALARMMASK_MINUTES;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL; // Disable subsecond precision
  sAlarm.Alarm = RTC_ALARM_B; // Configure Alarm B

  // Set Alarm B and enable interrupt
  if (HAL_RTC_SetAlarm_IT (&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler (); // Handle errors during alarm setup
  }

#ifdef USB_CDC_IS_ACTIVE
    // Debug information (via USB CDC)
    printf("-> ALARM_B SET\n\r");
    printf("Current time: %02d:%02d:%02d\n\r", sTime.Hours, sTime.Minutes, sTime.Seconds);
    printf("AlarmB set time: %02d:%02d:%02d\n\r", sAlarm.AlarmTime.Hours, sAlarm.AlarmTime.Minutes, sAlarm.AlarmTime.Seconds);
#endif
}

/**
  * @brief  Check the wake-up source of the MCU.
  * @retval wakeUpSource_t: Source of the wake-up
  */
uint8_t Check_RTC_Alarm (void)
{
  wakeUpSource_t wakeUpSource = WKUP_SRC_BOOT_UP; // Default wake-up source: Boot-up

  // Check if the MCU was in Standby mode
  if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) == SET)
  {
    // Check if the wake-up was caused by the Wake-up Pin
            if (__HAL_PWR_GET_FLAG(PWR_FLAG_WU) == SET)
            {
                if (HAL_GPIO_ReadPin(WKUP_BUTTON_GPIO_Port, WKUP_BUTTON_Pin) == GPIO_PIN_SET)
                {
                    wakeUpSource = WKUP_SRC_WKUP_PIN; // Wake-up caused by the Wake-up Pin
                }
            }

            // Check RTC Alarm flags (ALRAF i ALRBF)
            if (__HAL_RTC_ALARM_GET_FLAG(&hrtc, RTC_FLAG_ALRAF))
            {
                wakeUpSource = WKUP_SRC_ALARM_A; // Wake-up caused by Alarm A
            }
            else if (__HAL_RTC_ALARM_GET_FLAG(&hrtc, RTC_FLAG_ALRBF))
            {
                wakeUpSource = WKUP_SRC_ALARM_B; // Wake-up caused by Alarm B
            }

            // Clear Standby and Wake-up flags
            __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB); // Clear Standby flag
            __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU); // Clear Wake-up flag

            // Clear RTC Alarm flags
            __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
            __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRBF);

            // Clear EXTI line for Wake-up Pin
            __HAL_GPIO_EXTI_CLEAR_IT(EXTI_LINE_0);
  }
  return wakeUpSource;
}
