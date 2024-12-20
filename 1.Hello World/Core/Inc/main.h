/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "z_flash_W25QXXX.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define WKUP_BUTTON_Pin GPIO_PIN_0
#define WKUP_BUTTON_GPIO_Port GPIOA
#define WKUP_BUTTON_EXTI_IRQn EXTI0_IRQn
#define EPD_SPI_CS_Pin GPIO_PIN_1
#define EPD_SPI_CS_GPIO_Port GPIOA
#define EPD_SPI_DC_Pin GPIO_PIN_2
#define EPD_SPI_DC_GPIO_Port GPIOA
#define FLASH_CS_Pin GPIO_PIN_4
#define FLASH_CS_GPIO_Port GPIOA
#define FLASH_SCK_Pin GPIO_PIN_5
#define FLASH_SCK_GPIO_Port GPIOA
#define FLASH_MISO_Pin GPIO_PIN_6
#define FLASH_MISO_GPIO_Port GPIOA
#define FLASH_MOSI_Pin GPIO_PIN_7
#define FLASH_MOSI_GPIO_Port GPIOA
#define LIGHT_LEVEL_Pin GPIO_PIN_0
#define LIGHT_LEVEL_GPIO_Port GPIOB
#define BATTERY_LEVEL_Pin GPIO_PIN_1
#define BATTERY_LEVEL_GPIO_Port GPIOB
#define EPD_SPI2_SCK_Pin GPIO_PIN_10
#define EPD_SPI2_SCK_GPIO_Port GPIOB
#define EPD_SPI_BUSY_Pin GPIO_PIN_13
#define EPD_SPI_BUSY_GPIO_Port GPIOB
#define EPD_SPI_RST_Pin GPIO_PIN_14
#define EPD_SPI_RST_GPIO_Port GPIOB
#define EPD_SPI2_MOSI_Pin GPIO_PIN_15
#define EPD_SPI2_MOSI_GPIO_Port GPIOB
#define GPS_USART1_TX_Pin GPIO_PIN_9
#define GPS_USART1_TX_GPIO_Port GPIOA
#define GPS_USART1_RX_Pin GPIO_PIN_10
#define GPS_USART1_RX_GPIO_Port GPIOA
#define ANALOG_GND_Pin GPIO_PIN_12
#define ANALOG_GND_GPIO_Port GPIOA
#define LED_DATA_PWM_Pin GPIO_PIN_5
#define LED_DATA_PWM_GPIO_Port GPIOB
#define BME_I2C1_SCL_Pin GPIO_PIN_6
#define BME_I2C1_SCL_GPIO_Port GPIOB
#define BME_I2C1_SDA_Pin GPIO_PIN_7
#define BME_I2C1_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
