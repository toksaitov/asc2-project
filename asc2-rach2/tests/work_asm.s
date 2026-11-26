# The parts of `work.c` that are written in assembly.
#
# The code of this file starts at a new page. The program then has code that
# runs on its second page, and the kernel must load every page of a program.

.global keep_registers
.global unknown_call

.section .text

    .align 0x1000

# int unknown_call(void)
#
# Makes a system call with a number that the kernel does not have, and returns
# its result.
unknown_call:
    li 0x7fff r0
    int 0x80
    ret

# int keep_registers(int ticks)
#
# Checks that the kernel gives every register back as it was.
#
# The function fills `r1`-`r7`, `fp`, and `lr` with known values and counts
# down in `r0` for a while, with a system call in the middle. The kernel may
# interrupt the count at any instruction and run other processes before this
# one goes on, and the system call must change `r0` only. The function then
# compares every register with its value. It repeats this until `ticks` timer
# interrupts have arrived. Returns 0 if no register has lost its value.
#
# Two words on the stack hold what the registers cannot: the number of timer
# interrupts at which the function ends, and above it a buffer of 16 bytes
# for the system call `times`.
keep_registers:
    push lr
    push fp
    push r7
    push r6
    push r5
    push r4
    sub 0x10 sp

    # The end: times(the buffer) plus the argument, which is above the buffer and the six saved registers
    mov sp r1
    li 0x2b r0
    int 0x80
    lr 0x28(sp) r1
    add r1 r0
    push r0

_fill:
    li 0x11111111 r1
    li 0x22222222 r2
    li 0x33333333 r3
    li 0x44444444 r4
    li 0x55555555 r5
    li 0x66666666 r6
    li 0x77777777 r7
    li 0x88888888 fp
    li 0x99999999 lr

    li 0x1000 r0
_count_1:
    sub 0x1 r0
    jne _count_1

    # getpid(), which takes no arguments
    li 0x14 r0
    int 0x80

    li 0x1000 r0
_count_2:
    sub 0x1 r0
    jne _count_2

    li 0x11111111 r0
    cmp r0 r1
    jne _lost
    li 0x22222222 r0
    cmp r0 r2
    jne _lost
    li 0x33333333 r0
    cmp r0 r3
    jne _lost
    li 0x44444444 r0
    cmp r0 r4
    jne _lost
    li 0x55555555 r0
    cmp r0 r5
    jne _lost
    li 0x66666666 r0
    cmp r0 r6
    jne _lost
    li 0x77777777 r0
    cmp r0 r7
    jne _lost
    li 0x88888888 r0
    cmp r0 fp
    jne _lost
    li 0x99999999 r0
    cmp r0 lr
    jne _lost

    # Again, if times(the buffer) is still less than the end
    mov sp r1
    add 0x4 r1
    li 0x2b r0
    int 0x80
    lr 0(sp) r1
    cmp r1 r0
    jl _fill

    li 0x0 r0
    jmp _return
_lost:
    li 0x1 r0
_return:
    add 0x14 sp
    pop r4
    pop r5
    pop r6
    pop r7
    pop fp
    pop lr
    ret
