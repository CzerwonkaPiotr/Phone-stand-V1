
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


typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    float temperature;
    uint8_t humidity;
    uint16_t pressure;
    uint8_t battery_level;
    uint8_t padding[2];
}CHARTS_t;

void CHARTS_DrawCharts (Paint* paint, CHART_TYPE_POSITION_t type, CHART_RANGE_POSITION_t range);
void CHARTS_SaveData (CHARTS_t* data);

#endif /* INC_CHARTS_H_ */
