/*
 * led_ws2812b.h
 *
 *  Created on: Nov 4, 2024
 *      Author: piotr
 */

#ifndef INC_LED_WS2812B_H_
#define INC_LED_WS2812B_H_
#include "stdio.h"
#include "stdint.h"
#include "ui.h"
#include "tim.h"
#include "adc.h"

typedef  struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
}rgb_color;

typedef struct
{
    uint16_t g[8];
    uint16_t r[8];
    uint16_t b[8];
}neopixel_led;



//1.25us - period
// logical 1 - 0.8 us high
// logical 0 - 0.4 us high
//system frequency 84MHz, prescaler 3 -> timer full 84MHz
// Auto-reload value = 24 -> pwm frequency = 800kHz
// CCR = 18 -> 0.8 us High
// CCR = 6 -> 0.4 us high

#define LED_LOGICAL_ONE 18
#define LED_LOGICAL_ZERO 6

void LED_ResetAllLeds(neopixel_led* leds, uint16_t number_leds);
void LED_SetAllLeds(neopixel_led* leds, uint16_t number_leds);
void LED_SetSpecificLed(neopixel_led* leds, uint16_t led_position, rgb_color color);
void LED_InitRunProcess	(LED_SEQUENCE_POSITION_t sequence, LED_DURATION_POSITION_t duration, uint8_t current_minute);
uint8_t LED_RunProcess(void);


#endif /* INC_LED_WS2812B_H_ */
