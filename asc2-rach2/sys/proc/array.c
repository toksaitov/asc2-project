/*
    The array of the dot products of the chip, shared by the processes.

    The array has registers of its own: the weights `w0`-`w2` and the inputs
    `x0`-`x2`. A program keeps values in them from one instruction to the
    next, so every process needs values of its own, as it has its own values
    of `r0`-`r7`.

    The kernel does not save and load the six registers at every switch.
    Most processes never use the array, and each register costs an
    instruction and an access of memory. It does what the kernels of the PC
    do with the registers of the floating-point unit: it moves the registers
    when a process asks for them. The chip has a switch for that, the bit A
    of `cr0`. While the bit is clear, an instruction of a program that names
    a register of the array raises the privileged instruction exception, and
    changes nothing.

    * The kernel remembers the owner: the process whose values are in the
      registers of the chip.
    * A process that is not the owner runs with the bit clear. Its first
      instruction for the array enters the kernel. The kernel stores the
      registers of the chip for the old owner, loads the values of the
      process, makes it the owner, sets the bit, and returns. The CPU runs
      the instruction again.
    * The owner runs with the bit set, and the kernel does not see its
      instructions.

    A program starts with zeros in the registers, and a child of `fork` has
    the values of its parent. When the owner ends, the array has no owner,
    and the values in the chip are nobody's.

    The instruction `dot4` names no register of the array and is never
    refused. The network of `slice.c` uses it, so the kernel changes no value
    of a process.
*/

#include "sys/proc/array.h"

#include <stdbool.h>
#include <stddef.h>

#include "sys/cpu.h"
#include "sys/isr.h"
#include "sys/machine.h"
#include "sys/lib/kstring.h"

static process_t *owner; // the process whose values are in the registers of the chip, or NULL

// Sets or clears the bit A of `cr0`.
static void set_switch(bool is_on)
{
    uint32_t ctrl = read_cr0() & ~CPU_CTRL_A;

    write_cr0(is_on ? ctrl | CPU_CTRL_A : ctrl);
}

/*
    Handles the privileged instruction exception: gives the array to a
    process that is not the owner.

    The handler does not read the instruction. The owner runs with the
    switch on, so the CPU has refused its instruction for another reason, and
    that is a mistake of the program. An instruction of another process may
    be such a mistake too: it gets the array, runs again, is refused again,
    and now it is the owner's.
*/
static void privileged_handler(isr_params_t *params)
{
    process_t *proc = current_proc;

    if (!(params->flags & CPU_FLAG_U) || proc == owner) {
        fail_isr(params);
    }

    if (owner != NULL) {
        save_array(owner->array);
    }
    load_array(proc->array);
    owner = proc;

    // The exception returns to the instruction, and now the CPU runs it
    set_switch(true);
}

void init_array(void)
{
    owner = NULL;
    register_isr_handler(CPU_EXCPT_PRIVILEGED, privileged_handler);
}

void reset_array(process_t *proc)
{
    memset(proc->array, 0, sizeof(proc->array));

    // The owner goes on with the switch on, so the chip must hold its new values
    if (proc == owner) {
        load_array(proc->array);
    }
}

void release_array(process_t *proc)
{
    if (proc == owner) {
        owner = NULL;
    }
}

void copy_array(process_t *child, process_t *parent)
{
    // The newest values of the owner are in the chip
    if (parent == owner) {
        save_array(parent->array);
    }

    memcpy(child->array, parent->array, sizeof(parent->array));
}

void switch_array(const process_t *proc)
{
    set_switch(proc == owner);
}
