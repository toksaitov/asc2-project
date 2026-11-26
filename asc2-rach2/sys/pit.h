/*
    The timer of the chip. It interrupts the CPU at a fixed period, and the
    kernel counts the interrupts and lets another process run at each of them.
*/

#ifndef _PIT_H_
#define _PIT_H_

#include "sys/lib/kint.h"

/*
    Steps of the timer between two interrupts. A step is 256 clocks of the
    chip; the emulator makes one step per instruction.
*/
#define PIT_PERIOD 0x2000

// Starts the timer with a period in steps and lets it interrupt the CPU.
void init_pit(uint16_t period);

// Returns the number of timer interrupts since the start of the timer.
uint32_t get_ticks(void);

// Lets the running process sleep until `count` more timer interrupts have come.
void nap(uint32_t count);

#endif
