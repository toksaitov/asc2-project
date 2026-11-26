# Addition and subtraction: `add`, `adc`, `sub`, `sbb`, `cmp`, and their flags.
#
# The lowest four bits of `flags` are C (0x1), Z (0x2), S (0x4), and O (0x8).
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    # 0xffffffff + 1 is zero with a carry out.
    li 0x1 r7
    li -1 r0
    add 0x1 r0
    mov flags r1
    cmp 0x3 r1
    jne fail
    cmp 0x0 r0
    jne fail

    # The largest positive number plus 1 overflows into a negative one.
    li 0x2 r7
    li 0x7fffffff r0
    add 0x1 r0
    mov flags r1
    cmp 0xc r1
    jne fail

    # Two of the smallest negative number give zero with a carry and an overflow.
    li 0x3 r7
    li 0x80000000 r0
    mov r0 r1
    add r1 r0
    mov flags r1
    cmp 0xb r1
    jne fail

    # A plain sum sets no flag.
    li 0x4 r7
    li 0x2 r0
    li 0x3 r1
    add r1 r0
    mov flags r1
    cmp 0x0 r1
    jne fail
    cmp 0x5 r0
    jne fail

    # `adc` adds the carry of the lower half to the upper half of a 64-bit sum.
    li 0x5 r7
    li -1 r0
    li 0x1 r1
    add 0x1 r0
    adc 0x0 r1
    cmp 0x0 r0
    jne fail
    cmp 0x2 r1
    jne fail

    # `adc` with a carry in and a carry out.
    li 0x6 r7
    li -1 r0
    add 0x1 r0
    li -1 r1
    adc r1 r1
    mov flags r2
    cmp 0x5 r2
    jne fail
    cmp -1 r1
    jne fail

    # 5 - 7 borrows and is negative.
    li 0x7 r7
    li 0x5 r0
    sub 0x7 r0
    mov flags r1
    cmp 0x5 r1
    jne fail
    cmp -2 r0
    jne fail

    # The smallest negative number minus 1 overflows into a positive one.
    li 0x8 r7
    li 0x80000000 r0
    sub 0x1 r0
    mov flags r1
    cmp 0x8 r1
    jne fail

    # Equal numbers give zero.
    li 0x9 r7
    li 0x5 r0
    li 0x5 r1
    sub r1 r0
    mov flags r1
    cmp 0x2 r1
    jne fail

    # `cmp` sets the flags of a subtraction and keeps both operands.
    li 0xa r7
    li 0x5 r0
    li 0x7 r1
    cmp r1 r0
    mov flags r2
    cmp 0x5 r2
    jne fail
    cmp 0x5 r0
    jne fail
    cmp 0x7 r1
    jne fail

    # `sbb` subtracts the borrow of the lower half from the upper half of a 64-bit difference.
    li 0xb r7
    li 0x0 r0
    li 0x1 r1
    sub 0x1 r0
    sbb 0x0 r1
    mov flags r2
    cmp 0x2 r2
    jne fail
    cmp -1 r0
    jne fail
    cmp 0x0 r1
    jne fail

    # `sbb` with a borrow in and a borrow out.
    li 0xc r7
    li 0x0 r0
    sub 0x1 r0
    li 0x0 r1
    li 0x0 r2
    sbb r2 r1
    mov flags r2
    cmp 0x5 r2
    jne fail
    cmp -1 r1
    jne fail

    # An immediate value is extended with its sign.
    li 0xd r7
    li 0x5 r0
    add -1 r0
    cmp 0x4 r0
    jne fail
    sub -3 r0
    cmp 0x7 r0
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt
