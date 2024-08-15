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
#include "local_time.h"
#include "rtc.h"

#define RX_BUFFER_SIZE 256

static UART_HandleTypeDef *gps_huart;
static uint8_t sentenceBuffer[RX_BUFFER_SIZE];
static uint8_t rx_buffer[RX_BUFFER_SIZE];
volatile static uint8_t rx_data;
static uint8_t rx_index = 0;
static uint32_t UTC_updateInterval = 0;
static uint32_t TimerGetGPSData = 0; // actively scan for gps data
static uint32_t TimerUpdateGPSData = 0; // gps data update interval

DateTime currentDateTime = {0, 0, 0, 0, 0, 0};
GPSGetDataState dataState = WAITING_FOR_DATA;
GPSDataAcquiredState gpsDataAcquired = 0;

void GPS_SendCommands (void)
{
    uint8_t command[32];

    strcpy ((char*) command, "$PUBX,40,GLL,0,0,0,0,0,0*5C\r\n"); // Turn GLL OFF
    HAL_UART_Transmit (gps_huart, command, strlen ((char*) command),
    HAL_MAX_DELAY);

    strcpy ((char*) command, "$PUBX,40,GSA,0,0,0,0,0,0*4E\r\n"); // Turn GSA OFF
    HAL_UART_Transmit (gps_huart, command, strlen ((char*) command),
    HAL_MAX_DELAY);

    strcpy ((char*) command, "$PUBX,40,GGA,0,0,0,0,0,0*5A\r\n"); // Turn GGA OFF
    HAL_UART_Transmit (gps_huart, command, strlen ((char*) command),
    HAL_MAX_DELAY);

    strcpy ((char*) command, "$PUBX,40,RMC,0,0,0,0,0,0*47\r\n"); // Turn RMC OFF
    HAL_UART_Transmit (gps_huart, command, strlen ((char*) command),
    HAL_MAX_DELAY);

    strcpy ((char*) command, "$PUBX,40,GSV,0,0,0,0,0,0*59\r\n"); // Turn GSV OFF
    HAL_UART_Transmit (gps_huart, command, strlen ((char*) command),
    HAL_MAX_DELAY);

    strcpy ((char*) command, "$PUBX,40,VTG,0,0,0,0,0,0*5E\r\n"); // Turn VTG OFF
    HAL_UART_Transmit (gps_huart, command, strlen ((char*) command),
    HAL_MAX_DELAY);

    strcpy ((char*) command, "$PUBX,40,ZDA,0,1,0,0,0,0*45\r\n"); // Turn ZDA ON
    HAL_UART_Transmit (gps_huart, command, strlen ((char*) command),
    HAL_MAX_DELAY);

    //UBX-CFG-PM2 command
    uint8_t command2[] = {
        0xB5, 0x62, 0x06, 0x3B, 0x2C, 0x00, 0x01, 0x06,
        0x00, 0x00, 0x0E, 0x90, 0x40, 0x01, 0xE8, 0x03,
        0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x2C, 0x01,
        0x00, 0x00, 0x4F, 0xC1, 0x03, 0x00, 0x87, 0x02,
        0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x64, 0x40,
        0x01, 0x00, 0xE4, 0x62
    };
    HAL_UART_Transmit (gps_huart, command2, sizeof(command2), HAL_MAX_DELAY);

    //UBG-CFG-GNSS command

    uint8_t command3[] = {
            0xB5, 0x62, 0x06, 0x3E, 0x24, 0x00, 0x00, 0x00,
            0xFF, 0x04, 0x00, 0x08, 0xFF, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x7F, 0xAA
        };
        HAL_UART_Transmit (gps_huart, command3, sizeof(command3), HAL_MAX_DELAY);
}
void GPS_Init (UART_HandleTypeDef *huart, uint32_t UTC_updInt)
{
  UTC_updateInterval = UTC_updInt;
  gps_huart = huart;
  HAL_UART_Receive_IT (gps_huart,(uint8_t *) &rx_data, 1);
  GPS_SendCommands();
}

void GPS_Sleep (void)
{
  //UBX-CFG-RXM
  uint8_t command[] =
        { 0xB5 , 0x62 , 0x06 , 0x11 , 0x02 , 0x00 , 0x00 , 0x01 , 0x22 , 0x92 };
  HAL_UART_Transmit (gps_huart, command, sizeof(command), HAL_MAX_DELAY);
  //UBX-RXM-PMREQ (Power Mode Request)
  uint8_t command2[] =
      { 0xB5 , 0x62 , 0x02 , 0x41 , 0x08 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x02 , 0x00 , 0x00 , 0x00 , 0x4D , 0x3B };
  HAL_UART_Transmit (gps_huart, command2, sizeof(command2), HAL_MAX_DELAY);
}

void GPS_Wakeup (void)
{
  // just pull RX line high so the module wakes up
  uint8_t command[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  HAL_UART_Transmit (gps_huart, command, sizeof(command), HAL_MAX_DELAY);
}

GPSDataAcquiredState GPS_GetDateTime (DateTime *datetime)
{
  if (sscanf ((char*) sentenceBuffer, "$GPZDA,%2d%2d%2d.%*2d,%2d,%2d,%4d,%*2d,%*2d",
	      &datetime->hour, &datetime->minute, &datetime->second, &datetime->day,
	      &datetime->month, &datetime->year) == 6)
    {
      LT_SetTime(&hrtc, datetime);
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
		  gpsDataAcquired = GPS_GetDateTime (&currentDateTime);
		  dataState = DATA_READY;
		}
	      else
		{
		  GPS_SendCommands();
		}
	    }
	}
      else
	{
	  rx_index = 0;
	}
      HAL_UART_Receive_IT (gps_huart, (uint8_t *) &rx_data, 1);
    }
}
void GPS_RUN (void)
{
  if (((HAL_GetTick () - TimerGetGPSData) > 100) && dataState == DATA_READY)
  	{
  	  TimerGetGPSData = HAL_GetTick ();

  	  if (gpsDataAcquired == DATA_ACQUIRED)
  	    {
  	    RTC_TimeTypeDef sTime;
  	    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  	    RTC_DateTypeDef sDate;
  	    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
  	    printf ("\n\r************************************************************************\n\r");
	  printf ("Date updated: %02d-%02d-%04d Time: %02d:%02d:%02d\n\r",
		  sDate.Date, sDate.Month,
		  sDate.Year + 2000, sTime.Hours,
		  sTime.Minutes, sTime.Seconds);
	  printf ("************************************************************************\n\n\r");
  	      GPS_Sleep ();
  	      printf("\n\rSLEEP\n\r");

  	      TimerUpdateGPSData = HAL_GetTick();
  	      dataState = NO_DATA_NEEDED;
  	    }
  	  else if (gpsDataAcquired == DATA_NOT_ACQUIRED_NO_FIX)
  	    {
  	      printf ("GPS DATA NOT ACQUIRED, NO FIX\n\r");
  	      dataState = WAITING_FOR_DATA;
  	    }
  	  else if (gpsDataAcquired == DATA_INCORRECT)
  	    {
  	      printf ("GPS DATA INCORRECT\n\r");
  	      dataState = WAITING_FOR_DATA;
  	    }
  	}
        if (((HAL_GetTick () - TimerUpdateGPSData) > UTC_updateInterval) && dataState == NO_DATA_NEEDED)
        	{
  	  TimerUpdateGPSData = HAL_GetTick ();
  	  dataState = WAITING_FOR_DATA;
  	  printf("\n\rWAKE-UP\n\r");
  	  GPS_Wakeup ();
        	}
}
