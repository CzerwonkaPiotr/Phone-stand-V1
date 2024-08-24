/*
 * parse_commands.c
 *
 *  Created on: Jul 16, 2024
 *      Author: piotr
 */

#include "main.h"
#include "rtc.h"
#include "ring_buffer.h"
#include "parse_commands.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "bmp280.h"
#include "NEO_6M.h"
#include "local_time.h"

extern BMP280_t Bmp280;
extern GPSGetDataState GPSDataState;

extern RTC_HandleTypeDef hrtc;

//
// Get a one complete line from Ring Buffer
//
void Parser_TakeLine(RingBuffer_t *Buf, uint8_t *Destination)
{
	uint8_t Tmp;
	uint8_t i = 0;

	// Loop for every char in Ring Buffer
	do
	{
		// Read a one byte from Ring Buffer
		RB_Read(Buf, &Tmp);

		// Check if we take the endline char
		if(Tmp == ENDLINE)
		{
			// If endline - replace it with c-string end sign - 0x00
			Destination[i] = 0;
		}
		else
		{
			// If not endline - just write to work-buffer
			Destination[i] = Tmp;
		}

		i++; // increment array iterator
	}while(Tmp != ENDLINE); // If we hit the endline char - end the loop
}



// Parsing functions
// Commands:

// BMP280 GET_TEMP
// BMP280 GET_HUM
// BMP280 GET_PRESS
// BMP280 DEBUG_MODE_ON
// BMP280 DEBUG_MODE_OFF

// GPS SLEEP
// GPS WAKEUP
// GPS SEND_CONFIG
// GPS GET_DATA
// GPS DEBUG_MODE_ON
// GPS DEBUG_MODE_OFF

// TIME GET
// TIME SET_TIME H=%2d M=%2d S=%2d
// TIME SET_DATE D=%2d M=%2d Y=%2d
// TIME SET_UTC_TIME D=%2d MO=%2d Y=%2d H=%2d MI=%2d S=%2d
// TIME DEBUG_MODE_ON
// TIME DEBUG_MODE_OFF


static void Parser_ParseBMP280 (void)
{
  // String to parse:
  // GET_TEMP
  // GET_HUM
  // GET_PRESS
  // DEBUG_MODE_ON
  // DEBUG_MODE_OFF

  // Pointer to sub-string
  char ParsePointer[16];

  strcpy ((char*) ParsePointer, strtok (NULL, ","));

  if (strlen (ParsePointer) > 0) // Check if string exists
    {
      // Check what to do
      if (strncmp (ParsePointer, "GET_TEMP", 8) == 0)
	{
	  float temp = BMP280_ReadTemperature (&Bmp280);
	  printf ("Temperature = %.1f\r\n", temp);
	}
      else if (strncmp (ParsePointer, "GET_HUM", 7) == 0)
	{
	  float tempT, tempP, tempH;
	  BMP280_ReadSensorData (&Bmp280, &tempP, &tempT, &tempH);
	  printf ("Humidity = %.1f\r\n", tempH);
	}
      else if (strncmp (ParsePointer, "GET_PRESS", 9) == 0)
	{
	  float tempT, tempP, tempH;
	  BMP280_ReadSensorData (&Bmp280, &tempP, &tempT, &tempH);
	  printf ("Pressure = %.1f\r\n", tempP);
	}
      else if (strncmp (ParsePointer, "DEBUG_MODE_ON", 13) == 0)
	{
	  //TODO
	}
      else if (strncmp (ParsePointer, "DEBUG_MODE_OFF", 14) == 0)
	{
	  //TODO
	}
    }
}

static void Parser_ParseGPS(void)
{
  // String to parse:
  //  SLEEP
  //  WAKEUP
  //  SEND_CONFIG
  //  GET_DATA
  //  DEBUG_MODE_ON
  //  DEBUG_MODE_OFF

    // Pointer to sub-string
    char ParsePointer[16];

    strcpy ((char*) ParsePointer, strtok (NULL, ","));

    if (strlen (ParsePointer) > 0) // Check if string exists
      {
        // Check what to do
        if (strncmp (ParsePointer, "SLEEP", 5) == 0)
  	{
            GPS_Sleep ();
  	}
        else if (strncmp (ParsePointer, "WAKEUP", 6) == 0)
  	{
            GPS_Wakeup ();
  	}
        else if (strncmp (ParsePointer, "SEND_CONFIG", 11) == 0)
  	{
            GPS_SendCommands();
  	}
        else if (strncmp (ParsePointer, "GET_DATA", 8) == 0)
	{
            GPSDataState = WAITING_FOR_DATA;
	}
        else if (strncmp (ParsePointer, "DEBUG_MODE_ON", 13) == 0)
  	{
  	  //TODO
  	}
        else if (strncmp (ParsePointer, "DEBUG_MODE_OFF", 14) == 0)
  	{
  	  //TODO
  	}
      }
}

