/*
    The interrupt controller of the chip. It has eight lines for devices, and
    a mask decides which of them may interrupt the CPU.
*/

#ifndef _PIC_H_
#define _PIC_H_

#include "sys/lib/kint.h"

// Gives the lines their vectors and masks all of them.
void init_pic(void);

// Lets a line interrupt the CPU.
void enable_irq(uint8_t irq);

#endif
