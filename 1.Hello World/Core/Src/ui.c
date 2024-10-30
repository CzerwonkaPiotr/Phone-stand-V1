/*
 * ui.c
 *
 *  Created on: Oct 12, 2024
 *      Author: piotr
 */
#include "ui.h"
#include "main.h"
#include "rtc.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "bmp280.h"
#include "NEO_6M.h"
#include "button.h"
#include "epd2in9.h"
#include "epdif.h"
#include "epdpaint.h"
#include "imagedata.h"
#include "alarms_rtc.h"

#define CHART_TYPE_POSITION_AMOUNT  4
#define CHART_RANGE_POSITION_AMOUNT  3
#define LED_SEQUENCE_POSITION_AMOUNT  3
#define LED_DURATION_POSITION_AMOUNT  4

#define BKP_POSITION_REGISTER RTC_BKP_DR1
#define CHART_TYPE_REG_OFFSET 0
#define CHART_RANGE_REG_OFFSET 8
#define LED_SEQUENCE_REG_OFFSET 16
#define LED_DURATION_REG_OFFSET 24

typedef enum
{
  CLOCK = 1,
  CHARTS = 2,
  LEDS = 3
} SCREEN_t;

static SCREEN_t currentScreen = CLOCK;

static uint8_t chartTypeSetPosition = 1;
static uint8_t chartTypeSelectedPosition;
static uint8_t chartRangeSetPosition = 1;
static uint8_t chartRangeSelectedPosition;
static uint8_t chartSettingGroup; // 0 - no group in edit, 1 - group nr.1 in edit, 2 - group nr.2 in edit

static uint8_t ledSequenceSetPosition = 1;
static uint8_t ledSequenceSelectedPosition;
static uint8_t ledDurationSetPosition = 1;
static uint8_t ledDurationSelectedPosition;
static uint8_t ledSettingGroup; // 0 - no group in edit, 1 - group nr.1 in edit, 2 - group nr.2 in edit

BMP280_t Bmp280;
float Temperature, Pressure, Humidity;

RTC_TimeTypeDef sTime;
RTC_DateTypeDef sDate;

TButton userButton;

Paint paint;
static EPD epd;
static unsigned char frame_buffer[EPD_WIDTH * EPD_HEIGHT / 8];
static unsigned char *frame_buffer_p = frame_buffer;

void UI_Init (void)
{
  ButtonRegisterPressCallback (&userButton, UI_NextScreenCallback);
  ButtonRegisterLongPressCallback (&userButton, UI_EnterSettingsCallback);

  //settings backup
//  HAL_PWR_EnableBkUpAccess ();//
//  uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_POSITION_REGISTER);
//  chartTypeSetPosition = (uint8_t) ((regTemp >> CHART_TYPE_REG_OFFSET) & 0xFF);
//  chartRangeSetPosition = (uint8_t) ((regTemp >> CHART_RANGE_REG_OFFSET) & 0xFF);
//  ledSequenceSetPosition = (uint8_t) ((regTemp >> LED_SEQUENCE_REG_OFFSET) & 0xFF);
//  ledDurationSetPosition = (uint8_t) ((regTemp >> LED_DURATION_REG_OFFSET) & 0xFF);
//  HAL_PWR_DisableBkUpAccess ();

  if (EPD_Init (&epd, lut_full_update) != 0)
  {
    //TODO error
    Error_Handler ();
  }
  Paint_Init (&paint, frame_buffer_p, epd.width, epd.height);
  Paint_Clear (&paint, UNCOLORED);
}

