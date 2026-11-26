# Logic: `and`, `test`, `or`, `xor`, `not`, and their flags.
#
# The lowest four bits of `flags` are C (0x1), Z (0x2), S (0x4), and O (0x8).
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    # `and` keeps the bits set in both operands.
    li 0x1 r7
    li 0xff00ff00 r0
    li 0x0ff00ff0 r1
    and r1 r0
    mov flags r2
    cmp 0x0 r2
    jne fail
    li 0x0f000f00 r2
    cmp r2 r0
    jne fail

    # A logic instruction clears C and O left by an addition.
    li 0x2 r7
    li 0x80000000 r0
    mov r0 r1
    add r1 r0
    li 0x0 r0
    and 0x1 r0
    mov flags r2
    cmp 0x2 r2
    jne fail

    # `or` sets the bits of either operand; the top bit sets S.
    li 0x3 r7
    li 0x80000000 r0
    li 0x1 r1
    or r1 r0
    mov flags r2
    cmp 0x4 r2
    jne fail
    li 0x80000001 r2
    cmp r2 r0
    jne fail

    # `xor` of a value with itself is zero.
    li 0x4 r7
    li 0x12345678 r0
    xor r0 r0
    mov flags r2
    cmp 0x2 r2
    jne fail
    cmp 0x0 r0
    jne fail

    # `test` sets the flags of `and` and keeps both operands.
    li 0x5 r7
    li 0xf0 r0
    li 0x0f r1
    test r1 r0
    mov flags r2
    cmp 0x2 r2
    jne fail
    cmp 0xf0 r0
    jne fail
    cmp 0x0f r1
    jne fail

    # `not` inverts every bit and leaves the flags alone.
    li 0x6 r7
    li 0x0f0f0f0f r0
    cmp r0 r0
    not r0
    jne fail
    li 0xf0f0f0f0 r2
    cmp r2 r0
    jne fail

    # An immediate value is extended with its sign.
    li 0x7 r7
    li 0x12345678 r0
    and 0xff r0
    cmp 0x78 r0
    jne fail
    or -1 r0
    cmp -1 r0
    jne fail
    xor -1 r0
    cmp 0x0 r0
    jne fail
    li 0x12345678 r0
    and -256 r0
    li 0x12345600 r2
    cmp r2 r0
    jne fail
    test 0x100 r0
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt
