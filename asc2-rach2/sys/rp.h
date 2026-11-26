/*
    The RP2040 microcontroller next to the chip. It serves the memory of the
    machine, and it has devices of its own: a console, a disk, and a clock.
    Their registers are in memory, and all of them share one interrupt line.
*/

#ifndef _RP_H_
#define _RP_H_

#include "sys/lib/kint.h"

// Reads a device register of the RP2040.
uint32_t read_rp(uint32_t addr);

// Writes a device register of the RP2040.
void write_rp(uint32_t addr, uint32_t value);

// Sets the handler of the interrupt line of the RP2040. No device may interrupt yet.
void init_rp(void);

// Lets the devices of `flags` (`RP_IRQ_FLAG_` bits) interrupt the CPU.
void enable_rp_irq(uint32_t flags);

// Keeps the devices of `flags` (`RP_IRQ_FLAG_` bits) from interrupting the CPU.
void disable_rp_irq(uint32_t flags);

// Follows the clock of the RP2040. The timer handler calls it, which is often enough.
void update_uptime(void);

// Returns the number of seconds since the start of the kernel.
uint32_t get_uptime(void);

#endif
