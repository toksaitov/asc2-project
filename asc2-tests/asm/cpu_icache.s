# The instruction cache: which fetches go to the bus.
#
# With the C bit of `cr0` (0x10) set the CPU keeps the instructions that it
# has fetched, 32 words in four lines of eight. A fetch of a word that the
# cache holds asks nothing of the bus. The cache is emptied by an interrupt,
# an exception, `int`, `iret`, and a write to a control register. The CPU
# does not notice a write to memory: a program that writes instructions and
# runs them must pass one of these in between.
#
# The program cannot see the bus. The runner of the tests writes down every
# request that the CPU sends, and the list must equal `cpu_icache.bus`.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80020000 sp

    li 0x80000000 r0
    li on_int r1
    sr r1 24(r0)
    li 0x0 r6

    # A reset leaves the cache off: C is clear
    li 0x1 r7
    mov cr0 r0
    cmp 0x4 r0
    jne fail

    # A loop with the cache off: every turn fetches its instructions
    li 0x2 r7
    li 0x0 r2
    li 0x4 r3
_loop_off:
    add 0x3 r2
    sub 0x1 r3
    cmp 0x0 r3
    jne _loop_off
    cmp 0xc r2
    jne fail

    # The cache on. C reads back.
    li 0x3 r7
    li 0x14 r0
    mov r0 cr0
    mov cr0 r0
    cmp 0x14 r0
    jne fail

    # The same loop: the first turn fetches, the others do not
    li 0x4 r7
    li 0x0 r2
    li 0x8 r3
_loop_on:
    add 0x3 r2
    sub 0x1 r3
    cmp 0x0 r3
    jne _loop_on
    cmp 0x18 r2
    jne fail

    # A loop that calls: both places stay in the cache
    li 0x5 r7
    li 0x0 r2
    li 0x4 r3
_loop_call:
    call leaf
    sub 0x1 r3
    cmp 0x0 r3
    jne _loop_call
    cmp 0x4 r2
    jne fail

    # `int` and `iret` empty the cache: the loop fetches again after each
    li 0x6 r7
    li 0x3 r3
_loop_int:
    int 0x1
    sub 0x1 r3
    cmp 0x0 r3
    jne _loop_int
    cmp 0x3 r6
    jne fail

    # A write to memory is not noticed. `five` is copied to RAM and called,
    # then `nine` is copied over it: the call after that still runs `five`
    # from the cache, and after a write to a control register it runs `nine`.
    # The copy lies in line 1 of the cache, and the instructions of the
    # program between the calls lie in lines 2, 3, and 0, so they leave it
    # alone.
    li 0x7 r7
    li 0x800100a0 r2
    li five r1
    lr 0(r1) r0
    sr r0 0(r2)
    lr 4(r1) r0
    sr r0 4(r2)
    call r2
    cmp 0x5 r4
    jne fail
    li nine r1
    lr 0(r1) r0
    sr r0 0(r2)
    call r2
    cmp 0x5 r4
    jne fail
    mov cr2 r0
    mov r0 cr2
    call r2
    cmp 0x9 r4
    jne fail

    # Two blocks with the same line number, 64 words apart. Each call takes
    # the line from the other, so every call fetches.
    li 0x8 r7
    li five r1
    li 0x80010180 r2
    lr 0(r1) r0
    sr r0 0(r2)
    lr 4(r1) r0
    sr r0 4(r2)
    li 0x80010280 r5
    lr 0(r1) r0
    sr r0 0(r5)
    lr 4(r1) r0
    sr r0 4(r5)
    call r2
    call r5
    call r2
    call r5

    # The cache off again: every fetch goes to the bus
    li 0x9 r7
    li 0x4 r0
    mov r0 cr0
    li 0x0 r2
    li 0x3 r3
_loop_last:
    add 0x3 r2
    sub 0x1 r3
    cmp 0x0 r3
    jne _loop_last
    cmp 0x9 r2
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

leaf:
    add 0x1 r2
    ret

five:
    li 0x5 r4
    ret

nine:
    li 0x9 r4
    ret

on_int:
    add 0x1 r6
    iret
