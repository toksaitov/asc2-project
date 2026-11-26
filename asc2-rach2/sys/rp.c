/*
    The RP2040 microcontroller next to the chip.

    The interrupt line of the RP2040 is a level: it is high while a device
    that is allowed to interrupt has something to say. The interrupt status
    register tells which devices these are, and the interrupt enable register
    holds the devices that are allowed. The console drops its request when
    its last byte is taken, and the disk when 1 is written to its bit of the
    status register.

    The clock of the RP2040 counts microseconds in 32 bits and starts again
    after 71 minutes. The kernel reads it at every timer interrupt and adds
    up the seconds.
*/

#include "sys/rp.h"

#include "sys/console.h"
#include "sys/disk.h"
#include "sys/isr.h"
#include "sys/machine.h"
#include "sys/pic.h"

#define US_PER_SECOND 1000000

static uint32_t irq_enable; // devices that may interrupt: a copy of the register, which spares a read

static uint32_t uptime;     // seconds since the start of the kernel
static uint32_t last_us;    // clock of the RP2040 at the last look
static uint32_t rest_us;    // microseconds that do not make a second yet

uint32_t read_rp(uint32_t addr)
{
    volatile uint32_t *reg = (volatile uint32_t *) (KERN_BASE + addr);

    return *reg;
}

void write_rp(uint32_t addr, uint32_t value)
{
    volatile uint32_t *reg = (volatile uint32_t *) (KERN_BASE + addr);

    *reg = value;
}

static void rp_handler(isr_params_t *params)
{
    uint32_t requests = read_rp(RP_IRQ_STAT_ADDR) & irq_enable;

    if (requests & RP_IRQ_FLAG_DISK) {
        handle_disk_irq();
    }
    if (requests & RP_IRQ_FLAG_CON) {
        handle_console_irq();
    }
}

void init_rp(void)
{
    irq_enable = 0;
    write_rp(RP_IRQ_ENABLE_ADDR, irq_enable);

    uptime = 0;
    rest_us = 0;
    last_us = read_rp(RP_TIME_ADDR);

    register_isr_handler(IRQ_VEC_OFF + RP_IRQ, rp_handler);
    enable_irq(RP_IRQ);
}

void enable_rp_irq(uint32_t flags)
{
    irq_enable |= flags;
    write_rp(RP_IRQ_ENABLE_ADDR, irq_enable);
}

void disable_rp_irq(uint32_t flags)
{
    irq_enable &= ~flags;
    write_rp(RP_IRQ_ENABLE_ADDR, irq_enable);
}

void update_uptime(void)
{
    uint32_t now_us = read_rp(RP_TIME_ADDR);

    // The difference is right also when the clock has started again
    rest_us += now_us - last_us;
    last_us = now_us;

    while (rest_us >= US_PER_SECOND) {
        rest_us -= US_PER_SECOND;
        ++uptime;
    }
}

uint32_t get_uptime(void)
{
    return uptime;
}
