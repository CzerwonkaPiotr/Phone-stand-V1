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
#include "led_ws2812b.h"


#define CHART_TYPE_POSITION_AMOUNT  4
#define CHART_RANGE_POSITION_AMOUNT  3
#define LED_SEQUENCE_POSITION_AMOUNT  4
#define LED_DURATION_POSITION_AMOUNT  4

#define BKP_UI_SETTINGS_REGISTER RTC_BKP_DR1
#define CHART_TYPE_REG_OFFSET 0
#define CHART_RANGE_REG_OFFSET 8
#define LED_SEQUENCE_REG_OFFSET 16
#define LED_DURATION_REG_OFFSET 24

#define BMP280_ADDRESS 0x76
#define BATTERY_FULL_VALUE 2550 //Value caltulated based on 2:1 voltage divider and maximum ADC read at 3,3V 12bit(4095)
#define BATTERY_EMPTY_VALUE 2120



static SCREEN_t currentScreen = CLOCK;

static CHART_TYPE_POSITION_t chartTypeSetPosition = TEMPERATURE_CHART; // 1 = text, 2 = Hum, 3 = Press, 4 = Batlevel
static CHART_RANGE_POSITION_t chartRangeSetPosition = RANGE_8H; // 1 = 8h, 2 = 48h, 3 = 168h
static CHART_SETTING_GROUP_POSITION_t chartSettingGroup = CHART_EDIT_NO_GROUP; // 0 - no group in edit, 1 - group nr.1 in edit, 2 - group nr.2 in edit

static LED_SEQUENCE_POSITION_t ledSequenceSetPosition = FADE_LED_SEQUENCE; // 1 = Fade, 2 = CRICLE, 3 = Smooth
static LED_DURATION_POSITION_t ledDurationSetPosition = DURATION_3S; // 1 = 3s, 2 = 5s, 3 = 10s, 4 = infinite
static LED_SETTING_GROUP_POSITION_t ledSettingGroup = LED_EDIT_NO_GROUP; // 0 - no group in edit, 1 - group nr.1 in edit, 2 - group nr.2 in edit

static BMP280_t Bmp280;
static float Temperature, Pressure, Humidity;
static uint8_t lastMinuteBMPRead = 61; // 61 because it will always read data when MCU was in standby mode

static int32_t batteryLevel;

static RTC_TimeTypeDef sTime =
{ 0 };
static RTC_DateTypeDef sDate =
{ 0 };
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
  HAL_PWR_EnableBkUpAccess ();//
  uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
  chartTypeSetPosition = (uint8_t) ((regTemp >> CHART_TYPE_REG_OFFSET) & 0xFF);
  chartRangeSetPosition = (uint8_t) ((regTemp >> CHART_RANGE_REG_OFFSET) & 0xFF);
  ledSequenceSetPosition = (uint8_t) ((regTemp >> LED_SEQUENCE_REG_OFFSET) & 0xFF);
  ledDurationSetPosition = (uint8_t) ((regTemp >> LED_DURATION_REG_OFFSET) & 0xFF);
