/*
 * led_ws2812b.c
 *
 *  Created on: Nov 5, 2024
 *      Author: piotr
 *
 *  This file provides driver functions for controlling an array of WS2812B (NeoPixel) LEDs.
 *  It includes initialization, color setting, transition effects, and sequence control.
 */

#include"led_ws2812b.h"

// Defines the number of frames the LED update runs per second
// and the total number of LEDs in the strip.
#define LED_FRAMES_PER_SECOND 56
#define NUMBER_OF_LEDS 15

/*
 * An array of neopixel_led structures, where each element holds the bit patterns
 * for red, green, and blue channels of one LED. One extra is for the ending message.
 */
neopixel_led leds[NUMBER_OF_LEDS + 1];

/*
 * State variables used to track the current LED sequence, duration,
 * the maximum steps for the sequence, the current step in the sequence,
 * and the count for infinite loops (if any).
 */
static LED_SEQUENCE_POSITION_t processSequence;
static LED_DURATION_POSITION_t processDuration;
static uint16_t processMaxSteps,  /* The total number of steps the current sequence will run */
                 processStep,     /* The current step count in the running sequence */
                 infiniteLoopCount, /* Counter for how many loops have been completed (infinite sequences) */
                 lightLevel;      /* Light level expressed as a percentage (0–100) */
static uint8_t  minute;           /* Used to select color variation based on minute or other logic */

/*
 * Predefined colors array. Each color is defined by its RGB components.
 */
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

/*
 * Lookup table for gamma correction or brightness compensation.
 * Maps 8-bit values (0-255) to corrected 8-bit output values.
 */
static const uint8_t ledLookupTable[256] =
{ 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5,
    5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 12, 12, 12, 13, 13, 14, 14, 15, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 22,
    22, 23, 23, 24, 25, 25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 33, 33, 34, 35, 36, 36, 37, 38, 39, 40, 40, 41, 42, 43, 44, 45, 46, 46, 47, 48, 49, 50, 51,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 67, 68, 69, 70, 71, 72, 73, 75, 76, 77, 78, 80, 81, 82, 83, 85, 86, 87, 89, 90, 91, 93, 94, 95, 97,
    98, 99, 101, 102, 104, 105, 107, 108, 110, 111, 113, 114, 116, 117, 119, 121, 122, 124, 125, 127, 129, 130, 132, 134, 135, 137, 139, 141, 142, 144, 146,
    148, 150, 151, 153, 155, 157, 159, 161, 163, 165, 166, 168, 170, 172, 174, 176, 178, 180, 182, 184, 186, 189, 191, 193, 195, 197, 199, 201, 204, 206, 208,
    210, 212, 215, 217, 219, 221, 224, 226, 228, 231, 233, 235, 238, 240, 243, 245, 248, 250, 253, 255 };


/**
 * @brief Resets (turns off) all LEDs in the given array.
 *        This sets every bit for R, G, B channels to the logical zero pattern.
 *
 * @param[in] leds         Pointer to the LED array.
 * @param[in] number_leds  Number of LEDs to reset.
 */
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

/**
 * @brief Sets (turns on) all LEDs in the given array to maximum brightness (white).
 *        This sets every bit for R, G, B channels to the logical one pattern.
 *
 * @param[in] leds         Pointer to the LED array.
 * @param[in] number_leds  Number of LEDs to set.
 */
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

/**
 * @brief Sets a range of LEDs (from start_led to end_led) to a specified RGB color.
 *
 * @param[in] start_led  First LED in the range (inclusive).
 * @param[in] end_led    Last LED in the range (inclusive).
 * @param[in] color      Desired color in rgb_color format.
 */
void LED_SetColorForLeds (uint16_t start_led, uint16_t end_led, rgb_color color)
{
  // Validate the LED range before proceeding
  if (start_led > end_led || end_led >= NUMBER_OF_LEDS || start_led < 0)
  {
    return; // Invalid range, exit the function
  }

  // Set the color for each LED in the specified range
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

/**
 * @brief Initializes and starts an LED sequence based on the given parameters.
 *
 * @param[in] sequence        The sequence pattern (FADE, CIRCLE, SMOOTH, OFF).
 * @param[in] duration        How long the sequence will run (3S, 5S, 10S, or INFINITE).
 * @param[in] current_minute  Current minute or other time-based parameter (used for color selection).
 *
 * This function:
 *   1. Updates the sequence and duration states.
 *   2. Calculates the total number of steps based on the duration.
 *   3. Reads from an ADC channel to determine the current ambient light level.
 *   4. Starts the timer interrupt if needed, and sets the initial LED states.
 */
void LED_InitRunProcess (LED_SEQUENCE_POSITION_t sequence, LED_DURATION_POSITION_t duration, uint8_t current_minute)
{

  // If we are already running an infinite loop, do not reset the process
  if (processDuration == DURATION_INFINITE && duration == DURATION_INFINITE)
  {
    return;
  }

  // Update global process state
  processSequence = sequence;
  processDuration = duration;
  minute = current_minute;
  processStep = 0;

  // If the sequence is set to OFF, stop the timer, turn off LEDs, and start DMA to apply changes
  if (processSequence == OFF_LED_SEQUENCE)
  {
    // No steps needed for an off sequence
    processMaxSteps = 0;
    HAL_TIM_Base_Stop_IT (&htim4);
    LED_SetColorForLeds (0, NUMBER_OF_LEDS - 1, colors[10]);
    HAL_TIM_PWM_Start_DMA (&htim3, TIM_CHANNEL_2, (uint32_t*) leds, (NUMBER_OF_LEDS + 1) * 24);
    return;
  }

  // Calculate the total number of steps for the selected duration
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
    // Infinite loops run the same base steps but repeat forever
    processMaxSteps = 5 * LED_FRAMES_PER_SECOND;
  }

  // Start the timer interrupt to update LEDs each frame
  HAL_TIM_Base_Start_IT (&htim4);

  // Measure ambient light using ADC
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

  // Convert raw ADC value (0–4095) to a percentage (0–100)
  tempVar = (float) lightLevel / (float) 4095;
  lightLevel = tempVar * 100;

  // Ensure a minimum brightness of 5%
  if (lightLevel < 5) lightLevel = 5;
}

