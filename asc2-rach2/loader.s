# The first instructions of the kernel.
#
# The CPU starts here after a reset, in kernel mode with paging and interrupts
# off, so the linker gets this file first. The RP2040 has already copied the
# variables that have a starting value into RAM. C code needs two more things:
# a stack, and zeros in the other variables, which the linker has gathered
# between the symbols `_edata` and `_end`.
#
# The first page of RAM starts with the interrupt vector table, which takes
# 0x400 bytes. The rest of the page is the stack of the kernel until the first
# process runs, and the stack of the scheduler after that.

.global _start

.section .text
_start:
    li 0x80001000 sp

    # From the reset on, every instruction fetch carries its address on the
    # bus (the F bit of `cr0`, 0x4). With the bit clear the CPU asks the
    # RP2040 for "the next word" while it runs through consecutive
    # instructions, which is faster. On a chip whose sequential fetch does
    # not work, these three instructions are left out.
    mov cr0 r0
    and -5 r0
    mov r0 cr0

    # The linker starts these variables at a multiple of 4, so they are
    # cleared a word at a time
    li _edata r0
    li _end r1
    li 0x0 r2
_zero_next:
    cmp r1 r0
    jae _zero_end
    sr r2 0(r0)
    add 0x4 r0
    jmp _zero_next
_zero_end:

    # `kernel_main` does not return
    call kernel_main
_stop:
    cli
    hlt
    jmp _stop