//  HAL_PWR_DisableBkUpAccess ();

  if(chartTypeSetPosition == 0) chartTypeSetPosition = 1;
  if(chartRangeSetPosition == 0) chartRangeSetPosition = 1;
  if(ledSequenceSetPosition == 0) ledSequenceSetPosition = 1;
  if(ledDurationSetPosition == 0) ledDurationSetPosition = 1;

  ButtonInitKey (&userButton, WKUP_BUTTON_GPIO_Port, WKUP_BUTTON_Pin, 10, 1000, 2000);
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
static void GetBatteryLevel (void)
{
  float tempVar;
  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Channel = ADC_CHANNEL_9;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel (&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler ();
  }
  HAL_ADC_Start (&hadc1);

  if (HAL_ADC_PollForConversion (&hadc1, HAL_MAX_DELAY) == HAL_OK)
  {
    batteryLevel = HAL_ADC_GetValue (&hadc1);
  }
  HAL_ADC_Stop (&hadc1);

  tempVar = (float) ((batteryLevel - BATTERY_EMPTY_VALUE) / (float) (BATTERY_FULL_VALUE - BATTERY_EMPTY_VALUE));
  batteryLevel = tempVar * 100;
  if (batteryLevel > 100) batteryLevel = 100;
  if (batteryLevel < 0) batteryLevel = 0;
}
void UI_FullUpdateCurrentScreen (void)
{
  char text[128];
  Paint_Clear (&paint, UNCOLORED);

  if (currentScreen == CLOCK)
  {
    if (lastMinuteBMPRead != (RTC->TR & (RTC_TR_MNT_Msk | RTC_TR_MNU_Msk)) >> RTC_TR_MNU_Pos)// ensuring that this data is read no more than as once a minute
    {
      BMP280_SetMode (&Bmp280, BMP280_FORCEDMODE); // Measurement is made and the sensor goes to sleep
      HAL_Delay (50);
      BMP280_ReadSensorData (&Bmp280, &Pressure, &Temperature, &Humidity);
      GetBatteryLevel ();
      lastMinuteBMPRead = (RTC->TR & (RTC_TR_MNT_Msk | RTC_TR_MNU_Msk)) >> RTC_TR_MNU_Pos;
    }

    //Draw Time
    RTC_TimeTypeDef sTime =
    { 0 };
    HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
    RTC_DateTypeDef sDate =
    { 0 };
    HAL_RTC_GetDate (&hrtc, &sDate, RTC_FORMAT_BIN);
    sprintf (text, "%02d", sTime.Hours);
    Paint_DrawStringAt (&paint, 8, 5, text, &Font64, COLORED);
    sprintf (text, "%02d", sTime.Minutes);
    Paint_DrawStringAt (&paint, 142, 5, text, &Font64, COLORED);
    sprintf (text, "%s", weekDayName[sDate.WeekDay]);
    Paint_DrawStringAt (&paint, 11, 97, text, &Font24, COLORED);
    sprintf (text, "%02d/%02d/%02d", sDate.Date, sDate.Month, sDate.Year);
    Paint_DrawStringAt (&paint, 81, 91, text, &Font16, COLORED);
    Paint_DrawFilledCircle (&paint, 131, 22, 5, COLORED); // Colon
    Paint_DrawFilledCircle (&paint, 131, 68, 5, COLORED); // Colon

    sprintf (text, "%04dhPa", (int) Pressure);
    Paint_DrawStringAt (&paint, 85, 110, text, &Font16, COLORED);
    sprintf (text, "%.1f'C", Temperature);
    Paint_DrawStringAt (&paint, 183, 91, text, &Font16, COLORED);
    sprintf (text, "%02d%%Rh", (int) Humidity);
    Paint_DrawStringAt (&paint, 187, 110, text, &Font16, COLORED);

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
    uint8_t offsetBatteryLevel = 0;
    if (batteryLevel < 10) offsetBatteryLevel = 8;
    else if (batteryLevel < 100) offsetBatteryLevel = 4;
    sprintf (text, "%ld%%", batteryLevel);
    Paint_DrawStringAt (&paint, 265 + offsetBatteryLevel, 5, text, &Font12, COLORED);

    EPD_SetFrameMemory (&epd, frame_buffer_p, 0, 0, Paint_GetWidth (&paint), Paint_GetHeight (&paint));
    EPD_DisplayFrame (&epd);
    EPD_Sleep (&epd);
  }
  else if (currentScreen == CHARTS)
  {
    float valueMax = 25.5, valueMin = 25.5, valueNow = 25.5;// TODO

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
      sprintf (text, "Temperature");
      Paint_DrawStringAt (&paint, 53, 5, text, &Font24, COLORED);
      sprintf (text, "%.1fMax", valueMax);
      Paint_DrawStringAt (&paint, 245, 27, text, &Font12, COLORED);
      sprintf (text, "%.1f'C", valueNow);
      Paint_DrawStringAt (&paint, 247, 67, text, &Font12, COLORED);
      sprintf (text, "%.1fMin", valueMin);
      Paint_DrawStringAt (&paint, 245, 107, text, &Font12, COLORED);
    }
    else if (chartTypeSetPosition == HUMIDITY_CHART)
    {
      sprintf (text, "Humidity");
      Paint_DrawStringAt (&paint, 75, 5, text, &Font24, COLORED);
      sprintf (text, "%02d%%Max", (int) valueMax);
      Paint_DrawStringAt (&paint, 245, 27, text, &Font12, COLORED);
      sprintf (text, "%02d%%Rh", (int) valueNow);
      Paint_DrawStringAt (&paint, 247, 67, text, &Font12, COLORED);
      sprintf (text, "%02d%%Min", (int) valueMin);
      Paint_DrawStringAt (&paint, 245, 107, text, &Font12, COLORED);
    }
    else if (chartTypeSetPosition == PRESSURE_CHART)
    {
      sprintf (text, "Pressure");
      Paint_DrawStringAt (&paint, 75, 5, text, &Font24, COLORED);
      sprintf (text, "%04dMax", (int) valueMax);
      Paint_DrawStringAt (&paint, 245, 27, text, &Font12, COLORED);
      sprintf (text, "%04dhPa", (int) valueNow);
      Paint_DrawStringAt (&paint, 245, 67, text, &Font12, COLORED);
      sprintf (text, "%04dMin", (int) valueMin);
      Paint_DrawStringAt (&paint, 245, 107, text, &Font12, COLORED);
    }
    else if (chartTypeSetPosition == BATTERY_LEVEL_CHART)
    {
      sprintf (text, "Battery level");
      Paint_DrawStringAt (&paint, 30, 5, text, &Font24, COLORED);
      sprintf (text, "100%%");
      Paint_DrawStringAt (&paint, 260, 27, text, &Font12, COLORED);
      sprintf (text, "%d%%", (int) valueNow);
      Paint_DrawStringAt (&paint, 262, 67, text, &Font12, COLORED);
      sprintf (text, "0%%");
      Paint_DrawStringAt (&paint, 265, 107, text, &Font12, COLORED);
    }

    if (chartRangeSetPosition == RANGE_8H)
    {
      sprintf (text, "8");
      Paint_DrawStringAt (&paint, 10, 113, text, &Font12, COLORED);
      sprintf (text, "4");
      Paint_DrawStringAt (&paint, 130, 113, text, &Font12, COLORED);
    }
    else if (chartRangeSetPosition == RANGE_48H)
    {
      sprintf (text, "48");
      Paint_DrawStringAt (&paint, 7, 113, text, &Font12, COLORED);
      sprintf (text, "24");
      Paint_DrawStringAt (&paint, 127, 113, text, &Font12, COLORED);
    }
    else if (chartRangeSetPosition == RANGE_168H)
    {
      sprintf (text, "168");
      Paint_DrawStringAt (&paint, 5, 113, text, &Font12, COLORED);
      sprintf (text, "84");
      Paint_DrawStringAt (&paint, 127, 113, text, &Font12, COLORED);
    }
    if (chartSettingGroup == CHART_EDIT_TYPE_GROUP)
    {
      sprintf (text, "\"");
      Paint_DrawStringAt (&paint, 260, 0, text, &Font24, COLORED);
    }
    else if (chartSettingGroup == CHART_EDIT_RANGE_GROUP)
    {
      sprintf (text, "\"");
      Paint_DrawStringAt (&paint, 5, 85, text, &Font24, COLORED);
    }

    EPD_SetFrameMemory (&epd, frame_buffer_p, 0, 0, Paint_GetWidth (&paint), Paint_GetHeight (&paint));
    EPD_DisplayFrame (&epd);
    EPD_Sleep (&epd);
  }
  else if (currentScreen == LEDS)
  {
    Paint_DrawHorizontalLine (&paint, 5, SCREEN_HEIGHT / 2, SCREEN_WIDTH -6, COLORED);

    sprintf (text, "LED Mode");
    Paint_DrawStringAt (&paint, 87, 5, text, &Font24, COLORED);
    sprintf (text, "LED Duration");
    Paint_DrawStringAt (&paint, 50, (SCREEN_HEIGHT/2) +4, text, &Font24, COLORED);

    sprintf (text, "FADE");
    Paint_DrawStringAt (&paint, 20, 38, text, &Font16, COLORED);
    sprintf (text, "CIRCLE");
    Paint_DrawStringAt (&paint, 85, 38, text, &Font16, COLORED);
    sprintf (text, "SMOOTH");
    Paint_DrawStringAt (&paint, 170, 38, text, &Font16, COLORED);
    sprintf (text, "OFF");
    Paint_DrawStringAt (&paint, 255, 38, text, &Font16, COLORED);

    sprintf (text, "3s");
    Paint_DrawStringAt (&paint, 32, 101, text, &Font16, COLORED);
    sprintf (text, "5s");
    Paint_DrawStringAt (&paint, 103, 101, text, &Font16, COLORED);
    sprintf (text, "10s");
    Paint_DrawStringAt (&paint, 177, 101, text, &Font16, COLORED);
    sprintf (text, "INF");
    Paint_DrawStringAt (&paint, 250, 101, text, &Font16, COLORED);


    uint16_t offsetX, offsetY;

    offsetY = 34;
    if(ledSequenceSetPosition == FADE_LED_SEQUENCE) offsetX = 2;
    else if(ledSequenceSetPosition == CIRCLE_LED_SEQUENCE) offsetX = 67;
    else if (ledSequenceSetPosition == SMOOTH_LED_SEQUENCE) offsetX = 153;
    else if (ledSequenceSetPosition == OFF_LED_SEQUENCE) offsetX = 237;
    sprintf (text, "-");
        Paint_DrawStringAt (&paint, offsetX, offsetY, text, &Font24, COLORED);
        sprintf (text, ">");
        Paint_DrawStringAt (&paint, offsetX+3, offsetY, text, &Font24, COLORED);

    offsetY = 97;
    if (ledDurationSetPosition == DURATION_3S) offsetX = 15;
    else if (ledDurationSetPosition == DURATION_5S) offsetX = 86;
    else if (ledDurationSetPosition == DURATION_10S) offsetX = 160;
    else if (ledDurationSetPosition == DURATION_INFINITE) offsetX = 233;
    sprintf (text, "-");
        Paint_DrawStringAt (&paint, offsetX, offsetY, text, &Font24, COLORED);
        sprintf (text, ">");
        Paint_DrawStringAt (&paint, offsetX+3, offsetY, text, &Font24, COLORED);

    if (ledSettingGroup == LED_EDIT_SEQUENCE_GROUP)
        {
          sprintf (text, "\"");
          Paint_DrawStringAt (&paint, 270, 5, text, &Font24, COLORED);
        }
        else if (ledSettingGroup == LED_EDIT_DURATION_GROUP)
        {
          sprintf (text, "\"");
          Paint_DrawStringAt (&paint, 270, 64, text, &Font24, COLORED);
        }

    EPD_SetFrameMemory (&epd, frame_buffer_p, 0, 0, Paint_GetWidth (&paint), Paint_GetHeight (&paint));
    EPD_DisplayFrame (&epd);
    EPD_Sleep (&epd);
  }
}
void UI_RunOneMinuteProcess (void)
{
  HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate (&hrtc, &sDate, RTC_FORMAT_BIN);
  LED_InitRunProcess(ledSequenceSetPosition, ledDurationSetPosition, sTime.Minutes % 10);
  if (currentScreen == CLOCK)
  {
    if ((sTime.Minutes % 10) == 0)
    {
      Paint_Clear (&paint, UNCOLORED);
      EPD_SetFrameMemory (&epd, frame_buffer_p, 0, 0, Paint_GetWidth (&paint), Paint_GetHeight (&paint));
      EPD_DisplayFrame (&epd);
      // TODO zapisz dane z bme i baterii do Flash SPI
    }
    UI_FullUpdateCurrentScreen ();
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
    Paint_Clear (&paint, UNCOLORED);
    EPD_SetFrameMemory (&epd, frame_buffer_p, 0, 0, Paint_GetWidth (&paint), Paint_GetHeight (&paint));
    EPD_DisplayFrame (&epd);
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
    chartSettingGroup = CHART_EDIT_TYPE_GROUP;
  }
  else if (currentScreen == LEDS)
  {
    ledSettingGroup = LED_EDIT_SEQUENCE_GROUP;
    LED_InitRunProcess(ledSequenceSetPosition, ledDurationSetPosition, sTime.Minutes % 10);
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
      chartTypeSetPosition = (chartTypeSetPosition % CHART_TYPE_POSITION_AMOUNT + 1);
    }
    else if (chartSettingGroup == CHART_EDIT_RANGE_GROUP)
    {
      chartRangeSetPosition = (chartRangeSetPosition % CHART_RANGE_POSITION_AMOUNT + 1);
    }
  }
  else if (currentScreen == LEDS)
  {
    if (ledSettingGroup == LED_EDIT_SEQUENCE_GROUP)
    {
      ledSequenceSetPosition = (ledSequenceSetPosition % LED_SEQUENCE_POSITION_AMOUNT + 1);
    }
    else if (ledSettingGroup == LED_EDIT_DURATION_GROUP)
    {
      ledDurationSetPosition = (ledDurationSetPosition % LED_DURATION_POSITION_AMOUNT + 1);
    }
    LED_InitRunProcess(ledSequenceSetPosition, ledDurationSetPosition, sTime.Minutes % 10);
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
    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
    regTemp = (regTemp & 0xFFFFFF00) | (chartTypeSetPosition << CHART_TYPE_REG_OFFSET);
    HAL_RTCEx_BKUPWrite (&hrtc, BKP_UI_SETTINGS_REGISTER, regTemp);

    chartSettingGroup = CHART_EDIT_RANGE_GROUP;
  }
  else if (currentScreen == LEDS)
  {
    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
    regTemp = (regTemp & 0xFF00FFFF) | (ledSequenceSetPosition << LED_SEQUENCE_REG_OFFSET);
    HAL_RTCEx_BKUPWrite (&hrtc, BKP_UI_SETTINGS_REGISTER, regTemp);

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
    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
    regTemp = (regTemp & 0xFFFF00FF) | (chartRangeSetPosition << CHART_RANGE_REG_OFFSET);
    HAL_RTCEx_BKUPWrite (&hrtc, BKP_UI_SETTINGS_REGISTER, regTemp);

  chartSettingGroup = CHART_EDIT_NO_GROUP;
  }
  else if (currentScreen == LEDS)
  {
    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
    regTemp = (regTemp & 0x00FFFFFF) | (ledDurationSetPosition << LED_DURATION_REG_OFFSET);
    HAL_RTCEx_BKUPWrite (&hrtc, BKP_UI_SETTINGS_REGISTER, regTemp);

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
  chartSettingGroup = CHART_EDIT_NO_GROUP;
  ledSettingGroup = LED_EDIT_NO_GROUP;
  currentScreen = CLOCK;
  UI_FullUpdateCurrentScreen ();
}