static void Parser_ParseTIME(void)
{
  //  GET
  // SET_TIME H=%2d M=%2d S=%2d
  // SET_DATE D=%2d M=%2d Y=%2d
  // SET_UTC_TIME D=%2d MO=%2d Y=%2d H=%2d MI=%2d S=%2d
  // DEBUG_MODE_ON
  // DEBUG_MODE_OFF

    // Pointer to sub-string
    char ParsePointer[64];

    strcpy ((char*) ParsePointer, strtok (NULL, ","));

    if (strlen (ParsePointer) > 0) // Check if string exists
      {
        // Check what to do
      if (strncmp (ParsePointer, "GET", 3) == 0)
	{
	  RTC_TimeTypeDef sTime;
	  HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
	  RTC_DateTypeDef sDate;
	  HAL_RTC_GetDate (&hrtc, &sDate, RTC_FORMAT_BIN);
	  printf (
	      "Current date and time: %02d-%02d-%04d Time: %02d:%02d:%02d\n\r",
	      sDate.Date, sDate.Month, sDate.Year + 2000, sTime.Hours,
	      sTime.Minutes, sTime.Seconds);
	}
      else if (strncmp (ParsePointer, "SET_TIME", 8) == 0)
	{
	  RTC_TimeTypeDef sTime;
	  int h, m, s;
	  if (sscanf ( ParsePointer, "SET_TIME H=%2d M=%2d S=%2d",
		      &h, &m, &s) == 3)
	    {
	      sTime.Hours = (uint8_t)h;
	      sTime.Minutes = (uint8_t)m;
	      sTime.Seconds = (uint8_t)s;
	      HAL_RTC_SetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
	    }
	  else
	    {
	      sTime.Hours = 0;
	      sTime.Minutes = 0;
	      sTime.Seconds = 0;
	      HAL_RTC_SetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
	    }
	}
      else if (strncmp (ParsePointer, "SET_DATE", 8) == 0)
	{
	  RTC_DateTypeDef sDate;
	  int d, m, y;
	  if (sscanf (ParsePointer, "SET_DATE D=%2d M=%2d Y=%2d",
		      &d, &m, &y) == 3)
	    {
	      sDate.WeekDay = 0;
	      sDate.Date = (uint8_t)d;
	      sDate.Month = (uint8_t)m;
	      sDate.Year = (uint8_t)y;
	      HAL_RTC_SetDate (&hrtc, &sDate, RTC_FORMAT_BIN);
	    }
	  else
	    {
	      sDate.WeekDay = 0;
	      sDate.Date = 0;
	      sDate.Month = 0;
	      sDate.Year = 0;
	      HAL_RTC_SetDate (&hrtc, &sDate, RTC_FORMAT_BIN);
	    }
	}
      else if (strncmp (ParsePointer, "SET_UTC_TIME", 8) == 0)
      	{
      	  DateTime datetime;
      	  int d, mo, y, h, mi, s;
      	  if (sscanf ( ParsePointer, "SET_UTC_TIME D=%2d MO=%2d Y=%2d H=%2d MI=%2d S=%2d",
      		      &d, &mo, &y, &h, &mi, &s) == 6)
      	    {
      	      datetime.hour = h;
	      datetime.minute =  mi;
	      datetime.second = s;
	      datetime.day = d;
	      datetime.month =  mo;
	      datetime.year = (y + 2000);
      	      LT_SetTime(&hrtc, &datetime);
      	    }
      	  else
      	    {
	      datetime.hour = 0;
	      datetime.minute = 0;
	      datetime.second = 0;
	      datetime.day = 0;
	      datetime.month = 0;
	      datetime.year = 0;
	      LT_SetTime(&hrtc, &datetime);
      	    }
      	}
      else if (strncmp (ParsePointer, "DEBUG_MODE_ON", 13) == 0)
  	{
  	  //TODO
  	}
        else if (strncmp (ParsePointer, "DEBUG_MODE_OFF", 14) == 0)
  	{
  	  //TODO
  	}
      }
}

// Main parsing function
// Commands to detect:
// 	BMP280
// 	GPS
//
//
void Parser_Parse (uint8_t *DataToParse)
{
  // Pointer to sub-string
  char *ParsePointer = strtok ((char*) DataToParse, " "); // Create a sub-string

  // Compare provided array with line to parse with command template
  if (strcmp ("BMP280", ParsePointer) == 0)
    {
      Parser_ParseBMP280 (); // Call a parsing function for the BMP280 command
    }
  else if (strcmp ("GPS", ParsePointer) == 0)
    {
      Parser_ParseGPS (); // Call a parsing function for the GPS command
    }
  else if (strcmp ("TIME", ParsePointer) == 0)
    {
      Parser_ParseTIME (); // Call a parsing function for the TIME command
    }
  else
    printf ("Problem with parsing\r\n");

}