void UI_FullUpdateCurrentScreen (void)
{
  if (currentScreen == CLOCK)
  {
    char temp[128];
    Paint_Clear (&paint, UNCOLORED);
    //Draw Time
    RTC_TimeTypeDef sTime =
    { 0 };
    HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
    RTC_DateTypeDef sDate =
    { 0 };
    HAL_RTC_GetDate (&hrtc, &sDate, RTC_FORMAT_BIN);
    sprintf (temp, "%02d", sTime.Hours);
    Paint_DrawStringAt (&paint, 8, 5, temp, &Font64, COLORED);
    sprintf (temp, "%02d", sTime.Minutes);
    Paint_DrawStringAt (&paint, 142, 5, temp, &Font64, COLORED);
    Paint_DrawFilledCircle (&paint, 131, 22, 5, COLORED); // Colon
    Paint_DrawFilledCircle (&paint, 131, 68, 5, COLORED); // Colon
    EPD_SetFrameMemory (&epd, frame_buffer_p, 0, 0, Paint_GetWidth (&paint), Paint_GetHeight (&paint));
    EPD_DisplayFrame (&epd);
    EPD_Sleep (&epd);
  }
  else if (currentScreen == CHARTS)
  {

  }
  else if (currentScreen == LEDS)
  {

  }

//  if (BMP280_ReadSensorForcedMode (&Bmp280, &Pressure, &Temperature, &Humidity) == 0)
//  {
  //TODO this function has to be finished
//    return;
//  }
}
uint8_t UI_RunOneMinuteProcess (void)
{
  if (currentScreen == CLOCK)
  {
    UI_FullUpdateCurrentScreen ();
    //TODO uruchom proces ledów

    if ((sTime.Minutes % 10) == 0)
    {
      // TODO zapisz dane z bme i baterii do Flash SPI
    }
  }

  SetGPSAlarmB ();
  return 0; // return 0 if process can end
}
void UI_RunMenuProcess (uint8_t UserMenuFirstUse)
{
  ButtonTask (&userButton); // Machine state task for button
  if (UserMenuFirstUse == 1)
  {
    UI_NextScreenCallback ();
  }
}
void UI_NextScreenCallback (void)
{
  if (currentScreen == CLOCK)
  {
    currentScreen = CHARTS;
  }
  else if (currentScreen == CHARTS)
  {
    currentScreen = LEDS;
  }
  else if (currentScreen == LEDS)
  {
    currentScreen = CLOCK;
  }
  UI_FullUpdateCurrentScreen ();
}
void UI_EnterSettingsCallback (void)
{
  if (currentScreen == CLOCK)
  {
    // no settings here
    return;
  }
  else if (currentScreen == CHARTS)
  {
    chartSettingGroup = 1;
    chartTypeSelectedPosition = 1;
  }
  else if (currentScreen == LEDS)
  {
    ledSettingGroup = 1;
    ledSequenceSelectedPosition = 1;
  }
  ButtonRegisterPressCallback (&userButton, UI_NextPositionOnMenuCallback);
  ButtonRegisterLongPressCallback (&userButton, UI_NextSettingsGroupCallback);

  UI_FullUpdateCurrentScreen ();
}
void UI_NextPositionOnMenuCallback (void)
{
  if (currentScreen == CLOCK)
  {
    // no settings here
  }
  else if (currentScreen == CHARTS)
  {
    if (chartSettingGroup == 1)
    {
      chartTypeSelectedPosition = (chartTypeSelectedPosition + 1) % CHART_TYPE_POSITION_AMOUNT;
    }
    else if (chartSettingGroup == 2)
    {
      chartRangeSelectedPosition = (chartRangeSelectedPosition + 1) % CHART_RANGE_POSITION_AMOUNT;
    }
  }
  else if (currentScreen == LEDS)
  {
    if (ledSettingGroup == 1)
    {
      ledSequenceSelectedPosition = (ledSequenceSelectedPosition + 1) % LED_SEQUENCE_POSITION_AMOUNT;
    }
    else if (ledSettingGroup == 2)
    {
      ledDurationSelectedPosition = (ledDurationSelectedPosition + 1) % LED_DURATION_POSITION_AMOUNT;
    }
  }
  UI_FullUpdateCurrentScreen ();
}
void UI_NextSettingsGroupCallback (void)
{
  if (currentScreen == CLOCK)
  {
    // no settings here
  }
  else if (currentScreen == CHARTS)
  {
    chartTypeSetPosition = chartTypeSelectedPosition;

//    HAL_PWR_EnableBkUpAccess ();
//    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_POSITION_REGISTER);
//    regTemp = (regTemp & 0xFFFFFF00) | (chartTypeSetPosition << CHART_TYPE_REG_OFFSET);
//    HAL_RTCEx_BKUPWrite (&hrtc, BKP_POSITION_REGISTER, regTemp);
//    HAL_PWR_DisableBkUpAccess ();

    chartSettingGroup = 2;
    chartRangeSelectedPosition = 1;
  }
  else if (currentScreen == LEDS)
  {
    ledSequenceSetPosition = ledSequenceSelectedPosition;

//    HAL_PWR_EnableBkUpAccess ();
//    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_POSITION_REGISTER);
//    regTemp = (regTemp & 0xFF00FFFF) | (ledSequenceSetPosition << LED_SEQUENCE_REG_OFFSET);
//    HAL_RTCEx_BKUPWrite (&hrtc, BKP_POSITION_REGISTER, regTemp);
//    HAL_PWR_DisableBkUpAccess ();

    ledSettingGroup = 2;
    ledDurationSelectedPosition = 1;
  }
  ButtonRegisterLongPressCallback (&userButton, UI_ExitSettingsCallback);
  UI_FullUpdateCurrentScreen ();
}
void UI_ExitSettingsCallback (void)
{
  if (currentScreen == CLOCK)
  {
    // no settings here
  }
  else if (currentScreen == CHARTS)
  {
    chartRangeSetPosition = chartRangeSelectedPosition;

//    HAL_PWR_EnableBkUpAccess ();
//    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_POSITION_REGISTER);
//    regTemp = (regTemp & 0xFFFF00FF) | (chartRangeSetPosition << CHART_RANGE_REG_OFFSET);
//    HAL_RTCEx_BKUPWrite (&hrtc, BKP_POSITION_REGISTER, regTemp);
//    HAL_PWR_DisableBkUpAccess ();

    chartSettingGroup = 0;
  }
  else if (currentScreen == LEDS)
  {
    ledDurationSetPosition = ledDurationSelectedPosition;

//    HAL_PWR_EnableBkUpAccess ();
//    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_POSITION_REGISTER);
//    regTemp = (regTemp & 0x00FFFFFF) | (ledDurationSetPosition << LED_DURATION_REG_OFFSET);
//    HAL_RTCEx_BKUPWrite (&hrtc, BKP_POSITION_REGISTER, regTemp);
//    HAL_PWR_DisableBkUpAccess ();

    ledSettingGroup = 0;
  }
  ButtonRegisterPressCallback (&userButton, UI_NextScreenCallback);
  ButtonRegisterLongPressCallback (&userButton, UI_EnterSettingsCallback);
  UI_FullUpdateCurrentScreen ();
}
void UI_ShowClockScreen (void)
{
  ButtonRegisterPressCallback (&userButton, UI_NextScreenCallback);
  ButtonRegisterLongPressCallback (&userButton, UI_EnterSettingsCallback);
  currentScreen = CLOCK;
  UI_FullUpdateCurrentScreen ();
}

