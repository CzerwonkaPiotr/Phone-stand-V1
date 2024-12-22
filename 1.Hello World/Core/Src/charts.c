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

#define FLASH_START_ADDRESS    0x000000   // Początek przestrzeni zapisu w pamięci flash
#define FLASH_END_ADDRESS      0x006000   // Koniec przestrzeni zapisu 1 sektor na ostatni adres i 4 sektory na dane i 1 zapasu na kasowanie.
#define FLASH_SECTOR_SIZE      0x1000     // Rozmiar sektora (4 KB)
#define STRUCT_SIZE            16         // Rozmiar struktury (16 bajtów)
#define TOTAL_MEASUREMENTS	       1008	  // 168 godzin razy 6, pomiary są robione co 10 minut.

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
static void CHARTS_LoadData (CHARTS_t*data)
{
  uint32_t currentAddress = CHARTS_Flash_ReadLastAddress();

  for(uint16_t i = 0; i < TOTAL_MEASUREMENTS; i++)
  {
    if (currentAddress < FLASH_SECTOR_SIZE)
    {
      currentAddress = FLASH_END_ADDRESS - STRUCT_SIZE;
    }
    Flash_Read(currentAddress, (uint8_t*)&data[i], STRUCT_SIZE);

    currentAddress -= STRUCT_SIZE;
  }
}

void CHARTS_DrawCharts (Paint* paint, CHART_TYPE_POSITION_t type, CHART_RANGE_POSITION_t range)
{
  CHARTS_t data[TOTAL_MEASUREMENTS];
  CHARTS_LoadData((CHARTS_t*)&data);
}

void CHARTS_SaveData (CHARTS_t* data)
{
  uint32_t lastAddress = CHARTS_Flash_ReadLastAddress();

      // Obliczenie kolejnego adresu zapisu
      uint32_t nextAddress = lastAddress + STRUCT_SIZE;

      // Sprawdzenie, czy przekroczono zakres dostępnej przestrzeni
      if (nextAddress + STRUCT_SIZE > FLASH_END_ADDRESS)
      {
          nextAddress = FLASH_START_ADDRESS + FLASH_SECTOR_SIZE; // Przejście na początek dostępnej przestrzeni
      }

      // Sprawdzenie, czy wchodzimy w nowy sektor
      if (nextAddress % FLASH_SECTOR_SIZE == 0) {
          Flash_SErase4k(nextAddress); // Kasowanie nowego sektora
      }

      // Zapis struktury pod nowy adres
      Flash_Write(nextAddress, (uint8_t*)data, STRUCT_SIZE);

      // Aktualizacja ostatniego adresu zapisu
      CHARTS_Flash_WriteLastAddress(nextAddress);
}
