/*
 * NEO_6M.c
 *
 *  Created on: Jul 10, 2024
 *      Author: piotr
 *
 *  This file contains functions to configure and interface with the
 *  NEO-6M GPS module via UART. It provides:
 *    - GPS initialization
 *    - Sending configuration commands (turning specific NMEA messages ON/OFF,
 *      enabling low-power mode, etc.)
 *    - Waking and sleeping the GPS module
 *    - Receiving and parsing GPS ZDA messages
 *    - Updating the RTC date and time when a valid GPS timestamp is received
 */

#include "main.h"
#include "string.h"
#include "stdio.h"
#include "NEO_6M.h"
#include "local_time.h"
#include "rtc.h"
#include "alarms_rtc.h"
#include "usart.h"

#define RX_BUFFER_SIZE 256

/*
 * Uncomment this macro definition if you want to enable printing debug messages
 * via USB CDC (e.g., for tracing the received GPS data).
 */
//#define USB_CDC_IS_ACTIVE

/*
 * Global/static variables used for GPS data handling.
 */
static UART_HandleTypeDef *gps_huart;               ///< Pointer to UART handle configured for GPS communication
static uint8_t messageBuffer[RX_BUFFER_SIZE];       ///< Buffer to store the final, valid GPS message (e.g., ZDA sentence)
static uint8_t rx_buffer[RX_BUFFER_SIZE];           ///< Intermediate buffer for incoming UART data
volatile static uint8_t rx_data;                    ///< Temporary variable for receiving a single character from UART
static uint8_t rx_index = 0;                        ///< Index for writing into rx_buffer
static uint32_t TimerGetGPSData = 0;                ///< Timestamp for periodically checking if new data is available
static uint8_t notReceivedDataCount = 0;            ///< Counter for how many times we failed to receive valid GPS data

GPSGetDataState GPSDataState = NO_DATA_NEEDED;      ///< State machine variable controlling GPS data acquisition


/**
 * @brief Sends a series of configuration commands to the GPS module
 *        to enable or disable specific NMEA sentences.
 *
 * This function sends:
 *   - PUBX commands to turn off GLL, GSA, GGA, RMC, GSV, VTG
 *   - PUBX command to turn on ZDA (for date/time data)
 *   - UBX configuration commands (PM2 for low power mode, GNSS for constellation)
 */
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

    /*
      * UBX-CFG-PM2 command — configure low power mode.
      * Each UBX command has a specific header (0xB5, 0x62),
      * class and ID bytes (e.g., 0x06, 0x3B), payload length,
      * payload data, and checksum at the end.
      */
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

    /*
     * UBX-CFG-GNSS command — configure GNSS constellations (e.g., GPS, GLONASS).
     */

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

/**
 * @brief Initializes the GPS module by:
 *        - Storing the UART handle pointer
 *        - Enabling the UART receive interrupt
 *        - Sending the initial configuration commands
 *
 * @param[in] huart  Pointer to the UART handle for GPS communication.
 */
void GPS_Init (UART_HandleTypeDef *huart)
{
  gps_huart = huart;
  HAL_UART_Receive_IT (gps_huart,(uint8_t *) &rx_data, 1);
  GPS_SendCommands();
}

/**
 * @brief Sends commands to put the GPS module into sleep/backup mode.
 *
 *  This involves UBX-CFG-RXM and UBX-RXM-PMREQ commands.
 */
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

#ifdef USB_CDC_IS_ACTIVE
  printf ("-> GPS sleep\n\r");
#endif
}

/**
 * @brief Wakes the GPS module by sending a series of dummy bytes
 *        over the UART TX line.
 */
