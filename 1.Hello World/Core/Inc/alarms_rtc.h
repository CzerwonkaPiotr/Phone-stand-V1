/*
 * alarms_rtc.h
 *
 *  Created on: Aug 24, 2024
 *      Author: piotr
 */

#ifndef INC_ALARMS_RTC_H_
#define INC_ALARMS_RTC_H_

void SetGPSAlarmADataOk(void);
void SetGPSAlarmADataNOk(void);
void SetGPSAlarmB(void);
uint8_t Check_RTC_Alarm (void);

#endif /* INC_ALARMS_RTC_H_ */
