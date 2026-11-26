/*
    The scheduler: gives the CPU to the ready processes in turns, and lets a
    process sleep until something that it waits for has happened.
*/

#ifndef _SCHED_H_
#define _SCHED_H_

#include "sys/lib/kint.h"

/*
    The registers that `switch_context` of `switch.s` keeps on a kernel stack,
    the last one pushed first.
*/
typedef struct switch_frame
{
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t fp;
    uint32_t lr; // address at which the code continues
} switch_frame_t;

void switch_context(uint32_t *old_sp, uint32_t new_sp);

// Runs the ready processes in turns. Returns when no process is left.
void scheduler(void);

// Gives the CPU to the next ready process. Returns when the turn of the running process comes again.
void yield(void);

/*
    Takes the running process off the CPU until `wakeup` is called with the
    same channel. A channel is the address of the thing that the process waits
    for. The caller checks that thing again after the return: another process
    may have taken it first.
*/
void sleep(void *channel);

// Makes every process that sleeps on the channel ready.
void wakeup(void *channel);

/*
    Leaves the running process for the scheduler. The caller has set the new
    state of the process. Returns when the scheduler chooses the process
    again, so it does not return for a process that has exited.
*/
void switch_to_scheduler(void);

#endif