/**
 * @brief Calculates the intermediate color in a transition from start_color to end_color.
 *
 * @param[in] start_color   The initial color for the transition.
 * @param[in] end_color     The final color for the transition.
 * @param[in] max_steps     The total number of steps in the transition.
 * @param[in] current_step  The current step (0-based) in the transition.
 * @return                  The RGB color corresponding to the current step.
 *
 * This function applies gamma correction to the interpolated color and scales it by the global lightLevel.
 */
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

/**
 * @brief Updates the LED sequence frame-by-frame and handles sequence completion.
 *
 * @return uint8_t
 *         - 1 if the process is still running
 *         - 0 if the process has finished (for non-infinite sequences)
 *
 * This function handles:
 *   - FADE_LED_SEQUENCE: Fades from BLACK -> COLOR -> BLACK
 *   - CIRCLE_LED_SEQUENCE: Cycles color around the strip
 *   - SMOOTH_LED_SEQUENCE: Smoothly transitions through an array of colors
 */
uint8_t LED_RunProcess (void)
{
  rgb_color calcColor;

  // FADE sequence logic
  if (processSequence == FADE_LED_SEQUENCE)
  {
    if (processStep < (processMaxSteps / 2))
    {
      calcColor = LED_CalculateTransitionColor (colors[10], colors[(minute + infiniteLoopCount) % 10],
						processMaxSteps / 2, processStep);
    }
    else if (processStep >= (processMaxSteps / 2))
    {
      calcColor = LED_CalculateTransitionColor (colors[(minute + infiniteLoopCount) % 10],
						colors[10], processMaxSteps / 2,
						processStep - (processMaxSteps / 2));
    }

    // Apply the calculated color to all LEDs
    LED_SetColorForLeds (0, NUMBER_OF_LEDS - 1, calcColor);
    HAL_TIM_PWM_Start_DMA (&htim3, TIM_CHANNEL_2, (uint32_t*) leds, (NUMBER_OF_LEDS + 1) * 24);
  }

  // CIRCLE sequence logic
  else if (processSequence == CIRCLE_LED_SEQUENCE)
  {
    /*
     * stepsPerLED: how many steps to keep a color on one LED
     * before it moves on to the next LED in the strip.
     */
    uint16_t stepsPerLED = processMaxSteps / (NUMBER_OF_LEDS + 3);

    // The LED currently lit at this step
    uint16_t currentLED = processStep / stepsPerLED;

    // Turn off an LED a few positions behind the current one
    LED_SetColorForLeds (currentLED - 3, currentLED - 3, colors[10]);

    // Fade in the leading edge color
    calcColor = LED_CalculateTransitionColor (colors[(minute + infiniteLoopCount) % 10], colors[10],
					      stepsPerLED, processStep % stepsPerLED);
    LED_SetColorForLeds (currentLED - 2, currentLED - 2, calcColor);

    // Fade in the new leading LED
    calcColor = LED_CalculateTransitionColor (colors[10], colors[(minute + infiniteLoopCount) % 10],
					      stepsPerLED, processStep % stepsPerLED);
    LED_SetColorForLeds (currentLED, currentLED, calcColor);

    // Commit changes via DMA
    HAL_TIM_PWM_Start_DMA (&htim3, TIM_CHANNEL_2, (uint32_t*) leds, (NUMBER_OF_LEDS + 1) * 24);
  }

  // SMOOTH sequence logic
  else if (processSequence == SMOOTH_LED_SEQUENCE)
  {
    // Divide the total steps into segments for smooth transition through 11 colors
    uint16_t stepsPerColor = processMaxSteps / 11;

    // Which color pair we are currently transitioning between
    uint16_t currentColor = processStep / stepsPerColor;

    // Compute interpolated color
    calcColor = LED_CalculateTransitionColor (colors[(currentColor + 10) % 11],
					      colors[(currentColor + 11) % 11], stepsPerColor,
					      processStep % stepsPerColor);

    // Apply transition color to all LEDs
    LED_SetColorForLeds (0, NUMBER_OF_LEDS - 1, calcColor);
    HAL_TIM_PWM_Start_DMA (&htim3, TIM_CHANNEL_2, (uint32_t*) leds, (NUMBER_OF_LEDS + 1) * 24);
  }

  if (processStep >= processMaxSteps)
  {
    if (processDuration == DURATION_INFINITE)
    {
      // Infinite sequences: increment loop count and reset steps
      infiniteLoopCount++;
      processStep = 0;
    }
    else
    {
      // For finite sequences: reset, turn off LEDs, and stop the timer
      processStep = 0;
      infiniteLoopCount = 0;
      LED_SetColorForLeds (0, NUMBER_OF_LEDS - 1, colors[10]); // Turn strip black
      HAL_TIM_PWM_Start_DMA (&htim3, TIM_CHANNEL_2, (uint32_t*) leds, (NUMBER_OF_LEDS + 1) * 24);
      HAL_TIM_Base_Stop_IT (&htim4);
      return 0; // Process finished
    }
  }

  // Move to the next step and keep running
  processStep++;
  return 1; // process is ongoing
}
