/*
 * button.h
 *
 *  Created on: Apr 20, 2021
 *      Author: Mateusz Salamon
 */

#ifndef INC_BUTTON_H_
#define INC_BUTTON_H_

// States for state machine
typedef enum
{
	IDLE = 0,
	DEBOUNCE,
	PRESSED,
	REPEAT
} BUTTON_STATE;

// Struct for button
typedef struct
{
	BUTTON_STATE 	State; // Button current state

	GPIO_TypeDef* 	GpioPort; // GPIO Port for a button
	uint16_t		GpioPin; // GPIO Pin for a button

	uint32_t		LastTick; // Last remembered time before steps
	uint32_t		TimerDebounce; // Fixed, settable time for debounce timer
	uint32_t		TimerLongPress; // Fixed, settable time for long press timer
	uint32_t		TimerRepeat; // Fixed, settable time for repeat timer

	void(*ButtonPressed)(void); // A callback for buttos pressed
	void(*ButtonLongPressed)(void); // A callback for long press
	void(*ButtonRepeat)(void); // A callback for repeat action
} TButton; // Button_t

// Public functions
void ButtonTask(TButton* Key);

void ButtonSetDebounceTime(TButton* Key, uint32_t Milliseconds);
void ButtonSetLongPressTime(TButton* Key, uint32_t Milliseconds);
void ButtonSetRepeatTime(TButton* Key, uint32_t Milliseconds);

void ButtonRegisterPressCallback(TButton* Key, void (*Callback)(void));
void ButtonRegisterLongPressCallback(TButton* Key, void (*Callback)(void));
void ButtonRegisterRepeatCallback(TButton* Key, void (*Callback)(void));

void ButtonInitKey(TButton* Key, GPIO_TypeDef* GpioPort, uint16_t GpioPin, uint32_t	TimerDebounce,
					uint32_t TimerLongPress, uint32_t TimerRepeat);
#endif /* INC_BUTTON_H_ */
