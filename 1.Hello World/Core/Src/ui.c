/*
 * ui.c
 *
 *  Created on: Oct 12, 2024
 *      Author: piotr
 *
 *  This file manages the user interface (UI) for a system that includes:
 *    - An e-paper display (EPD) for visual output
 *    - A push button for user input
 *    - Battery and sensor readings (BMP280)
 *    - LED sequences (WS2812B/NeoPixel)
 *    - Real-time clock (RTC) for scheduling and timekeeping
 *
 *  Main responsibilities of this file:
 *    1. Displaying time, sensor data, and charts on the e-paper display
 *    2. Handling button presses to navigate between screens and manage settings
 *    3. Storing and retrieving settings in RTC backup registers
 *    4. Orchestrating LED patterns (e.g., fade, circle) with user-configurable durations
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
#include "charts.h"

/*
 * Macros defining sizes and offsets for different settings and chart parameters
 */
#define CHART_TYPE_POSITION_AMOUNT  4
#define CHART_RANGE_POSITION_AMOUNT  3
#define LED_SEQUENCE_POSITION_AMOUNT  4
#define LED_DURATION_POSITION_AMOUNT  4

/*
 * Using the RTC backup register to persist UI settings across power cycles.
 * The 32-bit backup register is partitioned into 4 offsets:
 *   - chart type:   bits [7:0]
 *   - chart range:  bits [15:8]
 *   - LED sequence: bits [23:16]
 *   - LED duration: bits [31:24]
 */
#define BKP_UI_SETTINGS_REGISTER RTC_BKP_DR1
#define CHART_TYPE_REG_OFFSET 0
#define CHART_RANGE_REG_OFFSET 8
#define LED_SEQUENCE_REG_OFFSET 16
#define LED_DURATION_REG_OFFSET 24

/*
 * Various thresholds and constants for sensors and battery.
 */
#define BMP280_ADDRESS 0x76
#define BATTERY_FULL_VALUE 2550 //Value caltulated based on 2:1 voltage divider and maximum ADC read at 3,3V 12bit(4095)
#define BATTERY_EMPTY_VALUE 2120

/*
 * Enumerations and global/static variables to maintain the current UI state
 */
static SCREEN_t currentScreen = CLOCK;

/* Chart-related UI states. */
static CHART_TYPE_POSITION_t chartTypeSetPosition = TEMPERATURE_CHART; // 1 = text, 2 = Hum, 3 = Press, 4 = Batlevel
static CHART_RANGE_POSITION_t chartRangeSetPosition = RANGE_8H; // 1 = 8h, 2 = 48h, 3 = 168h
static CHART_SETTING_GROUP_POSITION_t chartSettingGroup = CHART_EDIT_NO_GROUP; // 0 - no group in edit, 1 - group nr.1 in edit, 2 - group nr.2 in edit

/* LED-related UI states. */
static LED_SEQUENCE_POSITION_t ledSequenceSetPosition = FADE_LED_SEQUENCE; // 1 = Fade, 2 = CRICLE, 3 = Smooth
static LED_DURATION_POSITION_t ledDurationSetPosition = DURATION_3S; // 1 = 3s, 2 = 5s, 3 = 10s, 4 = infinite
static LED_SETTING_GROUP_POSITION_t ledSettingGroup = LED_EDIT_NO_GROUP; // 0 - no group in edit, 1 - group nr.1 in edit, 2 - group nr.2 in edit

/* Instances and data for sensors, e-paper, etc. */
static BMP280_t Bmp280;
static float Temperature, Pressure, Humidity;
static uint8_t lastMinuteBMPRead = 61; // 61 because it will always read data when MCU was in standby mode
static float batteryLevel;

/* RTC time/date structures updated periodically */
static RTC_TimeTypeDef sTime =
{ 0 };
static RTC_DateTypeDef sDate =
{ 0 };

