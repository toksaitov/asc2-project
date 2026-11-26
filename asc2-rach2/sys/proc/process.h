/*
    Processes: the programs that run, each with its own memory.
*/

#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <stdbool.h>

#include "sys/isr.h"
#include "sys/machine.h"
#include "sys/fs/file.h"
#include "sys/lib/kint.h"
#include "sys/proc/exec.h"

#define MAX_PROCS 8

#define INIT_PID 1 // the first process; the machine stops when the last process has exited, with the result of this one

/*
    What `waitpid` reports of a process that has ended, as Linux does: the
    exit code in the second byte if the process has called `exit`, and a
    number in the low 7 bits if it was stopped. Linux puts the number of the
    signal there; the kernel reports the signal of a wrong memory access for
    every mistake of a program.
*/
#define STATUS_OF_EXIT(code) (((code) & 0xff) << 8)
#define EXIT_CODE_OF(status) (((status) >> 8) & 0xff)
#define STATUS_STOPPED       11

typedef enum proc_state
{
    PROC_UNUSED,   // the entry of the table is free
    PROC_READY,    // waits for its turn on the CPU
    PROC_RUNNING,  // runs now
    PROC_SLEEPING, // waits for something to happen
    PROC_ZOMBIE    // has exited; its entry is not free yet
} proc_state_t;

typedef struct process
{
    uint32_t pid;
    proc_state_t state;
    struct process *parent;  // the process that has made this one, or NULL when that process has ended
    void *channel;           // what a sleeping process waits for
    int32_t status;          // what `waitpid` reports of a process that has exited
    uint32_t *page_dir;      // top page table
    void *kernel_stack;      // page of the kernel stack: the kernel runs on it when it works for the process
    uint32_t sp;             // stack pointer of the kernel stack while the process waits for its turn
    uint32_t ticks;          // timer interrupts that have arrived while the process ran
    uint32_t slice;          // timer interrupts that are left of the turn
    uint32_t turn_ticks;     // timer interrupts that have arrived in the turn
    uint32_t turn_calls;     // system calls of the turn
    uint32_t inputs;         // what the network of `slice.c` knows about the process, as four signed bytes
    bool is_expected_to_sleep; // what the network has guessed for the turn
    bool has_slept;          // the turn before this one has ended in a sleep
    bool is_woken;           // the process was woken up and has not run since
    uint32_t woken_at;       // number of timer interrupts at that wakeup
    uint32_t array[2 * CPU_DOT_ROWS]; // registers of the array of the dot products while the chip holds those of another process (`array.h`)
    file_t files[MAX_FILES]; // open files
} process_t;

extern process_t proc_table[MAX_PROCS];

// The process that runs now, or NULL while the scheduler chooses the next one
extern process_t *current_proc;

// Marks every entry of the process table free.
void init_proc(void);

/*
    Makes a process from a program of the disk, with the arguments and the
    environment of `args` and the console as its open files. The process is
    ready to run from the first instruction of the program. Returns NULL if
    the table is full, memory has run out, or the file is not a program for
    this machine.
*/
process_t *create_process(const char *path, const exec_args_t *args);

/*
    Makes a child of the running process: a process with a copy of the memory
    and of the open files. `params` are the registers of the program as it
    has entered the kernel; the child continues from there with 0 as the
    result of the system call. Returns the number of the child, or a negative
    number of an error.
*/
int32_t fork_process(const isr_params_t *params);

/*
    Gives the running process another program of the disk in place of its
    own. `params` are the registers of the program as it has entered the
    kernel; they become those of the new program at its first instruction.
    Returns 0, or a negative number of an error with the old program
    untouched.
*/
int32_t exec_process(const char *path, const exec_args_t *args, isr_params_t *params);

/*
    Waits until a child of the running process has ended, and frees its
    entry: the child with the number `pid`, or any child if `pid` is -1.
    `status` gets what the child has left (`STATUS_` values). Returns the
    number of the child, or `-ECHILD` if there is no such child.
*/
int32_t wait_process(int32_t pid, int32_t *status);

// Ends the running process with a status (`STATUS_` values) and frees its memory. Does not return.
void exit_process(int32_t status);

// Frees the entry and the kernel stack of a process that has exited. The process must not run.
void free_process(process_t *proc);

// Returns the status of the first process.
int32_t get_init_status(void);

#endif
