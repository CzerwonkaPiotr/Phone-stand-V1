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

#define SCREEN_WIDTH       128
#define SCREEN_HEIGHT      296

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
  HUMIDITY_CHART = 2,
  PRESSURE_CHART = 3,
  BATTERY_LEVEL_CHART = 4
} CHART_TYPE_POSITION_t;

typedef enum
{
  RANGE_8H = 1,
  RANGE_48H = 2,
  RANGE_168H = 3
} CHART_RANGE_POSITION_t;

typedef enum
{
  CHART_EDIT_NO_GROUP = 1,
  CHART_EDIT_TYPE_GROUP = 2,
  CHART_EDIT_RANGE_GROUP = 3
} CHART_SETTING_GROUP_POSITION_t;

typedef enum
{
  FADE_LED_SEQUENCE = 1,
  FLASH_LED_SEQUENCE = 2,
  SMOOTH_LED_SEQUENCE = 3,
} LED_SEQUENCE_POSITION_t;

typedef enum
{
  DURATION_1S = 1,
  DURATION_3S = 2,
  DURATION_10S = 3,
  DURATION_INFINITE = 4
} LED_DURATION_POSITION_t;

typedef enum
{
  LED_EDIT_NO_GROUP = 1,
  LED_EDIT_SEQUENCE_GROUP = 2,
  LED_EDIT_DURATION_GROUP = 3
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

#endif /* INC_UI_H_ */
