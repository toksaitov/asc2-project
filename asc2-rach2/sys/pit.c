/*
    The timer of the chip.

    It has two IO ports. A byte written to the command port tells what the
    next byte written to the data port means: the high byte of the period, or
    the low byte. The write of the low byte starts a new period.

    The timer runs from the reset with the longest period, and the interrupt
    controller remembers a request of a masked line. So the first interrupt
    may arrive as soon as a program runs, and the first turn of the first
    process may be short.
*/

#include "sys/pit.h"

#include <stddef.h>

#include "sys/cpu.h"
#include "sys/isr.h"
#include "sys/machine.h"
#include "sys/pic.h"
#include "sys/rp.h"
#include "sys/proc/process.h"
#include "sys/proc/sched.h"

#define PIT_CMD_SET_HIGH 0 // the next byte for the data port is the high byte of the period
#define PIT_CMD_SET_LOW  1 // the next byte for the data port is the low byte of the period

static uint32_t ticks;
static uint32_t naps; // processes that sleep until a timer interrupt

/*
    What a process sleeps on until the timer interrupt with a number: an odd
    value, which is the address of nothing in the kernel.
*/
static void *get_nap_channel(uint32_t tick)
{
    return (void *) (tick << 1 | 1);
}

/*
    Counts the interrupt for the running process. When the turn of the
    process is used up, the next ready process gets the CPU. `yield` returns
    when the turn of this process comes again, and the handler then returns
    into the interrupted program.
*/
static void pit_handler(isr_params_t *params)
{
    ++ticks;
    update_uptime();

    if (naps > 0) {
        wakeup(get_nap_channel(ticks));
    }

    if (current_proc != NULL) {
        ++current_proc->ticks;
        ++current_proc->turn_ticks;
        if (current_proc->slice <= 1) {
            yield();
        } else {
            --current_proc->slice;
        }
    }
}

void init_pit(uint16_t period)
{
    register_isr_handler(IRQ_VEC_OFF + PIT_IRQ, pit_handler);

    write_port(PIT_CMD_IO_PORT, PIT_CMD_SET_HIGH);
    write_port(PIT_DATA_IO_PORT, (uint8_t) (period >> 8));
    write_port(PIT_CMD_IO_PORT, PIT_CMD_SET_LOW);
    write_port(PIT_DATA_IO_PORT, (uint8_t) period);

    enable_irq(PIT_IRQ);
}

uint32_t get_ticks(void)
{
    return ticks;
}

void nap(uint32_t count)
{
    if (count == 0) {
        return;
    }

    ++naps;
    sleep(get_nap_channel(ticks + count));
    --naps;
}
