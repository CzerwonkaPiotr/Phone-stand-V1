/*
 * alarms_rtc.h
 *
 *  Created on: Aug 24, 2024
 *      Author: piotr
 */

#ifndef INC_ALARMS_RTC_H_
#define INC_ALARMS_RTC_H_

typedef enum
{
  WKUP_SRC_BOOT_UP = 1,
  WKUP_SRC_WKUP_PIN = 2,
  WKUP_SRC_ALARM_A_DATA_OK = 3,
  WKUP_SRC_ALARM_A_DATA_NOK = 4,
  WKUP_SRC_ALARM_B = 5,
  WKUP_SRC_ALARM_A_OR_B = 6
} wakeUpSource_t;

void SetGPSAlarmADataOk(void);
void SetGPSAlarmADataNOk(void);
void SetGPSAlarmB(void);
uint8_t Check_RTC_Alarm (void);

#endif /* INC_ALARMS_RTC_H_ */
