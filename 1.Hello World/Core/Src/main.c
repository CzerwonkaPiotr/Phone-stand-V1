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
#include "spi.h"
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
#include "button.h"
#include "epd2in9.h"
#include "epdif.h"
#include "epdpaint.h"
#include "imagedata.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BMP280_ADDRESS 0x76
#define ALARM_ACTIVE 1
#define ALARM_INACTIVE 0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

extern uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);


BMP280_t Bmp280;
float Temperature, Pressure, Humidity;

//uint8_t buffer[64], bufferLenght; //TODO not sure if legacy and has to be removed

RingBuffer_t ReceiveBuffer; // Ring Buffer for Receiving from UART
//uint8_t ReceiveTmp; // Temporary variable for receiving one byte
uint8_t ReceviedLines; // Complete lines counter

uint8_t ReceivedData[64]; // A buffer for parsing


volatile uint8_t AlarmA_active = ALARM_ACTIVE;
volatile uint8_t AlarmB_active = ALARM_ACTIVE;

TButton userButton;

#define COLORED      0
#define UNCOLORED    1

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
  unsigned char* frame_buffer = (unsigned char*)malloc(EPD_WIDTH * EPD_HEIGHT / 8);

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
  MX_SPI1_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */


   if(__HAL_PWR_GET_FLAG(PWR_FLAG_SB) != RESET) // Check and handle if the system was resumed from StandBy mode
   {
      __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);  // Clear Standby flag
    /* Clear all related wakeup flags*/
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
   }
   else
     {
       AlarmA_active = ALARM_ACTIVE;
       AlarmB_active = ALARM_ACTIVE;
     }
   /* Disable all used wakeup sources: PWR_WAKEUP_PIN1 */
   HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1);

   


  /**** check alarm wakeup ****/
    /* read programmed alarm */
  uint8_t wakeUpRoot = 0;
  wakeUpRoot = Check_RTC_Alarm();

  if (wakeUpRoot == 1 || wakeUpRoot == 2)
    {
      AlarmA_active = ALARM_ACTIVE;
    }
  else if (wakeUpRoot == 3)
    {
      AlarmB_active = ALARM_ACTIVE;
    }
  else if (wakeUpRoot == 9)
      {
        AlarmA_active = ALARM_ACTIVE;
        AlarmB_active = ALARM_ACTIVE;
      }


  //HAL_Delay (200);
  if (BMP280_Init (&Bmp280, &hi2c1, BMP280_ADDRESS))
    printf ("-> BMP280 init failed\n\r");

  GPS_Init (&huart1);
