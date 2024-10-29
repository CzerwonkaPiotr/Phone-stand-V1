/*
 * ui.h
 *
 *  Created on: Oct 12, 2024
 *      Author: piotr
 */

#ifndef INC_UI_H_
#define INC_UI_H_

#include "main.h"
#include "bmp280.h"
#include "button.h"

#define SCREEN_WIDTH       128
#define SCREEN_HEIGHT      296

#define COLORED      0
#define UNCOLORED    1

uint8_t UI_RunOneMinuteProcess (void);
void UI_RunMenuProcess (uint8_t UserMenuFirstUse);
void UI_NextScreenCallback (void);
void UI_EnterSettingsCallback (void);
void UI_NextPositionOnMenuCallback (void);
void UI_NextSettingsGroupCallback (void);
void UI_ExitSettingsCallback (void);
void UI_ShowClockScreen (void);
void UI_Init (void);

#endif /* INC_UI_H_ */