/* Weekday names displayed on screen (Monday=0, Sunday=6) */
const char weekDayName[7][4] =
{ "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };

/* Button structure for handling press, long press, and repeat callbacks. */
static TButton userButton;

/* E-Paper display structures and frame buffer. */
static Paint paint;
static EPD epd;
static unsigned char frame_buffer[EPD_WIDTH * EPD_HEIGHT / 8];
static unsigned char *frame_buffer_p = frame_buffer;

/**
 * @brief Initializes the UI module:
 *        - Configures button callbacks
 *        - Reads UI settings from RTC backup register
 *        - Initializes the e-paper display, BMP280 sensor, and flash memory
 *        - Clears or draws initial display content
 */
void UI_Init (void)
{
  // Register button callbacks for short press, long press, and repeat
  ButtonRegisterPressCallback (&userButton, UI_NextScreenCallback);
  ButtonRegisterLongPressCallback (&userButton, UI_EnterSettingsCallback);
  ButtonRegisterRepeatCallback(&userButton, UI_ResetDevice);

  // Read settings from the RTC backup register
  HAL_PWR_EnableBkUpAccess ();//
  uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
  chartTypeSetPosition = (uint8_t) ((regTemp >> CHART_TYPE_REG_OFFSET) & 0xFF);
  chartRangeSetPosition = (uint8_t) ((regTemp >> CHART_RANGE_REG_OFFSET) & 0xFF);
  ledSequenceSetPosition = (uint8_t) ((regTemp >> LED_SEQUENCE_REG_OFFSET) & 0xFF);
  ledDurationSetPosition = (uint8_t) ((regTemp >> LED_DURATION_REG_OFFSET) & 0xFF);
//  HAL_PWR_DisableBkUpAccess (); // leave commented

  /*
   * Set default values if settings read were zero (indicating they might be uninitialized).
   * Ranges: 1..CHART_TYPE_POSITION_AMOUNT, etc.
   */
  if(chartTypeSetPosition == 0) chartTypeSetPosition = 1;
  if(chartRangeSetPosition == 0) chartRangeSetPosition = 1;
  if(ledSequenceSetPosition == 0) ledSequenceSetPosition = 1;
  if(ledDurationSetPosition == 0) ledDurationSetPosition = 1;

  // Initialize the button hardware (debounce times, etc.)
  ButtonInitKey (&userButton, WKUP_BUTTON_GPIO_Port, WKUP_BUTTON_Pin, 25, 1000, 8000);

  // Initialize the e-paper display driver
  if (EPD_Init (&epd, lut_full_update) != 0)
  {
    Error_Handler ();
  }

  // Initialize the BMP280 sensor
  if (BMP280_Init (&Bmp280, &hi2c1, BMP280_ADDRESS))
  {
    Error_Handler ();
  }

  // Initialize flash memory (for logging chart data, etc.)
  if (!Flash_Init ())
  {
    Error_Handler ();
  }

  // Set up the drawing context
  Paint_Init (&paint, frame_buffer_p, epd.width, epd.height);

  // Read the current RTC time and date
  HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate (&hrtc, &sDate, RTC_FORMAT_BIN);
}

/**
 * @brief Reads and calculates the current battery level as a percentage [0..100].
 *        Uses ADC channel 9 with a 2:1 voltage divider and references global thresholds.
 */
static void GetBatteryLevel (void)
{
  uint32_t tempVar;
  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Channel = ADC_CHANNEL_9;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;

  // Configure and start the ADC conversion
  if (HAL_ADC_ConfigChannel (&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler ();
  }
  HAL_ADC_Start (&hadc1);

  // Wait for ADC to complete and retrieve the result
  if (HAL_ADC_PollForConversion (&hadc1, HAL_MAX_DELAY) == HAL_OK)
  {
    tempVar = HAL_ADC_GetValue (&hadc1);
  }
  HAL_ADC_Stop (&hadc1);

  // Map the raw ADC value into a [0..100]% range
  batteryLevel = (float) ((tempVar - BATTERY_EMPTY_VALUE) / (float) (BATTERY_FULL_VALUE - BATTERY_EMPTY_VALUE));
  batteryLevel = batteryLevel * 100;

  // Clamp values to [0..100]
  if (batteryLevel > 100) batteryLevel = 100;
  if (batteryLevel < 0) batteryLevel = 0;
}

/**
 * @brief Fully updates the content of the e-paper display based on the current screen
 *        (CLOCK, CHARTS, or LEDS). Reads sensor data if necessary, then draws the UI.
 */
void UI_FullUpdateCurrentScreen (void)
{
  char text[128];
  Paint_Clear (&paint, UNCOLORED);

  /*
   * Read BMP280 sensor data and battery level no more than once per minute
   * (as indicated by lastMinuteBMPRead).
   */
  if (lastMinuteBMPRead != (RTC->TR & (RTC_TR_MNT_Msk | RTC_TR_MNU_Msk)) >> RTC_TR_MNU_Pos) // ensuring that this data is read no more than as once a minute
  {
    BMP280_SetMode (&Bmp280, BMP280_FORCEDMODE); // Trigger a single forced measurement
    HAL_Delay (50);				// Wait for the measurement to complete
    BMP280_ReadSensorData (&Bmp280, &Pressure, &Temperature, &Humidity);
    GetBatteryLevel ();
    lastMinuteBMPRead = (RTC->TR & (RTC_TR_MNT_Msk | RTC_TR_MNU_Msk)) >> RTC_TR_MNU_Pos;
  }

  /*
   * Screen #1: CLOCK view
   *   - Displays HH:MM, date, weekday
   *   - Also prints temperature, pressure, humidity, and battery level
   */
  if (currentScreen == CLOCK)
  {
    sprintf (text, "%02d", sTime.Hours);
    Paint_DrawStringAt (&paint, 8, 5, text, &Font64, COLORED);
    sprintf (text, "%02d", sTime.Minutes);
    Paint_DrawStringAt (&paint, 142, 5, text, &Font64, COLORED);
    sprintf (text, "%s", weekDayName[sDate.WeekDay]);
    Paint_DrawStringAt (&paint, 11, 97, text, &Font24, COLORED);
    sprintf (text, "%02d/%02d/%02d", sDate.Date, sDate.Month, sDate.Year);
    Paint_DrawStringAt (&paint, 81, 91, text, &Font16, COLORED);

    // Draw the two dots (colon) between hour and minute
    Paint_DrawFilledCircle (&paint, 131, 22, 5, COLORED); // Colon
    Paint_DrawFilledCircle (&paint, 131, 68, 5, COLORED); // Colon

    // Pressure, Temperature, Humidity
    sprintf (text, "%dhPa", (int) Pressure);
    if (Pressure > 999) Paint_DrawStringAt (&paint, 85, 110, text, &Font16, COLORED);
    else Paint_DrawStringAt (&paint, 92, 110, text, &Font16, COLORED);
    sprintf (text, "%.1f'C", Temperature);
    Paint_DrawStringAt (&paint, 183, 91, text, &Font16, COLORED);
    sprintf (text, "%02d%%Rh", (int) Humidity);
    Paint_DrawStringAt (&paint, 187, 110, text, &Font16, COLORED);

    // Draw some UI separators
    Paint_DrawVerticalLine (&paint, 260, 8, 112, COLORED);
    Paint_DrawHorizontalLine (&paint, 7, 86, 246, COLORED);
    Paint_DrawVerticalLine (&paint, 73, 91, 29, COLORED);
    Paint_DrawVerticalLine (&paint, 174, 91, 29, COLORED);

    // Battery level gauge
    Paint_DrawRectangle (&paint, 268, 19, 288, 122, COLORED);
    Paint_DrawRectangle (&paint, 269, 18, 287, 123, COLORED);
    Paint_DrawRectangle (&paint, 270, 20, 286, 121, COLORED);

    // Little battery cap lines
    Paint_DrawHorizontalLine (&paint, 275, 16, 7, COLORED);
    Paint_DrawHorizontalLine (&paint, 274, 17, 9, COLORED);

    // Fill battery level rectangle from bottom up
    Paint_DrawFilledRectangle (&paint, 271, 121, 285, 121 - batteryLevel, COLORED);
    uint8_t offsetBatteryLevel = 0;
    if (batteryLevel < 10) offsetBatteryLevel = 8;
    else if (batteryLevel < 100) offsetBatteryLevel = 4;
    sprintf (text, "%02d%%", (int)batteryLevel);
    Paint_DrawStringAt (&paint, 265 + offsetBatteryLevel, 5, text, &Font12, COLORED);
  }

  /*
   * Screen #2: CHARTS view
   *   - Displays historical data for Temperature/Humidity/Pressure/Battery, etc.
   *   - If no valid year is set (GPS fix not acquired), show "NO GPS FIX"
   *   - Otherwise draws the chart axes and calls CHARTS_DrawCharts()
   */
  else if (currentScreen == CHARTS)
  {
    if (sDate.Year == 0)
    {
      sprintf (text, "NO GPS FIX");
      Paint_DrawStringAt (&paint, ((SCREEN_WIDTH / 2) - (10 * 17 / 2)), ((SCREEN_HEIGHT / 2) - (24 / 2)), text, &Font24, COLORED);
    }
    else
    {
      // Draw chart axes and small tick marks
      for (uint8_t i = 0; i < 12; i++)
      {
	Paint_DrawHorizontalLine (&paint, (4 + (i * 20)), 31, 7, COLORED);
	Paint_DrawHorizontalLine (&paint, (4 + (i * 20)), 71, 7, COLORED);
	Paint_DrawHorizontalLine (&paint, (4 + (i * 20)), 111, 7, COLORED);
	if (i != 0 && i != 6)
	{
	  Paint_DrawVerticalLine (&paint, 2 + (i * 20), 115, 5, COLORED);
	  Paint_DrawVerticalLine (&paint, 12 + (i * 20), 115, 5, COLORED);
	}
      }

      // Draw range labels depending on chartRangeSetPosition
      if (chartRangeSetPosition == RANGE_8H)
      {
	sprintf (text, "8");
	Paint_DrawStringAt (&paint, 4, 113, text, &Font12, COLORED);
	sprintf (text, "4");
	Paint_DrawStringAt (&paint, 124, 113, text, &Font12, COLORED);
      }
      else if (chartRangeSetPosition == RANGE_40H)
      {
	sprintf (text, "40");
	Paint_DrawStringAt (&paint, 2, 113, text, &Font12, COLORED);
	sprintf (text, "20");
	Paint_DrawStringAt (&paint, 121, 113, text, &Font12, COLORED);
      }
      else if (chartRangeSetPosition == RANGE_160H)
      {
	sprintf (text, "160");
	Paint_DrawStringAt (&paint, 0, 113, text, &Font12, COLORED);
	sprintf (text, "80");
	Paint_DrawStringAt (&paint, 121, 113, text, &Font12, COLORED);
      }

      // Indicate which parameter (type or range) is currently being edited
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

      // Draw the actual charts for the chosen type & range
      CHARTS_DrawCharts (&paint, chartTypeSetPosition, chartRangeSetPosition, sTime, sDate);
    }
  }


  /*
   * Screen #3: LEDS view
   *   - Allows the user to select LED sequence type (Fade, Circle, Smooth, Off)
   *     and the duration (3s, 5s, 10s, infinite)
   */
    else if (currentScreen == LEDS)
    {
      // Horizontal line dividing the screen
      Paint_DrawHorizontalLine (&paint, 5, SCREEN_HEIGHT / 2, SCREEN_WIDTH - 6, COLORED);

      sprintf (text, "LED Mode");
      Paint_DrawStringAt (&paint, 87, 5, text, &Font24, COLORED);
      sprintf (text, "LED Duration");
      Paint_DrawStringAt (&paint, 50, (SCREEN_HEIGHT / 2) + 4, text, &Font24, COLORED);

      // LED sequence names
      sprintf (text, "FADE");
      Paint_DrawStringAt (&paint, 20, 38, text, &Font16, COLORED);
      sprintf (text, "CIRCLE");
      Paint_DrawStringAt (&paint, 85, 38, text, &Font16, COLORED);
      sprintf (text, "SMOOTH");
      Paint_DrawStringAt (&paint, 170, 38, text, &Font16, COLORED);
      sprintf (text, "OFF");
      Paint_DrawStringAt (&paint, 255, 38, text, &Font16, COLORED);

      // LED duration names
      sprintf (text, "3s");
      Paint_DrawStringAt (&paint, 32, 101, text, &Font16, COLORED);
      sprintf (text, "5s");
      Paint_DrawStringAt (&paint, 103, 101, text, &Font16, COLORED);
      sprintf (text, "10s");
      Paint_DrawStringAt (&paint, 177, 101, text, &Font16, COLORED);
      sprintf (text, "INF");
      Paint_DrawStringAt (&paint, 250, 101, text, &Font16, COLORED);

      // Draw arrows indicating the currently selected sequence/duration
      uint16_t offsetX, offsetY;

      // LED sequence arrow
      offsetY = 34;
      if (ledSequenceSetPosition == FADE_LED_SEQUENCE) offsetX = 2;
      else if (ledSequenceSetPosition == CIRCLE_LED_SEQUENCE) offsetX = 67;
      else if (ledSequenceSetPosition == SMOOTH_LED_SEQUENCE) offsetX = 153;
      else if (ledSequenceSetPosition == OFF_LED_SEQUENCE) offsetX = 237;
      sprintf (text, "-");
      Paint_DrawStringAt (&paint, offsetX, offsetY, text, &Font24, COLORED);
      sprintf (text, ">");
      Paint_DrawStringAt (&paint, offsetX + 3, offsetY, text, &Font24, COLORED);

      // LED duration arrow
      offsetY = 97;
      if (ledDurationSetPosition == DURATION_3S) offsetX = 15;
      else if (ledDurationSetPosition == DURATION_5S) offsetX = 86;
      else if (ledDurationSetPosition == DURATION_10S) offsetX = 160;
      else if (ledDurationSetPosition == DURATION_INFINITE) offsetX = 233;
      sprintf (text, "-");
      Paint_DrawStringAt (&paint, offsetX, offsetY, text, &Font24, COLORED);
      sprintf (text, ">");
      Paint_DrawStringAt (&paint, offsetX + 3, offsetY, text, &Font24, COLORED);

      // Indicate if we are editing sequence or duration
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
    }

  // Commit the drawn frame buffer to the e-paper and then put it to sleep
    EPD_SetFrameMemory (&epd, frame_buffer_p, 0, 0, Paint_GetWidth (&paint), Paint_GetHeight (&paint));
    EPD_DisplayFrame (&epd);
    EPD_Sleep (&epd);
  }

/**
 * @brief Called once every minute, updates RTC time/date, runs an LED sequence,
 *        and logs chart data if certain conditions are met.
 */
void UI_RunOneMinuteProcess (void)
{

  // Update the RTC structures
  HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate (&hrtc, &sDate, RTC_FORMAT_BIN);

  // Initialize or restart the LED sequence
  LED_InitRunProcess(ledSequenceSetPosition, ledDurationSetPosition, sTime.Minutes % 10);

  // Update the display if we're on the CLOCK screen, or clear it periodically
  if (currentScreen == CLOCK)
  {
    // Every 10 minutes, do a partial clear to avoid e-paper ghosting
    if ((sTime.Minutes % 10) == 0)
    {
      Paint_Clear (&paint, UNCOLORED);
      EPD_SetFrameMemory (&epd, frame_buffer_p, 0, 0, Paint_GetWidth (&paint), Paint_GetHeight (&paint));
      EPD_DisplayFrame (&epd);
    }

    // Refresh the display with current data
    UI_FullUpdateCurrentScreen ();
    if ((sTime.Minutes % 10) == 0 && sDate.Year != 0)
    {
      CHARTS_t data;
      data.epoch_seconds = RTC_ToEpochSeconds(&sTime, &sDate);
      data.temperature = Temperature;
      data.humidity = Humidity;
      data.pressure = Pressure;
      data.battery_level = (uint8_t)batteryLevel;

      // Save the sensor readings to flash for charting
      CHARTS_SaveData(&data);
    }
  }

  // Set alarm for next GPS check (Alarm B)
  SetGPSAlarmB ();
}

/**
 * @brief Periodically called in the main loop to process button events.
 *
 * @param[in] UserMenuFirstUse  If 1, forces a screen update on first use.
 */
void UI_RunMenuProcess (uint8_t UserMenuFirstUse)
{

  // Process state machine for button
  ButtonTask (&userButton); // Machine state task for button

  // If this is the first time entering the UI menu and mcu was in standby mode, switch screen
  if (UserMenuFirstUse == 1)
  {
    UI_NextScreenCallback ();
  }
}

/**
 * @brief Callback for a short button press, cycles through available screens:
 *        CLOCK -> CHARTS -> LEDS -> back to CLOCK.
 */
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

/**
 * @brief Callback for a long button press when not in settings mode:
 *        - Enters the settings mode for the currently displayed screen.
 */
void UI_EnterSettingsCallback (void)
{
  if (currentScreen == CLOCK)
  {
    // No settings to edit on CLOCK screen
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

  // Change button callbacks to navigate or exit settings
  ButtonRegisterPressCallback (&userButton, UI_NextPositionOnMenuCallback);
  ButtonRegisterLongPressCallback (&userButton, UI_NextSettingsGroupCallback);

  UI_FullUpdateCurrentScreen ();
}

/**
 * @brief Callback for a short button press while in settings mode,
 *        moves to the next parameter (like next chart type or next LED mode).
 */
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
      // Cycle through possible chart types
      chartTypeSetPosition = (chartTypeSetPosition % CHART_TYPE_POSITION_AMOUNT + 1);
    }
    else if (chartSettingGroup == CHART_EDIT_RANGE_GROUP)
    {
      // Cycle through possible chart ranges
      chartRangeSetPosition = (chartRangeSetPosition % CHART_RANGE_POSITION_AMOUNT + 1);
    }
  }
  else if (currentScreen == LEDS)
  {
    if (ledSettingGroup == LED_EDIT_SEQUENCE_GROUP)
    {
      // Cycle through LED sequence types
      ledSequenceSetPosition = (ledSequenceSetPosition % LED_SEQUENCE_POSITION_AMOUNT + 1);
    }
    else if (ledSettingGroup == LED_EDIT_DURATION_GROUP)
    {
      // Cycle through LED durations
      ledDurationSetPosition = (ledDurationSetPosition % LED_DURATION_POSITION_AMOUNT + 1);
    }
    LED_InitRunProcess(ledSequenceSetPosition, ledDurationSetPosition, sTime.Minutes % 10);
  }
  UI_FullUpdateCurrentScreen ();
}

