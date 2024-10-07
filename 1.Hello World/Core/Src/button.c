/*
 * button.c
 *
 *  Created on: Apr 20, 2021
 *      Author: Mateusz Salamon
 */
#include "main.h"

#include "button.h"

// Button Init
void ButtonInitKey(TButton* Key, GPIO_TypeDef* GpioPort, uint16_t GpioPin, uint32_t	TimerDebounce,
					uint32_t TimerLongPress, uint32_t TimerRepeat)
{
	Key->State = IDLE; // Set initial state for the button

	Key->GpioPort = GpioPort; // Remember GPIO Port for the button
	Key->GpioPin = GpioPin; // Remember GPIO Pin for the button

	Key->TimerDebounce = TimerDebounce; // Remember Debounce Time for the button
	Key->TimerLongPress = TimerLongPress; // Remember Long Press Time for the button
	Key->TimerRepeat = TimerRepeat; // Remember Repeat Time for the button
}
// Time setting functions
void ButtonSetDebounceTime(TButton* Key, uint32_t Milliseconds)
{
	Key->TimerDebounce = Milliseconds; // Remembed new Debounce time
}

void ButtonSetLongPressTime(TButton* Key, uint32_t Milliseconds)
{
	Key->TimerLongPress = Milliseconds; // Remembed new Long Press time
}

void ButtonSetRepeatTime(TButton* Key, uint32_t Milliseconds)
{
	Key->TimerRepeat = Milliseconds; // Remembed new Repeat time
}

// Register callbacks
void ButtonRegisterPressCallback(TButton* Key, void (*Callback)(void))
{
	Key->ButtonPressed = Callback; // Set new callback for button press
}

void ButtonRegisterLongPressCallback(TButton* Key, void (*Callback)(void))
{
	Key->ButtonLongPressed = Callback; // Set new callback for long press
}

void ButtonRegisterRepeatCallback(TButton* Key, void (*Callback)(void))
{
	Key->ButtonRepeat = Callback; // Set new callback for repeat press
}

// States routine
void ButtonIdleRoutine(TButton* Key)
{
	// Check if button was pressed
  if (GPIO_PIN_RESET == HAL_GPIO_ReadPin (Key->GpioPort, Key->GpioPin))
	{
		// Button was pressed for the first time
		Key->State = DEBOUNCE; // Jump to DEBOUNCE State
		Key->LastTick = HAL_GetTick(); // Remember current tick for Debounce software timer
	}
}

void ButtonDebounceRoutine(TButton* Key)
{
	// Wait for Debounce Timer elapsed
	if((HAL_GetTick() - Key->LastTick) > Key->TimerDebounce)
	{
		// After Debounce Timer elapsed check if button is still pressed
    if (GPIO_PIN_RESET == HAL_GPIO_ReadPin (Key->GpioPort, Key->GpioPin))
		{
			// Still pressed
			Key->State = PRESSED; // Jump to PRESSED state
			Key->LastTick = HAL_GetTick(); // Remember current tick for Long Press action

			if(Key->ButtonPressed != NULL) // Check if callback for pressed button exists
			{
				Key->ButtonPressed(); // If exists - do the callback function
			}
		}
		else
		{
			// If button was released durong debounce time
			Key->State = IDLE; // Go back do IDLE state
		}
	}
}

void ButtonPressedRoutine(TButton* Key)
{
	// Check if button was released
  if (GPIO_PIN_SET == HAL_GPIO_ReadPin (Key->GpioPort, Key->GpioPin))
	{
		// If released - go back to IDLE state
		Key->State = IDLE;
	}
	else
	{
		// If button is still pressed
		if((HAL_GetTick() - Key->LastTick) > Key->TimerLongPress) // Check if Long Press Timer elapsed
		{
			Key->State = REPEAT; // Jump to REPEAT State
			Key->LastTick = HAL_GetTick(); // Remember current tick for Repeat Timer

			if(Key->ButtonLongPressed != NULL) // Check if callback for Long Press exists
			{
				Key->ButtonLongPressed(); // If exists - do the callback function
			}
		}
	}
}

void ButtonRepeatRoutine(TButton* Key)
{
	// Check if button was released
  if (GPIO_PIN_SET == HAL_GPIO_ReadPin (Key->GpioPort, Key->GpioPin))
	{
		// If released - go back to IDLE state
		Key->State = IDLE;
	}
	else
	{
		// If button is still pressed
		if((HAL_GetTick() - Key->LastTick) > Key->TimerRepeat) // Check if Repeat Timer elapsed
		{
			Key->LastTick = HAL_GetTick(); // Reload last tick for next Repeat action

			if(Key->ButtonRepeat != NULL) // Check if callback for repeat action exists
			{
				Key->ButtonRepeat(); // If exists - do the callback function
			}
		}
	}
}

// State Machine
void ButtonTask(TButton* Key)
{
	switch(Key->State)
	{
		case IDLE:
			// do IDLE
			ButtonIdleRoutine(Key);
			break;

		case DEBOUNCE:
			// do DEBOUNCE
			ButtonDebounceRoutine(Key);
			break;

		case PRESSED:
			// do PRESSED
			ButtonPressedRoutine(Key);
			break;

		case REPEAT:
			// do REPEAT
			ButtonRepeatRoutine(Key);
			break;

		default:
			break;
	}
}



