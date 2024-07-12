/*
 * NEO_6M.c
 *
 *  Created on: Jul 10, 2024
 *      Author: piotr
 */
#include "main.h"
#include "string.h"
#include "stdio.h"
#include "NEO_6M.h"

#define RX_BUFFER_SIZE 256

static UART_HandleTypeDef *gps_huart;
static uint8_t sentenceBuffer[RX_BUFFER_SIZE];
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint8_t rx_data;
static uint8_t rx_index = 0;

GPSGetDataState dataState = WAITING_FOR_DATA;
GPSDataAcquiredState gpsDataAcquired = 0;

void GPS_ManageCommands (void)
{
    uint8_t command[32];

    strcpy ((char*) command, "$PUBX,40,GLL,0,0,0,0,0,0*5C\r\n");
    HAL_UART_Transmit (gps_huart, command, strlen ((char*) command),
    HAL_MAX_DELAY);

    strcpy ((char*) command, "$PUBX,40,GSA,0,0,0,0,0,0*4E\r\n");
    HAL_UART_Transmit (gps_huart, command, strlen ((char*) command),
    HAL_MAX_DELAY);

    strcpy ((char*) command, "$PUBX,40,GGA,0,0,0,0,0,0*5A\r\n");
    HAL_UART_Transmit (gps_huart, command, strlen ((char*) command),
    HAL_MAX_DELAY);

    strcpy ((char*) command, "$PUBX,40,RMC,0,0,0,0,0,0*47\r\n");
    HAL_UART_Transmit (gps_huart, command, strlen ((char*) command),
    HAL_MAX_DELAY);

    strcpy ((char*) command, "$PUBX,40,GSV,0,0,0,0,0,0*59\r\n");
    HAL_UART_Transmit (gps_huart, command, strlen ((char*) command),
    HAL_MAX_DELAY);

    strcpy ((char*) command, "$PUBX,40,VTG,0,0,0,0,0,0*5E\r\n");
    HAL_UART_Transmit (gps_huart, command, strlen ((char*) command),
    HAL_MAX_DELAY);

    strcpy ((char*) command, "$PUBX,40,ZDA,0,1,0,0,0,0*45\r\n");
    HAL_UART_Transmit (gps_huart, command, strlen ((char*) command),
    HAL_MAX_DELAY);
}
void GPS_Init (UART_HandleTypeDef *huart)
{
  gps_huart = huart;
  HAL_UART_Receive_IT (gps_huart, &rx_data, 1);
  GPS_ManageCommands();
}

void GPS_Sleep (void)
{
  uint8_t command[] = { 0xB5 , 0x62 , 0x02 , 0x41 , 0x08 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x02 , 0x00 , 0x00 , 0x00 , 0x4D , 0x3B };
  HAL_UART_Transmit (gps_huart, command, sizeof(command), HAL_MAX_DELAY);
}

void GPS_Wakeup (void)
{
  uint8_t command[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  HAL_UART_Transmit (gps_huart, command, sizeof(command), HAL_MAX_DELAY);
}

GPSDataAcquiredState GPS_GetDateTime (DateTime *datetime)
{
  if (sscanf ((char*) sentenceBuffer, "$GPZDA,%2d%2d%2d.%*2d,%2d,%2d,%4d,%*2d,%*2d",
	      &datetime->hour, &datetime->minute, &datetime->second, &datetime->day,
	      &datetime->month, &datetime->year) == 6)
    {
      return DATA_ACQUIRED; // Data is ready to be read
    }
  if (strncmp ((char*) sentenceBuffer, "$GPZDA,,,,,00,00", 16) == 0)
    {
      return DATA_NOT_ACQUIRED_NO_FIX; // GPS module has no fix
    }
  else
    {
      return DATA_INCORRECT;
    }
}

void HAL_UART_RxCpltCallback (UART_HandleTypeDef *huart)
{
  if (huart->Instance == gps_huart->Instance)
    {
      printf ("%c", rx_data); //TODO it's here just for debugging purposes

      if (rx_index < RX_BUFFER_SIZE )
	{
	  if(rx_data == '$') rx_index = 0;
	  rx_buffer[rx_index++] = rx_data;


	  if ((dataState == WAITING_FOR_DATA) && rx_data == '\n')
	    {
	      rx_buffer[rx_index] = '\0';
	      rx_index = 0;
	      if (strncmp ((char*) rx_buffer, "$GPZDA", 6) == 0)
		{
		  strcpy((char*)sentenceBuffer,(const char*) rx_buffer);
		  dataState = DATA_READY;
		}
	      else
		{
		  GPS_ManageCommands();
		}
	    }
	}
      else
	{
	  rx_index = 0;
	}
      HAL_UART_Receive_IT (gps_huart, &rx_data, 1);
    }
}
