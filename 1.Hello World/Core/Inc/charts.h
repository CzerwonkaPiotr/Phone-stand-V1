
/*
 * charts.h
 *
 *  Created on: Dec 21, 2024
 *      Author: piotr
 */

#ifndef INC_CHARTS_H_
#define INC_CHARTS_H_

#include "main.h"
#include "epdpaint.h"
#include "ui.h"
#include "rtc.h"

typedef struct
{
    uint32_t  epoch_seconds; 	// 4 bytes
    float temperature;		// 4 bytes
    float humidity;		// 4 byte
    float pressure;		// 4 byte
    float battery_level;	// 4 byte
    uint8_t padding[12];	// padding do 32 bytes
}CHARTS_t;

void CHARTS_DrawCharts (Paint* paint, CHART_TYPE_POSITION_t type, CHART_RANGE_POSITION_t range, RTC_TimeTypeDef sTime, RTC_DateTypeDef sDate);
void CHARTS_SaveData (CHARTS_t* data);
uint32_t RTC_ToEpochSeconds(RTC_TimeTypeDef* time, RTC_DateTypeDef* date);

#endif /* INC_CHARTS_H_ */
