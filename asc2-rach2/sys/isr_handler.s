# The way into the kernel and back.
#
# Every entry of the interrupt vector table names `isr_entry`: an exception, a
# device interrupt, and `int` all arrive here, and the cause register tells
# them apart. The CPU has written nothing to memory. It has left the trap
# record in control registers: the address to continue at in `cr1`, the flags
# in `cr4`, the cause in `cr5`, and the fault address in `cr2`. It runs in
# kernel mode with interrupts off, so `sp` is the kernel's stack pointer, and
# the stack pointer of the user program waits in `cr6`.
#
# The entry saves the registers and the record on the kernel stack of the
# process. The saved words are a structure for C, `isr_params_t` of `isr.h`,
# with the last word pushed as its first field.
#
# While T of `cr0` (0x8) is set, an exception stops the CPU with a double
# fault, because it would overwrite a record that nobody has saved. The CPU
# sets T on the way in. The entry clears it when the record is on the stack,
# and the exit sets it again before it puts the record back.
#
# A new process starts at `isr_exit`: its kernel stack is built by hand with
# the words of a process that has just entered the kernel from its first
# instruction.

.global isr_entry
.global isr_exit

.section .text
isr_entry:
    push lr
    push fp
    push r7
    push r6
    push r5
    push r4
    push r3
    push r2
    push r1
    push r0
    mov cr1 r0
    push r0
    mov cr4 r0
    push r0
    mov cr5 r0
    push r0
    mov cr2 r0
    push r0
    mov cr6 r0
    push r0

    mov cr0 r0
    and -0x9 r0
    mov r0 cr0

    # isr_router(the address of the saved words)
    push sp
    call isr_router
    add 0x4 sp

isr_exit:
    mov cr0 r0
    or 0x8 r0
    mov r0 cr0

    pop r0
    mov r0 cr6
    # The fault address and the cause are not put back
    add 0x8 sp
    pop r0
    mov r0 cr4
    pop r0
    mov r0 cr1
    pop r0
    pop r1
    pop r2
    pop r3
    pop r4
    pop r5
    pop r6
    pop r7
    pop fp
    pop lr
    iret
