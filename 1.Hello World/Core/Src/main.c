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
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "bmp280.h"            // BMP280 sensor library
#include "NEO_6M.h"            // GPS module library
#include "alarms_rtc.h"        // Alarm and RTC handling library
#include "ui.h"                // User Interface (UI) library
#include "led_ws2812b.h"       // WS2812B LED handling library
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ACTIVE 1       // State: Active
#define INACTIVE 0     // State: Inactive
//#define USB_CDC_IS_ACTIVE // Turn on and off printf functionality
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
#ifdef USB_CDC_IS_ACTIVE
#include "ring_buffer.h"      // Circular buffer for UART/USB data
#include "parse_commands.h"   // Command parsing library
extern uint8_t CDC_Transmit_FS (uint8_t *Buf, uint16_t Len);  // USB transmit function

RingBuffer_t ReceiveBuffer;   // Circular buffer for received data
uint8_t ReceviedLines;        // Number of complete received lines
uint8_t ReceivedData[64];     // Buffer for processing received data
#endif

// Volatile flags for various processes
volatile uint8_t process_AlarmA;        // Alarm A processing flag
volatile uint8_t process_AlarmB;        // Alarm B processing flag
volatile uint8_t process_UserMenu;      // User menu processing flag
volatile uint8_t update_LED;            // LED update flag
uint8_t process_LED;                    // LED processing status

// This variable is a flag for turning on next screen when wakeup was initialized by the wkup button
// 0 - Button not pressed
// 1 - Button pressed, initial screen change required
// 2 - Initial screen change completed
volatile uint8_t UserMenuFirstUse = 0;

