/*
 * led_ws2812b.c
 *
 *  Created on: Nov 5, 2024
 *      Author: piotr
 */

#include"led_ws2812b.h"

#define LED_FRAMES_PER_SECOND 56
#define NUMBER_OF_LEDS 15

neopixel_led leds[NUMBER_OF_LEDS + 1];

static LED_SEQUENCE_POSITION_t processSequence;
static LED_DURATION_POSITION_t processDuration;
static uint16_t processMaxSteps, processStep, infiniteLoopCount, lightLevel; // lightLevel in %
static uint8_t minute;
static const rgb_color colors[] =
{
{ 255, 0, 0 },      // 0 RED
    { 255, 125, 0 },    // 1 ORANGE
    { 255, 255, 0 },    // 2 YELLOW
    { 125, 255, 0 },    // 3 SPRING GREEN
    { 0, 255, 0 },      // 4 GREEN
    { 0, 255, 255 },    // 5 CYAN
    { 0, 125, 255 },    // 6 OCEAN
    { 0, 0, 255 },      // 7 BLUE
    { 125, 0, 255 },    // 8 VIOLET
    { 255, 0, 255 },    // 9 MAGENTA
    { 0, 0, 0 },		// 10 BLACK
    { 255, 255, 255 }		// 11 WHITE

};
static const uint8_t ledLookupTable[256] =
{ 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5,
    5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 12, 12, 12, 13, 13, 14, 14, 15, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 22,
    22, 23, 23, 24, 25, 25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 33, 33, 34, 35, 36, 36, 37, 38, 39, 40, 40, 41, 42, 43, 44, 45, 46, 46, 47, 48, 49, 50, 51,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 67, 68, 69, 70, 71, 72, 73, 75, 76, 77, 78, 80, 81, 82, 83, 85, 86, 87, 89, 90, 91, 93, 94, 95, 97,
    98, 99, 101, 102, 104, 105, 107, 108, 110, 111, 113, 114, 116, 117, 119, 121, 122, 124, 125, 127, 129, 130, 132, 134, 135, 137, 139, 141, 142, 144, 146,
    148, 150, 151, 153, 155, 157, 159, 161, 163, 165, 166, 168, 170, 172, 174, 176, 178, 180, 182, 184, 186, 189, 191, 193, 195, 197, 199, 201, 204, 206, 208,
    210, 212, 215, 217, 219, 221, 224, 226, 228, 231, 233, 235, 238, 240, 243, 245, 248, 250, 253, 255 };

void LED_ResetAllLeds (neopixel_led *leds, uint16_t number_leds)
{
  for (int i = 0; i < number_leds; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      (leds + i)->r[j] = LED_LOGICAL_ZERO;
      (leds + i)->g[j] = LED_LOGICAL_ZERO;
      (leds + i)->b[j] = LED_LOGICAL_ZERO;
    }
  }

}
void LED_SetAllLeds (neopixel_led *leds, uint16_t number_leds)
{
  for (int i = 0; i < number_leds; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      (leds + i)->r[j] = LED_LOGICAL_ONE;
      (leds + i)->g[j] = LED_LOGICAL_ONE;
      (leds + i)->b[j] = LED_LOGICAL_ONE;
    }
  }
}
static void LED_SetColorForLeds (uint16_t start_led, uint16_t end_led, rgb_color color)
{
  // Check if LED range is valid
  if (start_led > end_led || end_led >= NUMBER_OF_LEDS || start_led < 0)
  {
    return; // Invalid range, exit the function
  }

  // Iterate through the selected LEDs
  for (uint16_t i = start_led; i <= end_led; i++)
  {
    // Set the red channel (R)
    for (int bit = 0; bit < 8; bit++)
    {
      leds[i].r[bit] = (color.r & (1 << (7 - bit))) ? LED_LOGICAL_ONE : LED_LOGICAL_ZERO;
    }

    // Set the green channel (G)
    for (int bit = 0; bit < 8; bit++)
    {
      leds[i].g[bit] = (color.g & (1 << (7 - bit))) ? LED_LOGICAL_ONE : LED_LOGICAL_ZERO;
    }

    // Set the blue channel (B)
    for (int bit = 0; bit < 8; bit++)
    {
      leds[i].b[bit] = (color.b & (1 << (7 - bit))) ? LED_LOGICAL_ONE : LED_LOGICAL_ZERO;
    }
  }
}
void LED_InitRunProcess (LED_SEQUENCE_POSITION_t sequence, LED_DURATION_POSITION_t duration, uint8_t current_minute)
{

  //Dont start process from 0 if it is going in a infinite loop.
  if (processDuration == DURATION_INFINITE && duration == DURATION_INFINITE)
  {
    return;
  }

  processSequence = sequence;
  processDuration = duration;
  minute = current_minute;
  processStep = 0;

  if (processSequence == OFF_LED_SEQUENCE)
  {
    //TURN OFF TIMER
    processMaxSteps = 0;
    HAL_TIM_Base_Stop_IT (&htim4);
    LED_SetColorForLeds (0, NUMBER_OF_LEDS - 1, colors[10]);
    HAL_TIM_PWM_Start_DMA (&htim3, TIM_CHANNEL_2, (uint32_t*) leds, (NUMBER_OF_LEDS + 1) * 24);
    return;
  }

  if (processDuration == DURATION_3S)
  {
    processMaxSteps = 3 * LED_FRAMES_PER_SECOND;
  }
  else if (processDuration == DURATION_5S)
  {
    processMaxSteps = 5 * LED_FRAMES_PER_SECOND;
  }
  else if (processDuration == DURATION_10S)
  {
    processMaxSteps = 10 * LED_FRAMES_PER_SECOND;
  }
  else if (processDuration == DURATION_INFINITE)
  {
    processMaxSteps = 5 * LED_FRAMES_PER_SECOND;
  }
  HAL_TIM_Base_Start_IT (&htim4);

  float tempVar;
  ADC_ChannelConfTypeDef sConfig =
  { 0 };
  sConfig.Channel = ADC_CHANNEL_8;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel (&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler ();
  }
  HAL_ADC_Start (&hadc1);

  if (HAL_ADC_PollForConversion (&hadc1, HAL_MAX_DELAY) == HAL_OK)
  {
    lightLevel = HAL_ADC_GetValue (&hadc1);
  }
  HAL_ADC_Stop (&hadc1);

  tempVar = (float) lightLevel / (float) 4095;
  lightLevel = tempVar * 100;
  if (lightLevel < 5) lightLevel = 5;
}