//  HAL_Delay (2000);


  ButtonInitKey(&userButton, BUTTON1_GPIO_Port, BUTTON1_Pin, 20, 1000, 500);
  //ButtonRegisterPressCallback(&userButton, simulateAlarmB); // Register callback for button pressed action
  //ButtonRegisterLongPressCallback(&userButton, simulateAlarmA); // Register callback for button long press action

  //printf ("-> AlarmA_active = %d\n\r-> AlarmB_active = %d\n\r", AlarmA_active, AlarmB_active);

  EPD epd;

  if (EPD_Init(&epd, lut_full_update) != 0)
    {
      printf("e-Paper init failed\n");
      return -1;
    }

    Paint paint;
    Paint_Init(&paint, frame_buffer, epd.width, epd.height);
    Paint_Clear(&paint, UNCOLORED);

      switch (wakeUpRoot)
      {
        case 0:
          Paint_DrawStringAt(&paint, 0, 14, "WAKEUP ??? (0)", &Font16, COLORED);
          break;
        case 1:
              Paint_DrawStringAt(&paint, 0, 14, "RTC alarmA", &Font16, COLORED);
              break;
        case 2:
              Paint_DrawStringAt(&paint, 0, 14, "RTC alarmA data NOK", &Font16, COLORED);
              break;
        case 3:
              Paint_DrawStringAt(&paint, 0, 14, "RTC alarmB", &Font16, COLORED);
              break;
        case 9:
      Paint_DrawStringAt (&paint, 0, 14, "RTC alarmB or alarmA", &Font16, COLORED);
              break;
      };
    /* Display the frame_buffer */

    /* Draw something to the frame buffer */
    Paint_DrawRectangle(&paint, 16, 60, 56, 110, COLORED);
    Paint_DrawLine(&paint, 16, 60, 56, 110, COLORED);
    Paint_DrawLine(&paint, 56, 60, 16, 110, COLORED);
    Paint_DrawCircle(&paint, 120, 90, 30, COLORED);
    Paint_DrawFilledRectangle(&paint, 16, 130, 56, 180, COLORED);
    Paint_DrawFilledCircle(&paint, 120, 160, 30, COLORED);


    EPD_SetFrameMemory(&epd, frame_buffer, 0, 0, Paint_GetWidth(&paint), Paint_GetHeight(&paint));
    EPD_DisplayFrame(&epd);
  //EPD_DelayMs (&epd, 3000);

    /**
     *  there are 2 memory areas embedded in the e-paper display
     *  and once the display is refreshed, the memory area will be auto-toggled,
     *  i.e. the next action of SetFrameMemory will set the other memory area
     *  therefore you have to set the frame memory and refresh the display twice.
     */
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
    {
      ButtonTask(&userButton); // Machine state task for button
    HAL_GPIO_WritePin (LED1_GPIO_Port, LED1_Pin, SET);

      //
      //// Alarm A sequence
      //

    if (AlarmA_active == ALARM_ACTIVE)
    {
      AlarmA_active = GpsRunSequence ();
    }

    //
    //// Alarm B sequence
    //

      if (AlarmB_active == ALARM_ACTIVE)
    {
      BMP280_SetMode (&Bmp280, BMP280_FORCEDMODE); // Measurement is made and the sensor goes to sleep
      //HAL_Delay (50); // TODO no delays allowed
      BMP280_ReadSensorData (&Bmp280, &Pressure, &Temperature, &Humidity);
      //printf ("-> Odczytano dane z czujnika BME280\n\r");

      //printf (
      //   "-> Temperature = %.1f   Pressure = %.1f   Humidity = %.1f\n\r",
      //    Temperature, Pressure, Humidity);
      char atmData[128];
      Paint_Clear (&paint, UNCOLORED);
      //HAL_Delay (500);
      sprintf (atmData, "-> Temperature = %.1f Pressure = %.1f Humidity = %.1f", Temperature, Pressure, Humidity);

      Paint_DrawStringAt (&paint, 0, 14, atmData, &Font8, COLORED);
      RTC_TimeTypeDef sTime =
      { 0 };
      HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
      RTC_DateTypeDef sDate =
      { 0 };
      HAL_RTC_GetDate (&hrtc, &sDate, RTC_FORMAT_BIN);
      sprintf (atmData, "Current date and time: %02d-%02d-%04d", sDate.Date, sDate.Month, sDate.Year + 2000);
      Paint_DrawStringAt (&paint, 0, 44, atmData, &Font12, COLORED);
      sprintf (atmData, "Time: %02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
      Paint_DrawStringAt (&paint, 0, 74, atmData, &Font12, COLORED);
      EPD_SetFrameMemory (&epd, frame_buffer, 0, 0, Paint_GetWidth (&paint), Paint_GetHeight (&paint));
      EPD_DisplayFrame (&epd);
      //HAL_Delay (500);
      AlarmB_active = ALARM_INACTIVE;

      SetGPSAlarmB ();
    }

    RTC_TimeTypeDef sTime =
    { 0 };
    HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
    RTC_DateTypeDef sDate =
    { 0 };
    HAL_RTC_GetDate (&hrtc, &sDate, RTC_FORMAT_BIN);
    uint8_t lastSec;
    if (sTime.Seconds != lastSec)
    {
      printf ("-> Current date and time: %02d-%02d-%04d Time: %02d:%02d:%02d\n\r", sDate.Date, sDate.Month, sDate.Year + 2000, sTime.Hours, sTime.Minutes,
	      sTime.Seconds);
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
    if (AlarmA_active == ALARM_INACTIVE && AlarmB_active == ALARM_INACTIVE /*&& HAL_GPIO_ReadPin(BUTTON1_GPIO_Port, BUTTON1_Pin) == GPIO_PIN_SET*/)
      {
      HAL_GPIO_WritePin (LED1_GPIO_Port, LED1_Pin, RESET);
      //printf ("-> Going to standby mode\n\r");

	/* Disable all used wakeup sources: PWR_WAKEUP_PIN2 */
	  HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1);

	  /* Clear all related wakeup and alar, flags*/
	  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
	  __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);

      /* Enable WakeUp Pin */
	  HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);
      __HAL_RTC_ALARM_EXTI_ENABLE_IT();  // Włącz przerwanie EXTI dla alarmu
      __HAL_RTC_ALARM_EXTI_ENABLE_RISING_EDGE();  // Włącz wybudzanie na narastającym zboczu

	  /* Enter the Standby mode */
	  //HAL_Delay(100);
	  HAL_PWR_EnterSTANDBYMode();
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
  /* EXTI0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
  /* USART1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
  /* RTC_Alarm_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);
  /* OTG_FS_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(OTG_FS_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
}

/* USER CODE BEGIN 4 */
int _write(int file, char *ptr, int len) {
    static uint8_t rc = USBD_OK;

    do {
        rc = CDC_Transmit_FS((uint8_t*)ptr, len);
    } while (USBD_BUSY == rc );

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

void HAL_RTC_AlarmAEventCallback (RTC_HandleTypeDef *hrtc)
{
  AlarmA_active = ALARM_ACTIVE;
}

void HAL_RTC_AlarmBEventCallback(RTC_HandleTypeDef *hrtc)
{
  AlarmB_active = ALARM_ACTIVE;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if(GPIO_Pin == BUTTON1_Pin) // Check if it's our button
	{
	    // Interrupt wakes up MCU
	}
	// Clear the EXTI Line 0 flag (for WKUP pin)
	__HAL_GPIO_EXTI_CLEAR_IT(EXTI_LINE_0);
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