/**
 * @brief Callback for a long button press in settings mode, advances to
 *        the next group of settings (e.g., from chart type to chart range).
 */
void UI_NextSettingsGroupCallback (void)
{
  if (currentScreen == CLOCK)
  {
    // no settings here
  }
  else if (currentScreen == CHARTS)
  {
    // Save the current chart type to backup register
    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
    regTemp = (regTemp & 0xFFFFFF00) | (chartTypeSetPosition << CHART_TYPE_REG_OFFSET);
    HAL_RTCEx_BKUPWrite (&hrtc, BKP_UI_SETTINGS_REGISTER, regTemp);

    // Move to the next group: chart range
    chartSettingGroup = CHART_EDIT_RANGE_GROUP;
  }
  else if (currentScreen == LEDS)
  {
    // Save the current LED sequence to backup register
    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
    regTemp = (regTemp & 0xFF00FFFF) | (ledSequenceSetPosition << LED_SEQUENCE_REG_OFFSET);
    HAL_RTCEx_BKUPWrite (&hrtc, BKP_UI_SETTINGS_REGISTER, regTemp);

    // Move to the next group: LED duration
    ledSettingGroup = LED_EDIT_DURATION_GROUP;
  }

  // Change the callback for another long press to exit settings
  ButtonRegisterLongPressCallback (&userButton, UI_ExitSettingsCallback);
  UI_FullUpdateCurrentScreen ();
}

