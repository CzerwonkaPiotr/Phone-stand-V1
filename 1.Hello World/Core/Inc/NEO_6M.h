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
  DATA_RECEIVED = 2
}GPSGetDataState;

void GPS_SendCommands (void);
void GPS_Init(UART_HandleTypeDef *huart);
void GPS_Sleep(void);
void GPS_Wakeup(void);
uint8_t GPS_RunProcess (void);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

#endif /* INC_NEO_6M_H_ */
