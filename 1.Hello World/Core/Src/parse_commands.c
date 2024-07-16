/*
 * parse_commands.c
 *
 *  Created on: Jul 16, 2024
 *      Author: piotr
 */

#include "main.h"
#include "ring_buffer.h"
#include "parse_commands.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

static char MyName[32] = "No Name";

//
// Get a one complete line from Ring Buffer
//
void Parser_TakeLine(RingBuffer_t *Buf, uint8_t *Destination)
{
	uint8_t Tmp;
	uint8_t i = 0;

	// Loop for every char in Ring Buffer
	do
	{
		// Read a one byte from Ring Buffer
		RB_Read(Buf, &Tmp);

		// Check if we take the endline char
		if(Tmp == ENDLINE)
		{
			// If endline - replace it with c-string end sign - 0x00
			Destination[i] = 0;
		}
		else
		{
			// If not endline - just write to work-buffer
			Destination[i] = Tmp;
		}

		i++; // increment array iterator
	}while(Tmp != ENDLINE); // If we hit the endline char - end the loop
}

// Parsing function for LED command
// LED=
//     0
//     1
static void Parser_ParseLED(void)
{
	// String to parse:
	// 	0
	// 	1

	// Pointer to sub-string
	char *ParsePointer = strtok(NULL, ",");

	if(strlen(ParsePointer) > 0) // Check if sunstring exists
	{
		if(ParsePointer[0] < '0' || ParsePointer[0] > '1') // Check if allowed char is in command parameter
		{
			printf("LED wrong value. Please type 0 or 1!\r\n");
			return; // return from function if error
		}

		// Check what to do
		if(ParsePointer[0] == '1')
		{
			HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
			printf("LED On\r\n");
		}
		else if (ParsePointer[0] == '0')
		{
			HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
			printf("LED Off\r\n");
		}
	}
}

// Parsing function for ENV command
// ENV=
//     X,Y,Z
static void Parser_ParseENV(void)
{
	// String to parse:
	// X,Y,Z - floats

	uint8_t i, j; // Iterators
	float EnvParameters[3]; // Array for each parameter

	for(i = 0; i < 3; i++) // For each of parameters
	{
		char *ParsePointer = strtok(NULL, ","); // Cut a sub-string

		if(strlen(ParsePointer) > 0) // Check if sub-string exists
		{
			// X.XX <- i = 0
			// Y.YY <- i = 1
			// Z.ZZ <- i = 2

			for(j = 0; ParsePointer[j] != 0; j++) // Check if only allowed chars are in sub-string
			{
				if((ParsePointer[j] < '0' || ParsePointer[j] > '9') && ParsePointer[j] != '.')
				{
				    printf("ENV wrong value. Don't use letters dude!\r\n");
					return;
				}
			}

			EnvParameters[i] = atof(ParsePointer); // Create a float from the sub-string
		}
		else
		{
		    printf("ENV too less values. ENV=X,Y,Z\\n\r\n");
			return;
		}
	}

	// Reaction - Send to log received values
	printf("Temperature: %.1f\r\n", EnvParameters[0]);

	printf("Humidity: %.1f\r\n", EnvParameters[1]);

	printf("Pressure: %.1f\r\n", EnvParameters[2]);
}

// Parsing function for NAME command
// NAME=
//      STRING
//      ?
static void Parser_ParseNAME(void)
{
	// STRING <- Rename device
	// ? <- Introduce device

	// Pointer to sub-string
	char *ParsePointer = strtok(NULL, ",");

	if(strlen(ParsePointer) > 0) // Check if sub-string exists
	{
		if(strcmp("?", ParsePointer) == 0) // Check if someone asked for our name
		{
			printf("My name is %s\r\n", MyName); // Just return the name
		}
		else // Save a new name
		{
			if(strlen(ParsePointer) > 32) // Check if name is no longer tham could be stored
			{
			    printf("Name should be less than 32\r\n");
				return;
			}

			strcpy(MyName, ParsePointer); // Cope name to our memory
			printf("My new name is %s\r\n", MyName);
		}
	}
	else
	{
	    printf("Name cannot be empty!\r\n");
	}
}

// Main parsing function
// Commands to detect:
// 	LED
// 	ENV
//	NAME
//
// uint8_t *DataToParse - an array with complete line taken from Ring Buffer earlier
//
void Parser_Parse(uint8_t *DataToParse)
{
	// Pointer to sub-string
	char *ParsePointer = strtok((char*)DataToParse, "="); // Create a sub-string

	// Compare provided array with line to parse with command template
	if(strcmp("LED", ParsePointer) == 0)
	{
		Parser_ParseLED(); // Call a parsing function for the LED command
	}
	else if(strcmp("ENV", ParsePointer) == 0)
	{
		Parser_ParseENV(); // Call a parsing function for the ENV command
	}
	else if(strcmp("NAME", ParsePointer) == 0)
	{
		Parser_ParseNAME(); // Call a parsing function for the NAME command
	}
}