/**
 * @brief Callback for a long button press in the last settings group,
 *        saves the final setting and returns to normal mode.
 */
void UI_ExitSettingsCallback (void)
{
  if (currentScreen == CLOCK)
  {
    // no settings here
  }
  else if (currentScreen == CHARTS)
  {
    // Save the current chart range to backup register
    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
    regTemp = (regTemp & 0xFFFF00FF) | (chartRangeSetPosition << CHART_RANGE_REG_OFFSET);
    HAL_RTCEx_BKUPWrite (&hrtc, BKP_UI_SETTINGS_REGISTER, regTemp);

  chartSettingGroup = CHART_EDIT_NO_GROUP;
  }
  else if (currentScreen == LEDS)
  {
    // Save the current LED duration to backup register
    uint32_t regTemp = HAL_RTCEx_BKUPRead (&hrtc, BKP_UI_SETTINGS_REGISTER);
    regTemp = (regTemp & 0x00FFFFFF) | (ledDurationSetPosition << LED_DURATION_REG_OFFSET);
    HAL_RTCEx_BKUPWrite (&hrtc, BKP_UI_SETTINGS_REGISTER, regTemp);

  ledSettingGroup = LED_EDIT_NO_GROUP;
  }

  // Restore default button callbacks for normal UI usage
  ButtonRegisterPressCallback (&userButton, UI_NextScreenCallback);
  ButtonRegisterLongPressCallback (&userButton, UI_EnterSettingsCallback);
  UI_FullUpdateCurrentScreen ();
}

