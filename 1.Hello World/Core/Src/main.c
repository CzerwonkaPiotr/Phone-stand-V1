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
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "bmp280.h"
#include "NEO_6M.h"
#include "alarms_rtc.h"
#include "ui.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BMP280_ADDRESS 0x76
#define ACTIVE 1
#define INACTIVE 0

//#define USB_CDC_IS_ACTIVE // Turn on and off printf functionality


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
#ifdef USB_CDC_IS_ACTIVE
#include "ring_buffer.h"
#include "parse_commands.h"
extern uint8_t CDC_Transmit_FS (uint8_t *Buf, uint16_t Len);
RingBuffer_t ReceiveBuffer; // Ring Buffer for Receiving from UART
uint8_t ReceviedLines; // Complete lines counter
uint8_t ReceivedData[64]; // A buffer for parsing
#endif


volatile uint8_t process_AlarmA;
volatile uint8_t process_AlarmB;
volatile uint8_t process_UserMenu;
volatile uint8_t UserMenuFirstUse = 0;
// 0 - button wasn't pressed
// 1 - button was pressed and initial screen change has to be done
// 2 - the initial screen change was done in current userMenu process

wakeUpSource_t wakeUpSource = 0;
uint32_t process_UserMenuTimer = 0;
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
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_RTC_Init();
  MX_SPI1_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */

  GPS_Init (&huart1);
  UI_Init ();

  wakeUpSource = Check_RTC_Alarm (); /**** check alarm wakeup ****/
  if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) == SET && __HAL_PWR_GET_FLAG(PWR_FLAG_WU) == SET) // Check and handle if the system was resumed from StandBy mode
  {
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);  	// Clear Standby flag
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU); 		// Clear Wkup flag
    __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
    __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRBF);
    __HAL_GPIO_EXTI_CLEAR_IT(EXTI_LINE_0);
  }
  switch (wakeUpSource)
  {
    case WKUP_SRC_ALARM_A_DATA_NOK:
      process_AlarmA = ACTIVE;
      process_AlarmB = INACTIVE;
      break;
    case WKUP_SRC_ALARM_A_DATA_OK:
      process_AlarmA = ACTIVE;
      process_AlarmB = INACTIVE;
      break;
    case WKUP_SRC_ALARM_B:
      process_AlarmA = INACTIVE;
      process_AlarmB = ACTIVE;
      break;
    case WKUP_SRC_ALARM_A_OR_B:
      process_AlarmA = ACTIVE;
      process_AlarmB = ACTIVE;
      break;
    case WKUP_SRC_BOOT_UP:
      process_AlarmA = ACTIVE;
      process_AlarmB = ACTIVE;
      break;
    case WKUP_SRC_WKUP_PIN:
      process_UserMenu = ACTIVE;
      UserMenuFirstUse = 1;
      process_UserMenuTimer = HAL_GetTick ();
      if (RTC->CR & RTC_CR_ALRAE)
      {
	RTC_AlarmTypeDef sAlarm =
	{ 0 };
	HAL_RTC_GetAlarm (&hrtc, &sAlarm, RTC_ALARM_A, RTC_FORMAT_BIN);
	if (HAL_RTC_SetAlarm_IT (&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
	{
	  Error_Handler ();
	}
      }
      if (RTC->CR & RTC_CR_ALRBE)
      {
	RTC_AlarmTypeDef sAlarm =
	{ 0 };
	HAL_RTC_GetAlarm (&hrtc, &sAlarm, RTC_ALARM_B, RTC_FORMAT_BIN);
	if (HAL_RTC_SetAlarm_IT (&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
	{
	  Error_Handler ();
	}
      }
      break;
  };

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    HAL_GPIO_WritePin (LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);

    if (((HAL_GetTick () - process_UserMenuTimer) < 10000) && process_UserMenu == ACTIVE)
    {
      UI_RunMenuProcess (UserMenuFirstUse);
      UserMenuFirstUse = 2;
    }
    else if (((HAL_GetTick () - process_UserMenuTimer) > 10000) && process_UserMenu == ACTIVE)
    {
      UI_ShowClockScreen ();
      UserMenuFirstUse = 0;
      process_UserMenu = INACTIVE;
    }

    //
    //// Alarm A sequence
    //

    if (process_AlarmA == ACTIVE)
    {
      process_AlarmA = GPS_RunProcess ();
    }

    //
    //// Alarm B sequence
    //

    if (process_AlarmB == ACTIVE)
    {

//      BMP280_SetMode (&Bmp280, BMP280_FORCEDMODE); // Measurement is made and the sensor goes to sleep
//      //HAL_Delay (50); // TODO no delays allowed
//      BMP280_ReadSensorData (&Bmp280, &Pressure, &Temperature, &Humidity);
//      char atmData[128];
//      Paint_Clear (&paint, UNCOLORED);
//      sprintf (atmData, "-> Temperature = %.1f Pressure = %.1f Humidity = %.1f", Temperature, Pressure, Humidity);
//
//      Paint_DrawStringAt (&paint, 0, 14, atmData, &Font8, COLORED);
//      RTC_TimeTypeDef sTime =
//      { 0 };
//      HAL_RTC_GetTime (&hrtc, &sTime, RTC_FORMAT_BIN);
//      RTC_DateTypeDef sDate =
//      { 0 };
//      HAL_RTC_GetDate (&hrtc, &sDate, RTC_FORMAT_BIN);
//      sprintf (atmData, "Current date and time: %02d-%02d-%04d", sDate.Date, sDate.Month, sDate.Year + 2000);
//      Paint_DrawStringAt (&paint, 0, 44, atmData, &Font12, COLORED);
//      sprintf (atmData, "Time: %02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
//      Paint_DrawStringAt (&paint, 0, 74, atmData, &Font12, COLORED);
//      EPD_SetFrameMemory (&epd, frame_buffer, 0, 0, Paint_GetWidth (&paint), Paint_GetHeight (&paint));
//      EPD_DisplayFrame (&epd);
//      EPD_Sleep (&epd);

      process_AlarmB = UI_RunOneMinuteProcess ();
    }

#ifdef USB_CDC_IS_ACTIVE
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
#endif

    // TODO uruchom proces ledów

    if (process_AlarmA == INACTIVE && process_AlarmB == INACTIVE && process_UserMenu == INACTIVE)
    {
#ifdef USB_CDC_IS_ACTIVE
      //printf ("-> Going to standby mode\n\r");
#endif
      /* Enable WakeUp Pin */
      HAL_PWR_EnableWakeUpPin (PWR_WAKEUP_PIN1);

      /* Enter the Standby mode */
      HAL_PWR_EnterSTANDBYMode ();
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
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
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
  /* RTC_Alarm_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);
  /* EXTI0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
  /* USART1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* USER CODE BEGIN 4 */
#ifdef USB_CDC_IS_ACTIVE
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
#endif
void HAL_RTC_AlarmAEventCallback (RTC_HandleTypeDef *hrtc)
{
  process_AlarmA = ACTIVE;
}

void HAL_RTCEx_AlarmBEventCallback (RTC_HandleTypeDef *hrtc)
{
  process_AlarmB = ACTIVE;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == BUTTON1_Pin)
  {
    // Interrupt wakes up MCU
    process_UserMenu = ACTIVE;
    if (UserMenuFirstUse == 0) UserMenuFirstUse = 1;
    process_UserMenuTimer = HAL_GetTick ();
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
