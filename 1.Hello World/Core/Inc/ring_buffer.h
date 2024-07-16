/*
 * ring_buffer.h
 *
 *  Created on: Jul 16, 2024
 *      Author: piotr
 */

#ifndef INC_RING_BUFFER_H_
#define INC_RING_BUFFER_H_

#define RING_BUFFER_SIZE 64

// Success status
typedef enum
{
  RB_OK = 0, RB_ERROR = 1
} RB_Status;

// Object Ring Buffer
typedef struct
{
  uint16_t Head; // Pointer to write
  uint16_t Tail; // Pointer to read
  uint8_t Buffer[RING_BUFFER_SIZE]; // Array to store data
} RingBuffer_t;

//Functions
// Write
RB_Status RB_Write (RingBuffer_t *Buf, uint8_t *Value, uint32_t Length);
// Read
RB_Status RB_Read (RingBuffer_t *Buf, uint8_t *Value);
// Flush
void RB_Flush (RingBuffer_t *Buf);

#endif /* INC_RING_BUFFER_H_ */
