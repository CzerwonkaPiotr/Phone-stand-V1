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

#define FLASH_START_ADDRESS   	0x000000   // Początek przestrzeni zapisu w pamięci flash
#define FLASH_END_ADDRESS      	0x00A000   // Koniec przestrzeni zapisu 1 sektor na ostatni adres i 8 sektory na dane i 1 zapasu na kasowanie.
#define FLASH_SECTOR_SIZE       0x1000     // Rozmiar sektora (4 KB)
#define STRUCT_SIZE           	32         // Rozmiar struktury (16 bajtów)
#define SECONDS_IN_10_MINUTES 	(10 * 60)
#define CHART_LEFT_END_PIXEL	2
#define CHART_RIGHT_END_PIXEL	242
#define CHART_TOP_END_PIXEL	31
#define CHART_DOWN_END_PIXEL	111
#define CHART_WIDTH		240
#define CHART_HEIGHT		80

static uint32_t CHARTS_Flash_ReadLastAddress() {
    uint32_t lastAddress;
    Flash_Read(FLASH_START_ADDRESS , (uint8_t*)&lastAddress, sizeof(lastAddress));
    if (lastAddress == 0x000000)lastAddress = FLASH_SECTOR_SIZE;


    return lastAddress;
}

static void CHARTS_Flash_WriteLastAddress(uint32_t address) {
    Flash_SErase4k(FLASH_START_ADDRESS); // Kasowanie pierwszego sektora
    Flash_Write(FLASH_START_ADDRESS, (uint8_t*)&address, sizeof(address));
}
static void CHARTS_ReadData (CHARTS_t*data, uint16_t measurementCount)
{
  uint32_t currentAddress = CHARTS_Flash_ReadLastAddress();

  for(uint16_t i = 0; i < measurementCount; i++)
  {
    if (currentAddress < FLASH_SECTOR_SIZE)
    {
      currentAddress = FLASH_END_ADDRESS - STRUCT_SIZE;
    }
    Flash_Read(currentAddress, (uint8_t*)&data[i], STRUCT_SIZE);

    currentAddress -= STRUCT_SIZE;
  }
}

static uint8_t DaysInMonth(uint8_t month, uint16_t year) {
    switch (month) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12:
            return 31;
        case 4: case 6: case 9: case 11:
            return 30;
        case 2:
            // Luty, uwzględnij rok przestępny
            return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
        default:
            return 0; // Niepoprawny miesiąc
    }
}

