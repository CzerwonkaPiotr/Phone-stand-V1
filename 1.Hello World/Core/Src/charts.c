/*
 * charts.c
 *
 *  Created on: Dec 21, 2024
 *      Author: piotr
 */
#include "charts.h"
#include "ui.h"
#include "epd2in9.h"
#include "epdif.h"
#include "epdpaint.h"
#include "rtc.h"
#include "stdio.h"

// ============================================================================
// Definitions and Constants
// ============================================================================

#define FLASH_START_ADDRESS   	0x000000   // Start of flash memory space for data
#define FLASH_END_ADDRESS      	0x00A000   // End of flash memory space
#define FLASH_SECTOR_SIZE       0x1000     // Sector size: 4 KB
#define STRUCT_SIZE           	32         // Size of each data structure in bytes
#define SECONDS_IN_10_MINUTES 	(10 * 60)  // Number of seconds in 10 minutes
#define CHART_LEFT_END_PIXEL	2
#define CHART_RIGHT_END_PIXEL	242
#define CHART_TOP_END_PIXEL	31
#define CHART_DOWN_END_PIXEL	111
#define CHART_WIDTH		240
#define CHART_HEIGHT		80

// ============================================================================
// Static Helper Functions
// ============================================================================

/**
 * @brief  Read the last used address from the flash memory.
 * @retval uint32_t: Last used address in flash memory.
 */
static uint32_t CHARTS_Flash_ReadLastAddress() {
    uint32_t lastAddress;
    Flash_Read(FLASH_START_ADDRESS, (uint8_t*)&lastAddress, sizeof(lastAddress));

    // If no valid address is found, initialize to the first available sector
    if (lastAddress == 0x000000)
    {
        lastAddress = FLASH_SECTOR_SIZE;
    }

    return lastAddress;
}

/**
 * @brief  Write the last used address to the flash memory.
 * @param  address: The last address to be stored in flash memory.
 * @retval None
 */
static void CHARTS_Flash_WriteLastAddress(uint32_t address) {
    Flash_SErase4k(FLASH_START_ADDRESS); // Erase the first flash sector
    Flash_Write(FLASH_START_ADDRESS, (uint8_t*)&address, sizeof(address));
}

/**
 * @brief  Read measurement data from flash memory.
 * @param  data: Pointer to an array where the read data will be stored.
 * @param  measurementCount: Number of measurements to read.
 * @retval None
 */
static void CHARTS_ReadData (CHARTS_t*data, uint16_t measurementCount)
{
  uint32_t currentAddress = CHARTS_Flash_ReadLastAddress();

  for(uint16_t i = 0; i < measurementCount; i++)
  {
    // Check if the current address has wrapped around to the beginning
    if (currentAddress < FLASH_SECTOR_SIZE)
    {
      // If so, jump to the end of the flash memory space
      currentAddress = FLASH_END_ADDRESS - STRUCT_SIZE;
    }
    // Read a single measurement structure from flash memory at the current address
    Flash_Read(currentAddress, (uint8_t*)&data[i], STRUCT_SIZE);

    // Move to the previous record (decrement the address by the size of one structure)
    currentAddress -= STRUCT_SIZE;
  }
}

/**
 * @brief  Calculate the number of days in a given month.
 * @param  month: Month (1-12).
 * @param  year: Year (e.g., 2024).
 * @retval uint8_t: Number of days in the given month.
 */
static uint8_t DaysInMonth(uint8_t month, uint16_t year) {
    switch (month) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12:
            return 31;
        case 4: case 6: case 9: case 11:
            return 30;
        case 2:
          // February, account for leap years
            return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
        default:
            return 0; // Invalid month
    }
}

// ============================================================================
// Public Functions
// ============================================================================

/**
 * @brief  Convert RTC time and date to Unix epoch time (seconds since 1970).
 * @param  time: Pointer to RTC_TimeTypeDef structure.
 * @param  date: Pointer to RTC_DateTypeDef structure.
 * @retval uint32_t: Unix epoch time in seconds.
 */