// Wake-up source tracking
wakeUpSource_t wakeUpSource = 0;        // Source of MCU wake-up
uint32_t process_UserMenuTimer = 0;     // Timer for user menu processing

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config (void);
static void MX_NVIC_Init (void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main (void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init ();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config ();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init ();
  MX_DMA_Init ();
  MX_I2C1_Init ();
  MX_USART1_UART_Init ();
  MX_RTC_Init ();
  MX_SPI1_Init ();
  MX_ADC1_Init ();
  MX_TIM3_Init ();
  MX_SPI2_Init ();
  MX_TIM4_Init ();

  /* Initialize interrupts */
  MX_NVIC_Init ();
  /* USER CODE BEGIN 2 */

  /* Check the wake-up source and initialize the UI */
  wakeUpSource = Check_RTC_Alarm ();
  UI_Init ();

  // Handle different wake-up scenarios
  switch (wakeUpSource)
  {
    case WKUP_SRC_ALARM_A:
      process_AlarmA = ACTIVE;
      process_AlarmB = INACTIVE;
      break;
    case WKUP_SRC_ALARM_B:
      process_AlarmA = INACTIVE;
      process_AlarmB = ACTIVE;
      break;
    case WKUP_SRC_WKUP_PIN:
      process_UserMenu = ACTIVE;
      UserMenuFirstUse = 1;
      process_UserMenuTimer = HAL_GetTick ();

      // Re-enable alarms if required
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
    case WKUP_SRC_BOOT_UP:
    default:
      process_AlarmA = ACTIVE;
      process_AlarmB = ACTIVE;
      break;
  };

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // Handle user menu processing, after 20s without action clock screen is displayed
    if (((HAL_GetTick () - process_UserMenuTimer) < 20000) && process_UserMenu == ACTIVE)
    {
      UI_RunMenuProcess (UserMenuFirstUse);
      UserMenuFirstUse = 2;  // Mark initial screen change as done
    }
    else if (((HAL_GetTick () - process_UserMenuTimer) >= 20000) && process_UserMenu == ACTIVE)
    {
      UI_ShowClockScreen (); // Return to clock screen
      UserMenuFirstUse = 0;
      process_UserMenu = INACTIVE;
    }

    // Process Alarm A
    if (process_AlarmA == ACTIVE && process_AlarmB == INACTIVE)
    {
      process_AlarmA = GPS_RunProcess ();
    }

    // Process Alarm B
    if (process_AlarmB == ACTIVE)
    {
      UI_RunOneMinuteProcess ();
      process_AlarmB = INACTIVE;
    }

    // Process LEDs
    if (update_LED == ACTIVE)
    {
      process_LED = LED_RunProcess ();
      update_LED = INACTIVE;
    }

    // USB CDC data parsing (if enabled)
#ifdef USB_CDC_IS_ACTIVE
    if (ReceviedLines > 0) {
      Parser_TakeLine(&ReceiveBuffer, ReceivedData);  // Fetch one line
      ReceviedLines--;                                // Decrement counter
      Parser_Parse(ReceivedData);                    // Parse the line
    }
#endif

    if (process_AlarmA == INACTIVE && process_AlarmB == INACTIVE &&
            process_UserMenu == INACTIVE && process_LED == INACTIVE) {
    #ifdef USB_CDC_IS_ACTIVE
          //printf("-> Going to standby mode\n\r");
    #endif
          HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);
          HAL_Delay(20);
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
void SystemClock_Config (void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct =
  { 0 };
  RCC_ClkInitTypeDef RCC_ClkInitStruct =
  { 0 };

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 13;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig (&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler ();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig (&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler ();
  }
}

/**
 * @brief NVIC Configuration.
 * @retval None
 */
static void MX_NVIC_Init (void)
{
  /* RTC_Alarm_IRQn interrupt configuration */
  HAL_NVIC_SetPriority (RTC_Alarm_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ (RTC_Alarm_IRQn);
  /* EXTI0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority (EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ (EXTI0_IRQn);
  /* USART1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority (USART1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ (USART1_IRQn);
  /* TIM4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority (TIM4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ (TIM4_IRQn);
  /* DMA1_Stream4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority (DMA1_Stream4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ (DMA1_Stream4_IRQn);
}

/* USER CODE BEGIN 4 */
#ifdef USB_CDC_IS_ACTIVE
/**
  * @brief  Overrides the _write function for USB CDC printf functionality.
  * @param  file: Unused parameter
  * @param  ptr: Pointer to the data buffer to transmit
  * @param  len: Length of the data buffer
  * @retval int: Number of bytes written or 0 in case of failure
  */
int _write(int file, char *ptr, int len) {
    static uint8_t rc = USBD_OK;

    // Wait for the USB to become ready if it's busy
    do {
        rc = CDC_Transmit_FS((uint8_t*)ptr, len);
    } while (USBD_BUSY == rc );

    // Handle a rare error case
    if (USBD_FAIL == rc) {
        // NOTE: This should not occur, but add error handling here if needed
        return 0;
    }
    return len;  // Return the number of bytes written
}

/**
  * @brief  Callback function for USB CDC data reception.
  * @param  Buffer: Pointer to the received data
  * @param  Length: Length of the received data
  */
void CDC_ReceiveCallback (uint8_t *Buffer, uint8_t Length)
{
  // Write received data to the ring buffer
  if (RB_OK == RB_Write(&ReceiveBuffer, Buffer, Length))
  {
      // Check for end-of-line characters in the received data
      for(uint32_t i = 0; i < Length ; i++)
      {
          if (Buffer[i] == ENDLINE)
          {
              ReceviedLines++;  // Increment line count for complete lines
          }
      }
  }
}
#endif

/**
  * @brief  RTC Alarm A event callback.
  * @param  hrtc: Pointer to the RTC handle
  * @retval None
  */
void HAL_RTC_AlarmAEventCallback (RTC_HandleTypeDef *hrtc)
{
  process_AlarmA = ACTIVE;  // Set the Alarm A processing flag
}

/**
  * @brief  RTC Alarm B event callback.
  * @param  hrtc: Pointer to the RTC handle
  * @retval None
  */
void HAL_RTCEx_AlarmBEventCallback (RTC_HandleTypeDef *hrtc)
{
  process_AlarmB = ACTIVE;  // Set the Alarm B processing flag
}

/**
  * @brief  External interrupt callback for GPIO pins.
  * @param  GPIO_Pin: The pin number that triggered the interrupt
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == WKUP_BUTTON_Pin) // Wakeup button interrupt
  {
    process_UserMenu = ACTIVE;  // Activate user menu process
    if (UserMenuFirstUse == 0) UserMenuFirstUse = 1;  // First-use flag
    process_UserMenuTimer = HAL_GetTick();  // Record the current time
  }

  // Clear the EXTI Line 0 flag for the WKUP pin
  __HAL_GPIO_EXTI_CLEAR_IT(EXTI_LINE_0);
}

/**
  * @brief  Callback for PWM pulse finished events.
  * @param  htim: Pointer to the timer handle
  * @retval None
  */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
  // Stop the PWM DMA and reset the CCR1 register
  HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_2);
  htim3.Instance->CCR1 = 0;
}

/**
  * @brief  Timer period elapsed callback (for TIM peripherals).
  * @param  htim: Pointer to the timer handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  update_LED = ACTIVE;  // Set the LED update flag
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler (void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  neopixel_led leds[16] = {0};


  //__disable_irq (); ///>irq needed for leds
  while (1)
  {
    LED_SetAllLeds(leds, 16);
    HAL_TIM_PWM_Start_DMA (&htim3, TIM_CHANNEL_2, (uint32_t*) leds, 16 * 24);
    HAL_Delay(500);

    // Turn them off
    LED_ResetAllLeds(leds, 16);
    HAL_TIM_PWM_Start_DMA (&htim3, TIM_CHANNEL_2, (uint32_t*) leds, 16 * 24);
    HAL_Delay(500);
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