void GPS_Wakeup (void)
{
  // Simply transmit a series of 0xFF bytes to toggle the RX pin high so the module wakes up
  uint8_t command[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  HAL_UART_Transmit (gps_huart, command, sizeof(command), HAL_MAX_DELAY);
#ifdef USB_CDC_IS_ACTIVE
  printf ("-> GPS wake up\n\r");
#endif
}

/**
 * @brief Callback triggered by the HAL UART IRQ when a new byte is received.
 *        Accumulates NMEA message data, detects the start '$' and end '\n'
 *        of sentences, and stores a ZDA message in a dedicated buffer if found.
 *
 * @param[in] huart  Pointer to the UART handle where the data was received.
 */
void HAL_UART_RxCpltCallback (UART_HandleTypeDef *huart)
{
    // Only handle data from the GPS UART
    if (huart->Instance == gps_huart->Instance)
    {
#ifdef USB_CDC_IS_ACTIVE
        printf ("%c", rx_data);
#endif

        if (rx_index < RX_BUFFER_SIZE)
        {
            // '$' indicates the start of a new NMEA sentence
            if (rx_data == '$')
            {
                rx_index = 0;
            }

            rx_buffer[rx_index++] = rx_data;

            // If we are waiting for data and encounter '\n', it might be the end of an NMEA sentence
            if (GPSDataState == WAITING_FOR_DATA && rx_data == '\n')
            {
                rx_buffer[rx_index] = '\0'; // Null-terminate the string
                rx_index = 0;

                // Check if the received sentence starts with "$GPZDA"
                if (strncmp((char*)rx_buffer, "$GPZDA", 6) == 0)
                {
                    // Copy the entire message to the messageBuffer for later parsing
                    strcpy((char*)messageBuffer, (const char*)rx_buffer);
                    GPSDataState = DATA_RECEIVED;
                }
            }

            // If we got some other message (not "$GPZDA") and reached the end, re-send commands
            // to ensure the module is outputting only ZDA messages
            if (strncmp((char*)rx_buffer, "$GPZDA", 6) != 0 && rx_data == '\n')
            {
                GPS_SendCommands();
            }
        }
        else
        {
            // If we exceed the buffer size, reset index to avoid overflow
            rx_index = 0;
        }

        /*
         * If we are still waiting for data and haven't reached the sentence end,
         * restart the UART receive interrupt to continue capturing data.
         */
        if (GPSDataState == WAITING_FOR_DATA && rx_data != '\n')
        {
            HAL_UART_Receive_IT(gps_huart, (uint8_t*)&rx_data, 1);
        }
    }
}

/**
 * @brief GPS state machine that:
 *        - Initializes GPS if no data was needed before but is needed now
 *        - Periodically checks if data arrived; if not, reinitialize the module
 *        - If ZDA data is received, parse it to set RTC time, then stop GPS
 *
 * @return 1 if still running/waiting for data, 0 if the process is complete
 */
uint8_t GPS_RunProcess (void)
{
    // If GPS data was not needed before, but now is requested:
    if (GPSDataState == NO_DATA_NEEDED)
    {
        GPS_Init(&huart1);
        GPSDataState = WAITING_FOR_DATA;
#ifdef USB_CDC_IS_ACTIVE
        printf ("-> No GPS data was needed but now it is, turn on IT on UART\n\r");
#endif
    }
    // Check every second if we are still waiting for data
    else if (((HAL_GetTick() - TimerGetGPSData) > 1000) && (GPSDataState == WAITING_FOR_DATA))
    {
        TimerGetGPSData = HAL_GetTick();
        HAL_UART_Receive_IT(gps_huart, (uint8_t*)&rx_data, 1);
#ifdef USB_CDC_IS_ACTIVE
        printf ("-> Waiting for data from GPS, notReceivedDataCount = %d\n\r", notReceivedDataCount);
#endif
        // If we've waited too long without valid data, try re-initializing the GPS
        if (notReceivedDataCount > 19)
        {
            GPS_Sleep();
            HAL_Delay(10);
            GPS_Wakeup();
            HAL_Delay(10);
            GPS_SendCommands();
            notReceivedDataCount = 0;
        }
        else
        {
            notReceivedDataCount++;
        }
    }
    // When valid data is received
    else if (GPSDataState == DATA_RECEIVED)
    {
#ifdef USB_CDC_IS_ACTIVE
        printf ("-> GPS data received !!!\n\r");
        printf ("-> messageBuffer = %s\n\r", messageBuffer);
#endif

        // Create a DateTime struct to parse the ZDA message into
        DateTime currentDateTime = { 0 };

        // Parse the ZDA string. E.g., "$GPZDA,hhmmss.ss,dd,mm,yyyy,xx,xx"
        // We only care about hour, minute, second, day, month, year
        if (sscanf((char*)messageBuffer,
                   "$GPZDA,%2d%2d%2d.%*2d,%2d,%2d,%4d,%*2d,%*2d",
                   &currentDateTime.hour, &currentDateTime.minute,
                   &currentDateTime.second, &currentDateTime.day,
                   &currentDateTime.month, &currentDateTime.year) == 6)
        {
            // Set the RTC time using the parsed data
            LT_SetTime(&hrtc, &currentDateTime);

            // Trigger an alarm or flag that valid GPS data has been received
            SetGPSAlarmADataOk();

#ifdef USB_CDC_IS_ACTIVE
            RTC_TimeTypeDef sTime = { 0 };
            HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

            RTC_DateTypeDef sDate = { 0 };
            HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

            printf ("-> Date updated: %02d-%02d-%04d Time: %02d:%02d:%02d\n\r",
                    sDate.Date, sDate.Month, sDate.Year + 2000,
                    sTime.Hours, sTime.Minutes, sTime.Seconds);
#endif
            // Put GPS module to sleep since we got what we needed
            GPS_Sleep();
            GPSDataState = NO_DATA_NEEDED;
            return 0;  // Process completed successfully
        }
        else if (strncmp((char*)messageBuffer, "$GPZDA,,,,,00,00", 16) == 0)
        {
            // Means GPS gave an empty/no-fix response
            SetGPSAlarmADataNOk();
#ifdef USB_CDC_IS_ACTIVE
            printf ("-> GPS DATA NOT ACQUIRED, NO FIX\n\r");
#endif
            GPSDataState = NO_DATA_NEEDED;
            return 0;  // Process completed, but no valid data
        }
        else
        {
            // Some other invalid data
            SetGPSAlarmADataNOk();
#ifdef USB_CDC_IS_ACTIVE
            printf ("GPS DATA INCORRECT\n\r");
#endif
            GPSDataState = NO_DATA_NEEDED;
            return 0;  // Process completed with incorrect data
        }
    }

    // If none of the above conditions are met, keep running
    return 1; // Still waiting or still in progress
}