uint32_t RTC_ToEpochSeconds(RTC_TimeTypeDef* time, RTC_DateTypeDef* date) {
    uint32_t totalSeconds = 0;

    // RTC year starts from 2000 (assume BCD format)
    uint16_t year = 2000 + date->Year;

    // Add seconds for full years
    for (uint16_t y = 2000; y < year; y++) {
        totalSeconds += (365 + (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) * 86400;
    }

    // Add seconds for full months in the current year
    for (uint8_t m = 1; m < date->Month; m++) {
        totalSeconds += DaysInMonth(m, year) * 86400;
    }

    // Add seconds for full days in the current month
    totalSeconds += (date->Date - 1) * 86400;

    // Add seconds for hours, minutes, and seconds in the current day
    totalSeconds += time->Hours * 3600;
    totalSeconds += time->Minutes * 60;
    totalSeconds += time->Seconds;

    return totalSeconds;
}

/**
 * @brief  Calculate the number of elapsed 10-minute intervals.
 * @param  savedEpochSeconds: Previously saved Unix epoch time.
 * @param  currentEpochSeconds: Current Unix epoch time.
 * @retval uint16_t: Number of elapsed 10-minute intervals.
 */
uint16_t CHARTS_GetElapsed10MinuteIntervals(uint32_t savedEpochSeconds, uint32_t currentEpochSeconds) {
    if (currentEpochSeconds < savedEpochSeconds) {
        return 0; // If the saved time is in the future, return 0
    }

    uint32_t elapsedSeconds = currentEpochSeconds - savedEpochSeconds;
    return (uint16_t)(elapsedSeconds / SECONDS_IN_10_MINUTES);
}

/**
 * @brief  Draw a chart on the e-paper display.
 * @param  paint: Pointer to Paint structure for e-paper rendering.
 * @param  type: Type of chart (e.g., temperature, humidity, pressure, battery).
 * @param  range: Time range of the chart (e.g., 8h, 40h, 160h).
 * @param  sTime: Current RTC time.
 * @param  sDate: Current RTC date.
 * @retval None
*
 * This function retrieves data from flash memory, processes it into a format
 * suitable for rendering, and draws the chart on the e-paper display.
 * It calculates min, max, and current values, and scales the data points
 * according to the specified time range and chart type.
 */
void CHARTS_DrawCharts (Paint* paint, CHART_TYPE_POSITION_t type, CHART_RANGE_POSITION_t range, RTC_TimeTypeDef sTime, RTC_DateTypeDef sDate)
{
  char text[128]; // Buffer for text to be displayed on the chart
  uint16_t measurementCount;

  // Determine the number of measurements based on the chart range
  if (range == RANGE_8H) measurementCount = 48;
  else if (range == RANGE_40H) measurementCount = 240;
  else if (range == RANGE_160H) measurementCount = 960;

  // Initialize variables for chart calculations
  float valueMax = 0, valueMin = 9999, valueNow = 0, valueTable[measurementCount];
  CHARTS_t data[measurementCount];

  // Read measurement data from flash memory
  CHARTS_ReadData ((CHARTS_t*) &data, measurementCount);

  // Convert RTC time to epoch time
  uint32_t currentEpochSeconds = RTC_ToEpochSeconds(&sTime, &sDate);

  // Initialize value table
  for (uint16_t i = 0; i < measurementCount; i++)
    valueTable[i] = 0;

  // Process measurement data
  for (uint16_t i = 0; i < measurementCount; i++)
  {
    // Calculate the elapsed 10-minute intervals since the data was recorded
    uint16_t elapsedIntervals = CHARTS_GetElapsed10MinuteIntervals (data[i].epoch_seconds, currentEpochSeconds);

    // Check if the data point is within the time range and is valid
    if (elapsedIntervals < measurementCount && data[i].epoch_seconds != 0xffffffff)
    {
      // Assign the appropriate value to the value table based on the chart type
      if (type == TEMPERATURE_CHART) valueTable[elapsedIntervals] = data[i].temperature;
      else if (type == HUMIDITY_CHART) valueTable[elapsedIntervals] = data[i].humidity;
      else if (type == PRESSURE_CHART) valueTable[elapsedIntervals] = data[i].pressure;
      else if (type == BATTERY_LEVEL_CHART) valueTable[elapsedIntervals] = data[i].battery_level;

      // Update the min and max values for the chart
      if (valueTable[elapsedIntervals] < valueMin) valueMin = valueTable[elapsedIntervals];
      if (valueTable[elapsedIntervals] > valueMax) valueMax = valueTable[elapsedIntervals];

      // Set the current value if it has not yet been set
      if (valueNow == 0) valueNow = valueTable[elapsedIntervals];
    }
  }

  // Special case for battery level charts: set fixed min and max values
  if (type == BATTERY_LEVEL_CHART)
  {
    valueMin = 0;
    valueMax = 100;

  }

  // Variables for drawing lines on the chart
  uint8_t x0 = 0, x1 = 0, y0 = 0, y1 = 0;

  // Iterate through the value table to draw the chart lines
  for (uint16_t i = 1; i < measurementCount; i++)
  {
    // Skip invalid data points or cases where min equals max
    if (valueTable[i] == 0 || valueTable[i - 1] == 0 || valueMax == valueMin)
    {
      continue;
    }

    // Calculate coordinates for the current and previous data points based on the range
    if (range == RANGE_8H)
    {
      y0 = (80 - (((valueTable[i - 1] - valueMin) / (valueMax - valueMin)) * CHART_HEIGHT)) + CHART_TOP_END_PIXEL;
      y1 = (80 - (((valueTable[i] - valueMin) / (valueMax - valueMin)) * CHART_HEIGHT)) + CHART_TOP_END_PIXEL;
      x0 = CHART_RIGHT_END_PIXEL - ((i - 1) * 5);
      x1 = CHART_RIGHT_END_PIXEL - ((i - 1) * 5) - 5;
    }
    else if (range == RANGE_40H)
    {
      y0 = (80 - (((valueTable[i - 1] - valueMin) / (valueMax - valueMin)) * CHART_HEIGHT)) + CHART_TOP_END_PIXEL;
      y1 = (80 - (((valueTable[i] - valueMin) / (valueMax - valueMin)) * CHART_HEIGHT)) + CHART_TOP_END_PIXEL;
      x0 = CHART_RIGHT_END_PIXEL - (i - 1);
      x1 = CHART_RIGHT_END_PIXEL - (i - 1) - 1;
    }
    else if (range == RANGE_160H)
    {
      if ((i - 1) % 4 == 0)
      {
	// Calculate averages for data smoothing (4-point moving average)
	if (valueTable[i - 1] == 0 || valueTable[i] == 0 || valueTable[i + 1] == 0 || valueTable[i + 2] == 0 || valueTable[i + 3] == 0 || valueTable[i + 4] == 0
	    || valueTable[i + 5] == 0 || valueTable[i + 6] == 0)
	{
	  continue;
	}
	float tempVar1, tempVar2;
	if (i + 6 < measurementCount)
	{
	  tempVar1 = (valueTable[i - 1] + valueTable[i] + valueTable[i + 1] + valueTable[i + 2]) / 4;
	  tempVar2 = (valueTable[i + 3] + valueTable[i + 4] + valueTable[i + 5] + valueTable[i + 6]) / 4;
	}
	y0 = (80 - (((tempVar1 - valueMin) / (valueMax - valueMin)) * CHART_HEIGHT)) + CHART_TOP_END_PIXEL;
	y1 = (80 - (((tempVar2 - valueMin) / (valueMax - valueMin)) * CHART_HEIGHT)) + CHART_TOP_END_PIXEL;
	x0 = CHART_RIGHT_END_PIXEL - ((i / 4) - 1);
	x1 = CHART_RIGHT_END_PIXEL - ((i / 4) - 1) - 1;
      }

    }

    // Draw a line between the two calculated points
    Paint_DrawLine (paint, x0, y0, x1, y1, COLORED);
  }

  // Add text labels and values to the chart
  if (type == TEMPERATURE_CHART)
  {
    sprintf (text, "Temperature");
    Paint_DrawStringAt (paint, 53, 5, text, &Font24, COLORED);
    sprintf (text, "%.1fMax", valueMax);
    Paint_DrawStringAt (paint, 245, 27, text, &Font12, COLORED);
    sprintf (text, "%.1f'C", valueNow);
    Paint_DrawStringAt (paint, 247, 67, text, &Font12, COLORED);
    sprintf (text, "%.1fMin", valueMin);
    Paint_DrawStringAt (paint, 245, 107, text, &Font12, COLORED);
  }
  else if (type == HUMIDITY_CHART)
  {
    sprintf (text, "Humidity");
    Paint_DrawStringAt (paint, 75, 5, text, &Font24, COLORED);
    sprintf (text, "%02d%%Max", (int) valueMax);
    Paint_DrawStringAt (paint, 245, 27, text, &Font12, COLORED);
    sprintf (text, "%02d%%Rh", (int) valueNow);
    Paint_DrawStringAt (paint, 247, 67, text, &Font12, COLORED);
    sprintf (text, "%02d%%Min", (int) valueMin);
    Paint_DrawStringAt (paint, 245, 107, text, &Font12, COLORED);
  }
  else if (type == PRESSURE_CHART)
  {
    sprintf (text, "Pressure");
    Paint_DrawStringAt (paint, 75, 5, text, &Font24, COLORED);
    sprintf (text, "%04dMax", (int) valueMax);
    Paint_DrawStringAt (paint, 245, 27, text, &Font12, COLORED);
    sprintf (text, "%04dhPa", (int) valueNow);
    Paint_DrawStringAt (paint, 245, 67, text, &Font12, COLORED);
    sprintf (text, "%04dMin", (int) valueMin);
    Paint_DrawStringAt (paint, 245, 107, text, &Font12, COLORED);
  }
  else if (type == BATTERY_LEVEL_CHART)
  {
    sprintf (text, "Battery level");
    Paint_DrawStringAt (paint, 30, 5, text, &Font24, COLORED);
    sprintf (text, "100%%");
    Paint_DrawStringAt (paint, 260, 27, text, &Font12, COLORED);
    sprintf (text, "%d%%", (int) valueNow);
    Paint_DrawStringAt (paint, 262, 67, text, &Font12, COLORED);
    sprintf (text, "0%%");
    Paint_DrawStringAt (paint, 265, 107, text, &Font12, COLORED);
  }
}

void CHARTS_SaveData (CHARTS_t* data)
{
  uint32_t lastAddress = CHARTS_Flash_ReadLastAddress ();

  // If the last address is uninitialized (0xFFFFFFFF) or below the minimum valid sector size,
  // reset it to the starting address of the first flash memory sector (FLASH_SECTOR_SIZE).
  if (lastAddress == 0xFFFFFFFF || lastAddress < FLASH_SECTOR_SIZE) lastAddress = FLASH_SECTOR_SIZE;

  // Calculate the next address for writing
  uint32_t nextAddress = lastAddress + STRUCT_SIZE;

  // Check if the available space is exceeded
  if (nextAddress + STRUCT_SIZE > FLASH_END_ADDRESS)
  {
    nextAddress = FLASH_START_ADDRESS + FLASH_SECTOR_SIZE; // Przejście na początek dostępnej przestrzeni
  }

  // Erase the new sector if entering a new one
  if (nextAddress % FLASH_SECTOR_SIZE == 0)
  {
    Flash_SErase4k (nextAddress); // Kasowanie nowego sektora
  }

  // Write the data to flash memory
  Flash_Write (nextAddress, (uint8_t*) data, STRUCT_SIZE);

  // Update the last written address
  CHARTS_Flash_WriteLastAddress (nextAddress);
}
