/*
 * ui.h
 *
 *  Created on: Oct 12, 2024
 *      Author: piotr
 */

#ifndef INC_UI_H_
#define INC_UI_H_

#include "main.h"
#include "bmp280.h"
#include "button.h"

#define SCREEN_HEIGHT      128
#define SCREEN_WIDTH 	   296

#define COLORED      0
#define UNCOLORED    1

typedef enum
{
  CLOCK = 1,
  CHARTS = 2,
  LEDS = 3
} SCREEN_t;

typedef enum
{
  TEMPERATURE_CHART = 1,
  HUMIDITY_CHART,
  PRESSURE_CHART,
  BATTERY_LEVEL_CHART
} CHART_TYPE_POSITION_t;

typedef enum
{
  RANGE_8H = 1,
  RANGE_40H,
  RANGE_160H
} CHART_RANGE_POSITION_t;

typedef enum
{
  CHART_EDIT_NO_GROUP = 1,
  CHART_EDIT_TYPE_GROUP,
  CHART_EDIT_RANGE_GROUP
} CHART_SETTING_GROUP_POSITION_t;

typedef enum
{
  FADE_LED_SEQUENCE = 1,
  CIRCLE_LED_SEQUENCE,
  SMOOTH_LED_SEQUENCE,
  OFF_LED_SEQUENCE
} LED_SEQUENCE_POSITION_t;

typedef enum
{
  DURATION_3S = 1,
  DURATION_5S,
  DURATION_10S,
  DURATION_INFINITE
} LED_DURATION_POSITION_t;

typedef enum
{
  LED_EDIT_NO_GROUP = 1,
  LED_EDIT_SEQUENCE_GROUP,
  LED_EDIT_DURATION_GROUP
} LED_SETTING_GROUP_POSITION_t;

void UI_RunOneMinuteProcess (void);
void UI_RunMenuProcess (uint8_t UserMenuFirstUse);
void UI_NextScreenCallback (void);
void UI_EnterSettingsCallback (void);
void UI_NextPositionOnMenuCallback (void);
void UI_NextSettingsGroupCallback (void);
void UI_ExitSettingsCallback (void);
void UI_ShowClockScreen (void);
void UI_Init (void);
void UI_ResetDevice(void);

#endif /* INC_UI_H_ */
