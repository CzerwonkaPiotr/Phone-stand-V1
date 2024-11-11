/*
 * ui.c
 *
 *  Created on: Oct 12, 2024
 *      Author: piotr
 */
#include "ui.h"
#include "main.h"
#include "adc.h"
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
#include "i2c.h"


#define CHART_TYPE_POSITION_AMOUNT  4
#define CHART_RANGE_POSITION_AMOUNT  3
#define LED_SEQUENCE_POSITION_AMOUNT  3
#define LED_DURATION_POSITION_AMOUNT  4

#define BKP_UI_SETTINGS_REGISTER RTC_BKP_DR1
#define CHART_TYPE_REG_OFFSET 0
#define CHART_RANGE_REG_OFFSET 8
#define LED_SEQUENCE_REG_OFFSET 16
#define LED_DURATION_REG_OFFSET 24

#define BMP280_ADDRESS 0x76
#define BATTERY_FULL_VALUE 2605 //Value caltulated based on 2:1 voltage divider and maximum ADC read at 3,3V 12bit(4095)
#define BATTERY_EMPTY_VALUE 1985



static SCREEN_t currentScreen = CLOCK;

static CHART_TYPE_POSITION_t chartTypeSetPosition = TEMPERATURE_CHART; // 1 = Temp, 2 = Hum, 3 = Press, 4 = Batlevel
static CHART_RANGE_POSITION_t chartRangeSetPosition = RANGE_8H; // 1 = 8h, 2 = 48h, 3 = 168h
static CHART_SETTING_GROUP_POSITION_t chartSettingGroup = CHART_EDIT_NO_GROUP; // 0 - no group in edit, 1 - group nr.1 in edit, 2 - group nr.2 in edit

static LED_SEQUENCE_POSITION_t ledSequenceSetPosition = FADE_LED_SEQUENCE; // 1 = Fade, 2 = Flash, 3 = S,ppth transition
static LED_DURATION_POSITION_t ledDurationSetPosition = DURATION_1S; // 1 = 1s, 2 = 3s, 3 = 10s, 4 = infinite
static LED_SETTING_GROUP_POSITION_t ledSettingGroup = LED_EDIT_NO_GROUP; // 0 - no group in edit, 1 - group nr.1 in edit, 2 - group nr.2 in edit

static BMP280_t Bmp280;
static float Temperature, Pressure, Humidity;
static uint8_t lastMinuteBMPRead = 61; // 61 because it will always read data when MCU was in standby mode

static uint32_t batteryLevel = 2300;

