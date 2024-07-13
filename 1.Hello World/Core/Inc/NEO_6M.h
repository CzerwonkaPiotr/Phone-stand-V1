/*
 * NEO_6M.h
 *
 *  Created on: Jul 10, 2024
 *      Author: piotr
 */

#ifndef INC_NEO_6M_H_
#define INC_NEO_6M_H_


typedef struct {
    int hour;
    int minute;
    int second;
    int day;
    int month;
    int year;
} DateTime;



typedef enum {
  NO_DATA_NEEDED = 0,
  WAITING_FOR_DATA = 1,
  DATA_READY = 2
}GPSGetDataState;



typedef enum {
  DATA_ACQUIRED = 0,
  DATA_NOT_ACQUIRED_NO_FIX = 1,
  DATA_INCORRECT = 2

}GPSDataAcquiredState;



void GPS_ManageCommands (void);
void GPS_Init(UART_HandleTypeDef *huart, uint32_t UTC_updInt);
void GPS_Sleep(void);
void GPS_Wakeup(void);
void GPS_RUN (void);
GPSDataAcquiredState GPS_GetDateTime(DateTime * datetime);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

#endif /* INC_NEO_6M_H_ */