/**
 * @brief Displays the clock screen, resets setting groups, and updates callbacks accordingly.
 */
void UI_ShowClockScreen (void)
{
  ButtonRegisterPressCallback (&userButton, UI_NextScreenCallback);
  ButtonRegisterLongPressCallback (&userButton, UI_EnterSettingsCallback);
  chartSettingGroup = CHART_EDIT_NO_GROUP;
  ledSettingGroup = LED_EDIT_NO_GROUP;
  currentScreen = CLOCK;
  UI_FullUpdateCurrentScreen ();
}

/**
 * @brief Callback triggered on repeated button presses for a certain duration.
 *        Erases critical data, resets the device, etc. (Factory reset scenario).
 */
void UI_ResetDevice()
{
  // Turn on all LEDs briefly for visual feedback
  neopixel_led leds[16] = {0};
  LED_SetAllLeds(leds, 16);
  HAL_TIM_PWM_Start_DMA (&htim3, TIM_CHANNEL_2, (uint32_t*) leds, 16 * 24);
  HAL_Delay(1000);

  // Turn them off
  LED_ResetAllLeds(leds, 16);
  HAL_TIM_PWM_Start_DMA (&htim3, TIM_CHANNEL_2, (uint32_t*) leds, 16 * 24);
  HAL_Delay(50);

  // Erase flash memory (clears charts, etc.)
  Flash_ChipErase();

  // Put GPS to sleep for a clean shutdown
  GPS_Sleep();

  // Trigger a system reset
  HAL_NVIC_SystemReset();
}
