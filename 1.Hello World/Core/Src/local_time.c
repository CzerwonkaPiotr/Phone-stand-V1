/*
 * local_time.c
 *
 *  Created on: Jul 15, 2024
 *      Author: piotr
 *
 *  This file provides functions for calculating and setting local time,
 *  including adjustments for daylight saving time (DST) specific to Poland.
 */

#include "main.h"
#include "local_time.h"
#include "rtc.h"
#include "NEO_6m.h"

/**
 * @brief Enumeration for Poland's time offset relative to UTC.
 *        - winter: UTC + 1
 *        - summer: UTC + 2
 */
typedef enum {
  winter = 1,
  summer = 2
}UTC_Offset_t;

/*
 * Global variable that holds the current UTC offset (winter or summer).
 * This value is determined dynamically based on the date.
 */
UTC_Offset_t UTC_Offset = winter;

/*
 * Lookup array used in day-of-week calculations.
 * Each index (0 to 11) corresponds to a month (January to December).
 */
  static const int list_mth[12] =
    { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };

  /**
   * @brief Determines whether a given year is a leap year, and returns
   *        the number of days in February for that year (28 or 29).
   *
   * @param[in] year  The year in question (e.g., 2024).
   * @return uint8_t  29 if leap year, otherwise 28.
   */
  uint8_t LeapYear(uint16_t year)
  {
      uint8_t Leap;
      if (year % 4 == 0)
      {
          if (year % 100 == 0)
          {
              if (year % 400 == 0)
                  Leap = 1;  // Divisible by 400 => leap year
              else
                  Leap = 0;  // Divisible by 100 but not 400 => not a leap year
          }
          else
          {
              Leap = 1;      // Divisible by 4 but not by 100 => leap year
          }
      }
      else
      {
          Leap = 0;          // Not divisible by 4 => not a leap year
      }

      /*
       * Return 29 if it's a leap year, else 28.
       * This is later used to check how many days are in February.
       */
      Leap = Leap ? 29 : 28;
      return Leap;
  }

  /**
   * @brief Calculates the day of the week for a given date (year, month, day).
   *        The result is an integer from 0 to 6, typically:
   *            0 = Sunday, 1 = Monday, ..., 6 = Saturday
   *
   * @note The algorithm shifts the year when months are Jan/Feb (because
   *       they are treated as the 13th/14th month of the previous year
   *       in some day-of-week computations).
   *
   * @param[in] year   The year (e.g., 2024).
   * @param[in] month  The month (1–12).
   * @param[in] day    The day of the month (1–31).
   *
   * @return uint8_t   Day of the week (0–6).
   */
  uint8_t DayOfWeek (uint8_t year, uint8_t month, uint8_t day)
  {
      if (month < 3)
      {
          // Treat Jan and Feb as months 13 and 14 of the previous year
          year -= 1;
      }

      // Using a known formula for day of the week calculation
      return (uint8_t)(( year
                       + (year / 4)
                       - (year / 100)
                       + (year / 400)
                       + list_mth[month - 1]
                       + day
                      ) % 7);
  }

  /**
   * @brief Updates the RTC date and time structure based on GPS data and
   *        applies the appropriate daylight saving time offset for Poland.
   *
   * @param[in] hrtc  Pointer to the RTC handle (for HAL RTC functions).
   * @param[in] time  Pointer to the DateTime structure obtained from the GPS module.
   *
   * This function:
   *   1. Determines the last Sundays of March and October (for DST changes).
   *   2. Decides if the current date/time falls in summer or winter time (UTC+2 or UTC+1).
   *   3. Adjusts the hour, and if necessary, the day/month/year rollover.
   *   4. Writes the adjusted time to the hardware RTC using HAL functions.
   */
void LT_SetTime (RTC_HandleTypeDef *hrtc, DateTime *time)
{

  // Checks if the code has already found the "last Sundays" for the current year
  static uint8_t checkChangeDate = 0;
  static uint8_t marchLastSunday = 0, octoberLastSunday = 0;

  // Number of days in each month (for quick reference).
  // Index: 0=Jan, 1=Feb, 2=Mar, etc.
  static uint8_t listDaysMonths[12] =
          { 31, 31, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

  /*
   * If the 'year' in time has changed since we last ran this check, recalculate
   * the day of the last Sunday in March and October.
   */
  if (checkChangeDate != time->year)
    {
      for (uint8_t i = 31; i >= 25; i--)
	{
	  if (DayOfWeek (time->year, 3, i) == 6) // 6 represents sunday
	    {
	      marchLastSunday = i;
	      break;
	    }
	}
      for (uint8_t i = 31; i >= 25; i--)
	{
	  if (DayOfWeek (time->year, 10, i) == 6) // 6 represents sunday
	    {
	      octoberLastSunday = i;
	      break;
	    }
	}
    }

  /*
   * Determine whether to use summer (UTC+2) or winter (UTC+1) time
   * for Poland.
   *
   * Conditions for summer time (UTC+2):
   *   - Month is between April (4) and September (9) inclusive, or
   *   - In March, after the last Sunday (including if the last Sunday is current day and hour >= 1)
   *   - In October, before the last Sunday (including if the last Sunday is current day but hour < 1)
   */
  if ((time->month >= 4 && time->month <= 9)
      || (time->month == 3 && time->day > marchLastSunday)
      || (time->month == 10 && time->day < octoberLastSunday)
      || (time->month == 3 && time->day == marchLastSunday && time->hour >= 1)
      || (time->month == 10 && time->day == octoberLastSunday && time->hour < 1))
  {
      UTC_Offset = summer;
  }
  else
  {
      UTC_Offset = winter;
  }

  // Apply the UTC offset to the current time
  time->hour = time->hour + UTC_Offset;

  // Adjust for day rollover if adding offset goes past 23:59
  if (time->hour > 23)
    {
      time->hour = time->hour - 24;

      time->day++;
    }

  // Check if day goes beyond the valid days in the current month.
  // Also, account for February in a leap year by checking LeapYear().
  // (time->month) - 1 => correct index in listDaysMonths.
  // Additional check: if month == 2 and day < LeapYear(time->year)
  // might be a condition to handle February edge cases more explicitly.
  if(time->day > listDaysMonths[(time->month)-1] ||( time->month == 2 && time->day < LeapYear(time->year)))
    {
      time->day = 1;
      time->month ++;
    }

  // If month goes beyond December, roll over to the next year
  if(time->month > 12)
    {
      time->month = 1;
      time->year ++;
    }

  RTC_DateTypeDef sDate = { 0 };
  sDate.Year = (uint8_t)(time->year - 2000); // RTC stores years as offset from 2000
  sDate.Month = (uint8_t)time->month;
  sDate.Date = (uint8_t)time->day;
  sDate.WeekDay = DayOfWeek(time->year, time->month, time->day);
  RTC_TimeTypeDef sTime = { 0 };
  sTime.Hours = (uint8_t)time->hour;
  sTime.Minutes = (uint8_t)time->minute;
  sTime.Seconds = (uint8_t)time->second;

  // Write date/time to the RTC
  HAL_RTC_SetDate (hrtc, &sDate, RTC_FORMAT_BIN);
  HAL_RTC_SetTime (hrtc, &sTime, RTC_FORMAT_BIN);
}
