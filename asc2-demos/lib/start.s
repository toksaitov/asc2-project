# The first instructions of a demo program.
#
# The CPU starts here after a reset, so the linker gets this file first. C
# code needs two things before `main` can run: a stack, and zeros in the
# variables that have no starting value. The linker gathers those variables
# after the data and marks the range with the symbols `_edata` and `_end`.
#
# When `main` returns, the CPU is parked with 0x600d in `r0`, as a test that
# has passed.

.global _start

.section .text
_start:
    li 0x80030000 sp

    # The CPU asks the RP2040 for "the next word" while it runs through
    # consecutive instructions, which is faster than a fetch with an address
    # (the F bit of `cr0`, 0x4, turns that off).
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
    li 0x600d r0
    hlt
