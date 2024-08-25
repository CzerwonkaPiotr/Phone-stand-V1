/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "rtc.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stdio.h"
#include "bmp280.h"
#include "NEO_6M.h"
#include "ring_buffer.h"
#include "parse_commands.h"
#include "alarms_rtc.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BMP280_ADDRESS 0x76
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);

BMP280_t Bmp280;
float Temperature, Pressure, Humidity;
uint8_t buffer[64], bufferLenght;

RingBuffer_t ReceiveBuffer; // Ring Buffer for Receiving from UART
uint8_t ReceiveTmp; // Temporary variable for receiving one byte
uint8_t ReceviedLines; // Complete lines counter

uint8_t ReceivedData[64]; // A buffer for parsing

typedef enum
{
  ALARM_ACTIVE = 1,
  ALARM_INACTIVE = 0
}ALARM_STATE;

volatile ALARM_STATE AlarmA_active = ALARM_ACTIVE;
volatile ALARM_STATE AlarmB_active = ALARM_INACTIVE;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */


  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USB_DEVICE_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_RTC_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */
	HAL_Delay(500);
	if (BMP280_Init(&Bmp280, &hi2c1, BMP280_ADDRESS)) printf("BMP280 init failed\n\r");

  GPS_Init(&huart1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
    {

      //
      //// Alarm A sequence
      //

      if(AlarmA_active == ALARM_ACTIVE)
	{
	  AlarmA_active = GpsRunSequence();
	}
      //		BMP280_SetMode(&Bmp280, BMP280_FORCEDMODE); // Measurement is made and the sensor goes to sleep
//		HAL_Delay(1000);
//		BMP280_ReadSensorData(&Bmp280, &Pressure, &Temperature, &Humidity);
//
//		printf( ">Temperature = %.1f\n\r>Pressure = %.1f\n\r>Humidity = %.1f\n\n\r", Temperature, Pressure, Humidity);


      RTC_TimeTypeDef sTime = { 0 };
      HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
      RTC_DateTypeDef sDate = { 0 };
      HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
      uint8_t lastSec;
      if(sTime.Seconds != lastSec)
	{
	  printf ("Current date and time: %02d-%02d-%04d Time: %02d:%02d:%02d\n\r",
	  	      sDate.Date, sDate.Month, sDate.Year + 2000, sTime.Hours,
	  	      sTime.Minutes, sTime.Seconds);
	  lastSec = sTime.Seconds;
	}

      // Check if there is something to parse - any complete line
      	  if(ReceviedLines > 0)
      	  {
      			// Take one line from the Ring Buffer to work-buffer
      			Parser_TakeLine(&ReceiveBuffer, ReceivedData);

      			// Decrement complete lines counter
      			ReceviedLines--;

      			// Run the parser with work-buffer
      			Parser_Parse(ReceivedData);
      	  }


    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* OTG_FS_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(OTG_FS_IRQn, 10, 0);
  HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
  /* USART1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
  /* RTC_Alarm_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);
}

/* USER CODE BEGIN 4 */
int _write(int file, char *ptr, int len) {
    static uint8_t rc = USBD_OK;

    do {
        rc = CDC_Transmit_FS((uint8_t*)ptr, len);
    } while (USBD_BUSY == rc);

    if (USBD_FAIL == rc) {
        /// NOTE: Should never reach here.
        /// TODO: Handle this error.
        return 0;
    }
    return len;
}

void CDC_ReceiveCallback (uint8_t *Buffer, uint8_t Length)
{
  if (RB_OK == RB_Write (&ReceiveBuffer, Buffer, Length))
    {
      // Check if current byte is the endline char
      for(uint32_t i = 0; i < Length ; i ++)
	{
	  if (Buffer[i] == ENDLINE)
	    {
	      // Increment complete lines
	      ReceviedLines++;
	    }
	}
    }
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{

  AlarmA_active = ALARM_ACTIVE;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
