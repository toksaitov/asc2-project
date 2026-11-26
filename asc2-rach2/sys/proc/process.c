/*
    Processes.

    A process is an entry of the process table with the things that the
    kernel keeps for a running program: the page tables, which give the
    program its own memory; a kernel stack, on which the kernel works for the
    process and keeps the registers of the program while another process
    runs; the open files; and the state.

    The first process comes from a program of the disk. Every other process
    is made by `fork` as a copy of its parent, and becomes another program
    with `exec`. A process that ends stays in the table as a zombie until its
    parent has asked for its status with `waitpid`.
*/

#include "sys/proc/process.h"

#include <stdbool.h>
#include <stddef.h>

#include "sys/cpu.h"
#include "sys/machine.h"
#include "sys/screen.h"
#include "sys/syscall.h"
#include "sys/lib/kstring.h"
#include "sys/mm/pages.h"
#include "sys/mm/paging.h"
#include "sys/proc/array.h"
#include "sys/proc/sched.h"
#include "sys/proc/slice.h"

process_t proc_table[MAX_PROCS];
process_t *current_proc;

static uint32_t next_pid = INIT_PID;
static int32_t init_status = STATUS_STOPPED;

void init_proc(void)
{
    memset(proc_table, 0, sizeof(proc_table));
    current_proc = NULL;
}

static process_t *alloc_process(void)
{
    for (uint32_t i = 0; i < MAX_PROCS; ++i) {
        if (proc_table[i].state == PROC_UNUSED) {
            return &proc_table[i];
        }
    }

    return NULL;
}

/*
    Prepares the kernel stack of a process that has never run.

    The scheduler starts every process in the same way: `switch_context` pops
    the registers that it keeps and returns. So the stack gets these registers
    with `isr_exit` as the address to return to, and above them the registers
    that `isr_exit` loads: `params`, those of a program that has entered the
    kernel. `isr_exit` then continues the program in user mode.
*/
static void init_kernel_stack(process_t *proc, const isr_params_t *params)
{
    uint8_t *top = (uint8_t *) proc->kernel_stack + PAGE_SIZE;

    isr_params_t *stack_params = (isr_params_t *) (top - sizeof(isr_params_t));
    memcpy(stack_params, params, sizeof(*params));

    switch_frame_t *frame = (switch_frame_t *) ((uint8_t *) stack_params - sizeof(switch_frame_t));
    memset(frame, 0, sizeof(*frame));
    frame->lr = (uint32_t) isr_exit;

    proc->sp = (uint32_t) frame;
}

/*
    Sets the registers of a program that starts: its first instruction, user
    mode with interrupts on, its stack pointer, and zeros in the rest.
*/
static void start_program(isr_params_t *params, const program_t *program)
{
    memset(params, 0, sizeof(*params));
    params->ip = program->entry;
    params->flags = CPU_FLAG_U | CPU_FLAG_I;
    params->usp = program->sp;
}

process_t *create_process(const char *path, const exec_args_t *args)
{
    process_t *proc = alloc_process();
    if (proc == NULL) {
        return NULL;
    }

    proc->kernel_stack = take_page();
    if (proc->kernel_stack == NULL) {
        return NULL;
    }

    program_t program;
    if (load_program(path, args, &program) != 0) {
        free_page(proc->kernel_stack);
        return NULL;
    }
    proc->page_dir = program.page_dir;

    isr_params_t params;
    start_program(&params, &program);
    init_kernel_stack(proc, &params);

    init_files(proc->files);

    proc->pid = next_pid++;
    proc->parent = NULL;
    proc->ticks = 0;
    init_slice(proc);
    reset_array(proc);
    proc->state = PROC_READY;

    return proc;
}

int32_t fork_process(const isr_params_t *params)
{
    process_t *parent = current_proc;

    process_t *child = alloc_process();
    if (child == NULL) {
        return -EAGAIN;
    }

    // Whatever the page had before does no harm: a stack is written before it is read
    child->kernel_stack = take_page();
    if (child->kernel_stack == NULL) {
        return -ENOMEM;
    }
    child->page_dir = copy_page_dir(parent->page_dir);
    if (child->page_dir == NULL) {
        free_page(child->kernel_stack);
        return -ENOMEM;
    }

    // The child is the parent at the same place of the same program; only the result of `fork` tells them apart
    isr_params_t child_params;
    memcpy(&child_params, params, sizeof(*params));
    child_params.r0 = 0;
    init_kernel_stack(child, &child_params);

    memcpy(child->files, parent->files, sizeof(parent->files));

    child->pid = next_pid++;
    child->parent = parent;
    child->ticks = 0;
    init_slice(child);
    copy_array(child, parent);
    child->state = PROC_READY;

    return (int32_t) child->pid;
}

int32_t exec_process(const char *path, const exec_args_t *args, isr_params_t *params)
{
    process_t *proc = current_proc;

    // The new memory is built next to the old one, so that a failure leaves the old program as it was
    program_t program;
    int32_t result = load_program(path, args, &program);
    if (result != 0) {
        return result;
    }

    // The new program has not asked for the screen, and it starts with zeros in the registers of the array
    release_screen(proc);
    reset_array(proc);

    // The CPU must not keep the address of a table that is freed
    uint32_t *old_page_dir = proc->page_dir;
    proc->page_dir = program.page_dir;
    write_cr3(KERN_TO_PHYS(proc->page_dir));
    free_page_dir(old_page_dir);

    start_program(params, &program);

    return 0;
}

int32_t wait_process(int32_t pid, int32_t *status)
{
    for (;;) {
        bool has_child = false;
        for (uint32_t i = 0; i < MAX_PROCS; ++i) {
            process_t *proc = &proc_table[i];
            if (proc->state == PROC_UNUSED || proc->parent != current_proc ||
                (pid != -1 && proc->pid != (uint32_t) pid)) {
                continue;
            }

            has_child = true;
            if (proc->state == PROC_ZOMBIE) {
                int32_t child_pid = (int32_t) proc->pid;
                *status = proc->status;
                free_process(proc);

                return child_pid;
            }
        }
        if (!has_child) {
            return -ECHILD;
        }

        // A child that ends wakes its parent up
        sleep(current_proc);
    }
}

void exit_process(int32_t status)
{
    process_t *proc = current_proc;

    if (proc->pid == INIT_PID) {
        init_status = status;
    }

    // Nobody will ask for the status of the children any more
    for (uint32_t i = 0; i < MAX_PROCS; ++i) {
        process_t *child = &proc_table[i];
        if (child->state != PROC_UNUSED && child->parent == proc) {
            child->parent = NULL;
            if (child->state == PROC_ZOMBIE) {
                free_process(child);
            }
        }
    }

    // The console gets the screen back, however the process has ended
    release_screen(proc);

    // The values that the chip holds for the process are nobody's
    release_array(proc);

    // The kernel stack is in use until the scheduler takes over, so it is freed later
    free_page_dir(proc->page_dir);
    proc->page_dir = NULL;

    proc->status = status;
    proc->state = PROC_ZOMBIE;
    if (proc->parent != NULL) {
        wakeup(proc->parent);
    }

    switch_to_scheduler();
}

void free_process(process_t *proc)
{
    free_page(proc->kernel_stack);
    proc->kernel_stack = NULL;

    proc->parent = NULL;
    proc->state = PROC_UNUSED;
}

int32_t get_init_status(void)
{
    return init_status;
}
