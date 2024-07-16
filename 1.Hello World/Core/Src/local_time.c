/*
 * local_time.c
 *
 *  Created on: Jul 15, 2024
 *      Author: piotr
 */

#include "main.h"
#include "local_time.h"
#include "rtc.h"
#include "NEO_6m.h"

typedef enum {
  winter = 1,
  summer = 2
}UTC_Offset_t;

UTC_Offset_t UTC_Offset = winter;
  static const int list_mth[12] =
    { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };

  uint8_t LeapYear(uint16_t year) {
    uint8_t Leap;
      if (year % 4 == 0) {
          if (year % 100 == 0) {
              if (year % 400 == 0)
        	Leap = 1;
              else
        	Leap = 0;
          } else {
              Leap =  1;
          }
      } else {
	  Leap =  0;
      }
      Leap = Leap ? 29 : 28;
    return Leap;
  }

//Calculate day of the week
uint8_t DayOfWeek (uint8_t year, uint8_t month, uint8_t day)
{
  if (month < 3)
      {
        year -= 1;
      }
    return (uint8_t) ((year + (year / 4) - (year / 100) + (year / 400)
        + list_mth[month - 1] + day) % 7);
}
// Set local time based on data from gps and modify it according to Poland daylight saving offset
void LT_SetTime (RTC_HandleTypeDef *hrtc, DateTime *time)
{

  static uint8_t checkChangeDate = 0; // variable to check if last sunday dates were already found
  static uint8_t marchLastSunday = 0, octoberLastSunday = 0;

  static uint8_t listDaysMonths[12] =
          { 31, 31, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

  if (checkChangeDate != time->year)
    {
      for (uint8_t i = 31; i >= 25; i--)
	{
	  if (DayOfWeek (time->year, 3, i) == 0)
	    {
	      marchLastSunday = i;
	      break;
	    }
	}
      for (uint8_t i = 31; i >= 25; i--)
	{
	  if (DayOfWeek (time->year, 10, i) == 0)
	    {
	      octoberLastSunday = i;
	      break;
	    }
	}
    }

  ///determine daylight season (winter UTC + 1, summer UTC + 2)
  if ((time->month >= 4 && time->month <= 9) || (time->month == 3 && time->day > marchLastSunday )||( time->month == 10
      && time->day < octoberLastSunday )||( time->month == 3 && time->day == marchLastSunday && time->hour >= 1 )||
      (time->month == 10 && time->day == octoberLastSunday && time->hour < 1))
    {
      UTC_Offset = summer;
    }
  else UTC_Offset = winter;

  time->hour = time->hour + UTC_Offset;

  if (time->hour > 23)
    {
      time->hour = time->hour - 24;
      time->day++;
    }
  if(time->day > listDaysMonths[time->month] ||( time->month == 2 && LeapYear(time->year) == time->day ))
    {
      time->day = 1;
      time->month ++;
    }
  if(time->month > 12)
    {
      time->month = 1;
	  time->year ++;
    }
  RTC_DateTypeDef sDate;
  sDate.Year = time->year - 2000;
  sDate.Month = time->month;
  sDate.Date = time->day;
  sDate.WeekDay = DayOfWeek(time->year, time->month, time->day);
  RTC_TimeTypeDef sTime;
  sTime.Hours = time->hour;
  sTime.Minutes = time->minute;
  sTime.Seconds = time->second;

  HAL_RTC_SetDate(hrtc, &sDate, RTC_FORMAT_BIN);
  HAL_RTC_SetTime(hrtc, &sTime, RTC_FORMAT_BIN);
}