rgb_color LED_CalculateTransitionColor (rgb_color start_color, rgb_color end_color, uint16_t max_steps, uint16_t current_step)
{
  rgb_color result_color =
  { 0, 0, 0 }; // Default to black

  // Prevent division by zero or out-of-range current_step
  if (max_steps == 0 || current_step >= max_steps)
  {
    return result_color;
  }

  // Interpolate the raw color values for the current step
  uint8_t raw_r = start_color.r + ((end_color.r - start_color.r) * current_step) / max_steps;
  uint8_t raw_g = start_color.g + ((end_color.g - start_color.g) * current_step) / max_steps;
  uint8_t raw_b = start_color.b + ((end_color.b - start_color.b) * current_step) / max_steps;

  // Apply gamma correction using the lookup table and light intensitivity
  result_color.r = (ledLookupTable[raw_r] * lightLevel) / 100;
  result_color.g = (ledLookupTable[raw_g] * lightLevel) / 100;
  result_color.b = (ledLookupTable[raw_b] * lightLevel) / 100;

  return result_color;
}

uint8_t LED_RunProcess (void)
{
  rgb_color calcColor;
  if (processSequence == FADE_LED_SEQUENCE)
  {
    if (processStep < (processMaxSteps / 2)) calcColor = LED_CalculateTransitionColor (colors[10], colors[(minute + infiniteLoopCount) % 10],
										       processMaxSteps / 2, processStep);
    else if (processStep >= (processMaxSteps / 2)) calcColor = LED_CalculateTransitionColor (colors[(minute + infiniteLoopCount) % 10], colors[10],
											     processMaxSteps / 2, processStep - (processMaxSteps / 2));
    LED_SetColorForLeds (0, NUMBER_OF_LEDS - 1, calcColor);
    HAL_TIM_PWM_Start_DMA (&htim3, TIM_CHANNEL_2, (uint32_t*) leds, (NUMBER_OF_LEDS + 1) * 24);
  }
  else if (processSequence == CIRCLE_LED_SEQUENCE)
  {
    uint16_t stepsPerLED = processMaxSteps / (NUMBER_OF_LEDS + 3);
    uint16_t currentLED = processStep / stepsPerLED;
    LED_SetColorForLeds (currentLED - 3, currentLED - 3, colors[10]);
    calcColor = LED_CalculateTransitionColor (colors[(minute + infiniteLoopCount) % 10], colors[10], stepsPerLED, processStep % stepsPerLED);
    LED_SetColorForLeds (currentLED - 2, currentLED - 2, calcColor);
    calcColor = LED_CalculateTransitionColor (colors[10], colors[(minute + infiniteLoopCount) % 10], stepsPerLED, processStep % stepsPerLED);
    LED_SetColorForLeds (currentLED, currentLED, calcColor);
    HAL_TIM_PWM_Start_DMA (&htim3, TIM_CHANNEL_2, (uint32_t*) leds, (NUMBER_OF_LEDS + 1) * 24);
  }
  else if (processSequence == SMOOTH_LED_SEQUENCE)
  {
    uint16_t stepsPerColor = processMaxSteps / 11;
    uint16_t currentColor = processStep / stepsPerColor;
    calcColor = LED_CalculateTransitionColor (colors[(currentColor + 10) % 11], colors[(currentColor + 11) % 11], stepsPerColor, processStep % stepsPerColor);
    LED_SetColorForLeds (0, NUMBER_OF_LEDS - 1, calcColor);
    HAL_TIM_PWM_Start_DMA (&htim3, TIM_CHANNEL_2, (uint32_t*) leds, (NUMBER_OF_LEDS + 1) * 24);
  }

  if (processStep >= processMaxSteps)
  {
    if (processDuration == DURATION_INFINITE)
    {
      infiniteLoopCount++;
      processStep = 0;
    }
    else
    {
      processStep = 0;
      infiniteLoopCount = 0;
      LED_SetColorForLeds (0, NUMBER_OF_LEDS - 1, colors[10]);
      HAL_TIM_PWM_Start_DMA (&htim3, TIM_CHANNEL_2, (uint32_t*) leds, (NUMBER_OF_LEDS + 1) * 24);
      HAL_TIM_Base_Stop_IT (&htim4);
      return 0;
    }
  }
  processStep++;
  return 1; // process is ongoing
}
