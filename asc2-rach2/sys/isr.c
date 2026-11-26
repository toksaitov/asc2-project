/*
    The way into the kernel: exceptions, device interrupts, and `int`.

    The CPU finds the handler of an event in the interrupt vector table at
    the physical address 0. Every entry of the table names `isr_entry`, which
    saves the registers and calls `isr_router`. The router reads the vector
    from the cause and calls the function registered for it.
*/

#include "sys/isr.h"

#include <stdbool.h>
#include <stddef.h>

#include "sys/cpu.h"
#include "sys/kio.h"
#include "sys/machine.h"
#include "sys/proc/process.h"

static const char *const EXCPT_NAMES[CPU_EXCPTS] = {
    "invalid instruction",
    "privileged instruction",
    "misaligned address",
    "page fault",
    "TLB miss",
    "bus error",
    "unknown number of an int instruction",
    "unknown exception"
};

static isr_handler_t isr_handlers[ISR_VECTORS];

void init_isr(void)
{
    uint32_t *ivt = (uint32_t *) KERN_BASE;

    for (uint32_t i = 0; i < IVT_SIZE / sizeof(uint32_t); ++i) {
        ivt[i] = (uint32_t) isr_entry;
    }
}

void register_isr_handler(uint8_t vector, isr_handler_t handler)
{
    isr_handlers[vector] = handler;
}

void fail_isr(isr_params_t *params)
{
    uint32_t vector = params->cause & CPU_CAUSE_VEC_MASK;
    const char *name = vector < CPU_EXCPTS ? EXCPT_NAMES[vector] : "unknown interrupt";

    // Only an exception of a memory access has a fault address
    bool has_addr = vector >= CPU_EXCPT_MISALIGNED && vector <= CPU_EXCPT_BUS_ERROR;

    if (params->flags & CPU_FLAG_U) {
        k_printf("Process %u is stopped: %s", current_proc->pid, name);
    } else {
        k_printf("Kernel panic: %s, cause %p", name, params->cause);
    }
    if (has_addr) {
        k_printf(" at %p", params->fault_addr);
    }
    k_printf(" (ip %p)\n", params->ip);

    if (params->flags & CPU_FLAG_U) {
        exit_process(STATUS_STOPPED);
    } else {
        k_halt(HALT_PANIC);
    }
}

/*
    Called by `isr_entry` with the address of the saved registers.
*/
void isr_router(isr_params_t *params)
{
    uint32_t vector = params->cause & CPU_CAUSE_VEC_MASK;

    if (vector < ISR_VECTORS && isr_handlers[vector] != NULL) {
        isr_handlers[vector](params);
    } else {
        fail_isr(params);
    }
}
