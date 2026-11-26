/*
    The way into the kernel: exceptions, device interrupts, and `int`.
*/

#ifndef _ISR_H_
#define _ISR_H_

#include "sys/lib/kint.h"

#define IRQ_VEC_OFF 8  // vector of line 0 of the interrupt controller; line n has the vector `IRQ_VEC_OFF + n`
#define ISR_VECTORS 16 // vectors in use: the exceptions of the CPU and the eight lines of the interrupt controller

/*
    The registers of the interrupted code, as `isr_entry` of `isr_handler.s`
    has saved them on the kernel stack. `isr_exit` loads them back, so a
    handler that changes a field changes the register of the code.
*/
typedef struct isr_params
{
    uint32_t usp;        // stack pointer of the user program (`cr6`)
    uint32_t fault_addr; // address that a memory exception could not access (`cr2`)
    uint32_t cause;      // vector and details of the event (`cr5`, `CPU_CAUSE_` bits)
    uint32_t flags;      // flags before the event (`cr4`); `CPU_FLAG_U` tells if a user program was interrupted
    uint32_t ip;         // address at which the code continues (`cr1`)
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t fp;
    uint32_t lr;
} isr_params_t;

typedef void (*isr_handler_t)(isr_params_t *params);

// The entry and the exit of `isr_handler.s`
void isr_entry(void);
void isr_exit(void);

// Fills the interrupt vector table.
void init_isr(void);

// Sets the function that handles the events of a vector.
void register_isr_handler(uint8_t vector, isr_handler_t handler);

/*
    Ends the handling of an event that nothing can repair. If a user program
    was interrupted, its process is stopped, and the others go on. If the
    kernel itself was interrupted, the machine stops. Does not return.
*/
void fail_isr(isr_params_t *params);

#endif
