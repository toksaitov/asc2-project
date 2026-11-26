/*
    The instructions that C has no words for, written in `cpu.s`: the IO
    ports, the control registers, the dot products, and `hlt`.
*/

#ifndef _CPU_H_
#define _CPU_H_

#include "sys/lib/kint.h"

/* Codes that `k_halt` leaves in `r0` */

#define HALT_OK     0x600d // the first process has exited with 0
#define HALT_FAILED 0x0bad // the first process has exited with another value, or it was stopped
#define HALT_PANIC  0xdead // the kernel has found a mistake of its own

// Reads a byte from an IO port of the chip.
uint8_t read_port(uint8_t port);

// Writes a byte to an IO port of the chip.
void write_port(uint8_t port, uint8_t value);

// Reads the switches of the CPU (`CPU_CTRL_` bits).
uint32_t read_cr0(void);

// Writes the switches of the CPU (`CPU_CTRL_` bits).
void write_cr0(uint32_t value);

// Sets the physical address of the top page table. The CPU forgets every translation that it remembers.
void write_cr3(uint32_t value);

/*
    Takes `a` and `b` as four signed bytes each, multiplies the bytes in the
    same places, and returns `sum` with the four products added (the
    instruction `dot4`, the dot-product unit of the chip).
*/
int32_t dot4(uint32_t a, uint32_t b, int32_t sum);

/*
    Stores the registers of the array of the dot products at `regs`: the
    weights `w0`-`w2`, then the inputs `x0`-`x2`.
*/
void save_array(uint32_t *regs);

// Loads the registers of the array of the dot products from `regs`, in the order of `save_array`.
void load_array(const uint32_t *regs);

// Stops the CPU until the next interrupt, and returns when its handler has run. Interrupts are off before and after the call.
void wait_for_interrupt(void);

// Stops the machine for good with a code in `r0`. Does not return.
void k_halt(uint32_t code);

#endif