RTC_TimeTypeDef sTime;
RTC_DateTypeDef sDate;
const char weekDayName[7][4] =
{ "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };

static TButton userButton;

static Paint paint;
static EPD epd;
static unsigned char frame_buffer[EPD_WIDTH * EPD_HEIGHT / 8];
static unsigned char *frame_buffer_p = frame_buffer;

void UI_Init (void)
{
  ButtonRegisterPressCallback (&userButton, UI_NextScreenCallback);
  ButtonRegisterLongPressCallback (&userButton, UI_EnterSettingsCallback);

  //settings backup
//  HAL_PWR_EnableBkUpAccess ();//
//  uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
//  chartTypeSetPosition = (uint8_t) ((regTemp >> CHART_TYPE_REG_OFFSET) & 0xFF);
//  chartRangeSetPosition = (uint8_t) ((regTemp >> CHART_RANGE_REG_OFFSET) & 0xFF);
//  ledSequenceSetPosition = (uint8_t) ((regTemp >> LED_SEQUENCE_REG_OFFSET) & 0xFF);
//  ledDurationSetPosition = (uint8_t) ((regTemp >> LED_DURATION_REG_OFFSET) & 0xFF);
//  HAL_PWR_DisableBkUpAccess ();

  ButtonInitKey (&userButton, BUTTON1_GPIO_Port, BUTTON1_Pin, 10, 1000, 2000);

  if (EPD_Init (&epd, lut_full_update) != 0)
  {
    //TODO error
    Error_Handler ();
  }
  if (BMP280_Init (&Bmp280, &hi2c1, BMP280_ADDRESS))
  {
    Error_Handler ();
  }
  Paint_Init (&paint, frame_buffer_p, epd.width, epd.height);
  //Paint_Clear (&paint, UNCOLORED);
}

void GetBatteryLevel (void)
{
  float tempVar;
  HAL_ADC_Start (&hadc1);

  if (HAL_ADC_PollForConversion (&hadc1, HAL_MAX_DELAY) == HAL_OK)
  {
    batteryLevel = HAL_ADC_GetValue (&hadc1);
  }
  batteryLevel = 2450;
  HAL_ADC_Stop (&hadc1);

  tempVar = (float) ((batteryLevel - BATTERY_EMPTY_VALUE) / (float) (BATTERY_FULL_VALUE - BATTERY_EMPTY_VALUE));
  batteryLevel = tempVar * 100;
  if (batteryLevel > 100) batteryLevel = 100;
  if (batteryLevel < 0) batteryLevel = 0;
}

void UI_FullUpdateCurrentScreen (void)
{
  char temp[128];
  Paint_Clear (&paint, UNCOLORED);

  if (currentScreen == CLOCK)
  {
    if (lastMinuteBMPRead != (RTC->TR & (RTC_TR_MNT_Msk | RTC_TR_MNU_Msk)) >> RTC_TR_MNU_Pos)
    {
      BMP280_SetMode (&Bmp280, BMP280_FORCEDMODE); // Measurement is made and the sensor goes to sleep
      HAL_Delay (50);
      BMP280_ReadSensorData (&Bmp280, &Pressure, &Temperature, &Humidity);
      lastMinuteBMPRead = (RTC->TR & (RTC_TR_MNT_Msk | RTC_TR_MNU_Msk)) >> RTC_TR_MNU_Pos;
      GetBatteryLevel ();
    }

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
    sprintf (temp, "%s", weekDayName[sDate.WeekDay]);
    Paint_DrawStringAt (&paint, 11, 97, temp, &Font24, COLORED);
    sprintf (temp, "%02d/%02d/%02d", sDate.Date, sDate.Month, sDate.Year);
    Paint_DrawStringAt (&paint, 81, 91, temp, &Font16, COLORED);
    Paint_DrawFilledCircle (&paint, 131, 22, 5, COLORED); // Colon
    Paint_DrawFilledCircle (&paint, 131, 68, 5, COLORED); // Colon

    sprintf (temp, "%04dhPa", (int) Pressure);
    Paint_DrawStringAt (&paint, 85, 110, temp, &Font16, COLORED);
    sprintf (temp, "%.1f'C", Temperature);
    Paint_DrawStringAt (&paint, 183, 91, temp, &Font16, COLORED);
    sprintf (temp, "%02d%%Rh", (int) Humidity);
    Paint_DrawStringAt (&paint, 187, 110, temp, &Font16, COLORED);

    Paint_DrawVerticalLine (&paint, 260, 8, 112, COLORED);
    Paint_DrawHorizontalLine (&paint, 7, 86, 246, COLORED);
    Paint_DrawVerticalLine (&paint, 73, 91, 29, COLORED);
    Paint_DrawVerticalLine (&paint, 174, 91, 29, COLORED);

    Paint_DrawRectangle (&paint, 268, 19, 288, 122, COLORED);
    Paint_DrawRectangle (&paint, 269, 18, 287, 123, COLORED);
    Paint_DrawRectangle (&paint, 270, 20, 286, 121, COLORED);
    Paint_DrawHorizontalLine (&paint, 275, 16, 7, COLORED);
    Paint_DrawHorizontalLine (&paint, 274, 17, 9, COLORED);

    Paint_DrawFilledRectangle (&paint, 271, 121, 285, 121 - batteryLevel, COLORED);
    uint8_t offsetBatteryLevel;
    if (batteryLevel < 10) offsetBatteryLevel = 8;
    else if (batteryLevel < 100) offsetBatteryLevel = 4;
    sprintf (temp, "%ld%%", batteryLevel);
    Paint_DrawStringAt (&paint, 265 + offsetBatteryLevel, 5, temp, &Font12, COLORED);

    EPD_SetFrameMemory (&epd, frame_buffer_p, 0, 0, Paint_GetWidth (&paint), Paint_GetHeight (&paint));
    EPD_DisplayFrame (&epd);
    EPD_Sleep (&epd);
  }
  else if (currentScreen == CHARTS)
  {
    float valueMax = 25.5, valueMin = 25.5, valueNow = 25.5;

    for (uint8_t i = 0; i < 12; i++)
    {
      Paint_DrawHorizontalLine (&paint, (10 + (i * 20)), 31, 7, COLORED);
      Paint_DrawHorizontalLine (&paint, (10 + (i * 20)), 71, 7, COLORED);
      Paint_DrawHorizontalLine (&paint, (10 + (i * 20)), 111, 7, COLORED);
      if (i != 0 && i != 6)
      {
	Paint_DrawVerticalLine (&paint, 8 + (i * 20), 115, 5, COLORED);
	Paint_DrawVerticalLine (&paint, 18 + (i * 20), 115, 5, COLORED);
      }
    }
    if (chartTypeSetPosition == TEMPERATURE_CHART)
    {
      sprintf (temp, "Temperature");
      Paint_DrawStringAt (&paint, 53, 5, temp, &Font24, COLORED);
      sprintf (temp, "%.1fMax", valueMax);
      Paint_DrawStringAt (&paint, 245, 27, temp, &Font12, COLORED);
      sprintf (temp, "%.1f'C", valueNow);
      Paint_DrawStringAt (&paint, 247, 67, temp, &Font12, COLORED);
      sprintf (temp, "%.1fMin", valueMin);
      Paint_DrawStringAt (&paint, 245, 107, temp, &Font12, COLORED);
    }
    else if (chartTypeSetPosition == HUMIDITY_CHART)
    {
      sprintf (temp, "Humidity");
      Paint_DrawStringAt (&paint, 75, 5, temp, &Font24, COLORED);
      sprintf (temp, "%02d%%Max", (int) valueMax);
      Paint_DrawStringAt (&paint, 245, 27, temp, &Font12, COLORED);
      sprintf (temp, "%02d%%Rh", (int) valueNow);
      Paint_DrawStringAt (&paint, 247, 67, temp, &Font12, COLORED);
      sprintf (temp, "%02d%%Min", (int) valueMin);
      Paint_DrawStringAt (&paint, 245, 107, temp, &Font12, COLORED);
    }
    else if (chartTypeSetPosition == PRESSURE_CHART)
    {
      sprintf (temp, "Pressure");
      Paint_DrawStringAt (&paint, 75, 5, temp, &Font24, COLORED);
      sprintf (temp, "%04dMax", (int) valueMax);
      Paint_DrawStringAt (&paint, 245, 27, temp, &Font12, COLORED);
      sprintf (temp, "%04dhPa", (int) valueNow);
      Paint_DrawStringAt (&paint, 245, 67, temp, &Font12, COLORED);
      sprintf (temp, "%04dMin", (int) valueMin);
      Paint_DrawStringAt (&paint, 245, 107, temp, &Font12, COLORED);
    }
    else if (chartTypeSetPosition == BATTERY_LEVEL_CHART)
    {
      sprintf (temp, "Battery level");
      Paint_DrawStringAt (&paint, 40, 5, temp, &Font24, COLORED);
      sprintf (temp, "100%%");
      Paint_DrawStringAt (&paint, 250, 27, temp, &Font12, COLORED);
      sprintf (temp, "%d%%", (int) valueNow);
      Paint_DrawStringAt (&paint, 252, 67, temp, &Font12, COLORED);
      sprintf (temp, "0%%");
      Paint_DrawStringAt (&paint, 255, 107, temp, &Font12, COLORED);
    }

    if (chartRangeSetPosition == RANGE_8H)
    {
      sprintf (temp, "8");
      Paint_DrawStringAt (&paint, 10, 113, temp, &Font12, COLORED);
      sprintf (temp, "4");
      Paint_DrawStringAt (&paint, 130, 113, temp, &Font12, COLORED);
    }
    else if (chartRangeSetPosition == RANGE_48H)
    {
      sprintf (temp, "48");
      Paint_DrawStringAt (&paint, 7, 113, temp, &Font12, COLORED);
      sprintf (temp, "24");
      Paint_DrawStringAt (&paint, 127, 113, temp, &Font12, COLORED);
    }
    else if (chartRangeSetPosition == RANGE_168H)
    {
      sprintf (temp, "168");
      Paint_DrawStringAt (&paint, 5, 113, temp, &Font12, COLORED);
      sprintf (temp, "84");
      Paint_DrawStringAt (&paint, 127, 113, temp, &Font12, COLORED);
    }
    if (chartSettingGroup == CHART_EDIT_TYPE_GROUP)
    {
      sprintf (temp, "\"");
      Paint_DrawStringAt (&paint, 260, 0, temp, &Font24, COLORED);
    }
    else if (chartSettingGroup == CHART_EDIT_RANGE_GROUP)
    {
      sprintf (temp, "\"");
      Paint_DrawStringAt (&paint, 5, 85, temp, &Font24, COLORED);
    }

    EPD_SetFrameMemory (&epd, frame_buffer_p, 0, 0, Paint_GetWidth (&paint), Paint_GetHeight (&paint));
    EPD_DisplayFrame (&epd);
    EPD_Sleep (&epd);
  }
  else if (currentScreen == LEDS)
  {

  }
}
void UI_RunOneMinuteProcess (void)
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
  return UI_FullUpdateCurrentScreen ();
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
  chartSettingGroup = CHART_EDIT_TYPE_GROUP;
  }
  else if (currentScreen == LEDS)
  {
  ledSettingGroup = LED_EDIT_SEQUENCE_GROUP;
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
    if (chartSettingGroup == CHART_EDIT_TYPE_GROUP)
    {
      chartTypeSetPosition = (chartTypeSetPosition % CHART_TYPE_POSITION_AMOUNT) + 1;
    }
    else if (chartSettingGroup == CHART_EDIT_RANGE_GROUP)
    {
      chartRangeSetPosition = (chartRangeSetPosition % CHART_RANGE_POSITION_AMOUNT) + 1;
    }
  }
  else if (currentScreen == LEDS)
  {
    if (ledSettingGroup == LED_EDIT_SEQUENCE_GROUP)
    {
      ledSequenceSetPosition = (ledSequenceSetPosition % LED_SEQUENCE_POSITION_AMOUNT) + 1;
    }
    else if (ledSettingGroup == LED_EDIT_DURATION_GROUP)
    {
      ledDurationSetPosition = (ledDurationSetPosition % LED_DURATION_POSITION_AMOUNT) + 1;
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

//    HAL_PWR_EnableBkUpAccess ();
//    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
//    regTemp = (regTemp & 0xFFFFFF00) | (chartTypeSetPosition << CHART_TYPE_REG_OFFSET);
//    HAL_RTCEx_BKUPWrite (&hrtc, BKP_UI_SETTINGS_REGISTER, regTemp);
//    HAL_PWR_DisableBkUpAccess ();

    chartSettingGroup = CHART_EDIT_RANGE_GROUP;
  }
  else if (currentScreen == LEDS)
  {

//    HAL_PWR_EnableBkUpAccess ();
//    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
//    regTemp = (regTemp & 0xFF00FFFF) | (ledSequenceSetPosition << LED_SEQUENCE_REG_OFFSET);
//    HAL_RTCEx_BKUPWrite (&hrtc, BKP_UI_SETTINGS_REGISTER, regTemp);
//    HAL_PWR_DisableBkUpAccess ();

    ledSettingGroup = LED_EDIT_DURATION_GROUP;
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

//    HAL_PWR_EnableBkUpAccess ();
//    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
//    regTemp = (regTemp & 0xFFFF00FF) | (chartRangeSetPosition << CHART_RANGE_REG_OFFSET);
//    HAL_RTCEx_BKUPWrite (&hrtc, BKP_UI_SETTINGS_REGISTER, regTemp);
//    HAL_PWR_DisableBkUpAccess ();

  chartSettingGroup = CHART_EDIT_NO_GROUP;
  }
  else if (currentScreen == LEDS)
  {

//    HAL_PWR_EnableBkUpAccess ();
//    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
//    regTemp = (regTemp & 0x00FFFFFF) | (ledDurationSetPosition << LED_DURATION_REG_OFFSET);
//    HAL_RTCEx_BKUPWrite (&hrtc, BKP_UI_SETTINGS_REGISTER, regTemp);
//    HAL_PWR_DisableBkUpAccess ();

  ledSettingGroup = LED_EDIT_NO_GROUP;
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

