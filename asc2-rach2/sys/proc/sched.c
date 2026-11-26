/*
    The scheduler.

    The kernel has one stack per process and one more, the boot stack, for the
    scheduler. `scheduler` is a loop on the boot stack: it chooses the next
    ready process and switches to the kernel stack of that process, where the
    process continues. The process comes back to the loop when a timer
    interrupt ends its turn, when it goes to sleep, or when it exits. The
    processes get the CPU in the order of the process table. A turn lasts one
    or four periods of the timer; `slice.c` chooses.

    A process always enters the kernel through `isr_entry`, so the code that a
    switch continues is the kernel's, and the registers of the program are
    saved on the kernel stack of its process.

    A process sleeps when it needs something that is not there yet: a line
    from the console, the end of a disk write, the end of a child. The
    kernel runs with interrupts off, so nothing can happen between the look
    at the thing and the sleep, and no wakeup is lost.

    When every process sleeps, the loop halts the CPU with interrupts on. The
    handler of the next interrupt runs on the boot stack and may wake a
    process up.
*/

#include "sys/proc/sched.h"

#include <stdbool.h>
#include <stddef.h>

#include "sys/cpu.h"
#include "sys/mm/pages.h"
#include "sys/proc/array.h"
#include "sys/proc/process.h"
#include "sys/proc/slice.h"

static uint32_t sched_sp; // stack pointer of the boot stack while a process runs

void scheduler(void)
{
    uint32_t next = 0; // the entry of the table at which the search for a ready process starts

    for (;;) {
        process_t *proc = NULL;
        bool is_any_asleep = false;
        for (uint32_t i = 0; i < MAX_PROCS && proc == NULL; ++i) {
            if (proc_table[next].state == PROC_READY) {
                proc = &proc_table[next];
            } else if (proc_table[next].state == PROC_SLEEPING) {
                is_any_asleep = true;
            }
            if (++next == MAX_PROCS) {
                next = 0;
            }
        }
        if (proc == NULL) {
            if (!is_any_asleep) {
                return;
            }

            wait_for_interrupt();
            continue;
        }

        // The CPU translates the addresses of the program through the tables of its process
        write_cr3(KERN_TO_PHYS(proc->page_dir));

        // An instruction for the array of the chip enters the kernel unless the chip holds the values of this process
        switch_array(proc);

        // The turn has as many timer interrupts as the network of `slice.c` gives it
        start_turn(proc);

        proc->state = PROC_RUNNING;
        current_proc = proc;
        switch_context(&sched_sp, proc->sp);
        current_proc = NULL;

        // The process is back: the timer has ended its turn, or it sleeps, or it has ended
        end_turn(proc, proc->state == PROC_READY ? TURN_END_TIMER : proc->state == PROC_SLEEPING ? TURN_END_SLEEP : TURN_END_EXIT);

        // A process that has exited runs on its kernel stack to the last, so the stack is freed here, or by the parent later
        if (proc->state == PROC_ZOMBIE && proc->parent == NULL) {
            free_process(proc);
        }
    }
}

void switch_to_scheduler(void)
{
    switch_context(&current_proc->sp, sched_sp);
}

void yield(void)
{
    current_proc->state = PROC_READY;
    switch_to_scheduler();
}

void sleep(void *channel)
{
    current_proc->channel = channel;
    current_proc->state = PROC_SLEEPING;
    switch_to_scheduler();
    current_proc->channel = NULL;
}

void wakeup(void *channel)
{
    for (uint32_t i = 0; i < MAX_PROCS; ++i) {
        if (proc_table[i].state == PROC_SLEEPING && proc_table[i].channel == channel) {
            proc_table[i].state = PROC_READY;
            note_wakeup(&proc_table[i]);
        }
    }
}
