# The part of `mark.c` that is written in assembly.
#
# The caller has pushed the arguments, the first one on the top of the stack.
# A function may change `r0`-`r3` and keeps every other register.

.global mix

.section .text

# unsigned int mix(unsigned int seed, int rounds)
#
# Stirs a number `rounds` times, each time with three shifts and three
# exclusive ors, and returns it. The loop is eleven instructions long, so it
# runs out of the cache of the chip: it measures the processor itself and
# not the bus.
mix:
    lr 0(sp) r0
    lr 4(sp) r2
_mix_next:
    mov r0 r1
    shl 0xd r1
    xor r1 r0
    mov r0 r1
    shr 0x11 r1
    xor r1 r0
    mov r0 r1
    shl 0x5 r1
    xor r1 r0
    sub 0x1 r2
    jne _mix_next
    ret
