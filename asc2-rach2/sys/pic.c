/*
    The interrupt controller of the chip.

    It has two IO ports. A byte written to the command port tells what the
    next byte written to the data port means: the mask, or the vector of line
    0. A read of the data port gives the mask. A bit of the mask that is 1
    keeps its line from interrupting the CPU.

    The controller forgets a request when the CPU takes it, so a handler has
    nothing to tell the controller at its end.
*/

#include "sys/pic.h"

#include "sys/cpu.h"
#include "sys/isr.h"
#include "sys/machine.h"

#define PIC_CMD_SET_MASK    0 // the next byte for the data port is the mask
#define PIC_CMD_SET_VEC_OFF 1 // the next byte for the data port is the vector of line 0

#define PIC_MASK_ALL 0xff

static void set_irq_mask(uint8_t mask)
{
    write_port(PIC_CMD_IO_PORT, PIC_CMD_SET_MASK);
    write_port(PIC_DATA_IO_PORT, mask);
}

void init_pic(void)
{
    write_port(PIC_CMD_IO_PORT, PIC_CMD_SET_VEC_OFF);
    write_port(PIC_DATA_IO_PORT, IRQ_VEC_OFF);

    set_irq_mask(PIC_MASK_ALL);
}

void enable_irq(uint8_t irq)
{
    uint8_t mask = read_port(PIC_DATA_IO_PORT);

    set_irq_mask(mask & ~(1 << irq));
}
