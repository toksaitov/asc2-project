# The first instructions of a C test program.
#
# The CPU starts here after a reset, so the linker gets this file first. C
# code needs two things before `main` can run: a stack, and zeros in the
# variables that have no starting value. The linker gathers those variables
# after the data and marks the range with the symbols `_edata` and `_end`.
#
# `main` returns 0 if every check has passed, or the number of the check that
# has failed. The runner of the tests reads the result in `r0`: 0x600d is a
# pass, and anything else is the number of a check.

.global _start

.section .text
_start:
    li 0x80030000 sp

    # From the reset on, every instruction fetch carries its address on the
    # bus (the F bit of `cr0`, 0x4). With the bit clear the CPU asks the
    # RP2040 for "the next word" while it runs through consecutive
    # instructions, which is faster. On a chip whose sequential fetch does
    # not work, these three instructions are left out.
    mov cr0 r0
    and -5 r0
    mov r0 cr0

    li _edata r0
    li _end r1
    li 0x0 r2
_zero_next:
    cmp r1 r0
    jae _zero_end
    srb r2 0(r0)
    add 0x1 r0
    jmp _zero_next
_zero_end:

    call main
    cmp 0x0 r0
    jne _park
    li 0x600d r0
_park:
    hlt