uint32_t RTC_ToEpochSeconds(RTC_TimeTypeDef* time, RTC_DateTypeDef* date) {
    uint32_t totalSeconds = 0;

    // Rok RTC zaczyna się od 2000 (zakładamy format BCD)
    uint16_t year = 2000 + date->Year;

    // Dodaj sekundy za pełne lata
    for (uint16_t y = 2000; y < year; y++) {
        totalSeconds += (365 + (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) * 86400;
    }

    // Dodaj sekundy za pełne miesiące w bieżącym roku
    for (uint8_t m = 1; m < date->Month; m++) {
        totalSeconds += DaysInMonth(m, year) * 86400;
    }

    // Dodaj sekundy za pełne dni w bieżącym miesiącu
    totalSeconds += (date->Date - 1) * 86400;

    // Dodaj sekundy za godziny, minuty i sekundy w bieżącym dniu
    totalSeconds += time->Hours * 3600;
    totalSeconds += time->Minutes * 60;
    totalSeconds += time->Seconds;

    return totalSeconds;
}

uint16_t CHARTS_GetElapsed10MinuteIntervals(uint32_t savedEpochSeconds, uint32_t currentEpochSeconds) {
    if (currentEpochSeconds < savedEpochSeconds) {
        return 0; // Jeśli czas zapisany jest w przyszłości, zwróć 0
    }

    uint32_t elapsedSeconds = currentEpochSeconds - savedEpochSeconds;
    return (uint16_t)(elapsedSeconds / SECONDS_IN_10_MINUTES);
}

void CHARTS_DrawCharts (Paint* paint, CHART_TYPE_POSITION_t type, CHART_RANGE_POSITION_t range, RTC_TimeTypeDef sTime, RTC_DateTypeDef sDate)
{
  char text[128];
  uint16_t measurementCount;
  if (range == RANGE_8H) measurementCount = 48;
  else if (range == RANGE_40H) measurementCount = 240;
  else if (range == RANGE_160H) measurementCount = 960;
  float valueMax = 0, valueMin = 9999, valueNow = 0, valueTable[measurementCount];
  CHARTS_t data[measurementCount];
  CHARTS_ReadData ((CHARTS_t*) &data, measurementCount);

  uint32_t currentEpochSeconds = RTC_ToEpochSeconds (&sTime, &sDate);

  for (uint16_t i = 0; i < measurementCount; i++)
    valueTable[i] = 0;

  for (uint16_t i = 0; i < measurementCount; i++)
  {
    uint16_t elapsedIntervals = CHARTS_GetElapsed10MinuteIntervals (data[i].epoch_seconds, currentEpochSeconds);
    if (elapsedIntervals < measurementCount && data[i].epoch_seconds != 0xffffffff)
    {
      if (type == TEMPERATURE_CHART) valueTable[elapsedIntervals] = data[i].temperature;
      else if (type == HUMIDITY_CHART) valueTable[elapsedIntervals] = data[i].humidity;
      else if (type == PRESSURE_CHART) valueTable[elapsedIntervals] = data[i].pressure;
      else if (type == BATTERY_LEVEL_CHART) valueTable[elapsedIntervals] = data[i].battery_level;

      if (valueTable[elapsedIntervals] < valueMin) valueMin = valueTable[elapsedIntervals];
      if (valueTable[elapsedIntervals] > valueMax) valueMax = valueTable[elapsedIntervals];
      if (valueNow == 0) valueNow = valueTable[elapsedIntervals];
    }
  }
  if (type == BATTERY_LEVEL_CHART)
  {
    valueMin = 0;
    valueMax = 100;

  }
  uint8_t x0 = 0, x1 = 0, y0 = 0, y1 = 0;
  for (uint16_t i = 1; i < measurementCount; i++)
  {
    if (!(valueTable[i] == 0 || valueTable[i - 1] == 0 || valueMax == valueMin))
    {
      if (range == RANGE_8H)
      {
	y0 = (80 - (((valueTable[i - 1] - valueMin) / (valueMax - valueMin)) * CHART_HEIGHT)) + CHART_TOP_END_PIXEL;
	y1 = (80 - (((valueTable[i] - valueMin) / (valueMax - valueMin)) * CHART_HEIGHT)) + CHART_TOP_END_PIXEL;
	x0 = CHART_RIGHT_END_PIXEL - ((i-1) * 5);
	x1 = CHART_RIGHT_END_PIXEL - ((i-1) * 5) - 5;
      }
      else if (range == RANGE_40H)
      {
	y0 = (80 - (((valueTable[i - 1] - valueMin) / (valueMax - valueMin)) * CHART_HEIGHT)) + CHART_TOP_END_PIXEL;
	y1 = (80 - (((valueTable[i] - valueMin) / (valueMax - valueMin)) * CHART_HEIGHT)) + CHART_TOP_END_PIXEL;
	x0 = CHART_RIGHT_END_PIXEL - (i-1);
	x1 = CHART_RIGHT_END_PIXEL - (i-1) - 1;
      }
      else if (range == RANGE_160H)
      {
	if ((i - 1) % 4 == 0)
	{
	  if(valueTable[i-1] == 0 ||valueTable[i] == 0 ||valueTable[i+1] == 0 ||valueTable[i+2] == 0 ||valueTable[i+3] == 0 ||valueTable[i+4] == 0 ||valueTable[i+5] == 0 ||valueTable[i+6] == 0)
	  {
	    continue;
	  }
	  float tempVar1, tempVar2;
	  if(i+6 < measurementCount)
	  {
	    tempVar1 = (valueTable[i-1] + valueTable[i] + valueTable[i + 1] + valueTable[i + 2]) /4;
	    tempVar2 = (valueTable[i+3] + valueTable[i + 4] + valueTable[i + 5] + valueTable[i + 6]) /4;
	  }
	  y0 = (80 - (((tempVar1 - valueMin) / (valueMax - valueMin)) * CHART_HEIGHT)) + CHART_TOP_END_PIXEL;
	  y1 = (80 - (((tempVar2 - valueMin) / (valueMax - valueMin)) * CHART_HEIGHT)) + CHART_TOP_END_PIXEL;
	  x0 = CHART_RIGHT_END_PIXEL - ((i / 4) - 1);
	  x1 = CHART_RIGHT_END_PIXEL - ((i / 4) - 1) - 1;
	}

      }
      Paint_DrawLine (paint, x0, y0, x1, y1, COLORED);
    }
  }

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
  if (lastAddress == 0xFFFFFFFF || lastAddress < FLASH_SECTOR_SIZE) lastAddress = FLASH_SECTOR_SIZE;

  // Obliczenie kolejnego adresu zapisu
  uint32_t nextAddress = lastAddress + STRUCT_SIZE;

  // Sprawdzenie, czy przekroczono zakres dostępnej przestrzeni
  if (nextAddress + STRUCT_SIZE > FLASH_END_ADDRESS)
  {
    nextAddress = FLASH_START_ADDRESS + FLASH_SECTOR_SIZE; // Przejście na początek dostępnej przestrzeni
  }

  // Sprawdzenie, czy wchodzimy w nowy sektor
  if (nextAddress % FLASH_SECTOR_SIZE == 0)
  {
    Flash_SErase4k (nextAddress); // Kasowanie nowego sektora
  }

  // Zapis struktury pod nowy adres
  Flash_Write (nextAddress, (uint8_t*) data, STRUCT_SIZE);

  // Aktualizacja ostatniego adresu zapisu
  CHARTS_Flash_WriteLastAddress (nextAddress);
}
